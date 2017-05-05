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

#include "common/expr_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

#ifndef WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif

#include "mysqlshdk/libs/utils/utils_string.h"
#include "common/tokenizer.h"

namespace mysqlx {

struct Expr_parser::operator_list Expr_parser::_ops;
Mysqlx::Datatypes::Scalar* Expr_builder::build_null_scalar() {
  Mysqlx::Datatypes::Scalar* sc = new Mysqlx::Datatypes::Scalar;
  sc->set_type(Mysqlx::Datatypes::Scalar::V_NULL);
  return sc;
}

Mysqlx::Datatypes::Scalar* Expr_builder::build_double_scalar(double d) {
  Mysqlx::Datatypes::Scalar* sc = new Mysqlx::Datatypes::Scalar;
  sc->set_type(Mysqlx::Datatypes::Scalar::V_DOUBLE);
  sc->set_v_double(d);
  return sc;
}

Mysqlx::Datatypes::Scalar* Expr_builder::build_int_scalar(
    google::protobuf::int64 i) {
  Mysqlx::Datatypes::Scalar* sc = new Mysqlx::Datatypes::Scalar;
  if (i < 0) {
    sc->set_type(Mysqlx::Datatypes::Scalar::V_SINT);
    sc->set_v_signed_int(i);
  } else {
    sc->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
    sc->set_v_unsigned_int((google::protobuf::uint64)i);
  }
  return sc;
}

Mysqlx::Datatypes::Scalar* Expr_builder::build_string_scalar(
    const std::string& s) {
  Mysqlx::Datatypes::Scalar* sc = new Mysqlx::Datatypes::Scalar;
  sc->set_type(Mysqlx::Datatypes::Scalar::V_OCTETS);
  sc->mutable_v_octets()->set_value(s);
  return sc;
}

Mysqlx::Datatypes::Scalar* Expr_builder::build_bool_scalar(bool b) {
  Mysqlx::Datatypes::Scalar* sc = new Mysqlx::Datatypes::Scalar;
  sc->set_type(Mysqlx::Datatypes::Scalar::V_BOOL);
  sc->set_v_bool(b);
  return sc;
}

Mysqlx::Expr::Expr* Expr_builder::build_literal_expr(
    Mysqlx::Datatypes::Scalar* sc) {
  Mysqlx::Expr::Expr* e(new Mysqlx::Expr::Expr());
  e->set_type(Mysqlx::Expr::Expr::LITERAL);
  e->set_allocated_literal(sc);
  return e;
}

Mysqlx::Expr::Expr* Expr_builder::build_unary_op(
    const std::string& name, std::unique_ptr<Mysqlx::Expr::Expr> param) {
  Mysqlx::Expr::Expr* e(new Mysqlx::Expr::Expr());
  e->set_type(Mysqlx::Expr::Expr::OPERATOR);
  Mysqlx::Expr::Operator* op = e->mutable_operator_();
  op->mutable_param()->AddAllocated(param.release());
  std::string name_normalized;
  name_normalized.assign(name);
  std::transform(name_normalized.begin(), name_normalized.end(),
                 name_normalized.begin(), ::tolower);
  op->set_name(name_normalized.c_str(), name_normalized.size());
  return e;
}

Expr_parser::Expr_parser(const std::string& expr_str, bool document_mode,
                         bool allow_alias,
                         std::vector<std::string>* place_holders)
    : _tokenizer(expr_str),
      _document_mode(document_mode),
      _allow_alias(allow_alias) {
  // If provided uses external placeholder information, if not uses the internal
  if (place_holders)
    _place_holder_ref = place_holders;
  else
    _place_holder_ref = &_place_holders;

  _tokenizer.get_tokens();
}

/*
 * paren_expr_list ::= LPAREN expr ( COMMA expr )* RPAREN
 */
void Expr_parser::paren_expr_list(
    ::google::protobuf::RepeatedPtrField<::Mysqlx::Expr::Expr>* expr_list) {
  // Parse a paren-bounded expression list for function arguments or IN list and
  // return a list of Expr objects
  _tokenizer.consume_token(Token::LPAREN);
  if (!_tokenizer.cur_token_type_is(Token::RPAREN)) {
    expr_list->AddAllocated(my_expr().release());
    while (_tokenizer.cur_token_type_is(Token::COMMA)) {
      _tokenizer.inc_pos_token();
      expr_list->AddAllocated(my_expr().release());
    }
  }
  _tokenizer.consume_token(Token::RPAREN);
}

/*
 * identifier ::= IDENT [ DOT IDENT ]
 */
std::unique_ptr<Mysqlx::Expr::Identifier> Expr_parser::identifier() {
  _tokenizer.assert_cur_token(Token::IDENT);
  std::unique_ptr<Mysqlx::Expr::Identifier> id(new Mysqlx::Expr::Identifier());
  if (_tokenizer.next_token_type(Token::DOT)) {
    const std::string& schema_name = _tokenizer.consume_token(Token::IDENT);
    id->set_schema_name(schema_name.c_str(), schema_name.size());
    _tokenizer.consume_token(Token::DOT);
  }
  const std::string& name = _tokenizer.consume_token(Token::IDENT);
  id->set_name(name.c_str(), name.size());
  return id;
}

/*
 * function_call ::= IDENT paren_expr_list
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::function_call() {
  std::unique_ptr<Mysqlx::Expr::Expr> e(new Mysqlx::Expr::Expr());
  e->set_type(Mysqlx::Expr::Expr::FUNC_CALL);
  Mysqlx::Expr::FunctionCall* func = e->mutable_function_call();
  func->set_allocated_name(identifier().release());

  paren_expr_list(func->mutable_param());
  return e;
}

/*
 * docpath_member ::= DOT ( IDENT | LSTRING | MUL )
 */
void Expr_parser::docpath_member(Mysqlx::Expr::DocumentPathItem& item) {
  _tokenizer.consume_token(Token::DOT);
  item.set_type(Mysqlx::Expr::DocumentPathItem::MEMBER);
  if (_tokenizer.cur_token_type_is(Token::IDENT)) {
    const std::string& ident = _tokenizer.consume_token(Token::IDENT);
    item.set_value(ident.c_str(), ident.size());
  } else if (_tokenizer.cur_token_type_is(Token::LSTRING)) {
    const std::string& lstring = _tokenizer.consume_token(Token::LSTRING);
    item.set_value(lstring.c_str(), lstring.size());
  } else if (_tokenizer.cur_token_type_is(Token::MUL)) {
    const std::string& mul = _tokenizer.consume_token(Token::MUL);
    item.set_value(mul.c_str(), mul.size());
    item.set_type(Mysqlx::Expr::DocumentPathItem::MEMBER_ASTERISK);
  } else {
    const Token& tok = _tokenizer.peek_token();
    throw Parser_error(
        "Expected token type IDENT or LSTRING in JSON path at position " +
        std::to_string(tok.get_pos()) + " (" + tok.get_text() + ")");
  }
}

/*
 * docpath_array_loc ::= LSQBRACKET ( MUL | LINTEGER ) RSQBRACKET
 */
void Expr_parser::docpath_array_loc(Mysqlx::Expr::DocumentPathItem& item) {
  _tokenizer.consume_token(Token::LSQBRACKET);
  const Token& tok = _tokenizer.peek_token();
  if (_tokenizer.cur_token_type_is(Token::MUL)) {
    _tokenizer.consume_token(Token::RSQBRACKET);
    item.set_type(Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX_ASTERISK);
  } else if (_tokenizer.cur_token_type_is(Token::LINTEGER)) {
    const std::string& value = _tokenizer.consume_token(Token::LINTEGER);
    int v = std::stoi(value);
    if (v < 0)
      throw Parser_error("Array index cannot be negative at position " +
                         std::to_string(tok.get_pos()));
    _tokenizer.consume_token(Token::RSQBRACKET);
    item.set_type(Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX);
    item.set_index(v);
  } else {
    throw Parser_error(
        "Exception token type MUL or LINTEGER in JSON path array index at "
        "token position " +
        std::to_string(tok.get_pos()) + " (" + tok.get_text() + ")");
  }
}

/*
 * document_path ::= ( docpath_member | docpath_array_loc | ( DOUBLESTAR ))+
 */
void Expr_parser::document_path(Mysqlx::Expr::ColumnIdentifier& colid) {
  // Parse a JSON-style document path, like WL#7909, prefixing with $
  while (true) {
    if (_tokenizer.cur_token_type_is(Token::DOT)) {
      docpath_member(*colid.mutable_document_path()->Add());
    } else if (_tokenizer.cur_token_type_is(Token::LSQBRACKET)) {
      docpath_array_loc(*colid.mutable_document_path()->Add());
    } else if (_tokenizer.cur_token_type_is(Token::DOUBLESTAR)) {
      _tokenizer.consume_token(Token::DOUBLESTAR);
      Mysqlx::Expr::DocumentPathItem* item =
          colid.mutable_document_path()->Add();
      item->set_type(Mysqlx::Expr::DocumentPathItem::DOUBLE_ASTERISK);
    } else {
      break;
    }
  }
  int size = colid.document_path_size();
  if (size > 0 && (colid.document_path(size - 1).type() ==
                   Mysqlx::Expr::DocumentPathItem::DOUBLE_ASTERISK)) {
    const Token& tok = _tokenizer.peek_token();
    throw Parser_error("JSON path may not end in '**' at position " +
                       std::to_string(tok.get_pos()) + " (" + tok.get_text() +
                       ")");
  }
}

/*
 * id ::= IDENT | MUL
 */
const std::string& Expr_parser::id() {
  if (_tokenizer.cur_token_type_is(Token::IDENT))
    return _tokenizer.consume_token(Token::IDENT);
  else
    return _tokenizer.consume_token(Token::MUL);
}

/*
 * column_field ::= [ id DOT ][ id DOT ] id [ ARROW QUOTE DOLLAR docpath QUOTE ]
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::column_field() {
  std::unique_ptr<Mysqlx::Expr::Expr> e(new Mysqlx::Expr::Expr());
  std::vector<std::string> parts;
  const std::string& part = id();

  if (part == "*") {
    e->set_type(Mysqlx::Expr::Expr::OPERATOR);
    e->mutable_operator_()->set_name("*");
    return e;
  }

  parts.push_back(part);

  while (_tokenizer.cur_token_type_is(Token::DOT)) {
    _tokenizer.consume_token(Token::DOT);
    parts.push_back(id());
  }
  if (parts.size() > 3) {
    const Token& tok = _tokenizer.peek_token();
    throw Parser_error("Too many parts to identifier at position " +
                       std::to_string(tok.get_pos()) + " (" + tok.get_text() +
                       ")");
  }
  Mysqlx::Expr::ColumnIdentifier* colid = e->mutable_identifier();
  std::vector<std::string>::reverse_iterator myend = parts.rend();
  int i = 0;
  for (std::vector<std::string>::reverse_iterator it = parts.rbegin();
       it != myend; ++it, ++i) {
    std::string& s = *it;
    if (i == 0)
      colid->set_name(s.c_str(), s.size());
    else if (i == 1)
      colid->set_table_name(s.c_str(), s.size());
    else if (i == 2)
      colid->set_schema_name(s.c_str(), s.size());
  }
  // Arrow & docpath
  if (_tokenizer.cur_token_type_is(Token::ARROW)) {
    _tokenizer.consume_token(Token::ARROW);
    _tokenizer.consume_token(Token::QUOTE);
    _tokenizer.consume_token(Token::DOLLAR);
    document_path(*colid);
    _tokenizer.consume_token(Token::QUOTE);
  }
  e->set_type(Mysqlx::Expr::Expr::IDENT);
  return e;
}

/*
 * document_field ::= [ DOLLAR ] IDENT document_path
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::document_field() {
  std::unique_ptr<Mysqlx::Expr::Expr> e(new Mysqlx::Expr::Expr());

  if (_tokenizer.cur_token_type_is(Token::DOLLAR))
    _tokenizer.consume_token(Token::DOLLAR);
  Mysqlx::Expr::ColumnIdentifier* colid = e->mutable_identifier();
  if (_tokenizer.cur_token_type_is(Token::IDENT)) {
    Mysqlx::Expr::DocumentPathItem* item =
        colid->mutable_document_path()->Add();
    item->set_type(Mysqlx::Expr::DocumentPathItem::MEMBER);
    const std::string& value = _tokenizer.consume_token(Token::IDENT);
    item->set_value(value.c_str(), value.size());
  }
  document_path(*colid);

  e->set_type(Mysqlx::Expr::Expr::IDENT);
  return e;
}

/*
 * atomic_expr ::=
 *   PLACEHOLDER | ( AT IDENT ) | ( LPAREN expr RPAREN ) | ( [ PLUS | MINUS ]
 * LNUM ) |
 *   (( PLUS | MINUS | NOT | NEG ) atomic_expr ) | LSTRING | NULL | LNUM |
 * LINTEGER | TRUE | FALSE | ( INTERVAL expr ( MICROSECOND | SECOND | MINUTE |
 * HOUR | DAY | WEEK | MONTH | QUARTER | YEAR )) | function_call |
 * column_identifier | cast | binary | placeholder | json_doc | MUL | array
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::atomic_expr() {
  // Parse an atomic expression and return a protobuf Expr object
  const Token& t = _tokenizer.consume_any_token();
  int type = t.get_type();
  if (type == Token::PLACEHOLDER) {
    return std::unique_ptr<Mysqlx::Expr::Expr>(Expr_builder::build_literal_expr(
        Expr_builder::build_string_scalar("?")));
  } else if (type == Token::LPAREN) {
    std::unique_ptr<Mysqlx::Expr::Expr> e(my_expr());
    _tokenizer.consume_token(Token::RPAREN);
    return e;
  } else if ((_tokenizer.cur_token_type_is(Token::LNUM) ||
              _tokenizer.cur_token_type_is(Token::LINTEGER)) &&
             ((type == Token::PLUS) || (type == Token::MINUS))) {
    const Token& token = _tokenizer.consume_any_token();
    const std::string& val = token.get_text();
    int sign = (type == Token::PLUS) ? 1 : -1;
    if (token.get_type() == Token::LNUM) {
      return std::unique_ptr<Mysqlx::Expr::Expr>(
          Expr_builder::build_literal_expr(Expr_builder::build_double_scalar(
              std::stod(val.c_str()) * sign)));
    } else {  // Token::LINTEGER
      return std::unique_ptr<Mysqlx::Expr::Expr>(
          Expr_builder::build_literal_expr(
              Expr_builder::build_int_scalar(std::stoi(val.c_str()) * sign)));
    }
  } else if (type == Token::PLUS || type == Token::MINUS ||
             type == Token::NOT || type == Token::NEG) {
    return std::unique_ptr<Mysqlx::Expr::Expr>(
        Expr_builder::build_unary_op(t.get_text(), atomic_expr()));
  } else if (type == Token::LSTRING) {
    return std::unique_ptr<Mysqlx::Expr::Expr>(Expr_builder::build_literal_expr(
        Expr_builder::build_string_scalar(t.get_text())));
  } else if (type == Token::T_NULL) {
    return std::unique_ptr<Mysqlx::Expr::Expr>(
        Expr_builder::build_literal_expr(Expr_builder::build_null_scalar()));
  } else if ((type == Token::LNUM) || (type == Token::LINTEGER)) {
    const std::string& val = t.get_text();
    if (t.get_type() == Token::LNUM) {
      return std::unique_ptr<Mysqlx::Expr::Expr>(
          Expr_builder::build_literal_expr(
              Expr_builder::build_double_scalar(std::stod(val.c_str()))));
    } else {  // Token::LINTEGER
      return std::unique_ptr<Mysqlx::Expr::Expr>(
          Expr_builder::build_literal_expr(
              Expr_builder::build_int_scalar(std::stoi(val.c_str()))));
    }
  } else if (type == Token::TRUE_ || type == Token::FALSE_) {
    return std::unique_ptr<Mysqlx::Expr::Expr>(Expr_builder::build_literal_expr(
        Expr_builder::build_bool_scalar(type == Token::TRUE_)));
  } else if (type == Token::INTERVAL) {
    std::unique_ptr<Mysqlx::Expr::Expr> e(new Mysqlx::Expr::Expr());
    e->set_type(Mysqlx::Expr::Expr::OPERATOR);

    Mysqlx::Expr::Operator* op = e->mutable_operator_();
    op->set_name("interval");
    op->mutable_param()->AddAllocated(my_expr().release());
    // validate the interval units
    if (!_tokenizer.tokens_available() ||
        !_tokenizer.is_interval_units_type()) {
      const Token& tok = _tokenizer.peek_token();
      throw Parser_error("Expected interval units at " +
                         std::to_string(tok.get_pos()) + " (" + tok.get_text() +
                         ")");
    }
    const Token& val = _tokenizer.consume_any_token();
    e->mutable_operator_()->mutable_param()->AddAllocated(
        Expr_builder::build_literal_expr(
            Expr_builder::build_string_scalar(val.get_text())));
    return e;
  } else if (type == Token::MUL) {
    _tokenizer.unget_token();
    if (!_document_mode)
      return column_field();
    else
      return document_field();
  } else if (type == Token::CAST) {
    _tokenizer.unget_token();
    return cast();
  } else if (type == Token::PLACEHOLDER || type == Token::COLON) {
    _tokenizer.unget_token();
    return placeholder();
  } else if (type == Token::LCURLY) {
    _tokenizer.unget_token();
    return json_doc();
  } else if (type == Token::BINARY) {
    _tokenizer.unget_token();
    return binary();
  } else if (type == Token::LSQBRACKET) {
    _tokenizer.unget_token();
    return array_();
  } else if (type == Token::IDENT || type == Token::DOT) {
    _tokenizer.unget_token();
    if (type == Token::IDENT &&
        (_tokenizer.next_token_type(Token::LPAREN) ||
         (_tokenizer.next_token_type(Token::DOT) &&
          _tokenizer.pos_token_type_is(_tokenizer.get_token_pos() + 2,
                                       Token::IDENT) &&
          _tokenizer.pos_token_type_is(_tokenizer.get_token_pos() + 3,
                                       Token::LPAREN)))) {
      return function_call();
    } else {
      if (!_document_mode)
        return column_field();
      else
        return document_field();
    }
  } else if (type == Token::DOLLAR && _document_mode) {
    _tokenizer.unget_token();
    return document_field();
  }
  const Token& tok = _tokenizer.peek_token();
  throw Parser_error("Unexpected token type = " + std::to_string(type) +
                     " when expecting atomic expression at position " +
                     std::to_string(tok.get_pos()) + " (" + tok.get_text() +
                     ")");
}

/**
 * array ::= LSQBRACKET [ expr (COMMA expr)* ] RQKBRACKET
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::array_() {
  std::unique_ptr<Mysqlx::Expr::Expr> result(new Mysqlx::Expr::Expr());

  result->set_type(Mysqlx::Expr::Expr_Type_ARRAY);
  Mysqlx::Expr::Array* a = result->mutable_array();

  _tokenizer.consume_token(Token::LSQBRACKET);

  if (!_tokenizer.cur_token_type_is(Token::RSQBRACKET)) {
    std::unique_ptr<Mysqlx::Expr::Expr> e(my_expr());
    Mysqlx::Expr::Expr* item = a->add_value();
    item->CopyFrom(*e);

    while (_tokenizer.cur_token_type_is(Token::COMMA)) {
      _tokenizer.consume_token(Token::COMMA);
      e = std::move(my_expr());
      item = a->add_value();
      item->CopyFrom(*e);
    }
  }

  _tokenizer.consume_token(Token::RSQBRACKET);

  return result;
}

/**
 * json_key_value ::= LSTRING COLON expr
 */
void Expr_parser::json_key_value(Mysqlx::Expr::Object* obj) {
  Mysqlx::Expr::Object_ObjectField* fld = obj->add_fld();
  const std::string& key = _tokenizer.consume_token(Token::LSTRING);
  _tokenizer.consume_token(Token::COLON);
  fld->set_key(key.c_str());
  fld->set_allocated_value(my_expr().release());
}

/**
 * json_doc ::= LCURLY ( json_key_value ( COMMA json_key_value )* )? RCURLY
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::json_doc() {
  std::unique_ptr<Mysqlx::Expr::Expr> result(new Mysqlx::Expr::Expr());
  Mysqlx::Expr::Object* obj = result->mutable_object();
  result->set_type(Mysqlx::Expr::Expr_Type_OBJECT);
  _tokenizer.consume_token(Token::LCURLY);
  if (_tokenizer.cur_token_type_is(Token::LSTRING)) {
    json_key_value(obj);
    while (_tokenizer.cur_token_type_is(Token::COMMA)) {
      _tokenizer.consume_any_token();
      json_key_value(obj);
    }
  }
  _tokenizer.consume_token(Token::RCURLY);
  return result;
}

/**
 * placeholder ::= ( COLON INT ) | ( COLON IDENT ) | PLACECHOLDER
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::placeholder() {
  std::unique_ptr<Mysqlx::Expr::Expr> result(new Mysqlx::Expr::Expr());
  result->set_type(Mysqlx::Expr::Expr_Type_PLACEHOLDER);

  std::string placeholder_name;
  if (_tokenizer.cur_token_type_is(Token::COLON)) {
    _tokenizer.consume_token(Token::COLON);

    if (_tokenizer.cur_token_type_is(Token::LINTEGER))
      placeholder_name = _tokenizer.consume_token(Token::LINTEGER);
    else if (_tokenizer.cur_token_type_is(Token::IDENT))
      placeholder_name = _tokenizer.consume_token(Token::IDENT);
    else
      placeholder_name = std::to_string(_place_holder_ref->size());
  } else if (_tokenizer.cur_token_type_is(Token::PLACEHOLDER)) {
    _tokenizer.consume_token(Token::PLACEHOLDER);
    placeholder_name = std::to_string(_place_holder_ref->size());
  }

  // Adds a new placeholder if needed
  int position = static_cast<int>(_place_holder_ref->size());
  std::vector<std::string>::iterator index = std::find(
      _place_holder_ref->begin(), _place_holder_ref->end(), placeholder_name);
  if (index == _place_holder_ref->end())
    _place_holder_ref->push_back(placeholder_name);
  else
    position = static_cast<int>(index - _place_holder_ref->begin());

  result->set_position(position);

  return result;
}

/**
 * cast ::= CAST LPAREN expr AS cast_data_type RPAREN
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::cast() {
  _tokenizer.consume_token(Token::CAST);
  _tokenizer.consume_token(Token::LPAREN);
  std::unique_ptr<Mysqlx::Expr::Expr> e(my_expr());
  std::unique_ptr<Mysqlx::Expr::Expr> result(new Mysqlx::Expr::Expr());
  // function
  result->set_type(Mysqlx::Expr::Expr::FUNC_CALL);
  Mysqlx::Expr::FunctionCall* func = result->mutable_function_call();
  std::unique_ptr<Mysqlx::Expr::Identifier> id(new Mysqlx::Expr::Identifier());
  id->set_name(std::string("cast"));
  func->set_allocated_name(id.release());
  // params
  // 1st arg, expr
  _tokenizer.consume_token(Token::AS);
  ::google::protobuf::RepeatedPtrField<::Mysqlx::Expr::Expr>* params =
      func->mutable_param();
  params->AddAllocated(e.release());
  // 2nd arg, cast_data_type
  const std::string& type_to_cast = cast_data_type();
  std::unique_ptr<Mysqlx::Expr::Expr> type_expr(new Mysqlx::Expr::Expr());
  type_expr->set_type(Mysqlx::Expr::Expr::LITERAL);
  Mysqlx::Datatypes::Scalar* sc(type_expr->mutable_literal());
  sc->set_type(Mysqlx::Datatypes::Scalar_Type_V_OCTETS);
  sc->mutable_v_octets()->set_value(type_to_cast);
  params->AddAllocated(type_expr.release());
  _tokenizer.consume_token(Token::RPAREN);

  return result;
}

/**
 * cast_data_type ::= ( BINARY dimension? ) | ( CHAR dimension? opt_binary ) | (
 * NCHAR dimension? ) | ( DATE ) | ( DATETIME dimension? ) | ( TIME dimension? )
 *   | ( DECIMAL dimension? ) | ( SIGNED INTEGER? ) | ( UNSIGNED INTEGER? ) |
 * INTEGER | JSON
 */
std::string Expr_parser::cast_data_type() {
  std::string result;
  const Token& token = _tokenizer.peek_token();
  Token::TokenType type = token.get_type();

  if ((type == Token::BINARY) || (type == Token::NCHAR) ||
      (type == Token::DATETIME) || (type == Token::TIME)) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    std::string dimension = cast_data_type_dimension();
    if (!dimension.empty())
      result += dimension;
  } else if (type == Token::DECIMAL) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    std::string dimension = cast_data_type_dimension(true);
    if (!dimension.empty())
      result += dimension;
  } else if (type == Token::DATE) {
    _tokenizer.consume_any_token();
    result += token.get_text();
  } else if (type == Token::CHAR) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    if (_tokenizer.cur_token_type_is(Token::LPAREN))
      result += cast_data_type_dimension();
    const std::string& opt_binary_result = opt_binary();
    if (!opt_binary_result.empty())
      result += " " + opt_binary_result;
  } else if (type == Token::SIGNED) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    if (_tokenizer.cur_token_type_is(Token::INTEGER))
      result += " " + _tokenizer.consume_token(Token::INTEGER);
  } else if (type == Token::UNSIGNED) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    if (_tokenizer.cur_token_type_is(Token::INTEGER))
      result += " " + _tokenizer.consume_token(Token::INTEGER);
  } else if (type == Token::INTEGER) {
    result += token.get_text();
    _tokenizer.consume_any_token();
  } else if (type == Token::JSON) {
    result += token.get_text();
    _tokenizer.consume_any_token();
  } else {
    throw Parser_error("Unexpected token type = " + token.get_type_name() +
                       " when expecting atomic expression at position " +
                       std::to_string(token.get_pos()) + " (" +
                       token.get_text() + ")");
  }
  return result;
}

/**
 * dimension ::= LPAREN LINTEGER RPAREN
 */
std::string Expr_parser::cast_data_type_dimension(bool double_dimension) {
  if (!_tokenizer.cur_token_type_is(Token::LPAREN))
    return "";
  _tokenizer.consume_token(Token::LPAREN);
  std::string result = "(" + _tokenizer.consume_token(Token::LINTEGER);
  if (double_dimension) {
    _tokenizer.consume_token(Token::COMMA);
    result += ", " + _tokenizer.consume_token(Token::LINTEGER);
  }
  result += ")";
  _tokenizer.consume_token(Token::RPAREN);
  return result;
}

/**
 * opt_binary ::= ( ASCII BINARY? ) | ( UNICODE BINARY? ) | ( BINARY ( ASCII |
 * UNICODE | charset_def )? ) | BYTE | < nothing >
 */
std::string Expr_parser::opt_binary() {
  std::string result;
  const Token& token = _tokenizer.peek_token();
  if (token.get_type() == Token::ASCII) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    if (_tokenizer.cur_token_type_is(Token::BINARY))
      result += " " + _tokenizer.consume_any_token().get_text();
    return result;
  } else if (token.get_type() == Token::UNICODE) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    if (_tokenizer.cur_token_type_is(Token::BINARY))
      result += " " + _tokenizer.consume_any_token().get_text();
    return result;
  } else if (token.get_type() == Token::BINARY) {
    result += token.get_text();
    _tokenizer.consume_any_token();
    const Token& token2 = _tokenizer.peek_token();
    if ((token2.get_type() == Token::ASCII) ||
        (token2.get_type() == Token::UNICODE))
      result += " " + token2.get_text();
    else if ((token2.get_type() == Token::CHARACTER) ||
             (token2.get_type() == Token::CHARSET))
      result += " " + charset_def();
    return result;
  } else if (token.get_type() == Token::BYTE) {
    return token.get_text();
  } else {
    return "";
  }
}

/**
 * charset_def ::= (( CHARACTER SET ) | CHARSET ) ( IDENT | STRING | BINARY )
 */
std::string Expr_parser::charset_def() {
  std::string result;
  const Token& token = _tokenizer.consume_any_token();
  if (token.get_type() == Token::CHARACTER) {
    _tokenizer.consume_token(Token::SET);
  } else if (token.get_type() == Token::CHARSET) {
    /* nothing */
  } else {
    throw Parser_error(
        "Expected CHARACTER or CHARSET token, but got Unexpected token type "
        "= " +
        token.get_type_name() +
        " when expecting atomic expression at position " +
        std::to_string(token.get_pos()) + " (" + token.get_text() + ")");
  }

  const Token& token2 = _tokenizer.peek_token();
  if ((token2.get_type() == Token::IDENT) ||
      (token2.get_type() == Token::LSTRING) ||
      (token2.get_type() == Token::BINARY)) {
    _tokenizer.consume_any_token();
    result = "charset " + token2.get_text();
  } else {
    throw Parser_error(
        "Expected either IDENT, LSTRING or BINARY, but got Unexpected token "
        "type "
        "= " +
        token2.get_type_name() +
        " when expecting atomic expression at position " +
        std::to_string(token2.get_pos()) + " (" + token2.get_text() + ")");
  }
  return result;
}

/**
 * binary ::= BINARY expr
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::binary() {
  // binary
  _tokenizer.consume_token(Token::BINARY);

  std::unique_ptr<Mysqlx::Expr::Expr> e(new Mysqlx::Expr::Expr());
  e->set_type(Mysqlx::Expr::Expr::FUNC_CALL);
  Mysqlx::Expr::FunctionCall* func = e->mutable_function_call();
  std::unique_ptr<Mysqlx::Expr::Identifier> id(new Mysqlx::Expr::Identifier());
  id->set_name(std::string("binary"));
  func->set_allocated_name(id.release());
  ::google::protobuf::RepeatedPtrField<::Mysqlx::Expr::Expr>* params =
      func->mutable_param();
  // expr
  params->AddAllocated(my_expr().release());
  return e;
}

std::unique_ptr<Mysqlx::Expr::Expr>
Expr_parser::parse_left_assoc_binary_op_expr(std::set<Token::TokenType>& types,
                                             inner_parser_t inner_parser) {
  // Given a `set' of types and an Expr-returning inner parser function, parse a
  // left associate binary operator expression
  std::unique_ptr<Mysqlx::Expr::Expr> lhs(inner_parser());
  while (_tokenizer.tokens_available() &&
         _tokenizer.is_type_within_set(types)) {
    std::unique_ptr<Mysqlx::Expr::Expr> e(new Mysqlx::Expr::Expr());
    e->set_type(Mysqlx::Expr::Expr::OPERATOR);
    const Token& t = _tokenizer.consume_any_token();
    const std::string& op_val = t.get_text();
    Mysqlx::Expr::Operator* op = e->mutable_operator_();
    std::string& op_normalized = _tokenizer.map.operator_names.at(op_val);
    op->set_name(op_normalized.c_str(), op_normalized.size());
    op->mutable_param()->AddAllocated(lhs.release());

    op->mutable_param()->AddAllocated(inner_parser().release());
    lhs = std::move(e);
  }
  return lhs;
}

/*
 * mul_div_expr ::= atomic_expr (( MUL | DIV | MOD ) atomic_expr )*
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::mul_div_expr() {
  return parse_left_assoc_binary_op_expr(
      _ops.mul_div_expr_types, std::bind(&Expr_parser::atomic_expr, this));
}

/*
 * add_sub_expr ::= mul_div_expr (( PLUS | MINUS ) mul_div_expr )*
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::add_sub_expr() {
  return parse_left_assoc_binary_op_expr(
      _ops.add_sub_expr_types, std::bind(&Expr_parser::mul_div_expr, this));
}

/*
 * shift_expr ::= add_sub_expr (( LSHIFT | RSHIFT ) add_sub_expr )*
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::shift_expr() {
  return parse_left_assoc_binary_op_expr(
      _ops.shift_expr_types, std::bind(&Expr_parser::add_sub_expr, this));
}

/*
 * bit_expr ::= shift_expr (( BITAND | BITOR | BITXOR ) shift_expr )*
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::bit_expr() {
  return parse_left_assoc_binary_op_expr(
      _ops.bit_expr_types, std::bind(&Expr_parser::shift_expr, this));
}

/*
 * comp_expr ::= bit_expr (( GE | GT | LE | LT | QE | NE ) bit_expr )*
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::comp_expr() {
  return parse_left_assoc_binary_op_expr(
      _ops.comp_expr_types, std::bind(&Expr_parser::bit_expr, this));
}

/*
 * ilri_expr ::= comp_expr [ NOT ] (( IS [ NOT ] comp_expr ) | ( IN
 * paren_expr_list ) | ( LIKE comp_expr [ ESCAPE comp_expr ] ) | ( BETWEEN
 * comp_expr AND comp_expr ) | ( REGEXP comp_expr )
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::ilri_expr() {
  std::unique_ptr<Mysqlx::Expr::Expr> e(new Mysqlx::Expr::Expr());
  std::unique_ptr<Mysqlx::Expr::Expr> lhs(comp_expr());
  bool is_not = false;
  if (_tokenizer.cur_token_type_is(Token::NOT)) {
    is_not = true;
    _tokenizer.consume_token(Token::NOT);
  }
  if (_tokenizer.tokens_available()) {
    ::google::protobuf::RepeatedPtrField<::Mysqlx::Expr::Expr>* params =
        e->mutable_operator_()->mutable_param();
    const Token& op_name_tok = _tokenizer.peek_token();
    std::string op_name(op_name_tok.get_text());
    bool has_op_name = true;
    std::transform(op_name.begin(), op_name.end(), op_name.begin(), ::tolower);
    if (_tokenizer.cur_token_type_is(Token::IS)) {
      _tokenizer.consume_token(Token::IS);
      // for IS, NOT comes AFTER
      if (_tokenizer.cur_token_type_is(Token::NOT)) {
        is_not = true;
        _tokenizer.consume_token(Token::NOT);
      }
      params->AddAllocated(lhs.release());
      params->AddAllocated(comp_expr().release());
    } else if (_tokenizer.cur_token_type_is(Token::IN_)) {
      _tokenizer.consume_token(Token::IN_);
      params->AddAllocated(lhs.release());
      if (_tokenizer.cur_token_type_is(Token::LSQBRACKET)) {
        _tokenizer.consume_token(Token::LSQBRACKET);
        params->AddAllocated(my_expr().release());
        while (_tokenizer.cur_token_type_is(Token::COMMA)) {
          _tokenizer.inc_pos_token();
          params->AddAllocated(my_expr().release());
        }
        _tokenizer.consume_token(Token::RSQBRACKET);
      } else {
        paren_expr_list(params);
      }
    } else if (_tokenizer.cur_token_type_is(Token::LIKE)) {
      _tokenizer.consume_token(Token::LIKE);
      params->AddAllocated(lhs.release());
      params->AddAllocated(comp_expr().release());
      if (_tokenizer.cur_token_type_is(Token::ESCAPE)) {
        _tokenizer.consume_token(Token::ESCAPE);
        params->AddAllocated(comp_expr().release());
      }
    } else if (_tokenizer.cur_token_type_is(Token::BETWEEN)) {
      _tokenizer.consume_token(Token::BETWEEN);
      params->AddAllocated(lhs.release());
      params->AddAllocated(comp_expr().release());
      _tokenizer.consume_token(Token::AND);
      params->AddAllocated(comp_expr().release());
    } else if (_tokenizer.cur_token_type_is(Token::REGEXP)) {
      _tokenizer.consume_token(Token::REGEXP);
      params->AddAllocated(lhs.release());
      params->AddAllocated(comp_expr().release());
    } else {
      if (is_not)
        throw Parser_error("Unexpected token after NOT as position " +
                           std::to_string(op_name_tok.get_pos()) + " (" +
                           op_name_tok.get_text() + ")");
      has_op_name = false;
    }
    if (has_op_name) {
      e->set_type(Mysqlx::Expr::Expr::OPERATOR);
      Mysqlx::Expr::Operator* op = e->mutable_operator_();
      op->set_name(op_name.c_str(), op_name.size());
      if (is_not) {
        // wrap if `NOT'-prefixed
        lhs.reset(Expr_builder::build_unary_op("not", std::move(e)));
      } else {
        lhs = std::move(e);
      }
    }
  }

  return lhs;
}

/*
 * and_expr ::= ilri_expr ( AND ilri_expr )*
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::and_expr() {
  return parse_left_assoc_binary_op_expr(
      _ops.and_expr_types, std::bind(&Expr_parser::ilri_expr, this));
}

/*
 * or_expr ::= and_expr ( OR and_expr )*
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::or_expr() {
  return parse_left_assoc_binary_op_expr(
      _ops.or_expr_types, std::bind(&Expr_parser::and_expr, this));
}

/*
 * my_expr ::= or_expr
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::my_expr() {
  return or_expr();
}

/*
 * expr ::= or_expr
 */
std::unique_ptr<Mysqlx::Expr::Expr> Expr_parser::expr() {
  std::unique_ptr<Mysqlx::Expr::Expr> result(or_expr());
  if (_tokenizer.tokens_available()) {
    const Token& tok = _tokenizer.peek_token();
    throw Parser_error(
        "Expr parser: Expected EOF, instead stopped at position " +
        std::to_string(tok.get_pos()) + " (" + tok.get_text() + ")");
  }
  return result;
}

std::string Expr_unparser::any_to_string(const Mysqlx::Datatypes::Any& a) {
  if (a.type() == Mysqlx::Datatypes::Any::SCALAR) {
    return Expr_unparser::scalar_to_string(a.scalar());
  } else {
    // TODO(?): handle objects & array here
    throw Parser_error("Unknown type tag at Any" + a.DebugString());
  }
  return "";
}

std::string Expr_unparser::escape_literal(const std::string& s) {
  std::string result = s;
  Expr_unparser::replace(result, "\"", "\"\"");
  return result;
}

std::string Expr_unparser::scalar_to_string(
    const Mysqlx::Datatypes::Scalar& s) {
  switch (s.type()) {
    case Mysqlx::Datatypes::Scalar::V_SINT:
      return std::to_string(s.v_signed_int());
    case Mysqlx::Datatypes::Scalar::V_UINT:
      return std::to_string(s.v_unsigned_int());
    case Mysqlx::Datatypes::Scalar::V_DOUBLE:
      return std::to_string(s.v_double());
    case Mysqlx::Datatypes::Scalar::V_BOOL: {
      if (s.v_bool())
        return "TRUE";
      else
        return "FALSE";
    }
    case Mysqlx::Datatypes::Scalar::V_OCTETS: {
      const char* value = s.v_octets().value().c_str();
      return "\"" + Expr_unparser::escape_literal(value) + "\"";
    }
    case Mysqlx::Datatypes::Scalar::V_NULL:
      return "NULL";
    default:
      throw Parser_error("Unknown type tag at Scalar: " + s.DebugString());
  }
}

std::string Expr_unparser::document_path_to_string(
    const ::google::protobuf::RepeatedPtrField<
        ::Mysqlx::Expr::DocumentPathItem>& dp) {
  std::string s;
  std::vector<std::string> parts;
  for (int i = 0; i < dp.size(); ++i) {
    const Mysqlx::Expr::DocumentPathItem& dpi = dp.Get(i);
    switch (dpi.type()) {
      case Mysqlx::Expr::DocumentPathItem::MEMBER:
        parts.push_back("." + dpi.value());
        break;
      case Mysqlx::Expr::DocumentPathItem::MEMBER_ASTERISK:
        parts.push_back("." + dpi.value());
        break;
      case Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX:
        parts.push_back("[" + std::to_string(dpi.index()) + "]");
        break;
      case Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX_ASTERISK:
        parts.push_back("[*]");
        break;
      case Mysqlx::Expr::DocumentPathItem::DOUBLE_ASTERISK:
        parts.push_back("**");
        break;
    }
  }

  std::vector<std::string>::const_iterator myend = parts.end();
  for (std::vector<std::string>::const_iterator it = parts.begin(); it != myend;
       ++it) {
    s = s + *it;
  }

  return s;
}

std::string Expr_unparser::column_identifier_to_string(
    const Mysqlx::Expr::ColumnIdentifier& colid) {
  std::string s = Expr_unparser::quote_identifier(colid.name());
  if (colid.has_table_name()) {
    s = Expr_unparser::quote_identifier(colid.table_name()) + "." + s;
  }
  if (colid.has_schema_name()) {
    s = Expr_unparser::quote_identifier(colid.schema_name()) + "." + s;
  }
  std::string dp =
      Expr_unparser::document_path_to_string(colid.document_path());
  if (!dp.empty())
    s = s + "$" + dp;
  return s;
}

std::string Expr_unparser::function_call_to_string(
    const Mysqlx::Expr::FunctionCall& fc) {
  std::string s = Expr_unparser::quote_identifier(fc.name().name()) + "(";
  if (fc.name().has_schema_name()) {
    s = Expr_unparser::quote_identifier(fc.name().schema_name()) + "." + s;
  }
  for (int i = 0; i < fc.param().size(); ++i) {
    s = s + Expr_unparser::expr_to_string(fc.param().Get(i));
    if (i + 1 < fc.param().size()) {
      s = s + ", ";
    }
  }
  return s + ")";
}

std::string Expr_unparser::operator_to_string(
    const Mysqlx::Expr::Operator& op) {
  const ::google::protobuf::RepeatedPtrField<::Mysqlx::Expr::Expr>& ps =
      op.param();
  std::string name = shcore::str_upper(op.name());
  if (name == "IN") {
    std::string s = Expr_unparser::expr_to_string(ps.Get(0)) + " IN (";
    for (int i = 1; i < ps.size(); ++i) {
      s = s + Expr_unparser::expr_to_string(ps.Get(i));
      if (i + 1 < ps.size())
        s = s + ", ";
    }
    return s + ")";
  } else if (name == "INTERVAL") {
    std::string result =
        "INTERVAL " + Expr_unparser::expr_to_string(ps.Get(0)) + " ";
    std::string data = Expr_unparser::expr_to_string(ps.Get(1));
    Expr_unparser::replace(data, "\"", "");
    return result + data;
  } else if (name == "BETWEEN") {
    std::string result = Expr_unparser::expr_to_string(ps.Get(0));
    result += " BETWEEN ";
    result += Expr_unparser::expr_to_string(ps.Get(1));
    result += " AND ";
    result += Expr_unparser::expr_to_string(ps.Get(2));
    return result;
  } else if (name == "LIKE" && ps.size() == 3) {
    return Expr_unparser::expr_to_string(ps.Get(0)) + " LIKE " +
           Expr_unparser::expr_to_string(ps.Get(1)) + " ESCAPE " +
           Expr_unparser::expr_to_string(ps.Get(2));
  } else if (ps.size() == 2) {
    std::string op_name(op.name());
    std::transform(op_name.begin(), op_name.end(), op_name.begin(), ::toupper);
    std::string result = "(" + Expr_unparser::expr_to_string(ps.Get(0)) + " " +
                         op_name + " " +
                         Expr_unparser::expr_to_string(ps.Get(1)) + ")";
    return result;
  } else if (ps.size() == 1) {
    std::string op_name(op.name());
    std::transform(op_name.begin(), op_name.end(), op_name.begin(), ::toupper);
    if (op_name.size() == 1) {
      return op_name + Expr_unparser::expr_to_string(ps.Get(0));
    } else {
      // something like NOT
      return op_name + " ( " + Expr_unparser::expr_to_string(ps.Get(0)) + ")";
    }
  } else if (name == "*" && ps.size() == 0) {
    return "*";
  } else {
    throw Parser_error("Unknown operator structure " + op.name());
  }

  return "";
}

void Expr_unparser::replace(std::string& target, const std::string& old_val,
                            const std::string& new_val) {
  size_t len_skip =
      std::abs(static_cast<int64_t>(old_val.size() - new_val.size()) - 1);
  std::string result = target;
  std::string::size_type pos = 0;
  while ((pos = result.find(old_val, pos + len_skip)) != std::string::npos) {
    result = result.replace(pos, old_val.size(), new_val);
  }
  target = result;
}

std::string Expr_unparser::quote_identifier(const std::string& id) {
  if (id.find("`") != std::string::npos || id.find("\"") != std::string::npos ||
      id.find("'") != std::string::npos || id.find("$") != std::string::npos ||
      id.find(".") != std::string::npos) {
    std::string result = id;
    Expr_unparser::replace(result, "`", "``");
    return "`" + result + "`";
  } else {
    return id;
  }
}

std::string Expr_unparser::expr_to_string(const Mysqlx::Expr::Expr& e) {
  if (e.type() == Mysqlx::Expr::Expr::LITERAL) {
    return Expr_unparser::scalar_to_string(e.literal());
  } else if (e.type() == Mysqlx::Expr::Expr::IDENT) {
    return Expr_unparser::column_identifier_to_string(e.identifier());
  } else if (e.type() == Mysqlx::Expr::Expr::FUNC_CALL) {
    return Expr_unparser::function_call_to_string(e.function_call());
  } else if (e.type() == Mysqlx::Expr::Expr::OPERATOR) {
    return Expr_unparser::operator_to_string(e.operator_());
  } else if (e.type() == Mysqlx::Expr::Expr::VARIABLE) {
    return std::string("$") + Expr_unparser::quote_identifier(e.variable());
  } else if (e.type() == Mysqlx::Expr::Expr::OBJECT) {
    return Expr_unparser::object_to_string(e.object());
  } else if (e.type() == Mysqlx::Expr::Expr::PLACEHOLDER) {
    return Expr_unparser::placeholder_to_string(e);
  } else if (e.type() == Mysqlx::Expr::Expr::ARRAY) {
    return Expr_unparser::array_to_string(e);
  } else {
    throw Parser_error("Unknown expression type: " + std::to_string(e.type()));
  }

  return "";
}

std::string Expr_unparser::placeholder_to_string(const Mysqlx::Expr::Expr& e) {
  std::string result = ":" + std::to_string(e.position());
  return result;
}

std::string Expr_unparser::array_to_string(const Mysqlx::Expr::Expr& e) {
  std::string result = "[ ";

  const Mysqlx::Expr::Array& a = e.array();
  bool first = true;
  for (int i = 0; i < a.value_size(); i++) {
    if (first)
      first = false;
    else
      result += ", ";
    result += Expr_unparser::expr_to_string(a.value(i));
  }

  result += " ]";

  return result;
}

std::string Expr_unparser::object_to_string(const Mysqlx::Expr::Object& o) {
  bool first = true;
  std::string result = "{ ";
  for (int i = 0; i < o.fld_size(); ++i) {
    if (first)
      first = false;
    else
      result += ", ";
    const Mysqlx::Expr::Object_ObjectField& fld = o.fld(i);
    result += "'" + fld.key() + "' : ";
    result += Expr_unparser::expr_to_string(fld.value());
  }
  result += " }";
  return result;
}

std::string Expr_unparser::column_to_string(const Mysqlx::Crud::Projection& c) {
  std::string result = Expr_unparser::expr_to_string(c.source());

  if (c.has_alias())
    result += " as " + c.alias();
  return result;
}

std::string Expr_unparser::order_to_string(const Mysqlx::Crud::Order& c) {
  std::string result = Expr_unparser::expr_to_string(c.expr());
  if ((!c.has_direction()) ||
      (c.direction() == Mysqlx::Crud::Order_Direction_ASC))
    result += " asc";
  else
    result += " desc";
  return result;
}

std::string Expr_unparser::column_list_to_string(
    google::protobuf::RepeatedPtrField<::Mysqlx::Crud::Projection> columns) {
  std::string result("projection (");
  for (int i = 0; i < columns.size(); i++) {
    std::string strcol = Expr_unparser::column_to_string(columns.Get(i));
    result += strcol;
    if (i + 1 < columns.size())
      result += ", ";
  }
  result += ")";
  return result;
}

std::string Expr_unparser::order_list_to_string(
    google::protobuf::RepeatedPtrField<::Mysqlx::Crud::Order> columns) {
  std::string result("orderby (");
  for (int i = 0; i < columns.size(); i++) {
    std::string strcol = Expr_unparser::order_to_string(columns.Get(i));
    result += strcol;
    if (i + 1 < columns.size())
      result += ", ";
  }
  result += ")";
  return result;
}

}  // namespace mysqlx
