#include "blather.h"

// Gets a pointer to the client_t struct at the given index.
client_t *server_get_client(server_t *server, int idx){
  return &(server->client[idx]);
}

void server_start(server_t *server, char *server_name, int perms){
  // Initializes and starts the server with the given name.
  strcpy(server->server_name,server_name);

  char fifo_name[MAXPATH] = {};
  strcpy(fifo_name,server_name);
  strcat(fifo_name,".fifo");

  //Removes any existing file of that name prior to creation.
  remove(fifo_name);

  // A join fifo called "server_name.fifo" should be created.
  // Opens the FIFO and stores its file descriptor in join_fd.
  mkfifo(fifo_name, perms); //read and write permission for the owner
  server->join_fd = open(fifo_name, O_RDWR);

  // ADVANCED INITIALIZES segemantation fault
  // ADVANCED: create the log file "server_name.log" and write the
  // initial empty who_t contents to its beginning. Ensure that the
  // log_fd is position for appending to the end of the file.

  char log_fname[MAXPATH] = {};
  who_t empty = {};

  strcpy(log_fname,server_name);
  strcat(log_fname,".log");

  remove(log_fname);

  server->log_fd = fileno(fopen(log_fname, "w+"));

  write(server->log_fd, &empty, sizeof(who_t));
  lseek(server->log_fd,0,SEEK_END);

  // Create the POSIX semaphore "/server_name.sem" and initialize it to 1 to
  // control access to the who_t portion of the log.
  char sem_name[MAXPATH] = {};

  strcpy(sem_name,"/");
  strcat(sem_name,server_name);
  strcat(sem_name,".sem");

  server->log_sem = sem_open(sem_name, O_CREAT);
  sem_init(server->log_sem, 0, 1);

  server->time_sec = 0;

}

void server_shutdown(server_t *server){
  char fifo_name[MAXPATH] = {};
  strcpy(fifo_name,server->server_name);
  strcat(fifo_name,".fifo");
  // Shut down the server.
  // Close the join FIFO and unlink (remove) it so that no further clients can join.
  close(server->join_fd);
  remove(fifo_name);

  mesg_t server_down_msg = {};
  server_down_msg.kind = BL_SHUTDOWN;
  server_broadcast(server,&server_down_msg);

  //clients and proceed to remove all clients in any order.
  client_t *temp_client;
  for(int j = 0; j < server->n_clients; j++){
    temp_client = server_get_client(server,j);
    close(temp_client->to_client_fd);
    close(temp_client->to_server_fd);
    remove(temp_client->to_client_fname);
    remove(temp_client->to_client_fname);
  }
  // ADVANCED: Close the log file. Close the log semaphore and unlink
  // it.
  char sem_name[MAXPATH] = {};

  strcpy(sem_name,"/");
  strcat(sem_name,server->server_name);
  strcat(sem_name,".sem");

  close(server->log_fd);
  sem_close(server->log_sem);
  sem_unlink(sem_name);
}

int server_add_client(server_t *server, join_t *join){
  if (server->n_clients == MAXCLIENTS){
    // return non-zero if the server as no space for clients (n_clients == MAXCLIENTS)
    return 1;
  }
  else{
    // Adds a client to the server according to the parameter
    // join which should have fileds such as name filed in.
    int client_idx = server->n_clients;
    char to_c_fname[MAXPATH] = {};
    char to_s_fname[MAXPATH] = {};
    strcpy(to_c_fname,join->to_client_fname);
    strcpy(to_s_fname,join->to_server_fname);

    client_t *temp_client;
    temp_client = server_get_client(server,client_idx);

    // setup client name
    strcpy(temp_client->name,join->name);
    // setup fifo names
    strcpy(temp_client->to_client_fname,to_c_fname);
    strcpy(temp_client->to_server_fname,to_s_fname);
    // open fifos and set file descriptors
    temp_client->to_client_fd = open(to_c_fname, O_RDWR);
    temp_client->to_server_fd = open(to_s_fname, O_RDWR);
    // Initializes the data_ready field for the client to 0.
    temp_client->data_ready = 0;
    // update the number of clients that are connected to server
    server->n_clients += 1;

    printf("server_add_client(): %s %s %s\n",
      join->name,join->to_client_fname,join->to_server_fname);
    //Returns 0 on success
    return 0;
  }
}

int server_remove_client(server_t *server, int idx){
  //Close fifos associated with the client and remove them
  client_t *temp_client;
  temp_client = server_get_client(server,idx);

  close(temp_client->to_client_fd);
  close(temp_client->to_server_fd);
  remove(temp_client->to_client_fname);
  remove(temp_client->to_server_fname);

  printf("server_remove_client(): %d\n", idx);

  //copy client[i+1] to client[i]
  for(int i = idx; i < server->n_clients-1; i++){
    server->client[i] = server->client[i+1];
  }
  //decrease n_clients
  server->n_clients--;
  return 0;
}

int server_broadcast(server_t *server, mesg_t *mesg){
  client_t *temp_client;

  for(int i = 0; i < server->n_clients; i++){
    temp_client = server_get_client(server,i);
    write(temp_client->to_client_fd,mesg,sizeof(mesg_t));
  }

  // ADVANCED: Log the broadcast message unless it is a PING which
  // should not be written to the log.
  if(mesg->kind != BL_PING){
    server_log_message(server, mesg);
  }
  else{
    printf("server_broadcast(): %d from %s - %s\n",mesg->kind,mesg->name,mesg->body);
  }
  return 0;
}

void server_check_sources(server_t *server){
  int maxfd = -1;
  int signal_flag;
  fd_set read_set;
  FD_ZERO(&read_set);

  //Sets the servers join_ready flag
  FD_SET(server->join_fd,&read_set);
  server->join_ready = 0;
  if(server->join_fd > maxfd){
    maxfd = server->join_fd;
  }

  client_t *temp_client;

  for(int i = 0 ; i < server->n_clients; i++){
    temp_client = server_get_client(server,i);

    FD_SET(temp_client->to_server_fd,&read_set);
    temp_client->data_ready = 0;
    if(temp_client->to_server_fd > maxfd){
      maxfd = temp_client->to_server_fd;
    }
  }

  signal_flag = select(maxfd+1,&read_set,NULL,NULL,NULL);

  // if server_check_sources is signaled by ctrl+c
  if(signal_flag == -1){
    return;
  }

  if(FD_ISSET(server->join_fd,&read_set)){
    server->join_ready = 1;
  }
  for(int j = 0; j < server->n_clients; j++){
    temp_client = server_get_client(server,j);
    if(FD_ISSET(temp_client->to_server_fd,&read_set)){
      temp_client->data_ready = 1;
    }
  }
}

int server_join_ready(server_t *server){
  // Return the join_ready flag from the server which indicates whether
  // a call to server_handle_join() is safe.
  return server->join_ready;
}

int server_handle_join(server_t *server){
  join_t new_request = {};
  //Read a join request
  read(server->join_fd,&new_request,sizeof(join_t));
  //add the new client to the server
  server_add_client(server,&new_request);
  // set the servers join_ready flag to 0
  server->join_ready = 0;

  //boradcast to all clients including newly joined client
  mesg_t new_member = {};
  new_member.kind = BL_JOINED;
  strcpy(new_member.name, new_request.name);

  server_broadcast(server,&new_member);
  server_write_who(server);
  return 0;

}

int server_client_ready(server_t *server, int idx){
  // Return the data_ready field of the given client which indicates
  // whether the client has data ready to be read from it.
  client_t *temp_client;
  temp_client = server_get_client(server,idx);
  return temp_client->data_ready;
}

int server_handle_client(server_t *server, int idx){
  mesg_t new_msg = {};
  client_t *temp_client;
  temp_client = server_get_client(server,idx);
  // read new message
  read(temp_client->to_server_fd,&new_msg,sizeof(mesg_t));
  //analyze the message kind,Departure and Message types
  //should be broadcast to all other clients.
  if(new_msg.kind == BL_MESG){
    printf("server: mesg received from client %d %s : %s\n", idx,new_msg.name,new_msg.body);
    server_broadcast(server,&new_msg);
  }
  else if(new_msg.kind == BL_DEPARTED){
    printf("server: departed client %d %s\n", idx,new_msg.name);
    server_remove_client(server,idx);
    server_broadcast(server,&new_msg);
    server_write_who(server);
  }
  else if(new_msg.kind == BL_PING){
    temp_client->last_contact_time = server->time_sec;
    // ADVANCED: Ping responses should only change the last_contact_time below. Behavior
    // for other message types is not specified.
    // ADVANCED: Update the last_contact_time of the client to the current
    // server time_sec.
  }
  temp_client->data_ready = 0;
  return 0;
}

void server_tick(server_t *server){
  // ADVANCED: Increment the time for the server
  server->time_sec+=ALARM_INTERVAL;
}

void server_ping_clients(server_t *server){
  // ADVANCED: Ping all clients in the server by broadcasting a ping.
  mesg_t ping = {};
  ping.kind = BL_PING;

  server_broadcast(server,&ping);
}

// ADVANCED: Check all clients to see if they have contacted the
// server recently. Any client with a last_contact_time field equal to
// or greater than the parameter disconnect_secs should be
// removed. Broadcast that the client was disconnected to remaining
// clients.  Process clients from lowest to highest and take care of
// loop indexing as clients may be removed during the loop
// necessitating index adjustments.
void server_remove_disconnected(server_t *server, int disconnect_secs){
  client_t *temp_client;

  for(int i = 0; i < server->n_clients; i++){
    temp_client = server_get_client(server,i);
    if((server->time_sec - temp_client->last_contact_time) >= disconnect_secs){
      mesg_t disconnected = {};
      disconnected.kind = BL_DISCONNECTED;
      strcpy(disconnected.name, temp_client->name);
      server_remove_client(server,i);
      server_broadcast(server, &disconnected);
    }
  }
}

void server_write_who(server_t *server){
  client_t *temp_client;
  who_t update = {};
  update.n_clients = server->n_clients;

  for(int i = 0; i < server->n_clients; i++){
    temp_client = server_get_client(server,i);
    strcpy(update.names[i],temp_client->name);
  }

  // Write the current set of clients logged into
  // the server to the BEGINNING the log_fd.
  // Ensure that the write is protected by locking the semaphore
  sem_wait(server->log_sem);
  pwrite(server->log_fd,&update,sizeof(who_t),0);
  sem_post(server->log_sem);
}

// ADVANCED: Write the given message to the end of log file associated
// with the server.
void server_log_message(server_t *server, mesg_t *mesg){
  write(server->log_fd,mesg,sizeof(mesg_t));
}
