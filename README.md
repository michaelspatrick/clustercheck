# clustercheck
This plugin is designed to mimic the function of the Percona clustercheck script for PXC.

The plugin introduces some new Status Variables to MySQL for controlling behavior:

    mysql> SHOW GLOBAL VARIABLES LIKE "cluster%";
    +------------------------------------+-------+
    | Variable_name                      | Value |
    +------------------------------------+-------+
    | clustercheck_available_if_donor    | 1     |
    | clustercheck_available_if_readonly | 0     |
    | clustercheck_enabled               | 1     |
    +------------------------------------+-------+
    3 rows in set (0.00 sec)
    
It will also read the status variable "read_only" to determine whether the node is in Read Only mode.  You can controll the behavior of whether or not the system should tell the proxy whether the node is available to take traffic or not by setting the variable, "clustercheck_available_if_readonly".  It can be set with the following:

    SET GLOBAL clustercheck_available_if_readonly=1;
    
The plugin will listen to port 9200 and will send back a header code and status message when you connect to that port as you can see below:

    [clustercheck]$ telnet localhost 9200
    Trying 127.0.0.1...
    Connected to localhost.
    Escape character is '^]'.
    HTTP/1.1 200 OK
    Content-Type: text/plain
    Connection: close
    Content-Length: 40
    
    Percona XtraDB Cluster Node is synced.
    Connection closed by foreign host.
    
### Future Enhancements
    Need to finish up the code by making it look at the wsrep_local_state variable.  I am testing on a regular MySQL installation and need to setup a PXC installation.  
    Also, need to make the port upon which it listens configurable.
    Finally, make sure that the new variables are available via the MySQL config file.
