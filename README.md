# clustercheck
This plugin is designed to mimic the function of the Percona clustercheck script for PXC. This plugin is still in development and not completed.

### How does it work?
The MySQL server daemon plugin initiates a listener on port 9200 which awaits a connection.  Once one is made, the node's availability is checked with the same logic as is used in scripts commonly used.  Instead of querying the database, however, the plugin utilizes internal system calls to verify availability.  An HTTP header, status code, and message are then output and the connection is closed. This mimics the functionality of the scripts currently used for this purpose.

### Why the need for a clustercheck plugin?
In order to use a load balancer with PXC, such as HAProxy, you want to be able to query the server easily to see if the node is available to respond to traffic.  Most of the time this requires configuring a script, usually written in Bash or Python, to login to MySQL and check a few status variables and then respond with a HTTP header, status code, and text.  The load balancer then reads this response and uses it to determine whether to route queries to the node or not.

While this approach works fine, it does offer a few disadvantages:
* Must be able to become root to install the script.  This is problematic for some DBAs who do not have root access, thus requiring a SysAdmin to do the work for them.
* Must be knowledgeable of xinetd in Linux and know how to setup the service.
* Username and password for the connection are stored unencrypted in plain text format in the script, jeopardizing security.
* The script must execute a query every time it is called, which can impact performance.

Advantages of the plugin method:
* Easy to install and configure.
* No need for root privileges to configure.
* No need for knowledge of xinetd or services.
* No need to configure access or store login informaiton in a text file.  The plugin runs internal to the MySQL daemon.
* Plugin uses system calls and variables instead of performing SQL queries.

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
    mysql> ALTER USER 'root'@'localhost' IDENTIFIED BY '##MySQL123##';
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
    
### Plugin Status Variables

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
    
It will also read the status variable "read_only" to determine whether the node is in Read Only mode.  You can control the behavior of whether or not the system should tell the proxy if the node is available to take traffic or not by setting the variable, "clustercheck_available_if_readonly".  It can be set with the following:

    SET GLOBAL clustercheck_available_if_readonly=1;
    
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
    
### Future Enhancements
There are a few things I would like to do to make the plugin more useful if there is interest in doing so.

* Add a test to ensure the Galera plugin is enabled
* Make the port upon which it listens configurable
* Make sure that the global variables are available to set via the MySQL config file
