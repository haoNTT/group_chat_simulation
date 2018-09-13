#include "blather.h"

//declare server_t instance
server_t my_server = {};

// create time thread
//pthread_t time_thread;

int signalled = 0;
void handle_signals(int sig_num){
  signalled = 1;
}

void handle_times(int sig_alarm){
  server_tick(&my_server);
  server_ping_clients(&my_server);
  server_remove_disconnected(&my_server, DISCONNECT_SECS);
}
/*
void *time_worker(void *arg){
  while(1){
    sleep(ALARM_INTERVAL);
    server_tick(&my_server);
    server_ping_clients(&my_server);
    server_remove_disconnected(&my_server, DISCONNECT_SECS);
  }
  return NULL;
}
*/

int main(int argc, char *argv[]){

  // setup signal handler SIGTERM and SIGINT
  struct sigaction my_sa = {};
  struct sigaction time_count = {};
  my_sa.sa_handler = handle_signals;
  time_count.sa_handler = handle_times;
  sigaction(SIGTERM, &my_sa, NULL);
  sigaction(SIGINT, &my_sa, NULL);
  sigaction(SIGALRM, &time_count, NULL);

  if(argc < 2){
    printf("usage: %s <server_name>\n",argv[0]);
    exit(1);
  }

  //start server argv[1]
  //mode_t perms = S_IRUSR | S_IWUSR;
  printf("server_start()\n");
  server_start(&my_server,argv[1],DEFAULT_PERMS);
  printf("server_start(): end\n");

  alarm(ALARM_INTERVAL);

  // keep track of time thread starts
  //pthread_create(&time_thread, NULL, time_worker, NULL);
  //pthread_join(time_thread, NULL);

  //REPEAT:
  while(!signalled){
    //check all sources, setup select()
    server_check_sources(&my_server);

    //handle a join request if one is ready
    if(server_join_ready(&my_server)){
      printf("server_process_join()\n");
      server_handle_join(&my_server);
    }

    //check for each client
    for(int i = 0; i < my_server.n_clients; i++){
      if(server_client_ready(&my_server,i)){
        server_handle_client(&my_server,i);
      }
    }
  }

  //pthread_cancel(time_thread);
  //gracefully shut down
  server_shutdown(&my_server);

  return 0;
}
