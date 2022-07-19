#include <stdio.h>
#include <mysql_version.h>
#include <mysql/plugin.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include <mysql/service_security_context.h>
#include <mysqld_error.h>
#include <typelib.h>
#include <mysql_com.h>

#include <sql/wsrep_mysqld.h>

// Variables from the daemon
extern bool read_only;
extern const char *wsrep_cluster_status;

// Local variables
long enabled = 1;
long available_if_donor = 0;
long available_if_readonly = 0;
static MYSQL_SYSVAR_LONG(available_if_readonly, available_if_readonly, 0, "Availability of node if it is readonly", NULL, NULL, 0, 0, 1, 0);
static MYSQL_SYSVAR_LONG(available_if_donor, available_if_donor, 0, "Availability of node if it is a donor", NULL, NULL, 0, 0, 1, 0);
static MYSQL_SYSVAR_LONG(enabled, enabled, 0, "Whether cluster check is enabled", NULL, NULL, 0, 0, 1, 0);
static SYS_VAR *system_variables[] = {MYSQL_SYSVAR(enabled), MYSQL_SYSVAR(available_if_donor), MYSQL_SYSVAR(available_if_readonly), nullptr};

void *listener(void *) {
  int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr;
  int listen_port = 9200;
  int max_connections_to_queue = 10;
  char sendBuff[1025];

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&serv_addr, '0', sizeof(serv_addr));
  memset(sendBuff, '0', sizeof(sendBuff));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(listen_port);
  bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  listen(listenfd, max_connections_to_queue);
  while(1) {
    connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

    if ((strcmp(wsrep_cluster_status, "Primary") == 0) && (wsrep_node_is_synced() || (wsrep_node_is_donor() && available_if_donor == 1))) {
      if (enabled != 1) { // check is disabled
        strcpy(sendBuff, "HTTP/1.1 503 Service Unavailable\r\n");
        strcat(sendBuff, "Content-Type: text/plain\r\n");
        strcat(sendBuff, "Connection: close\r\n");
        strcat(sendBuff, "Content-Length: 51\r\n");
        strcat(sendBuff, "\r\n");
        strcat(sendBuff, "Percona XtraDB Cluster Node is manually disabled.\r\n");
        [&]{ return write(connfd, sendBuff, strlen(sendBuff)); }();
        close(connfd);
      } else {  // check is enabled
        if (available_if_readonly == 0) {
          if (read_only) {  // in read-only state
            strcpy(sendBuff, "HTTP/1.1 503 Service Unavailable\r\n");
            strcat(sendBuff, "Content-Type: text/plain\r\n");
            strcat(sendBuff, "Connection: close\r\n");
            strcat(sendBuff, "Content-Length: 43\r\n");
            strcat(sendBuff, "\r\n");
            strcat(sendBuff, "Percona XtraDB Cluster Node is read-only.\r\n");
            [&]{ return write(connfd, sendBuff, strlen(sendBuff)); }();
            close(connfd);
          }
        }
        // must be a success if we made it here
        strcpy(sendBuff, "HTTP/1.1 200 OK\r\n");
        strcat(sendBuff, "Content-Type: text/plain\r\n");
        strcat(sendBuff, "Connection: close\r\n");
        strcat(sendBuff, "Content-Length: 40\r\n");
        strcat(sendBuff, "\r\n");
        strcat(sendBuff, "Percona XtraDB Cluster Node is synced.\r\n");
        [&]{ return write(connfd, sendBuff, strlen(sendBuff)); }();
        close(connfd);
      }
    } else {  // node is non-primary or not synced
      strcpy(sendBuff, "HTTP/1.1 503 Service Unavailable\r\n");
      strcat(sendBuff, "Content-Type: text/plain\r\n");
      strcat(sendBuff, "Connection: close\r\n");
      strcat(sendBuff, "Content-Length: 57\r\n");
      strcat(sendBuff, "\r\n");
      strcat(sendBuff, "Percona XtraDB Cluster Node is not synced or non-PRIM.\r\n");
      [&]{ return write(connfd, sendBuff, strlen(sendBuff)); }();
      close(connfd);
    }

    close(connfd);
    sleep(1);
  }
}

static struct st_mysql_daemon clustercheck_daemon_plugin = { MYSQL_DAEMON_INTERFACE_VERSION };

static int clustercheck_daemon_plugin_init(void *) {
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, listener, NULL);
  return 0;
}

static int clustercheck_daemon_plugin_deinit(void *) {
  return 0;
}

mysql_declare_plugin(clustercheck_daemon) {
  MYSQL_DAEMON_PLUGIN,
  &clustercheck_daemon_plugin,
  "clustercheck",
  "Michael Patrick",
  "Returns status of the cluster for proxy",
  PLUGIN_LICENSE_GPL,
  clustercheck_daemon_plugin_init, /* Plugin Init */
  nullptr,
  clustercheck_daemon_plugin_deinit, /* Plugin Deinit */
  0x0100,
  NULL,
  system_variables, /* clustercheck_system_variables, */
  NULL,
  0
}
mysql_declare_plugin_end;
