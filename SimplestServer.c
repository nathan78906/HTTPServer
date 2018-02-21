#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>


void clean_exit(int rc, int fd, char *message){
  if (rc == -1 || fd == -1){
    if (fd != -1){
      close(fd);
    }
    perror(message);
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char * argv[]){
  int server_fd, client_fd, rc, client_addr_len, opt;
  struct sockaddr_in server_addr, client_addr;
  char client_msg[1024], *server_msg = "Message";

  //determine port number to listen on
  if (argc != 2){
    fprintf(stderr, "[Usage]: %s PORT\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  //create server address struct
  memset(&server_addr, 0, sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[1]));

  //bind to available ip on machine
  server_addr.sin_addr.s_addr = INADDR_ANY;

  //open socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  clean_exit(server_fd, server_fd, "[Server socket error]: ");

  //allow reusable port after disconnect or termination of server
  rc = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, (socklen_t)sizeof(int));
  clean_exit(rc, server_fd, "[Server setsockopt error]: ");

  //bind socket to address and PORT
  rc = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
  clean_exit(rc, server_fd, "[Server bind error]");

  //listen on the socket for up to 5 connections and check perror
  rc = listen(server_fd, 5);
  clean_exit(rc, server_fd, "[Server listen error]: ");

  //Accept connections forever
  fprintf(stdout, "Server at %s:%d Listening for connections\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
  while(1){
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    clean_exit(client_fd, server_fd, "[Server accept error]: ");

    //process requests
    fprintf(stdout, "Received connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    read(client_fd, client_msg, 1024);
    fprintf(stdout, "%s said: %s", inet_ntoa(client_addr.sin_addr), client_msg);
    write(client_fd, server_msg, strlen(server_msg));
    //close client
    close(client_fd);
  }



}
