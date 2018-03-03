# How to run server?

## Compile c code

ex gcc SimpleServer.c

## Execute

Takes PORT and ROOT_DIR params

./a.out 10001 ~/user

# How to run netcat client?

ex nc 127.0.0.1 10001

Enter get requests in client after.

GET /hello.txt HTTP/1.0

if-modified-since: Sunday, 18-Feb-18 21:49:37 GMT

Be careful when entering multiline get requests, pressing enter only sends the first line. Copy paste it instead.
This is a possible bug in the code that needs to be investigated.

# Linux instructions
## To compile

`gcc -Wall SimpleServer.c -o sim` 

## Execute 

`./sim 10001 ~/Documents/server/`

## Testing

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


