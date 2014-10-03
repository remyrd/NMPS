compile:
$	gcc -o serveur serveur.c rtspanalyze.c
run:
$	./serveur <port_number>
	and
$	vlc -vvv rtsp://<ip_addr>:<port_number>
