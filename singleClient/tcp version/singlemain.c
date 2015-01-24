/*
 * NMPS 2014: Screencaster server main program
 * Based on serveur.c and rtspserver.c / rtspanalyze.c
 * 2015-01-12 Rewritten (several modules instead of one)
 * 2015-01-19 Demo version supporting a single client
 */
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdbool.h>
#include <errno.h>

#include "rtspconsts.h"
#include "rtsputils.h"
#include "rtspanalyze.h"
#include "singlertsp.h"
#include "fdcomm.h"

/*
 * The screencaster server main program.
 * The listening socket is set up at the given port number,
 * connections are accepted and a process forked for each
 * new connection. This version is demo serving one client.
 */
int main(int argc, char* argv[])
{
  int sockfd, portno;
  socklen_t clilen;
  fd_set readfds;
  struct sockaddr_in serv_addr, cli_addr;
  int result;
  int fid, acc;

  if (argc < 2)
    {
      fprintf(stderr, "Error: no port provided.\nUsage: %s port\n",
	      argv[0]);
      exit(-1);
    }

  /* Set up the listening socket, start to listen */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      err_exit("Error: failed to open socket");
    }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  /* Socket setup */
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
           sizeof(serv_addr)) == -1)
    err_exit("bind");
  if (listen(sockfd, 5) == -1)
    err_exit("listen");
  clilen = sizeof(cli_addr);
  while (1)
    {
      FD_ZERO(&readfds);
      FD_SET(sockfd, &readfds);
      result = select(6, &readfds, NULL, NULL, NULL);
      if (result < 0)
	err_exit("select");
      if (FD_ISSET(sockfd, &readfds))
	{
	  acc = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
	  fid = fork();
	  if (fid < 0)
	    err_exit("fork");
	  else if (fid > 0)
	    { // close acc, update clientlist, send command to videoswitcher
	      close(acc);
	      fprintf(stdout,
		      "rtspserver/parent: will close acc\n");
	    }
	  else
	    {
	      close(sockfd);
	      fprintf(stdout,"rtspserver/child: starting rtspsession\n");
	      result = rtspsession(acc, portno);
	      fprintf(stdout,"rtspserver/child: session ended with return code %d\n",result);
	      exit(result);
	    }
	} //FDSET activated
    } // while forever
} // m a i n ()
