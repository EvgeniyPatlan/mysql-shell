/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "mysqlshdk/libs/db/mysql/row.h"
#include <cerrno>
#include <climits>  // C limit constants
#include <cmath>    // HUGE_VAL
#include <limits>   // std::numeric_limits
#include <string>
#include <utility>
#include "mysqlshdk/libs/db/mysql/result.h"
#include "mysqlshdk/libs/utils/utils_string.h"

#define bit_uint1korr(A) (*(((uint8_t *)(A))))

#define bit_uint2korr(A)                                \
  ((uint16_t)(((uint16_t)(((unsigned char *)(A))[1])) + \
              ((uint16_t)(((unsigned char *)(A))[0]) << 8)))
#define bit_uint3korr(A)                                       \
  ((uint32_t)(((uint32_t)(((unsigned char *)(A))[2])) +        \
              (((uint32_t)(((unsigned char *)(A))[1])) << 8) + \
              (((uint32_t)(((unsigned char *)(A))[0])) << 16)))
#define bit_uint4korr(A)                                        \
  ((uint32_t)(((uint32_t)(((unsigned char *)(A))[3])) +         \
              (((uint32_t)(((unsigned char *)(A))[2])) << 8) +  \
              (((uint32_t)(((unsigned char *)(A))[1])) << 16) + \
              (((uint32_t)(((unsigned char *)(A))[0])) << 24)))
#define bit_uint5korr(A)                                         \
  ((uint64_t)(((uint32_t)(((unsigned char *)(A))[4])) +          \
              (((uint32_t)(((unsigned char *)(A))[3])) << 8) +   \
              (((uint32_t)(((unsigned char *)(A))[2])) << 16) +  \
              (((uint32_t)(((unsigned char *)(A))[1])) << 24)) + \
   (((uint64_t)(((unsigned char *)(A))[0])) << 32))
#define bit_uint6korr(A)                                         \
  ((uint64_t)(((uint32_t)(((unsigned char *)(A))[5])) +          \
              (((uint32_t)(((unsigned char *)(A))[4])) << 8) +   \
              (((uint32_t)(((unsigned char *)(A))[3])) << 16) +  \
              (((uint32_t)(((unsigned char *)(A))[2])) << 24)) + \
   (((uint64_t)(((uint32_t)(((unsigned char *)(A))[1])) +        \
                (((uint32_t)(((unsigned char *)(A))[0]) << 8)))) \
    << 32))
#define bit_uint7korr(A)                                          \
  ((uint64_t)(((uint32_t)(((unsigned char *)(A))[6])) +           \
              (((uint32_t)(((unsigned char *)(A))[5])) << 8) +    \
              (((uint32_t)(((unsigned char *)(A))[4])) << 16) +   \
              (((uint32_t)(((unsigned char *)(A))[3])) << 24)) +  \
   (((uint64_t)(((uint32_t)(((unsigned char *)(A))[2])) +         \
                (((uint32_t)(((unsigned char *)(A))[1])) << 8) +  \
                (((uint32_t)(((unsigned char *)(A))[0])) << 16))) \
    << 32))
#define bit_uint8korr(A)                                          \
  ((uint64_t)(((uint32_t)(((unsigned char *)(A))[7])) +           \
              (((uint32_t)(((unsigned char *)(A))[6])) << 8) +    \
              (((uint32_t)(((unsigned char *)(A))[5])) << 16) +   \
              (((uint32_t)(((unsigned char *)(A))[4])) << 24)) +  \
   (((uint64_t)(((uint32_t)(((unsigned char *)(A))[3])) +         \
                (((uint32_t)(((unsigned char *)(A))[2])) << 8) +  \
                (((uint32_t)(((unsigned char *)(A))[1])) << 16) + \
                (((uint32_t)(((unsigned char *)(A))[0])) << 24))) \
    << 32))

namespace mysqlshdk {
namespace db {
namespace mysql {

Row::Row(std::shared_ptr<Result> result, MYSQL_ROW row,
         const unsigned long *lengths)
    : _result(result), _row(row) {
  // TODO(alfredo) there's actually no need to keep a copy of the lengths list
  // because it is valid for as long as MYSQL_ROW is and both should become
  // invalidated when the next row is fetched
  for (size_t index = 0; index < result->get_metadata().size(); index++)
    _lengths.push_back(lengths[index]);
}

#define FIELD_ERROR(index, msg) \
  std::invalid_argument(        \
      shcore::str_format("%s(%u): " msg, __FUNCTION__, index).c_str())

#define FIELD_ERROR1(index, msg, arg) \
  std::invalid_argument(              \
      shcore::str_format("%s(%u): " msg, __FUNCTION__, index, arg).c_str())

#define VALIDATE_INDEX(index)                          \
  do {                                                 \
    if (index >= num_fields())                         \
      throw FIELD_ERROR(index, "index out of bounds"); \
  } while (0)

#define VALIDATE_TYPE(index, TYPE_CHECK)                                       \
  do {                                                                         \
    if (index >= static_cast<uint32_t>(_result->get_metadata().size()))        \
      throw FIELD_ERROR(index, "index out of bounds");                         \
    if (_row[index] == nullptr)                                                \
      throw FIELD_ERROR(index, "field is NULL");                               \
    Type ftype = get_type(index);                                              \
    if (!(TYPE_CHECK))                                                         \
      throw FIELD_ERROR1(index, "field type is %s", to_string(ftype).c_str()); \
  } while (0)

bool Row::is_null(uint32_t index) const {
  VALIDATE_INDEX(index);

  return _row[index] == NULL;
}

uint32_t Row::num_fields() const {
  return static_cast<uint32_t>(_result->get_metadata().size());
}

Type Row::get_type(uint32_t index) const {
  VALIDATE_INDEX(index);
  return _result->get_metadata().at(index).get_type();
}

std::string Row::get_as_string(uint32_t index) const {
  VALIDATE_INDEX(index);
  if (!_row[index])
    // Now we mimic the old row behavior since AdminAPI dependes on "NULL"
    // being returned on these cases, if previous logic is required, AdminAPI
    // must be fixed
    // throw FIELD_ERROR(index, "field is NULL");
    return "NULL";
  if (get_type(index) == Type::Bit)
    return shcore::bits_to_string(get_bit(index),
                                  _result->get_metadata()[index].get_length());
  return std::string(_row[index], _lengths[index]);
}

int64_t Row::get_int(uint32_t index) const {
  int64_t ret_val = 0;

  VALIDATE_TYPE(index, (ftype == Type::Integer || ftype == Type::UInteger));

  if (_result->get_metadata()[index].is_unsigned()) {
    uint64_t unsigned_val = strtoull(_row[index], nullptr, 10);

    if ((errno == ERANGE && unsigned_val == ULLONG_MAX) ||
        unsigned_val > (std::numeric_limits<int64_t>::max)())
      throw FIELD_ERROR(index, "field value exceeds allowed range");

    ret_val = static_cast<int64_t>(unsigned_val);
  } else {
    ret_val = strtoll(_row[index], nullptr, 10);

    if (errno == ERANGE && (ret_val == LLONG_MAX || ret_val == LLONG_MIN))
      throw FIELD_ERROR(index, "field value out of the allowed range");
  }
  return ret_val;
}

uint64_t Row::get_uint(uint32_t index) const {
  uint64_t ret_val = 0;

  VALIDATE_TYPE(index, (ftype == Type::Integer || ftype == Type::UInteger));

  if (_result->get_metadata()[index].is_unsigned()) {
    ret_val = strtoull(_row[index], nullptr, 10);
    if (ret_val == ULLONG_MAX && errno == ERANGE)
      throw FIELD_ERROR(index, "field value exceeds allowed range");
  } else {
    int64_t signed_val = strtoll(_row[index], nullptr, 10);
    if (signed_val < 0 || (errno == ERANGE && (signed_val == LLONG_MAX ||
                                               signed_val == LLONG_MIN)))
      throw FIELD_ERROR(index, "field value out of the allowed range");
    ret_val = static_cast<uint64_t>(signed_val);
  }
  return ret_val;
}

std::string Row::get_string(uint32_t index) const {
  VALIDATE_TYPE(index, (is_string_type(ftype)));

  return std::string(_row[index], _lengths[index]);
}

std::pair<const char *, size_t> Row::get_string_data(uint32_t index) const {
  VALIDATE_TYPE(index, (is_string_type(ftype)));
  return std::pair<const char *, size_t>(_row[index], _lengths[index]);
}

float Row::get_float(uint32_t index) const {
  float ret_val = 0;

  VALIDATE_TYPE(index, (ftype == Type::Float || ftype == Type::Double ||
                        ftype == Type::Decimal));

  ret_val = strtof(_row[index], nullptr);
  if (errno == ERANGE && (ret_val == HUGE_VAL || ret_val == -HUGE_VAL))
    throw FIELD_ERROR(index, "float value out of the allowed range");
  return ret_val;
}

double Row::get_double(uint32_t index) const {
  double ret_val = 0;

  VALIDATE_TYPE(index, (ftype == Type::Float || ftype == Type::Double ||
                        ftype == Type::Decimal));

  ret_val = strtod(_row[index], nullptr);
  if (errno == ERANGE && (ret_val == HUGE_VAL || ret_val == -HUGE_VAL))
    throw FIELD_ERROR(index, "double value out of the allowed range");
  return ret_val;
}

uint64_t Row::get_bit(uint32_t index) const {
  VALIDATE_TYPE(index, (ftype == Type::Bit));
  uint64_t uval = 0;
  switch (_lengths[index]) {
    case 8:
      uval = static_cast<uint64_t>(bit_uint8korr(_row[index]));
      break;
    case 7:
      uval = static_cast<uint64_t>(bit_uint7korr(_row[index]));
      break;
    case 6:
      uval = static_cast<uint64_t>(bit_uint6korr(_row[index]));
      break;
    case 5:
      uval = static_cast<uint64_t>(bit_uint5korr(_row[index]));
      break;
    case 4:
      uval = static_cast<uint64_t>(bit_uint4korr(_row[index]));
      break;
    case 3:
      uval = static_cast<uint64_t>(bit_uint3korr(_row[index]));
      break;
    case 2:
      uval = static_cast<uint64_t>(bit_uint2korr(_row[index]));
      break;
    case 1:
      uval = static_cast<uint64_t>(bit_uint1korr(_row[index]));
      break;
    case 0:
      uval = 0;
      break;
  }
  return uval;
}

}  // namespace mysql
}  // namespace db
}  // namespace mysqlshdk
