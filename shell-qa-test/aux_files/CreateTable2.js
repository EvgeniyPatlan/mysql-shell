var mysqlx=require('mysqlx').mysqlx;
var session=mysqlx.getNodeSession('root:guidev!@localhost:33060');
session.sql('use sakila;').execute();
session.sql('drop table if exists testdb2;').execute();
session.sql('create table testdb2 (name varchar(50), age integer, last_name varchar(100));').execute();