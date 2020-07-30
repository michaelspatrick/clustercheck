# clustercheck
This plugin is designed to mimic the function of the Percona clustercheck script for PXC.

### Server Setup

    sudo yum install wget
    wget https://www.percona.com/downloads/Percona-XtraDB-Cluster-LATEST/Percona-XtraDB-Cluster-8.0.19-10.1/source/tarball/Percona-XtraDB-Cluster-8.0.19-10.tar.gz
    tar -zxvf Percona-XtraDB-Cluster-8.0.19-10.tar.gz
    cd Percona-XtraDB-Cluster-8.0.19-10
    sudo yum install git
    sudo yum install gcc
    sudo yum install gcc-c++
    sudo yum install make
    sudo yum install cmake
    sudo yum install boost
    sudo yum install ncurses-devel
    sudo yum install readline-devel
    sudo yum install openssl-devel
    sudo yum install libtirpc-devel
    sudo yum install curl-devel
    sudo rpm -i http://mirror.centos.org/centos/8/PowerTools/x86_64/os/Packages/rpcgen-1.3.1-4.el8.x86_64.rpm
    cmake -DDOWNLOAD_BOOST=1 -DFORCE_INSOURCE_BUILD=1 -DWITH_BOOST=..

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
