#include <stdio.h>
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#include "rtspconsts.h"
#include "rtspanalyze.h"
#include "rtsputils.h"
#include "fdcomm.h"
#include "ffmpegexec.h"
#include "singlertp.h"

/*
 * rtpstreamer -- serve video in RTP packets for the given client.
 * To be executed in its own process (child of RTSP session process).
 * Single client demo version.
 * Parameters:
 *  pfd: pipe read end for the command pipe from RTSP (PLAY,PAUSE,etc.)
 *  portno: the port number
 * Returns:
 *  returns 0 if command TEARDOWN received OK,
 *  returns -1 in case of an error.
 */
int rtpstreamer(int pfd,int portno)
{
  
	int rtp_sockfd, rtp_newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
 	int dataport=portno+10;
	
    rtp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (rtp_sockfd < 0)
         error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(dataport);
    if (bind(rtp_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(rtp_sockfd, 5);
    clilen = sizeof(cli_addr);
rtp_newsockfd = accept(rtp_sockfd, (struct sockaddr *) &cli_addr, &clilen);
fprintf(stdout,"TCP SOCKET ACCEPTED?");

  socklen_t socksize;
  int res,sockfd,snd;
  int command,old_command,noted;
  char *videobuf;
  char cmdchar;
  ssize_t nbytes,vbytes = 0;
  Rtp_packet rtp_packet;
  Rtp_header rtp_header;
  int videofd[2] = {-1,-1}; // important initialization, see below
  pid_t vspid;

  /* preparations */
  if ( (videobuf = malloc(sizeof(char)*VIDEOBUFSIZE)) == NULL ||
       (rtp_packet.payload = malloc(sizeof(char)*RTP_PAYLOAD_SIZE)) == NULL )
    {
      fprintf(stderr,"Error: rtpstreamer: malloc failed\n");
      return -1;
    }
  command = -1; noted = 0;
  memset(videobuf,0,VIDEOBUFSIZE);
  
  fprintf(stdout,"rtpstreamer: dataport is %d\n", dataport);

  for ( ; ; )
    { /* video reading, command pipe reading, RTP serving
       * Read pipe from FFmpeg only if it has been opened (PLAY called)
       */
      if (videofd[0] != -1)
	{ // TODO: read() could be used instead of read_fd()
	  memset(videobuf, 0, VIDEOBUFSIZE);
	  vbytes = 0;
	  res = read_fd(videofd[0], videobuf, VIDEOBUFSIZE, &vbytes);
	  if (res == 0)
	    {
	      /* Data was read OK from FFmpeg */
	      fprintf(stdout,"rtpstreamer: read %zu bytes\n",vbytes);
		fprintf(stdout,"rtpstreamer: command %d, vbytes %d\n",command,vbytes);
	      if (old_command == PLAY && vbytes > 0)
		{ // send the video data
		  memcpy(rtp_packet.payload, videobuf, vbytes);
		  rtp_packet.payload_len = htonl((uint32_t)vbytes);
		  fprintf(stderr,"rtpstreamer: rtp_packet.payload:");		  
		  print_hex(rtp_packet.payload,sizeof(rtp_packet.payload),40);
		fprintf("rtpstreamer: sending data to port %d\n",portno+10);
		  if ( (snd = sendto(rtp_newsockfd, &rtp_packet,
				     sizeof(rtp_packet),0,
				     (struct sockaddr *)&cli_addr, socksize)) == -1 )
		    fprintf(stderr, "Error: rtpstreamer: send failed in PLAY(RTP loop):%s\n",
			    strerror(errno));				  
		  rtp_header.seq++;
		  old_command = PLAY;
		}
		else
		
			fprintf(stderr, "nothing was sent\n");
	      noted = 0; // clear blocking-note flag if it was set
	    }
	  else if (res == 1)
	    {
	      if (!noted)
		{
		  fprintf(stderr,"Note: rtpstreamer: videofd[0] (%d) read would block\n",videofd[0]);
		  noted = 1;
		}
	      usleep(100000); // microseconds
	    }
	  else if (res == 2)
	    {
	      fprintf(stderr,"Note: rtpstreamer: videofd[0] (%d) read returned 0\n",videofd[0]);
	      return -1;
	    }
	  else
	    {
	      fprintf(stderr,"Error: rtpstreamer: videofd[0] (%d) read failed: %s\n",videofd[0],strerror(errno));
	      return -1;
	    }
	}

      /* 2. Read a single character command if available from pipe 'pfd' */
    cmdpiperead:
      cmdchar = 0;
      nbytes = read(pfd, &cmdchar, 1);
      if (nbytes == -1)
	{
	  if (errno == EINTR)
	    goto cmdpiperead;
	  else if (errno == EWOULDBLOCK)
	    command = old_command;  // no command coming from the pipe
	  else
	    {
	      fprintf(stderr,"Error: rtpstreamer: command pipe read failed: %s\n",strerror(errno));
	      return -1;
	    }
	}
      else if (nbytes == 0)
	{
	  fprintf(stdout,"Note: rtpstreamer: command pipe read zero (closed)\n");
	  return -1;
	}
      else if (nbytes == 1)
	{
	  command = cmdchar - 0x30; // ASCII '0'...'9' to 0...9
	  fprintf(stderr,"rtpstreamer: command character %c received\n", cmdchar);
	}
      else
	command = -1;

      /* 3. Determine the command from RTSP pipe and act accordingly */		
      switch (command)
	{
	case SETUP:
	  if (old_command != SETUP)
	    {
	      fprintf(stdout,"rtpstreamer: RTP enters SETUP\n");
	      /**Construct first rtp packet*/
	      rtp_header.seq=0;
	      rtp_header.version=2;
	      rtp_header.p=0;
	      rtp_header.x=0;
	      rtp_header.pt=96;
	      rtp_header.cc=0;
	      rtp_header.m=0;
	      rtp_packet.header=rtp_header;
	      memcpy(rtp_packet.payload,"This payload does not make any sense...",39);
	      rtp_packet.payload_len = 39;
	      old_command = SETUP; //set old command
	    }
	  break;
	case PLAY:
	  if (old_command != PLAY)
	    fprintf(stdout,"rtpstreamer: RTP enters PLAY\n");
	  /* Start FFmpeg if it is not running yet. */
	  if (videofd[0] == -1)
	    {
	      if (pipe2(videofd, O_NONBLOCK) == -1)
		{
		  fprintf(stderr, "Error: rtpstreamer: pipe2 failed\n");
		  return -1;
		}
	      vspid = fork();
	      if (vspid == -1)
		{
		  fprintf(stderr, "Error: rtpstreamer: fork failed\n");
		  return -1;
		}
	      else if (vspid == 0)
		{ /* execute FFmpeg as an external program with redirection */
		  fprintf(stderr,"rtpstreamer/child: starting ffmpeg\n");
		  close(videofd[0]); // unneeded video pipe read end in child
		  close(pfd);        // unneeded cmd pipe read end
		  ffmpegexec(videofd[1]);
		  fprintf(stderr,"rtpstreamer/child: ffmpeg has exited\n");
		  return -1;
		}
	      else
		{
		  close(videofd[1]); // unneeded video pipe write end in parent
		  fprintf(stderr,"rtpstreamer/parent: fork ffmpeg done\n");
		}
	    }
		old_command = PLAY;
	  break;
	case PAUSE:
	  break;
	case TEARDOWN:
	  printf("rtpstreamer: RTP enters TEARDOWN\n");
	  return 0;
	  break;
	default:
	  break;
	} // RTP switch/case
    } // RTP infinite loop
} // RTP child function
