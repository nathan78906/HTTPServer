# HTTP/1.0 & HTTP/1.1 Server

## Features

- GET Requests
- Designed to mimic the Apache HTTP Server
- Persistent connections (Connection: Keep-Alive, Close)
- HTTP Pipelining
- Conditional headers (If-Modified-Since, If-Unmodified-Since, If-Match, If-None-Match)
- Entity tags for HTTP/1.1 requests
- Supports file extensions: `gif, txt, css, js, htm, html, php, jpg, jpeg, png, ico, zip, gz, tar, pdf`

### Compile c code

gcc -Wall server_file_name.c -o serv

### Execute

Takes PORT and ROOT_DIR params

./serv 10001 ~/user

### Testing

`curl --http1.0 --header 'If-Modified-Since: Sunday, 18-Feb-18 21:49:37 GMT' 0.0.0.0:10001/hello.txt`

### If-Match Testing

First use 

`curl --http1.1 0.0.0.0:10001/hello.txt`

to get ETag for `hello.txt` assume ETag is `46288911-1519927953-10`

Then 

`curl -v --http1.1 --header 'If-Match: "46288911-1519927953-10"' 0.0.0.0:10001/hello.txt`

`curl -v --http1.1 --header 'If-Match: W/"46288911-1519927953-10"' 0.0.0.0:10001/hello.txt`

`curl -v --http1.1 --header 'If-Match: W/"46288911-1519927953-10", W/"afas"' 0.0.0.0:10001/hello.txt`

`curl -v --http1.1 --header 'If-Match: "ayy", "46288911-1519927953-10"' 0.0.0.0:10001/hello.txt`


### If-None-Match Testing

First use 

`curl --http1.1 0.0.0.0:10001/hello.txt`

to get ETag for `hello.txt` assume ETag is `46783227-1519946231-21`

Then 

`curl -v --http1.1 --header 'If-None-Match: "46783227-1519946231-21"' 0.0.0.0:10001/hello.txt`
=> Should return preconditional error 

`curl -v --http1.1 --header 'If-None-Match: W/"46783227-1519946231-21"' 0.0.0.0:10001/hello.txt`
=> Should return preconditional error 

`curl -v --http1.1 --header 'If-None-Match: "12345"' 0.0.0.0:10001/hello.txt`
=> Should return a response

`curl -v --http1.1 --header 'If-None-Match: W/"12345"' 0.0.0.0:10001/hello.txt`
=> Should return a response

`curl -v --http1.1 --header 'If-None-Match: "12345", "random"' 0.0.0.0:10001/hello.txt`
=> Should return a response 

`curl -v --http1.1 --header 'If-None-Match: "46783227-1519946231-21", "random"' 0.0.0.0:10001/hello.txt`
=> Should return preconditional error 

### Pipeline Testing

(echo -en "GET /b.txt HTTP/1.1\r\nHost: localhost:10001\r\n\r\nGET /a.txt HTTP/1.1\r\nHost: localhost:10001\r\n\r\n"; sleep 0.1) | nc localhost 10001

## Resources
All server implementations make use of the following information:

http://www.rfc-editor.org/rfc/rfc1945.txt

https://www.ietf.org/rfc/rfc2068.txt

https://www.ietf.org/rfc/rfc2616.txt

https://developer.mozilla.org/en-US/docs/Web/HTTP/Conditional_requests

https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers

http://www.utsc.utoronto.ca/~bharrington/cscd58/slides/week3-slides.pdf

http://www.utsc.utoronto.ca/~bharrington/cscd58/tutorials/week4_tutorial.pdf

https://stackoverflow.com/questions/19619124/http-pipelining-request-text-example

https://brianbondy.com/blog/119/what-you-should-know-about-http-pipelining

https://en.wikipedia.org/wiki/HTTP_persistent_connection

https://www.wikiwand.com/en/HTTP_pipelining

https://httpd.apache.org/docs/2.4/mod/core.html#keepalivetimeout

https://httpd.apache.org/docs/2.4/mod/core.html#fileetag

http://www-net.cs.umass.edu/kurose-ross-ppt-6e/

https://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html

https://tools.ietf.org/html/rfc2616

https://tools.ietf.org/html/rfc2616#section-14.24

https://avinetworks.com/docs/16.3/overview-of-server-persistence/

https://en.wikipedia.org/wiki/List_of_HTTP_header_fields



