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
 {0,0} };

// gets the extension for the filename
const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

// gets the mimetype based off the extension
const char *get_mime_type(const char *full_path){
  const char *file_ext = get_filename_ext(full_path);
  int i;
  for (i = 0; extensions[i].ext != NULL; i++) {
    if (strcasecmp(file_ext, extensions[i].ext) == 0){
      return extensions[i].mediatype;
    }
  }
  return NULL;
}

// handles the If-Modified-Since header
int check_last_modified_parameter(const char *modified_date, time_t mtime, int client_fd, const char *rfc_format, const char *not_modified){
  if (modified_date != NULL){
    struct tm tm;
    // check 3 different date formats 
    if (strptime(modified_date, "%c", &tm) != NULL
    || strptime(modified_date, rfc_format, &tm) != NULL
    || strptime(modified_date, "%A, %d-%b-%y %T GMT", &tm) != NULL){
      //converts broken-down time into time since the Epoch
      //returns difference of seconds btw time1, time2
      if (difftime(mktime(&tm), mtime) >= 0){
        //not modified
        write(client_fd, not_modified, strlen(not_modified));
        return -1;
      }
    }
  }
  return 0;
}

// allows a clean exit in case of error
void clean_exit(int rc, int fd, char *message){
  if (rc == -1 || fd == -1){
    if (fd != -1){
      close(fd);
    }
    perror(message);
    exit(EXIT_FAILURE);
  }
}

// handle error and exit if we get one
void handle_error(int status, char *message){
  if (status < 0){
    perror(message);
    exit(EXIT_FAILURE);
  }
}

void process_request(int client_fd, char *client_msg, char *root_path){
  //declare response messages
  char *bad_request = "HTTP/1.0 400 Bad Request\r\n\r\n";
  char *file_not_found = "HTTP/1.0 404 File Not Found\r\n\r\n";
  char *success_response = "HTTP/1.0 200 OK\r\n";
  char *server_error = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
  char *not_modified = "HTTP/1.0 304 Not Modified\r\n\r\n";
  char *not_supported = "HTTP/1.0 505 HTTP Version Not Supported\r\n\r\n";
  char *path, *http_type;

  //parse the http request
  //check for get request
  if (strcasecmp(strtok(client_msg, " \t"), "GET") != 0){
    write(client_fd, bad_request, strlen(bad_request));
    return;
  }

  //retrieve path
  if ((path = strtok(NULL, " \t")) == NULL){
    write(client_fd, bad_request, strlen(bad_request));
    return;
  }

  //determine if HTTP type is provided
  http_type = strtok(NULL, "\r\n");
  if ((strcasecmp(http_type, "HTTP/1.0")) != 0){
    write(client_fd, not_supported, strlen(not_supported));
    return;
  }

  char *key, *value;
  char *modified_date = NULL;

  //look for the last modified date;
  while(1){
    if ((key = strtok(NULL, " \t")) == NULL || (value = strtok(NULL, "\r\n")) == NULL){
      break;
    }

    if (strcasecmp(key, "\nIf-Modified-Since:") == 0 || strcasecmp(key, "If-Modified-Since:") == 0){
      modified_date = value;
      //fprintf(stdout, "date %s\n", modified_date);
      break;
    }
  }

  //allocate memory for full path
  int path_size = strlen(root_path) + strlen(path) + 1;
  char *full_path = (char *)malloc(path_size * sizeof(char));
  memset(full_path, '\0', path_size);

  // check if path is null
  if (full_path == NULL){
    write(client_fd, server_error, strlen(server_error));
    return;
  }

  strcat(full_path, root_path);

  //look at index.html if only root is asked
  if (strcmp(path, "/") == 0){
    strcat(full_path, "/index.html");
  }else{
    strcat(full_path, path);
  }

  int file_fd, length;
  struct stat stat_struct;

  //open the file if it exists
  if ( (file_fd=open(full_path, O_RDONLY))!=-1 )
  {
    //find file metadata
    if(fstat(file_fd, &stat_struct) == -1 ){
      write(client_fd, server_error, strlen(server_error));
      return;
    }

    length = stat_struct.st_size;
    char *rfc_format =  "%a, %d %b %Y %T GMT";

    //handle if-modified-since parameter, check if time is in a correct format
    if (check_last_modified_parameter(modified_date, stat_struct.st_mtime, client_fd, rfc_format, not_modified) == -1){
      return;
    }

    //figure out mime type from extension, send back octet-stream if unsupported
    const char *mime_type = get_mime_type(full_path);

    if (mime_type == NULL){
      mime_type = "application/octet-stream";
    }

    //no errors, send success response
    write(client_fd, success_response, strlen(success_response));

    //determine current date and last modified date to place in response header
    char rfc_time[80];
    char current_time[80];
    time_t time_sec = time(NULL);
    strftime(rfc_time, 80, rfc_format, localtime(&(stat_struct.st_mtime)));
    strftime(current_time, 80, rfc_format, localtime(&(time_sec)));

    // send back headers to the client
    char *header;
    asprintf(&header, "Date: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\nLast-Modified: %s\r\n\r\n",
     current_time, (int)length, mime_type, rfc_time);
    write(client_fd, header, strlen(header));
    free(header);

    //sendfile to client
    if (sendfile(client_fd, file_fd, NULL, length) == -1){
        write(client_fd, server_error, strlen(server_error));
        return;
    }
	}else{
    //File not found
     write(client_fd, file_not_found, strlen(file_not_found));
  }
}

//argv will contain 2 params : PORT #, http_root_path
int main(int argc, char * argv[]){
  int server_fd, client_fd, rc, client_addr_len, opt;
  struct sockaddr_in server_addr, client_addr;
  char client_msg[MESSAGE_LENGTH];
  memset(client_msg, '\0', MESSAGE_LENGTH);

  //determine port number to listen on
  if (argc != 3){
    fprintf(stderr, "[Usage]: %s PORT HTTP_ROOT_PATH\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  //create server address struct
  memset(&server_addr, 0, sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  //convert port number(string) to integer
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

  fprintf(stdout, "Server at %s:%d Listening for connections\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

  client_addr_len = sizeof(client_addr);
  //Accept connections
  while(1){
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    clean_exit(client_fd, server_fd, "[Server accept error]: ");

    //fork and let child handle processing request for the accepted client
    int pid = fork();
    handle_error(pid, "Fork error");

    if (pid == 0){
      //close server_fd in child process, it isn't required
      rc = close(server_fd);
      //process requests
      fprintf(stdout, "Received connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
      read(client_fd, client_msg, MESSAGE_LENGTH);
      fprintf(stdout, "Recieved Client Message:\n%s\n", client_msg);
      process_request(client_fd, client_msg, argv[2]);
      handle_error(close(client_fd), "close error");
      //exit from child process
      exit(EXIT_SUCCESS);
    }else{
      //close client fd in parent process and loop again
      handle_error(close(client_fd), "close error");
    }
  }
  //shouldn't ever exit out of loop
  return(EXIT_FAILURE);
}
