/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef MYSQLSHDK_LIBS_MYSQL_UTILS_H_
#define MYSQLSHDK_LIBS_MYSQL_UTILS_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include "mysqlshdk/libs/db/session.h"
#include "mysqlshdk/libs/utils/enumset.h"

namespace mysqlshdk {
namespace mysql {

enum class Account_attribute { Grants };

static constexpr size_t kPASSWORD_LENGTH = 32;

using Account_attribute_set =
    utils::Enum_set<Account_attribute, Account_attribute::Grants>;

void clone_user(const std::shared_ptr<db::ISession> &session,
                const std::string &orig_user, const std::string &orig_host,
                const std::string &new_user, const std::string &new_host,
                const std::string &password,
                Account_attribute_set flags =
                    Account_attribute_set(Account_attribute::Grants));

/** Drops all accounts for a given username */
void drop_all_accounts_for_user(const std::shared_ptr<db::ISession> &session,
                                const std::string &user);

void create_user_with_random_password(
    const std::shared_ptr<db::ISession> &session, const std::string &user,
    const std::vector<std::string> &hosts,
    const std::vector<std::tuple<std::string, std::string, bool>> &grants,
    std::string *out_password);

std::string generate_password(size_t password_length = kPASSWORD_LENGTH);

}  // namespace mysql
}  // namespace mysqlshdk

#endif  // MYSQLSHDK_LIBS_MYSQL_UTILS_H_
