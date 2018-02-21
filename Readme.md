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
