ports statically set: RTP should communicate with 3010-3011

try using port_no=3000
cvlc for no interface
vlc for interface

$ gcc -o rtspserver rtspserver.c rtspanalyze.c

$ ./rtspserver <port_no>
$ cvlc -vvv rtsp://<ip_addr>:<port_no>


