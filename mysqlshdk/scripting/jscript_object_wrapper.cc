/*
 * Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms, as
 * designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "scripting/jscript_object_wrapper.h"
#include <iostream>

#include "mysqlshdk/include/scripting/jscript_collectable.h"
#include "mysqlshdk/libs/db/session.h"
#include "mysqlshdk/libs/utils/debug.h"
#include "scripting/jscript_context.h"

namespace shcore {

namespace {

static int magic_pointer = 0;
static int magic_method_pointer = 0;

DEBUG_OBJ_ENABLE(JSObjectWrapper);

class Object_collectable : public Collectable<Object_bridge> {
 public:
  Object_collectable(const std::shared_ptr<Object_bridge> &d,
                     v8::Isolate *isolate, const v8::Local<v8::Object> &object)
      : Collectable<Object_bridge>(d, isolate, object) {
    DEBUG_OBJ_ALLOC_N(JSObjectWrapper, data()->class_name());
  }

  ~Object_collectable() override { DEBUG_OBJ_DEALLOC(JSObjectWrapper); }
};

class Method_collectable : public Collectable<Object_bridge> {
 public:
  Method_collectable(const std::shared_ptr<Object_bridge> &d,
                     const std::string &method, v8::Isolate *isolate,
                     const v8::Local<v8::Object> &object)
      : Collectable<Object_bridge>(d, isolate, object), m_method{method} {
    DEBUG_OBJ_ALLOC_N(JSObjectWrapper, data()->class_name() + "." + method);
  }

  ~Method_collectable() override { DEBUG_OBJ_DEALLOC(JSObjectWrapper); }

  const std::string &method() const { return m_method; }

 private:
  std::string m_method;
};

v8::Handle<v8::Value> translate_exception(JScript_context *context) {
  try {
    throw;
  } catch (mysqlshdk::db::Error &e) {
    return context->shcore_value_to_v8_value(shcore::Value(
        shcore::Exception::mysql_error_with_code(e.what(), e.code()).error()));
  } catch (Exception &e) {
    return context->shcore_value_to_v8_value(shcore::Value(e.error()));
  }
}

}  // namespace

JScript_object_wrapper::JScript_object_wrapper(JScript_context *context,
                                               bool indexed)
    : _context(context), _method_wrapper(context) {
  v8::Handle<v8::ObjectTemplate> templ =
      v8::ObjectTemplate::New(_context->isolate());
  _object_template.Reset(_context->isolate(), templ);

  templ->SetNamedPropertyHandler(&JScript_object_wrapper::handler_getter,
                                 &JScript_object_wrapper::handler_setter,
                                 &JScript_object_wrapper::handler_query, 0,
                                 &JScript_object_wrapper::handler_enumerator);

  if (indexed)
    templ->SetIndexedPropertyHandler(
        &JScript_object_wrapper::handler_igetter,
        &JScript_object_wrapper::handler_isetter, 0, 0,
        &JScript_object_wrapper::handler_ienumerator);

  templ->SetInternalFieldCount(3);
}

JScript_object_wrapper::~JScript_object_wrapper() { _object_template.Reset(); }

v8::Handle<v8::Object> JScript_object_wrapper::wrap(
    std::shared_ptr<Object_bridge> object) {
  v8::Local<v8::ObjectTemplate> templ =
      v8::Local<v8::ObjectTemplate>::New(_context->isolate(), _object_template);
  if (!templ.IsEmpty()) {
    v8::Local<v8::Object> self(templ->NewInstance());
    if (!self.IsEmpty()) {
      const auto holder =
          new Object_collectable(object, _context->isolate(), self);

      self->SetAlignedPointerInInternalField(0, &magic_pointer);
      self->SetAlignedPointerInInternalField(1, holder);
      self->SetAlignedPointerInInternalField(2, this);
    }
    return self;
  }
  return {};
}

void JScript_object_wrapper::handler_getter(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value> &info) {
  v8::HandleScope hscope(info.GetIsolate());
  v8::Handle<v8::Object> obj(info.Holder());
  JScript_object_wrapper *self = static_cast<JScript_object_wrapper *>(
      obj->GetAlignedPointerFromInternalField(2));
  const auto &object = static_cast<Object_collectable *>(
                           obj->GetAlignedPointerFromInternalField(1))
                           ->data();
  if (!object) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(
        info.GetIsolate(), "Reference to invalid object"));
    return;
  }

  v8::String::Utf8Value prop(property);
  /*if (prop == "__members__")
  {
  std::vector<std::string> members(object->get_members());
  v8::Handle<v8::Array> marray = v8::Array::New(info.GetIsolate());
  int i = 0;
  for (std::vector<std::string>::const_iterator iter = members.begin(); iter !=
  members.end(); ++iter, ++i)
  {
  marray->Set(i, v8::String::NewFromUtf8(info.GetIsolate(), iter->c_str()));
  }
  info.GetReturnValue().Set(marray);
  }
  else*/
  if (strcmp(*prop, "length") == 0 && object->has_member("length")) {
    try {
      info.GetReturnValue().Set(self->_context->shcore_value_to_v8_value(
          object->get_member("length")));
      return;
    } catch (Exception &exc) {
      if (!exc.is_attribute())
        info.GetIsolate()->ThrowException(
            self->_context->shcore_value_to_v8_value(Value(exc.error())));
      // fallthrough
    } catch (std::exception &exc) {
      info.GetIsolate()->ThrowException(
          v8::String::NewFromUtf8(info.GetIsolate(), exc.what()));
    }
  }
  {
    try {
      if (object->has_method(*prop)) {
        info.GetReturnValue().Set(self->_method_wrapper.wrap(object, *prop));
      } else {
        Value member = object->get_member(*prop);
        info.GetReturnValue().Set(
            self->_context->shcore_value_to_v8_value(member));
      }
    } catch (...) {
      info.GetIsolate()->ThrowException(translate_exception(self->_context));
    }
  }
}

void JScript_object_wrapper::handler_setter(
    v8::Local<v8::String> property, v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value> &info) {
  v8::HandleScope hscope(info.GetIsolate());
  v8::Handle<v8::Object> obj(info.Holder());
  JScript_object_wrapper *self = static_cast<JScript_object_wrapper *>(
      obj->GetAlignedPointerFromInternalField(2));
  const auto &object = static_cast<Object_collectable *>(
                           obj->GetAlignedPointerFromInternalField(1))
                           ->data();
  if (!object) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(
        info.GetIsolate(), "Reference to invalid object"));
    return;
  }

  v8::String::Utf8Value prop(property);
  try {
    object->set_member(*prop, self->_context->v8_value_to_shcore_value(value));
    info.GetReturnValue().Set(value);
  } catch (Exception &exc) {
    info.GetIsolate()->ThrowException(
        self->_context->shcore_value_to_v8_value(Value(exc.error())));
  } catch (std::exception &exc) {
    info.GetIsolate()->ThrowException(
        v8::String::NewFromUtf8(info.GetIsolate(), exc.what()));
  }
}

void JScript_object_wrapper::handler_query(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Integer> &info) {
  v8::HandleScope hscope(info.GetIsolate());
  v8::Handle<v8::Object> obj(info.Holder());
  const auto &object = static_cast<Object_collectable *>(
                           obj->GetAlignedPointerFromInternalField(1))
                           ->data();
  if (!object) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(
        info.GetIsolate(), "Reference to invalid object"));
    return;
  }

  v8::String::Utf8Value prop(property);

  if (object->has_member(*prop)) {
    // v8::Integer::New(info.GetIsolate(), 1);
    info.GetReturnValue().Set(v8::None);
  }
}

void JScript_object_wrapper::handler_enumerator(
    const v8::PropertyCallbackInfo<v8::Array> &info) {
  v8::HandleScope hscope(info.GetIsolate());
  v8::Handle<v8::Object> obj(info.Holder());
  const auto &object = static_cast<Object_collectable *>(
                           obj->GetAlignedPointerFromInternalField(1))
                           ->data();
  if (!object) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(
        info.GetIsolate(), "Reference to invalid object"));
    return;
  }
  std::vector<std::string> members(object->get_members());
  v8::Handle<v8::Array> marray = v8::Array::New(info.GetIsolate());
  int i = 0;
  for (std::vector<std::string>::const_iterator iter = members.begin();
       iter != members.end(); ++iter, ++i) {
    marray->Set(i, v8::String::NewFromUtf8(info.GetIsolate(), iter->c_str()));
  }
  info.GetReturnValue().Set(marray);
}

void JScript_object_wrapper::handler_igetter(
    uint32_t i, const v8::PropertyCallbackInfo<v8::Value> &info) {
  v8::HandleScope hscope(info.GetIsolate());
  v8::Handle<v8::Object> obj(info.Holder());
  JScript_object_wrapper *self = static_cast<JScript_object_wrapper *>(
      obj->GetAlignedPointerFromInternalField(2));
  const auto &object = static_cast<Object_collectable *>(
                           obj->GetAlignedPointerFromInternalField(1))
                           ->data();
  if (!object) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(
        info.GetIsolate(), "Reference to invalid object"));
    return;
  }

  {
    try {
      Value member = object->get_member(i);
      info.GetReturnValue().Set(
          self->_context->shcore_value_to_v8_value(member));
    } catch (Exception &exc) {
      info.GetIsolate()->ThrowException(
          self->_context->shcore_value_to_v8_value(Value(exc.error())));
    } catch (std::exception &exc) {
      info.GetIsolate()->ThrowException(
          v8::String::NewFromUtf8(info.GetIsolate(), exc.what()));
    }
  }
}

void JScript_object_wrapper::handler_isetter(
    uint32_t i, v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value> &info) {
  v8::HandleScope hscope(info.GetIsolate());
  v8::Handle<v8::Object> obj(info.Holder());
  JScript_object_wrapper *self = static_cast<JScript_object_wrapper *>(
      obj->GetAlignedPointerFromInternalField(2));
  const auto &object = static_cast<Object_collectable *>(
                           obj->GetAlignedPointerFromInternalField(1))
                           ->data();
  if (!object) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(
        info.GetIsolate(), "Reference to invalid object"));
    return;
  }

  try {
    object->set_member(i, self->_context->v8_value_to_shcore_value(value));
    info.GetReturnValue().Set(value);
  } catch (Exception &exc) {
    info.GetIsolate()->ThrowException(
        self->_context->shcore_value_to_v8_value(Value(exc.error())));
  } catch (std::exception &exc) {
    info.GetIsolate()->ThrowException(
        v8::String::NewFromUtf8(info.GetIsolate(), exc.what()));
  }
}

void JScript_object_wrapper::handler_ienumerator(
    const v8::PropertyCallbackInfo<v8::Array> &info) {
  v8::HandleScope hscope(info.GetIsolate());
  v8::Handle<v8::Object> obj(info.Holder());
  const auto &object = static_cast<Object_collectable *>(
                           obj->GetAlignedPointerFromInternalField(1))
                           ->data();
  if (!object) {
    info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(
        info.GetIsolate(), "Reference to invalid object"));
    return;
  }
  std::vector<std::string> members(object->get_members());
  v8::Handle<v8::Array> marray = v8::Array::New(info.GetIsolate());
  int i = 0;
  for (std::vector<std::string>::const_iterator iter = members.begin();
       iter != members.end(); ++iter, ++i) {
    uint32_t x = 0;
    if (sscanf(iter->c_str(), "%u", &x) == 1)
      marray->Set(i, v8::Integer::New(info.GetIsolate(), x));
  }
  info.GetReturnValue().Set(marray);
}

bool JScript_object_wrapper::unwrap(
    v8::Handle<v8::Object> value, std::shared_ptr<Object_bridge> &ret_object) {
  if (value->InternalFieldCount() == 3 &&
      value->GetAlignedPointerFromInternalField(0) == (void *)&magic_pointer) {
    const auto &object = static_cast<Object_collectable *>(
                             value->GetAlignedPointerFromInternalField(1))
                             ->data();
    if (!object) {
      return false;
    }
    ret_object = object;
    return true;
  }
  return false;
}

bool JScript_object_wrapper::is_object(v8::Handle<v8::Object> value) {
  return (value->InternalFieldCount() == 3 &&
          value->GetAlignedPointerFromInternalField(0) ==
              static_cast<void *>(&magic_pointer));
}

bool JScript_object_wrapper::is_method(v8::Handle<v8::Object> value) {
  return (value->InternalFieldCount() == 3 &&
          value->GetAlignedPointerFromInternalField(0) ==
              static_cast<void *>(&magic_method_pointer));
}

JScript_method_wrapper::JScript_method_wrapper(JScript_context *context)
    : _context(context) {
  v8::Handle<v8::ObjectTemplate> templ =
      v8::ObjectTemplate::New(_context->isolate());
  _object_template.Reset(_context->isolate(), templ);
  templ->SetInternalFieldCount(3);
  templ->SetCallAsFunctionHandler(call);
}

JScript_method_wrapper::~JScript_method_wrapper() { _object_template.Reset(); }

v8::Handle<v8::Object> JScript_method_wrapper::wrap(
    std::shared_ptr<Object_bridge> object, const std::string &method) {
  v8::Local<v8::ObjectTemplate> templ =
      v8::Local<v8::ObjectTemplate>::New(_context->isolate(), _object_template);

  if (!templ.IsEmpty()) {
    v8::Local<v8::Object> self(templ->NewInstance());

    if (!self.IsEmpty()) {
      const auto holder =
          new Method_collectable(object, method, _context->isolate(), self);

      self->SetAlignedPointerInInternalField(0, &magic_method_pointer);
      self->SetAlignedPointerInInternalField(1, holder);
      self->SetAlignedPointerInInternalField(2, this);
    }

    return self;
  }

  return {};
}

void JScript_method_wrapper::call(
    const v8::FunctionCallbackInfo<v8::Value> &args) {
  v8::Handle<v8::Object> obj(args.Holder());
  JScript_method_wrapper *self = static_cast<JScript_method_wrapper *>(
      obj->GetAlignedPointerFromInternalField(2));
  auto mdata = static_cast<Method_collectable *>(
      obj->GetAlignedPointerFromInternalField(1));
  const auto &hold_object = mdata->data();
  try {
    Value r =
        hold_object->call(mdata->method(), self->_context->convert_args(args));
    args.GetReturnValue().Set(self->_context->shcore_value_to_v8_value(r));
  } catch (Exception &exc) {
    auto jsexc = self->_context->shcore_value_to_v8_value(Value(exc.error()));
    if (jsexc.IsEmpty())
      jsexc = self->_context->shcore_value_to_v8_value(Value(exc.format()));
    args.GetIsolate()->ThrowException(jsexc);
  } catch (std::exception &exc) {
    args.GetIsolate()->ThrowException(
        v8::String::NewFromUtf8(args.GetIsolate(), exc.what()));
  }
}

}  // namespace shcore
