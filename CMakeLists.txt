MYSQL_ADD_PLUGIN(clustercheck src/clustercheck.cc
  MODULE_ONLY)
  
ADD_DEFINITIONS(-DMYSQL_SERVER)
