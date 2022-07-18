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

extern my_bool read_only;
long long enabled;
long long available_if_donor;
long long available_if_readonly;

int enabled_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value) {
  long long buf;
  value->val_int(value, &buf);
  *(long long*) save = buf;
  return 0;
}
void enabled_update(MYSQL_THD thd, struct st_mysql_sys_var *var, void *var_ptr, const void *save) {
    enabled = *(long long *)save;
}
MYSQL_SYSVAR_LONGLONG(enabled, enabled, 0, "Status of clustercheck", enabled_check, enabled_update, 1, 0, 1, 0);

int available_if_donor_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value) {
  long long buf;
  value->val_int(value, &buf);
  *(long long*) save = buf;
  return 0;
}
void available_if_donor_update(MYSQL_THD thd, struct st_mysql_sys_var *var, void *var_ptr, const void *save) {
    available_if_donor = *(long long *)save;
}
MYSQL_SYSVAR_LONGLONG(available_if_donor, available_if_donor, 0, "Availability of node if it is a donor", available_if_donor_check, available_if_donor_update, 1, 0, 1, 0);

int available_if_readonly_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value) {
  long long buf;
  value->val_int(value, &buf);
  *(long long*) save = buf;
  return 0;
}
void available_if_readonly_update(MYSQL_THD thd, struct st_mysql_sys_var *var, void *var_ptr, const void *save) {
    available_if_readonly = *(long long *)save;
}
MYSQL_SYSVAR_LONGLONG(available_if_readonly, available_if_readonly, 0, "Availability of node if it is readonly", available_if_readonly_check, available_if_readonly_update, 0, 0, 1, 0);

struct st_mysql_sys_var* vars_enabled[] =
{
MYSQL_SYSVAR(enabled),
MYSQL_SYSVAR(available_if_donor),
MYSQL_SYSVAR(available_if_readonly),
NULL
};

void *listener(void *vargp) {
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

    if (enabled != 1) {
      // disabled
      strcpy(sendBuff, "HTTP/1.1 503 Service Unavailable\r\n");
      strcat(sendBuff, "Content-Type: text/plain\r\n");
      strcat(sendBuff, "Connection: close\r\n");
      strcat(sendBuff, "Content-Length: 51\r\n");
      strcat(sendBuff, "\r\n");
      strcat(sendBuff, "Percona XtraDB Cluster Node is manually disabled.\r\n");
      write(connfd, sendBuff, strlen(sendBuff));
      close(connfd);
    //} else if (wsrep_local_state == "4" || ((wsrep_local_state == "2") && (available_if_donor == 1))) {
    } else if (available_if_donor == 1) {
      if (available_if_readonly == 1) {
        if (read_only) {
          // in read-only state
          strcpy(sendBuff, "HTTP/1.1 503 Service Unavailable\r\n");
          strcat(sendBuff, "Content-Type: text/plain\r\n");
          strcat(sendBuff, "Connection: close\r\n");
          strcat(sendBuff, "Content-Length: 43\r\n");
          strcat(sendBuff, "\r\n");
          strcat(sendBuff, "Percona XtraDB Cluster Node is read-only.\r\n");
          write(connfd, sendBuff, strlen(sendBuff));
          close(connfd);
        } else {
          // success
          strcpy(sendBuff, "HTTP/1.1 200 OK\r\n");
          strcat(sendBuff, "Content-Type: text/plain\r\n");
          strcat(sendBuff, "Connection: close\r\n");
          strcat(sendBuff, "Content-Length: 40\r\n");
          strcat(sendBuff, "\r\n");
          strcat(sendBuff, "Percona XtraDB Cluster Node is synced.\r\n");
          write(connfd, sendBuff, strlen(sendBuff));
          close(connfd);
        }
      } else {
	// success
        strcpy(sendBuff, "HTTP/1.1 200 OK\r\n");
        strcat(sendBuff, "Content-Type: text/plain\r\n");
        strcat(sendBuff, "Connection: close\r\n");
        strcat(sendBuff, "Content-Length: 40\r\n");
        strcat(sendBuff, "\r\n");
        strcat(sendBuff, "Percona XtraDB Cluster Node is synced.\r\n");
        write(connfd, sendBuff, strlen(sendBuff));
        close(connfd);
      }
    } else {
      // not synced
      strcpy(sendBuff, "HTTP/1.1 503 Service Unavailable\r\n");
      strcat(sendBuff, "Content-Type: text/plain\r\n");
      strcat(sendBuff, "Connection: close\r\n");
      strcat(sendBuff, "Content-Length: 44\r\n");
      strcat(sendBuff, "\r\n");
      strcat(sendBuff, "Percona XtraDB Cluster Node is not synced.\r\n");
      write(connfd, sendBuff, strlen(sendBuff));
      close(connfd);
    }

    close(connfd);
    sleep(1);
  }
}

struct st_mysql_daemon clustercheck_daemon_plugin =
{ MYSQL_DAEMON_INTERFACE_VERSION  };

//static FILE *fp;

int clustercheck_daemon_plugin_init(void *p)
{
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, listener, NULL);
  //pthread_join(thread_id, NULL);
  return 0;
}

int clustercheck_daemon_plugin_deinit(void *p)
{
  //close(connfd);
  //fprintf(fp, "Shutting down Cluster Check Daemon!!\n");
  //fclose(fp);
  return 0;
}

mysql_declare_plugin(clustercheck_daemon)
{
  MYSQL_DAEMON_PLUGIN,
  &clustercheck_daemon_plugin,
  "clustercheck",
  "Michael Patrick",
  "Returns status of the cluster for proxy",
  PLUGIN_LICENSE_GPL,
  clustercheck_daemon_plugin_init, /* Plugin Init */
  clustercheck_daemon_plugin_deinit, /* Plugin Deinit */
  0x0100,
  NULL,
  vars_enabled, /* clustercheck_system_variables, */
  NULL
}
mysql_declare_plugin_end;