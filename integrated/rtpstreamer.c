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
#include "rtpstreamer.h"
#include "fdcomm.h"

/*
 * rtpstreamer -- serve video in RTP packets for the given client.
 * To be executed in its own process (child of RTSP session process).
 * It is connected with a pipe to the RTSP session parent which sends it
 * "commands" regarding the state of the RTSP session. Video stream is
 * read from the client-specific FIFO to which video is written to by a
 * separate process (every rtpstreamer gets their own copy of the data).
 *
 * Parameters:
 *  pcli : client number to be used for reading the FIFO
 *  pfd : pipe end for reading commands from the RTSP process
 *  portno : port number for communication
 *
 * Returns:
 *  exits with exit code 0 if TEARDOWN received properly,
 *  returns -1 in case of an error.
 */
int rtpstreamer(int pcli, int pfd, int portno)
{
  struct sockaddr_in rtpcliaddr;
  socklen_t socksize;
  int res,fifofd,sockfd,dataport;
  int command,old_command,snd;
  char *cmdstr;
  char *videobuf;
  ssize_t nbytes,vbytes = 0;
  Rtp_packet rtp_packet;
  Rtp_header rtp_header;

  if ((cmdstr = malloc(sizeof(char)*CMDSTRLEN)) == NULL ||
      (videobuf = malloc(sizeof(char)*VIDEOBUFSIZE)) == NULL ||
      (rtp_packet.payload = malloc(sizeof(char)*RTP_PAYLOAD_SIZE)) == NULL)
    {
      fprintf(stderr,"Error: rtpstreamer: malloc failed\n");
      return -1;
    }
  command = -1;
  memset(cmdstr,0,CMDSTRLEN);
  memset(videobuf,0,VIDEOBUFSIZE);
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      fprintf(stderr,"Error: rtpstreamer: socket call failed: %s\n",strerror(errno));
      free(rtp_packet.payload);
      return -1;
    }
  memset((char*)&rtpcliaddr, 0, sizeof(rtpcliaddr));
  rtpcliaddr.sin_family = AF_INET;
  dataport = portno + 10; //3010
  fprintf(stdout,"rtpstreamer: dataport is %d\n", dataport);
  rtpcliaddr.sin_port = htons(dataport);
  socksize = sizeof(rtpcliaddr);
  if (inet_aton("127.0.0.1", &rtpcliaddr.sin_addr) == 0)
    {
      fprintf(stderr,"Error: inet_aton() failed\n");
      return -1;
    }
  /* Open the FIFO for video */
  res = open_fd(pcli, O_RDONLY, 0644, &fifofd);
  if (res == 0)
    fprintf(stdout,"rtpstreamer: open OK for client %d FIFO (fd %d)\n",pcli,fifofd);
  else
    {
      fprintf(stderr,"Error: rtpstreamer: open_fd(child %d) failed: %s\n",pcli,strerror(errno));
      return -1;
    }
  for ( ; ; )
    {
      /* 1. Read the "video" (output of videoswitcher) from the client FIFO */
      memset(videobuf,0,VIDEOBUFSIZE);
      vbytes = 0;
      res = read_fd(fifofd, videobuf, VIDEOBUFSIZE, &vbytes);
      if (res == 0)
	{
	  /************************************************************/
	  /* All OK, process the data here (vbytes is length of data) */
	  /* It can be then sent in 'PLAY' (reset vbytes after that)  */
	  /************************************************************/
	  fprintf(stdout,"rtspstreamer: client %d read %zu bytes\n",pcli,vbytes);
	}
      else if (res == 1)
	{
	  fprintf(stderr,"Note: rtpstreamer: FIFO %d read would block\n",pcli);
	  sleep(1);
	}
      else if (res == 2)
	{
	  fprintf(stderr,"Note: rtpstreamer: FIFO %d read returned 0\n",pcli);
	  return -1;
	}
      else
	{
	  fprintf(stderr,"Error: rtpstreamer: read of FIFO %d failed: %s\n",pcli,strerror(errno));
	  return -1;
	}

      /* 2. Read a command, if any is available in the pipe from RTSP */
      memset(cmdstr,0,CMDSTRLEN);
    cmdpiperead:
      nbytes = read(pfd, &cmdstr, sizeof(cmdstr));
      if (nbytes == -1)
	{
	  if (errno == EINTR)
	    goto cmdpiperead;
	  else if (errno == EWOULDBLOCK) // TODO: Ensure that the command pipe is nonblocking
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
      else
	command = atoi(cmdstr);

      /* 3. Determine the command from RTSP pipe and act accordingly */		
      switch (command)
	{
	case SETUP:
	  if (old_command != SETUP)
	    {
	      fprintf(stdout,"rtpstreamer: client %d RTP enters SETUP\n",pcli);
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
	    fprintf(stdout,"rtpstreamer: client %d RTP enters PLAY\n",pcli);
	  /********************************************************************/
	  /* TODO: Construct an RTP packet with timestamp and sequence number */
	  /* and send it to the client. Keep track of sent/unsent data!       */
	  /********************************************************************/
	  if (vbytes > 0)
	    {
		memcpy(rtp_packet.payload,videobuf,VIDEOBUFSIZE);
	      if ( (snd = sendto(sockfd, &rtp_packet,
				 sizeof(rtp_packet),0,
				 (struct sockaddr *)&rtpcliaddr, socksize)) == -1 )
		fprintf(stderr, "Error: rtpstreamer: send failed in PLAY(RTP loop):%s\n",
			strerror(errno));				  
	      old_command = PLAY;
	      rtp_header.seq++;
	    }
	  break;
	case PAUSE:
	  break;
	case TEARDOWN:
	  printf("rtpstreamer: client %d RTP enters TEARDOWN\n",pcli);
	  exit(0);
	  break;
	} // RTP switch/case
    } // RTP infinite loop
} // RTP child function
