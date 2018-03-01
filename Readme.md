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

### ETag Testing

First use 

`curl --http1.1 0.0.0.0:10001/hello.txt`

to get ETag for `hello.txt` assume ETag is `46288911-1519927953-10`

Then 

`curl -v --http1.1 --header 'If-Match: "46288911-1519927953-10"' 0.0.0.0:10001/hello.txt`

`curl -v --http1.1 --header 'If-Match: W/"46288911-1519927953-10"' 0.0.0.0:10001/hello.txt`

`curl -v --http1.1 --header 'If-Match: W/"46288911-1519927953-10", W/"afas"' 0.0.0.0:10001/hello.txt`

`curl -v --http1.1 --header 'If-Match: "ayy", "46288911-1519927953-10"' 0.0.0.0:10001/hello.txt`

