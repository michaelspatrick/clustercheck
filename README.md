# clustercheck
This MySQL plugin is designed to mimic the function of the Percona clustercheck script for PXC. This plugin is still in development and is currently a POC.  Do not use in a Production environment until it has been further tested.

### How does it work?
The MySQL server daemon plugin initiates a listener on port 9200 which awaits a connection.  Once one is made, the node's availability is checked with the same logic as is used in scripts commonly used.  Instead of querying the database, however, the plugin utilizes internal system calls to verify availability.  An HTTP header, status code, and message are then output and the connection is closed. This mimics the functionality of the scripts currently used for this purpose.

### Why the need for a clustercheck plugin?
In order to use a load balancer with PXC, such as HAProxy, you want to be able to query the server easily to see if the node is available to respond to traffic.  Most of the time this requires configuring a script, usually written in Bash or Python, to login to MySQL and check a few status variables and then respond with a HTTP header, status code, and text.  The load balancer then reads this response and uses it to determine whether to route queries to the node or not.  There is also a maintenance mode capability that is simple to enable.

Above all, the goal of this plugin is to provide a simple way to interface with a load balancer.  Not everyone needs a complex system with lots of configurable options.  Those systems are great but require more knowledge from the user to configure it properly.  This is as simple as enable it and it just works.

While this approach works fine, it does offer a few disadvantages:
* Must be able to become root to install the script.  This is problematic for some DBAs who do not have root access, thus requiring a SysAdmin to do the work for them.
* Must be knowledgeable of xinetd in Linux and know how to setup the service.
* Username and password for the connection are stored unencrypted in plain text format in the script, jeopardizing security.
* The script must execute a query every time it is called, which can impact performance.

Advantages of the plugin method:
* Easy and simple to install and configure.
* No need for root privileges to configure.
* No need for knowledge of xinetd or services.
* No need to configure access or store login information in a text file.  The plugin runs internal to the MySQL daemon.
* Plugin uses system calls and variables instead of performing SQL queries.
* Ability to enter maintenance mode via a simple SQL command.

In all fairness, performance impact needs to be further tested.

### Development Server Setup

The following was used to setup a t2.medium instance running Amazon Linux on an AWS EC2 instance for development and compilation of the plugin:

    sudo yum -y install git
    sudo yum -y install gcc
    sudo yum -y install gcc-c++
    sudo yum -y install make
    sudo yum -y install cmake
    sudo yum -y install boost
    sudo yum -y install ncurses-devel
    sudo yum -y install readline-devel
    sudo yum -y install openssl-devel
    sudo yum -y install libtirpc-devel
    sudo yum -y install curl-devel
    sudo yum -y install rpcgen
    sudo yum -y remove cmake
    sudo yum -y install gcc-c++
    sudo yum -y install cyrus-sasl cyrus-sasl-devel
    sudo yum -y install openldap-devel
    sudo yum -y install telnet

    sudo wget https://cmake.org/files/v3.5/cmake-3.5.2.tar.gz
    sudo tar -xvzf cmake-3.5.2.tar.gz
    cd cmake-3.5.2/
    sudo ./bootstrap
    sudo make
    sudo make install
    cd ..

    sudo wget https://boostorg.jfrog.io/artifactory/main/release/1.73.0/source/boost_1_73_0.tar.bz2
    sudo tar --bzip2 -xf boost_1_73_0.tar.bz2
    cd boost_1_73_0/
    sudo ./bootstrap.sh
    sudo ./b2
    cd ..

    sudo wget http://zlib.net/zlib-1.2.12.tar.gz
    sudo tar -xvf zlib-1.2.12.tar.gz
    cd zlib-1.2.12/
    sudo ./configure
    sudo make
    sudo make install
    cd ..

    sudo wget https://downloads.percona.com/downloads/Percona-XtraDB-Cluster-LATEST/Percona-XtraDB-Cluster-8.0.27/source/tarball/Percona-XtraDB-Cluster-8.0.27-18.tar.gz
    sudo tar -zxvf Percona-XtraDB-Cluster-8.0.27-18.tar.gz
    cd Percona-XtraDB-Cluster-8.0.27-18/
    cmake -DDOWNLOAD_BOOST=1 -DFORCE_INSOURCE_BUILD=1 -DWITH_BOOST=.. -DWITH_AUTHENTICATION_LDAP=OFF
    make
    sudo make install

    # Start MySQL
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/lib64
    sudo /usr/local/mysql/bin/mysqld --initialize
    sudo chown ec2-user:ec2-user /var/lib/mysql -R
    /usr/local/mysql/bin/mysqld &
    
    # Connect to MySQL and reset password
    /usr/local/mysql/bin/mysql -uroot -p -h127.0.0.1
    mysql> ALTER USER 'root'@'localhost' IDENTIFIED BY 'PASSWORD';
    mysql> FLUSH PRIVILEGES;

### Compile the Plugin

You will want to have the source code compiled for PXC and have the MySQL daemon up and running before you attempt to compile the plugin.  Once that is done, you will want to go into the source directory for PXC and clone the repo.  You can then compile the plugin by a simple "make clustercheck" command in the base directory.  After that, you will need to run the "INSTALL PLUGIN" command.

    cd Percona-XtraDB-Cluster-8.0.27-18/plugin
    sudo git clone https://github.com/michaelspatrick/clustercheck.git
    sudo chown ec2-user:ec2-user clustercheck -R
    cd ..
    cmake .
    make clustercheck

### Install the Plugin
    mysql> INSTALL PLUGIN clustercheck SONAME 'clustercheck.so';
    
### Uninstall the Plugin
    mysql> UNINSTALL PLUGIN clustercheck;
    
### Plugin Global Variables

The plugin introduces some new Status Variables to MySQL for controlling behavior:
   
    mysql> SHOW GLOBAL VARIABLES LIKE "cluster%";
    +------------------------------------+-------+
    | Variable_name                      | Value |
    +------------------------------------+-------+
    | clustercheck_available_if_donor    | 0     |
    | clustercheck_available_if_readonly | 1     |
    | clustercheck_maintenance_mode      | 0     |
    | clustercheck_port                  | 9200  |
    +------------------------------------+-------+
    4 rows in set (0.00 sec)
    
It will also read the status variable "read_only" to determine whether the node is in Read Only mode.  You can control the behavior of whether or not the system should tell the proxy if the node is available to take traffic or not by setting the variable, "clustercheck_available_if_readonly".  It can be set with the following:

    SET GLOBAL clustercheck_available_if_readonly=1;
    
If you want to make the values more permanent and make them persist a restart:

    SET PERSIST clustercheck_available_if_readonly=1;

You can also set availability if the node is in donor mode:

    SET GLOBAL clustercheck_available_if_donor=1;

Likewise, you can make the values persist a restart with:

    SET PERSIST clustercheck_available_if_donor=1;

If you want to change ports on which it is listening, you can do so.  There is one caveat, however.  The plugin will still be listening on the port for one more connection before it is able to switch ports.  I am not sure if there is an easy way around this, but it is one minor detail to keep in mind when you make a change.  A simple "telnet localhost <oldport>" is enough to close the open socket and re-open with the new port number.

As before, you can make the change to port 8080 (as an example) temporary with the following:

    SET GLOBAL clustercheck_port=8080;

Or, you can make the change persist a restart with:

    SET PERSIST clustercheck_port=8080;

### Plugin Status Variables

If you want to see how many connections have been serviced, as well as how many dropped connections, you can query global status such as:

    mysql> SHOW GLOBAL STATUS LIKE "cluster%";
    +----------------------------------+--------+
    | Variable_name                    | Value  |
    +----------------------------------+--------+
    | clustercheck_connections         | 141003 |
    | clustercheck_refused_connections | 0      |
    +----------------------------------+--------+
    2 rows in set (0.00 sec)

### Maintenance Mode

The plugin allows you to set the node unavailable for receiving traffic by setting the clustercheck_maintenance_mode variable via a SET GLOBAL command;

    SET GLOBAL clustercheck_maintenance_mode=1;

If you want to keep the node in maintenance mode even after a restart, you can make the change persist with the following:

    SET PERSIST clustercheck_maintenance_mode=1;

This allows for putting a node in maintenance mode manually.  This could be useful for performing upgrades, running backups, etc.  With the ability to do this via a SQL command, it is very easy to do.
    
### Testing the Plugin

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

I have also tested it with multiple connections with no refused connections as of yet.  In this test I did 100,000 connections:

    for i in {1..100000}; do echo $i; telnet localhost 9200 & done

This was done with a tcp backlog of 4096 which was the value of /proc/sys/net/core/somaxconn.  Below check of the status variable shows no dropped connections:

    mysql> SHOW GLOBAL STATUS LIKE "cluster%";
    +----------------------------------+--------+
    | Variable_name                    | Value  |
    +----------------------------------+--------+
    | clustercheck_connections         | 141003 |
    | clustercheck_refused_connections | 0      |
    +----------------------------------+--------+
    2 rows in set (0.00 sec)


### Future Enhancements
There are a few things I would like to do to make the plugin more useful if there is interest in doing so.

* Add a test to ensure the Galera plugin is enabled
* Make the port upon which it listens configurable
* Plan to do a version in the near future for Group Replication
