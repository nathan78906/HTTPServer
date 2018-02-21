#define __USE_XOPEN
#define _GNU_SOURCE
#include<stdio.h>
#include<time.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<sys/sendfile.h>
#include<sys/types.h>
#include<sys/stat.h>
#define MESSAGE_LENGTH 1024

//TODO check if / not at beginning of path
//TODO check if we need to consider other optional headers besides if modified since
//TODO send proper responses
//TODO clean up
//TODO ask if last modified time will be in asctime standard
//TODO what if incorrect key value pairs or incorrect date? ignores right
//TODO handle other cases for requests
//TODO make sure responses are in the correct format

typedef struct {
 char *ext;
 char *mediatype;
} extn;

//Possible media types
const extn extensions[] ={
 {"gif", "image/gif" },
 {"txt", "text/plain" },
 {"css", "text/css"},
 {"js", "text/javascript"},
 {"htm", "text/html" },
 {"html","text/html" },
 {"php", "text/html" },
 {"jpg", "image/jpeg" },
 {"jpeg","image/jpeg"},
 {"png", "image/png" },
 {"ico", "image/ico" },
 {"zip", "image/zip" },
 {"gz",  "image/gz"  },
 {"tar", "image/tar" },
 {"pdf","application/pdf"},
 {"zip","application/octet-stream"},
 {"rar","application/octet-stream"},
 {0,0} };

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void clean_exit(int rc, int fd, char *message){
  if (rc == -1 || fd == -1){
    if (fd != -1){
      close(fd);
    }
    perror(message);
    exit(EXIT_FAILURE);
  }
}

void process_request(int client_fd, char *client_msg, char *root_path){
  char *not_implemented = "HTTP/1.0 501 Not Implemented\r\n";
  char *bad_request = "HTTP/1.0 400 Bad Request\r\n";
  char *file_not_found = "HTTP/1.0 404 File Not Found\r\n";
  char *success_response = "HTTP/1.0 200 OK\r\n";
  char *server_error = "HTTP/1.0 500 Internal Server Error\r\n";
  char *not_modified = "HTTP/1.0 304 Not Modified\r\n";
  int failure_len = strlen(bad_request);
  char *path, *http_type;

  if (strcasecmp(strtok(client_msg, " \t"), "GET") != 0){
    fprintf(stdout, "Missing GET\n");
    write(client_fd, bad_request, failure_len);
    return;
  }

  if ((path = strtok(NULL, " \t")) == NULL){
    fprintf(stdout, "Missing path\n");
    write(client_fd, bad_request, failure_len);
    return;
  }

  if ((http_type = strtok(NULL, "\r\n")) == NULL || strcasecmp(http_type, "HTTP/1.0") != 0){
    fprintf(stdout, "Missing http type\n");
    write(client_fd, bad_request, failure_len);
    return;
  }

  char *key, *value;
  char *modified_date = NULL;

  //look for the last modified date;
  while(1){
    if ((key = strtok(NULL, " \t")) == NULL || (value = strtok(NULL, "\r\n")) == NULL){
      break;
    }

    fprintf(stdout, "key %s\n", key);
    fprintf(stdout, "value %s\n",value);

    if (strcasecmp(key, "If-Modified-Since:") == 0){
      modified_date = value;
      fprintf(stdout, "date %s\n", modified_date);
      break;
    }
  }

  int path_size = strlen(root_path) + strlen(path) + 1;

  char *full_path = (char *)malloc(path_size * sizeof(char));
  memset(full_path, '\0', path_size);


  if (full_path == NULL){
    printf("Error allocating memory!!\n");
    write(client_fd, server_error, strlen(server_error));
    return;
  }

  strcat(full_path, root_path);
  if (strcmp(path, "/") == 0){
    strcat(full_path, "/index.html");
  }else{
    strcat(full_path, path);
  }

  fprintf(stdout, "full path %s\n", full_path);

  int file_fd, length;
  char *file_buffer;
  struct stat stat_struct;


  if ( (file_fd=open(full_path, O_RDONLY))!=-1 )
  {
    if(fstat(file_fd, &stat_struct) == -1 ){
      printf("Error retrieving file metadata \n");
      write(client_fd, server_error, strlen(server_error));
      return;
    }

    length = stat_struct.st_size;
    fprintf(stdout, "length %d\n", length);
    char *rfc_format =  "%a, %d %b %Y %T GMT";
    //handle if-modified-since parameter, check if time is in a correct format
    if (modified_date != NULL){
      struct tm tm;
      if (strptime(modified_date, "%c", &tm) != NULL
      || strptime(modified_date, rfc_format, &tm) != NULL
      || strptime(modified_date, "%A, %d-%b-%y %T GMT", &tm) != NULL){
        time_t req_time = mktime(&tm);
        if (difftime(mktime(&tm), stat_struct.st_mtime) >= 0){
          printf("Not modified!!\n");
          write(client_fd, not_modified, strlen(not_modified));
          return;
        }
      }
    }

    //figure out mime type from extension
    const char *file_ext = get_filename_ext(full_path);
    printf("extension %s\n", file_ext);
    char *mime_type = NULL;
    int i;
    for (i = 0; extensions[i].ext != NULL; i++) {
      if (strcasecmp(file_ext, extensions[i].ext) == 0){
        mime_type = extensions[i].mediatype;
        break;
      }
    }

    if (mime_type == NULL){
      printf("file not supported");
      write(client_fd, not_implemented, strlen(not_implemented));
      return;
    }

    write(client_fd, success_response, strlen(success_response));

    char rfc_time[80];
    char current_time[80];
    time_t time_sec = time(NULL);
    strftime(rfc_time, 80, rfc_format, localtime(&(stat_struct.st_mtime)));
    strftime(current_time, 80, rfc_format, localtime(&(time_sec)));

    char *header;
    asprintf(&header, "Date: %s\nContent-Length: %d\nContent-Type: %s\nLast-Modified: %s\n\r\n",
     current_time, (int)length, mime_type, rfc_time);
    write(client_fd, header, strlen(header));
    free(header);

    if (sendfile(client_fd, file_fd, NULL, length) == -1){
        printf("Send err!!\n");
        write(client_fd, server_error, strlen(server_error));
        return;
    }
	}else{
    //File not found
     write(client_fd, file_not_found, strlen(file_not_found));
  }


}

int main(int argc, char * argv[]){
  int server_fd, client_fd, rc, client_addr_len, opt;
  struct sockaddr_in server_addr, client_addr;
  char client_msg[MESSAGE_LENGTH], *server_msg = "Message";

  //determine port number to listen on
  if (argc != 3){
    fprintf(stderr, "[Usage]: %s PORT HTTP_ROOT_PATH\n", argv[0]);
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

 //  while(1){
 //   client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
 //   clean_exit(client_fd, server_fd, "[Server accept error]: ");
 //
 //   //process requests
 //   fprintf(stdout, "Received connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
 //   read(client_fd, client_msg, 1024);
 //   fprintf(stdout, "%s said: %s", inet_ntoa(client_addr.sin_addr), client_msg);
 //   write(client_fd, server_msg, strlen(server_msg));
 //   //close client
 //   close(client_fd);
 // }


  while(1){
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    printf("client fd %d\n", client_fd);
    clean_exit(client_fd, server_fd, "[Server accept error]: ");

    int pid = fork();
    if (fork < 0){
      perror("Error on fork");
      exit(EXIT_FAILURE);
    }

    if (pid == 0){
      //close server_fd in child process
      rc = close(server_fd);
      //process requests
      fprintf(stdout, "Received connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
      read(client_fd, client_msg, MESSAGE_LENGTH);
      fprintf(stdout, "Recieved Client Message %s\n", client_msg);
      process_request(client_fd, client_msg, argv[2]);
      close(client_fd);
      //exit from child process
      exit(EXIT_SUCCESS);
    }else{
      //close client fd in parent process and loop again
      close(client_fd);
    }
  }
  //shouldn't ever exit out of loop
  return(EXIT_FAILURE);

}
