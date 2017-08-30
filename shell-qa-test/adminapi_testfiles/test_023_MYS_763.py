import subprocess
import time
import sys
import datetime
import platform
import os
import threading
import functools
import unittest
import json
import xmlrunner
import shutil
#sys.path.append('/home/guidev/Descargas/AdminAPI_Scripts/testsFiles/')
#sys.path.append('../')
from testFunctions import read_line
from testFunctions import read_til_getShell
from testFunctions import kill_process
from testFunctions import exec_xshell_commands
import logging
logger = logging.getLogger()



############   Retrieve variables from configuration file    ##########################
class LOCALHOST:
    user =""
    password = ""
    host = ""
    xprotocol_port = ""
    port =""
class REMOTEHOST:
    user = ""
    password =""
    host = ""
    xprotocol_port = ""
    port = ""

if 'CONFIG_PATH' in os.environ and 'MYSQLX_PATH' in os.environ and os.path.isfile(os.environ['CONFIG_PATH']) and os.path.isfile(os.environ['MYSQLX_PATH']):
    # **** JENKINS EXECUTION ****
    config_path = os.environ['CONFIG_PATH']
    config=json.load(open(config_path))
    MYSQL_SHELL = os.environ['MYSQLX_PATH']
    Exec_files_location = os.environ['AUX_FILES_PATH']
    cluster_Path = os.environ['CLUSTER_PATH']
    XSHELL_QA_TEST_ROOT = os.environ['XSHELL_QA_TEST_ROOT']
    XMLReportFilePath = XSHELL_QA_TEST_ROOT+"/adminapi_qa_test.xml"
else:
    # **** LOCAL EXECUTION ****
    config=json.load(open('config_local.json'))
    MYSQL_SHELL = str(config["general"]["xshell_path"])
    Exec_files_location = str(config["general"]["aux_files_path"])
    cluster_Path = str(config["general"]["cluster_path"])
    XMLReportFilePath = "adminapi_qa_test.xml"

#########################################################################

LOCALHOST.user = str(config["local"]["user"])
LOCALHOST.password = str(config["local"]["password"])
LOCALHOST.host = str(config["local"]["host"])
LOCALHOST.xprotocol_port = str(config["local"]["xprotocol_port"])
LOCALHOST.port = str(config["local"]["port"])

REMOTEHOST.user = str(config["remote"]["user"])
REMOTEHOST.password = str(config["remote"]["password"])
REMOTEHOST.host = str(config["remote"]["host"])
REMOTEHOST.xprotocol_port = str(config["remote"]["xprotocol_port"])
REMOTEHOST.port = str(config["remote"]["port"])



class globalvar:
    last_found=""
    last_search=""

###########################################################################################

class XShell_TestCases(unittest.TestCase):


  def test_023_MYS_763_getCluster(self):
      '''MYS-763 [MYAA] dba.getCluster([name][, options])'''
      logger.debug("--------- " + str(self._testMethodName) + " ---------")
      instance = "3322"
      results = ''
      init_command = [MYSQL_SHELL, '--interactive=full', '--passwords-from-stdin']
      x_cmds = [("dba.deploySandboxInstance(" + instance + ", { sandboxDir: \"" + cluster_Path + "\"});\n",
                 'Please enter a MySQL root password for the new instance:'),
                (LOCALHOST.password + '\n',
                 "Instance localhost:" + instance + " successfully deployed and started."),
                ]
      results = exec_xshell_commands(init_command, x_cmds)
      try:
          if results.find(bytearray("FAIL", "ascii"), 0, len(results)) > -1:
              self.assertEqual(results, 'PASS')
      except Exception as ex:
              self.assertEqual('FAIL', 'PASS')
      results = ''
      init_command = [MYSQL_SHELL, '--interactive=full', '-u' + LOCALHOST.user, '--password=' + LOCALHOST.password,
                      '-h' + LOCALHOST.host, '-P' + instance, '--mysql']
      x_cmds = [("dba.createCluster(\"Cluster2\", {\"clusterAdminType\": \"local\"});\n", "<Cluster:Cluster2>"),
                ("dba.getCluster('Cluster2');\n", "<Cluster:Cluster2>"),
                ]
      results = exec_xshell_commands(init_command, x_cmds)
      self.assertEqual(results, 'PASS')


  # ----------------------------------------------------------------------
#
# if __name__ == '__main__':
#     unittest.main()

if __name__ == '__main__':
  unittest.main( testRunner=xmlrunner.XMLTestRunner(file(XMLReportFilePath,"w")))
