cmake_minimum_required(VERSION 3.5)

MYSQL_ADD_PLUGIN(clustercheck src/clustercheck.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "clustercheck")

ADD_DEFINITIONS(-DMYSQL_SERVER)
