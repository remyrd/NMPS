/*
 * NMPS 2014: Screencaster server main program
 * Based on serveur.c and rtspserver.c / rtspanalyze.c
 * 2015-01-12 Rewritten (several modules instead of one)
 *
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
#include "rtspsession.h"
#include "fdcomm.h"
#include "videoswitcher.h"

int num_for_client(pid_t*);
int write_cmd(int,char*,size_t);

/* variables outside any function, to be accessed from signal handlers */
pid_t clientlist[MAX_CLIENTS];  // Child PIDs of taken clients
int clinum; // For maintaining the clientlist

/*
 * The screencaster server main program.
 * The listening socket is set up at the given port number,
 * connections are accepted and a process forked for each
 * new connection. The process calling a function to handle
 * a client reserves a position from the clientlist array
 * so that the index plus one is the client number.
 * The child process PID indicates a reserved client number,
 * zero marks a free client. A client number is the index
 * of the clientlist array plus one.
 * No more new clients will be served if the list is full.
 * When the client serving function (child) exits, the
 * respective PID is cleared from the list. (TODO)
 */
int main(int argc, char* argv[])
{
  int sockfd, portno;
  socklen_t clilen;
  fd_set readfds;
  struct sockaddr_in serv_addr, cli_addr;
  int result, ix;
  int fid, acc;
  int cfd[2] = {0,0};    // command pipe
  char cstr[MAX_CMDLEN]; // command string
  pid_t vspid;


  if (argc < 2)
    {
      fprintf(stderr, "Error: no port provided.\nUsage: %s port\n",
	      argv[0]);
      exit(-1);
    }

  /* 1. Start the videoswitcher in a child process */
  if (pipe2(cfd, O_NONBLOCK))
      err_exit("pipe2");
  for (ix = 0; ix < MAX_CLIENTS; ix++)
    {
      if (create_fifo(ix + 1) == -1)
        exit(-1);
    }

  vspid = fork();
  if (vspid < 0)
    {
      perror("fork");
      exit(-1);
    }
  else if (vspid == 0)
    { // child gets command pipe read end
      videoswitcher(cfd[0]); // should not return ever
      fprintf(stderr, "rtspserver/child: videoswitcher has returned\n");
      exit(0);
    }
  else
    { // parent has command pipe write end
      close(cfd[0]);
    }

  /* 2. Set up the listening socket, start to listen */
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
           sizeof(serv_addr)) < 0)
    err_exit("bind");
  if (listen(sockfd, 5) == -1)
    err_exit("listen");
  clilen = sizeof(cli_addr);
  /* Keeping track of available client numbers */
  clinum = 0;
  memset(clientlist,0,MAX_CLIENTS);
  memset(cstr,0,MAX_CMDLEN);
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
	  clinum = num_for_client(clientlist);
	  if (clinum == 0)
	    { // print error message and go back to infinite loop
	      fprintf(stdout,"rtspserver: No free client positions available.\n");
	      close(acc);
	      sleep(5);
	      continue;
	    }
	  fid = fork();
	  if (fid < 0)
	    err_exit("fork");
	  else if (fid > 0)
	    { // close acc, update clientlist, send command to videoswitcher
	      close(acc);
	      fprintf(stdout,
		      "rtspserver/parent: will close acc %d, clinum is %d\n",
		      acc, clinum);
	      clientlist[clinum - 1] = fid;
	      snprintf(cstr, MAX_CMDLEN - 1, "+%d", clinum);
	      result = write_cmd(cfd[1], cstr, strlen(cstr));
	      if (result != 0)
		{
		  fprintf(stderr,
			  "rtpserver/parent: write_cmd returned %d\n",
			  result);
		}
	    }
	  else
	    {
	      close(sockfd);
	      fprintf(stdout,"rtspserver/child: starting rtspsession"
		     " with acc %d and clinum %d\n",
		     acc, clinum);

	      result = rtspsession(acc, portno, clinum);

	      fprintf(stdout,
		      "rtspserver/child: session for client %d ended"
		      " with return code %d\n",
		      clinum, result);
	      exit(0); // child ends, parent gets signal and removes clinum PID
	    }
	} //FDSET activated
    } // while forever
} // m a i n ()

/*
 * num_for_client -- return a number for a new client if possible
 * Returns the index+1 to the first free position of the given list 
 * or zero if there is no free positions.
 */
int num_for_client(pid_t* pclist)
{
  int ix;
  for (ix = 0; ix < MAX_CLIENTS; ix++)
    {
      if (!pclist[ix])
	return ix + 1;
    }
  return 0;
}

/*
 * write_cmd -- write a command to the given pipe
 */
int write_cmd(int p, char *cs, size_t cc)
{
  ssize_t written = write_fd(p, cs, cc);
  if (written == 0)
    {
      fprintf(stderr,
              "write_cmd: pipe %d write %s (length %d) OK\n",
              p, cs, cc);
    }
  else if (written == 1)
    {
      fprintf(stderr, "write_cmd: pipe %d write would block\n",
              p);
      return 1;
    }
  else
    {
      fprintf(stderr, "Error: write_cmd: pipe write failed\n");
      return -1;
    }
  return 0;
}
