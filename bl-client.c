#include "blather.h"

simpio_t simpio_actual;
simpio_t *simpio = &simpio_actual;

client_t client_actual;
client_t *client = &client_actual;

pthread_t user_thread;          // thread managing user input
pthread_t background_thread;

char user_name[MAXNAME] = {};
char to_c_fname[MAXNAME] = {};
char to_s_fname[MAXNAME] = {};
int to_c_fd;
int to_s_fd;

// Worker thread to manage user input
void *user_worker(void *arg){
  char last_message[MAXLINE] = {};
  while(!simpio->end_of_input){
    simpio_reset(simpio);
    iprintf(simpio, "");                                          // print prompt
    while(!simpio->line_ready && !simpio->end_of_input){          // read until line is complete
      simpio_get_char(simpio);
    }
    if(simpio->line_ready){
      // Create new BL_MESG message
      mesg_t new_msg = {};
      new_msg.kind = BL_MESG;
      strcpy(new_msg.body, simpio->buf);

//      if(strncmp(simpio->buf,"%last", 5)){

//      }

      //update last_message
      strcpy(last_message,new_msg.body);
      strcpy(new_msg.name, user_name);

      // Send mesg_t to server, write to to_s_fd
      write(to_s_fd,&new_msg,sizeof(mesg_t));
    }
  }
  // send Departure message to server
  mesg_t depart = {};
  depart.kind = BL_DEPARTED;
  strcpy(depart.name,user_name);
  strcpy(depart.body, last_message);

  // Send mesg_t to server
  write(to_s_fd,&depart,sizeof(mesg_t));

  pthread_cancel(background_thread); // kill the background thread
  return NULL;
}

// Worker thread to listen to the info from the server.
void *background_worker(void *arg){
  while(1){
    // create a new mesg_t instance to be filled
    mesg_t receive = {};
    // Read mesg_t from server, read to_c_fd
    read(to_c_fd,&receive,sizeof(mesg_t));

    // analyze mesg_t kind
    if(receive.kind == BL_MESG){
      iprintf(simpio, "[%s] : %s\n",receive.name,receive.body);
    }
    else if(receive.kind == BL_JOINED){
      iprintf(simpio, "-- %s JOINED --\n", receive.name);
    }
    else if(receive.kind == BL_DEPARTED){
      iprintf(simpio, "-- %s DEPARTED --\n", receive.name);
    }
    else if(receive.kind == BL_SHUTDOWN){
      iprintf(simpio, "!!! server is shutting down !!!\n");
      break;
    }
    else if(receive.kind == BL_DISCONNECTED){
      iprintf(simpio, "-- %s DISCONNECTED --\n", receive.name);
    }
    else if(receive.kind == BL_PING){
      mesg_t ping = {};
      ping.kind = BL_PING;
      // Send PING to server
      write(to_s_fd,&ping,sizeof(mesg_t));
    }
  }

  pthread_cancel(user_thread); // kill the user thread
  return NULL;
}

int main(int argc, char *argv[]){

  if(argc < 3){
    printf("usage: %s <server_name> <user_name>\n",argv[0]);
    exit(1);
  }

  char prompt[MAXNAME] = {};;
  char server_join_fname[MAXNAME] = {};;
  char pid[MAXNAME] = {};;

  //set up fifo names for join_t
  int pid_num = getpid();
  snprintf(pid, MAXNAME, "%d",pid_num);

  strcpy(server_join_fname, argv[1]);
  strcpy(user_name, argv[2]);

  strcpy(to_c_fname,pid);
  strcpy(to_s_fname,pid);

  strcat(server_join_fname,".fifo");

  strcat(to_c_fname,".");
  strcat(to_s_fname,".");
  strcat(to_c_fname,"client.fifo");
  strcat(to_s_fname,"server.fifo");

  //make fifos
  mkfifo(to_s_fname, DEFAULT_PERMS);
  mkfifo(to_c_fname, DEFAULT_PERMS);

  //open fifos
  to_s_fd = open(to_s_fname, O_RDWR);
  to_c_fd = open(to_c_fname, O_RDWR);

  //build join_t
  join_t request = {};
  strcpy(request.name,user_name);
  strcpy(request.to_client_fname,to_c_fname);
  strcpy(request.to_server_fname,to_s_fname);

  //get server's join_fd and request joining
  int join_fd = open(server_join_fname, O_WRONLY);
  write(join_fd,&request,sizeof(join_t));

  snprintf(prompt, MAXNAME, "%s>> ", user_name); // create a prompt string
  simpio_set_prompt(simpio, prompt);         // set the prompt
  simpio_reset(simpio);                      // initialize io
  simpio_noncanonical_terminal_mode();       // set the terminal into a compatible mode

  pthread_create(&user_thread,   NULL, user_worker,   NULL);     // start user thread to read input
  pthread_create(&background_thread, NULL, background_worker, NULL);
  pthread_join(user_thread, NULL);
  pthread_join(background_thread, NULL);

  simpio_reset_terminal_mode();
  printf("\n");                 // newline just to make returning to the terminal prettier
  return 0;
}
