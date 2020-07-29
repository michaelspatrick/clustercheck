PLUGINDIR=/jet/var/mysqld/plugin

CC=g++
CFLAGS=-DMYSQL_DYNAMIC_PLUGIN -Wall -fPIC -shared \
-I/usr/include/mysql -m64 \
-I../../sql \
-I../../include \
-I../../libbinlogevents/export \
-I../../libbinlogevents/include \
-I../../sql \
-I./includes \
-I./

all: clustercheck.so

clustercheck.so: clustercheck.c
        $(CC) $(CFLAGS) -o clustercheck.so clustercheck.c

clean:
      	rm -f ${PLUGINDIR}/clustercheck.so
        rm -f clustercheck.so

install:
        cp -f clustercheck.so ${PLUGINDIR}

uninstall:
	rm -f ${PLUGINDIR}/clustercheck.so

distclean:
	rm -f clustercheck.so
