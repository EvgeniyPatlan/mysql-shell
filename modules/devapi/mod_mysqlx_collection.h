/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

// Interactive Table access module
// (the one exposed as the table members of the db object in the shell)

#ifndef MODULES_DEVAPI_MOD_MYSQLX_COLLECTION_H_
#define MODULES_DEVAPI_MOD_MYSQLX_COLLECTION_H_

#include <memory>
#include <string>
#include "modules/devapi/base_database_object.h"
#include "mysqlx_crud.h"
#include "scripting/types.h"
#include "scripting/types_cpp.h"

#include "modules/devapi/mod_mysqlx_collection_add.h"
#include "modules/devapi/mod_mysqlx_collection_find.h"
#include "modules/devapi/mod_mysqlx_collection_modify.h"
#include "modules/devapi/mod_mysqlx_collection_remove.h"

namespace mysqlsh {
namespace mysqlx {
class Schema;
/**
 * Represents a Collection on an Schema, retrieved with session created using
 * the mysqlx module.
 * \ingroup XDevAPI
 */
class Collection : public DatabaseObject,
                   public std::enable_shared_from_this<Collection> {
 public:
  Collection(std::shared_ptr<Schema> owner, const std::string &name);
  Collection(std::shared_ptr<const Schema> owner, const std::string &name);
  ~Collection();

  virtual std::string class_name() const { return "Collection"; }

#if DOXYGEN_JS
  CollectionAdd add(...);
  CollectionFind find(...);
  CollectionRemove remove(String searchCondition);
  CollectionModify modify(String searchCondition);
  CollectionCreateIndex createIndex(String name);
  CollectionCreateIndex createIndex(String name, IndexType type);
  CollectionDropIndex dropIndex(String name);
#elif DOXYGEN_PY
  CollectionAdd add(...);
  CollectionFind find(...);
  CollectionRemove remove(str search_condition);
  CollectionModify modify(str search_condition);
  CollectionCreateIndex create_index(str name);
  CollectionCreateIndex create_index(str name, IndexType type);
  CollectionDropIndex drop_index(str name);
#endif
  shcore::Value add_(const shcore::Argument_list &args);
  shcore::Value find_(const shcore::Argument_list &args);
  shcore::Value modify_(const shcore::Argument_list &args);
  shcore::Value remove_(const shcore::Argument_list &args);
  shcore::Value create_index_(const shcore::Argument_list &args);
  shcore::Value drop_index_(const shcore::Argument_list &args);

 private:
  void init();
  std::shared_ptr< ::mysqlx::Collection> _collection_impl;

  // Allows initial functions on the CRUD operations
  friend shcore::Value CollectionAdd::add(const shcore::Argument_list &args);
  friend shcore::Value CollectionFind::find(const shcore::Argument_list &args);
  friend shcore::Value CollectionRemove::remove(
      const shcore::Argument_list &args);
  friend shcore::Value CollectionModify::modify(
      const shcore::Argument_list &args);
};
}  // namespace mysqlx
}  // namespace mysqlsh

#endif  // MODULES_DEVAPI_MOD_MYSQLX_COLLECTION_H_
