/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef MODULES_ADMINAPI_DBA_CONFIGURE_LOCAL_INSTANCE_H_
#define MODULES_ADMINAPI_DBA_CONFIGURE_LOCAL_INSTANCE_H_

#include <map>
#include <memory>
#include <string>

#include "modules/adminapi/dba/configure_instance.h"
#include "modules/adminapi/dba/preconditions.h"
#include "modules/adminapi/mod_dba_provisioning_interface.h"
#include "mysqlshdk/include/shellcore/console.h"
#include "mysqlshdk/libs/mysql/instance.h"
#include "scripting/lang_base.h"

namespace mysqlsh {
namespace dba {

class Configure_local_instance : public Configure_instance {
 public:
  Configure_local_instance(
      mysqlshdk::mysql::IInstance *target_instance,
      const std::string &mycnf_path, const std::string &output_mycnf_path,
      const std::string &cluster_admin,
      const mysqlshdk::utils::nullable<std::string> &cluster_admin_password,
      mysqlshdk::utils::nullable<bool> clear_read_only, const bool interactive,
      mysqlshdk::utils::nullable<bool> restart,
      std::shared_ptr<ProvisioningInterface> provisioning_interface,
      std::shared_ptr<mysqlsh::IConsole> console_handler);
  ~Configure_local_instance();

  void prepare() override;
  shcore::Value execute() override;
  void rollback() override;
  void finish() override;

 private:
  GRInstanceType m_instance_type;

  void update_mycnf();
};

}  // namespace dba
}  // namespace mysqlsh

#endif  // MODULES_ADMINAPI_DBA_CONFIGURE_LOCAL_INSTANCE_H_
