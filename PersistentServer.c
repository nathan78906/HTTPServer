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


// handles the If-Unmodified-Since header
int check_last_unmodified_parameter(const char *modified_date, time_t mtime, int client_fd, const char *rfc_format, const char *precondition_failed){
  if (modified_date != NULL){
    struct tm tm;
    // check 3 different date formats 
    if (strptime(modified_date, "%c", &tm) != NULL
    || strptime(modified_date, rfc_format, &tm) != NULL
    || strptime(modified_date, "%A, %d-%b-%y %T GMT", &tm) != NULL){
      //converts broken-down time into time since the Epoch
      //returns difference of seconds btw time1, time2
      if (!(difftime(mktime(&tm), mtime) >= 0)){
        write(client_fd, precondition_failed, strlen(precondition_failed));
        return -1;
      }
    }
  }
  return 0;
}

// handles the If-Match header
// check if the ETag of the resource matches any of the ETags provided
int check_if_match(char *etag_given, const char *etag_computed, int client_fd, const char *precondition_failed) {
  if (etag_given != NULL) {
    if (strcmp(etag_given, "*") == 0) {
      return 0;
    }
    char *token;

    token = strtok(etag_given, ", ");
    while(token != NULL) {
      // check for W/ for each ETag
      if(strncmp(token, "W/", 2) == 0) {
        token = token + 2;
      }
      if (strcmp(token, etag_computed) == 0) {
        return 0;
      }
      token = strtok(NULL, ", ");
    }
    //etags dont match
    write(client_fd, precondition_failed, strlen(precondition_failed));
    return -1;
  }
  return 0;
}

// handles the If-None-Match header
// check if none of the ETags given match the ETag of the resource
int check_if_none_match(char *etag_given, const char *etag_computed, int client_fd, const char *precondition_failed){
  if(etag_given != NULL){
    if (strcmp(etag_given, "*") == 0) {
      return 0;
    }
    char *token;

    //parse etag with commas
    token = strtok(etag_given, ", ");
    while(token != NULL){
      // check for W/ for each ETag
      if(strncmp(token, "W/", 2) == 0) {
        token = token + 2;
      }
      if(strcmp(token, etag_computed) == 0){
        //etag matched
        write(client_fd, precondition_failed, strlen(precondition_failed));
        return -1;
      }
      //if its not matched, go to the next etag
      token = strtok(NULL, ", ");
    }
    //no etags were found, successful
    return 0;
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

// concat strings together
char* concat(const char *s1, const char *s2){
    char *result = malloc(strlen(s1)+strlen(s2)+1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}


// process a client's request
int process_request(int client_fd, char *client_msg, char *root_path){
  //declare response messages
  char *file_not_found = " 404 File Not Found\r\n\r\n";
  char *success = " 200 OK\r\n";
  char *server_error = " 500 Internal Server Error\r\n\r\n";

  char *not_modified = "HTTP/1.0 304 Not Modified\r\n\r\n";
  char *bad_request_one = "HTTP/1.1 400 Bad Request\r\n\r\n";
  char *not_modified_one = "HTTP/1.1 304 Not Modified\r\n\r\n";
  char *precondition_failed = "HTTP/1.1 412 Precondition Failed\r\n\r\n";
  char *not_supported = "HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n";
  char *path, *http_type;

  //parse the http request
  //check for get request
  if (strcasecmp(strtok(client_msg, " \t"), "GET") != 0){
    write(client_fd, bad_request_one, strlen(bad_request_one));
    return 0;
  }

  //retrieve path
  if ((path = strtok(NULL, " \t")) == NULL){
    write(client_fd, bad_request_one, strlen(bad_request_one));
    return 0;
  }

  //determine if HTTP type is provided
  http_type = strtok(NULL, "\r\n");
  if (strcasecmp(http_type, "HTTP/1.0") != 0 && strcasecmp(http_type, "HTTP/1.1") != 0){
    write(client_fd, not_supported, strlen(not_supported));
    return 0;
  }

  char *key, *value;
  char *modified_date = NULL;
  char *unmodified_date = NULL;
  char *etag_given = NULL;
  char *etag_given_none = NULL;
  char *host = NULL;
  int connection;

  // manage connection header for HTTP versions
  if (strcasecmp(http_type, "HTTP/1.1") == 0){
    connection = 1;
  }else{
    connection = 0;
  }

  //look for the headers
  while(1){
    if ((key = strtok(NULL, " \t")) == NULL || (value = strtok(NULL, "\r\n")) == NULL){
      break;
    }

    if (strcasecmp(key, "\nHost:") == 0 || strcasecmp(key, "Host:") == 0){
      host = value;
    }else if (strcasecmp(key, "\nConnection:") == 0 || strcasecmp(key, "Connection:") == 0){
      if (strcasecmp(value, "keep-alive") == 0){
        connection = 1;
      }else if (strcasecmp(value, "close") == 0){
        connection = 0;
      }
    }else if (strcasecmp(key, "\nIf-Modified-Since:") == 0 || strcasecmp(key, "If-Modified-Since:") == 0){
      modified_date = value;
    }else if (strcasecmp(key, "\nIf-Unmodified-Since:") == 0 || strcasecmp(key, "If-Unmodified-Since:") == 0){
      unmodified_date = value;
    }else if (strcasecmp(key, "\nIf-Match:") == 0 || strcasecmp(key, "If-Match:") == 0){
      etag_given = value;
    }else if (strcasecmp(key, "\nIf-None-Match:") == 0 || strcasecmp(key, "If-None-Match:") == 0){
      etag_given_none = value;
    }
  }

  // check for HTTP1.1 and host header
  if (strcasecmp(http_type, "HTTP/1.1") == 0) {
    if (host == NULL) {
      write(client_fd, bad_request_one, strlen(bad_request_one));
      return connection;
    }
  }

  //allocate memory for full path
  int path_size = strlen(root_path) + strlen(path) + 1;
  char *full_path = (char *)malloc(path_size * sizeof(char));
  memset(full_path, '\0', path_size);


  // check if path is null
  if (full_path == NULL){
    char *response = concat(http_type, server_error);
    write(client_fd, response, strlen(response));
    return connection;
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
      char *response = concat(http_type, server_error);
      write(client_fd, response, strlen(response));
      return connection;
    }
 
    length = stat_struct.st_size;
    char *rfc_format =  "%a, %d %b %Y %T GMT";

    // compute etag
    char *etag;
    asprintf(&etag, "\"%ld-%ld-%lld\"", (long)stat_struct.st_ino, (long)stat_struct.st_mtime, (long long)stat_struct.st_size);

    if (strcasecmp(http_type, "HTTP/1.1") == 0) {
      //handle if-modified-since parameter, check if time is in a correct format
      if (check_last_modified_parameter(modified_date, stat_struct.st_mtime, client_fd, rfc_format, not_modified_one) == -1){
        return connection;
      }

      //handle if-unmodified-since parameter, check if time is in a correct format
      if (check_last_unmodified_parameter(unmodified_date, stat_struct.st_mtime, client_fd, rfc_format, precondition_failed) == -1){
        return connection;
      }

      //handle if-match header
      if (check_if_match(etag_given, etag, client_fd, precondition_failed) == -1) {
        return connection;
      }

      //handle if-none-match header
      if (check_if_none_match(etag_given_none, etag, client_fd, precondition_failed) == -1) {
        return connection;
      }
    } else if (strcasecmp(http_type, "HTTP/1.0") == 0) {
      //handle if-modified-since parameter, check if time is in a correct format
      if (check_last_modified_parameter(modified_date, stat_struct.st_mtime, client_fd, rfc_format, not_modified) == -1){
        return connection;
      }
    }

    //figure out mime type from extension, send back octet-stream if unsupported
    const char *mime_type = get_mime_type(full_path);

    if (mime_type == NULL){
      mime_type = "application/octet-stream";
    }

    //no errors, send success response
    char *response = concat(http_type, success);
    write(client_fd, response, strlen(response));

    //determine current date and last modified date to place in response header
    char rfc_time[80];
    char current_time[80];
    time_t time_sec = time(NULL);
    strftime(rfc_time, 80, rfc_format, localtime(&(stat_struct.st_mtime)));
    strftime(current_time, 80, rfc_format, localtime(&(time_sec)));
    char *connect_string;
    if (connection == 1){
      connect_string = "keep-alive";
    }else{
      connect_string = "close";
    }

    // send back headers to the client
    char *header;
    if (strcasecmp(http_type, "HTTP/1.1") == 0) {
        asprintf(&header, "Date: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\nConnection: %s\r\nLast-Modified: %s\r\nETag: %s\r\n\r\n",
        current_time, (int)length, mime_type, connect_string, rfc_time, etag);
    }
    else if (strcasecmp(http_type, "HTTP/1.0") == 0) {
        asprintf(&header, "Date: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\nConnection: %s\r\nLast-Modified: %s\r\n\r\n",
        current_time, (int)length, mime_type, connect_string, rfc_time);
    }
    write(client_fd, header, strlen(header));
    free(header);
    free(etag);


    //sendfile to client
    if (sendfile(client_fd, file_fd, NULL, length) == -1){
        char *response = concat(http_type, server_error);
        write(client_fd, response, strlen(response));
        return connection;
    }
  }else{
    //File not found
    char *response = concat(http_type, file_not_found);
    write(client_fd, response, strlen(response));
  }
  return connection;
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

  //struct for setting read timeout
  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;

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

      //set timeout on client_fd read
      setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
      int connection = 1;
      while (connection == 1 && read(client_fd, client_msg, MESSAGE_LENGTH) > 0){
        fprintf(stdout, "Recieved Client Message:\n%s\n", client_msg);
        connection = process_request(client_fd, client_msg, argv[2]);
      }
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
