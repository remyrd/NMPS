/*
 * 2015-01-18 NMPS screencaster
 * rtspsession.c
 * 
 * TODO: SIGCHLD handling
 */
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
#include "singlertp.h"
#include "singlertsp.h"

/**Add string to buffer
   if endline is TRUE, add an extra "\r\n" in the end
   No checking for target string length (ensure beforehand)
*/
void add_to_buffer(char *dest, const char *src, int endline)
{
  strcat(dest, src);
  if (endline)
    strcat(dest,"\r\n");
}

/**Construct the first SDP offer for negotiation
 * TODO: Parameterize
 */
void init_sdp1(char* pst)
{
  bzero(pst, sizeof(pst));
  strcat(pst, "v=0 0\r\n");
  strcat(pst, "o=remy 123456789 987654321 IN IP4 127.0.0.1\r\n");
  strcat(pst, "s=nmps-session\r\n");
  strcat(pst, "c=IN IP4 127.0.0.1\r\n");
  strcat(pst, "t=0 0\r\n");
  strcat(pst, "a=tool:LIVE555 Streaming Media v2014.01.13\r\n");
  strcat(pst, "m=video 3010 RTP/AVP 96\r\n");
  strcat(pst, "a=rtpmap:96 H264/90000\r\n");
  strcat(pst, "a=fmtp:96 packetization-mode=1\r\n");
}

/*
 * rtspsession -- process an RTSP session (single client version)
 * Parameters:
 *  pacc: the accepted socket
 *  portno: the RTSP port number
 * Returns:
 *  0 if session exited OK, -1 in case of an error
 */
int rtspsession(int pacc, int portno)
{
  int fd[2]; // command pipe to RTP streamer
  pid_t rtp_pid;
  int rcv, snd, result, streamer;
  struct sockaddr_in cli_addr;
  socklen_t clilen;
  Rtspblock rtspdata;
  char* remy_SDP1;
  char buf[COMMBUFSIZE], buffer[COMMBUFSIZE], sdpbuff[COMMLINESIZE];
  char cmdchar;

  if (pipe2(fd, O_NONBLOCK) == -1)
    err_exit("pipe2");

  remy_SDP1 = malloc(sizeof(char)*512);
  if (remy_SDP1 != NULL)
    init_sdp1(remy_SDP1);
  else
    err_exit("malloc");

  streamer = 0; // Flag to indicate whether rtpstreamer has been forked or not

  for ( ; ; )
    {
      memset(buf, 0, sizeof(buf));
      memset(buffer, 0, sizeof(buffer));
      /* Receive data from socket: RTSP instruction message over TCP or UDP */
    recvfrom_try:
      rcv = recvfrom(pacc,
		     buffer,
		     sizeof(buffer),
		     0,
		     (struct sockaddr *)&cli_addr,
		     &clilen);
      if (rcv == -1)
	{
	  if (errno == EINTR)
	    goto recvfrom_try;
	  else
	    {
	      fprintf(stderr,"Error: rtspsession: recvfrom failed: %s(%d)\n",strerror(errno),errno);
	      exit(-1); // TODO: more errno tests
	    }
	}
      else if (rcv == 0)
	{
	  fprintf(stderr,"rtspsession: recvfrom returned 0, peer has shut down\n");
	  exit(0);
	}

      if (!buffer_has_binary(buffer,rcv))
	{
	  printf("--->received: \n%s\n",buffer);
	}
      else
	{
	  printf("--->received binary data in buffer:\n");
	  print_hex(buffer, rcv, 32);
	  continue; // back to reading loop
	}

      memcpy(buf, buffer, rcv); // not strcpy because it might be binary
      if ( (result = rtspanalyze(buf, &rtspdata)) != 0 )
	{
	  fprintf(stderr, "rtspsession: rtspanalyze returned error %d\n", result);
	}
      else
	{
	  /**DECIDE WHAT TO DO*/
	  switch(rtspdata.method)
	    {
	    case OPTIONS:
	      bzero(buffer, sizeof(buffer));
	      add_to_buffer(buffer, "RTSP/1.0 200 OK", TRUE);
	      if (rtspdata.cseq != NULL)
		{
		  add_to_buffer(buffer,"Cseq: ", FALSE);
		  add_to_buffer(buffer,rtspdata.cseq, TRUE);
		}
	      else
		printf("\n***Cseq is missing from received method!\n");
	      add_to_buffer(buffer,
			    "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER",
			    TRUE);
	      add_to_buffer(buffer,
			    "Content-Length: 0", TRUE);
	      add_to_buffer(buffer,
			    "\r\n",
			    FALSE); // there is already one CRLF above
	      printf("--->about to send:\n%s", buffer);
	      // usleep(100000);
	      if ( (snd = send(pacc,buffer,strlen(buffer), 0)) < 0 )
		err_exit("\nsend error in OPTIONS\n");
	      break;

	    case DESCRIBE:
	      bzero(buffer, sizeof(buffer));
	      bzero(sdpbuff, sizeof(sdpbuff));
	      add_to_buffer(buffer, "RTSP/1.0 200 OK", TRUE);
	      add_to_buffer(buffer, "Content-Type: ", FALSE);
	      if (rtspdata.accept != NULL)
		add_to_buffer(buffer, rtspdata.accept, TRUE);
	      else
		add_to_buffer(buffer, "ACCEPT_IS_MISSING", TRUE);
	      add_to_buffer(buffer,"Cseq: ", FALSE);
	      if (rtspdata.cseq != NULL)
		add_to_buffer(buffer, rtspdata.cseq, TRUE);
	      else
		add_to_buffer(buffer, "CSEQ_IS_MISSING", TRUE);
	      printf("--->will add SDP:%s\n"
		     "--->length %zu\n",
		     remy_SDP1, strlen(remy_SDP1));
	      sprintf(sdpbuff, "Content-Length: %zu", strlen(remy_SDP1));
	      add_to_buffer(buffer, sdpbuff, TRUE);
	      add_to_buffer(buffer, "\r\n", FALSE);
	      // SDP:
	      strcat(buffer, remy_SDP1);
	      printf("\n--->about to send (SDP length is %zu):\n%s",
		     strlen(remy_SDP1), buffer);
	      // usleep(100000);
	      if ((snd = send(pacc, buffer, strlen(buffer), 0)) < 0)
		err_exit("\nsend error in DESCRIBE\n");
	      break;

	    case SETUP:
	      if (!streamer)
		{
		  if ((rtp_pid = fork()) == -1)
		    {
		      fprintf(stderr,"Error: rtspsession: fork failed\n");
		      return -1;
		    }
		  else if (rtp_pid == 0)
		    { // child : prepare to stream RTP video
		      close(fd[1]); // close write end
		      if ((result = rtpstreamer(fd[0], portno)) != 0)
			{
			  fprintf(stderr, "Error: rtpstreamer failed: %s\n",strerror(errno));
			  exit(-1);
			}
		      else
			{
			  fprintf(stdout,"rtspsession: streaming ended OK\n"); // Can it?
			  exit(0);
			}
		    }
		  else
		    { // parent: handle all incoming RTSP messages
		      close(fd[0]); // close read end
		      bzero(buffer, sizeof(buffer));
		      add_to_buffer(buffer, "RTSP/1.0 200 OK", TRUE);
		      add_to_buffer(buffer, "Transport: ", FALSE);
		      if (rtspdata.transport != NULL)
			add_to_buffer(buffer, rtspdata.transport, TRUE);
		      else
			add_to_buffer(buffer, "TRANSPORT_IS_MISSING", TRUE);
		      //add_to_buffer(buffer, ";server_port=3012-3013", TRUE);
		      add_to_buffer(buffer, "Cseq: ", FALSE);
		      if (rtspdata.cseq != NULL)
			add_to_buffer(buffer, rtspdata.cseq, TRUE);
		      else
			add_to_buffer(buffer, "CSEQ_IS_MISSING", TRUE);
		      add_to_buffer(buffer, "Content-Length: 0", TRUE);
		      add_to_buffer(buffer, "Session: 26101992", TRUE);
		      add_to_buffer(buffer, "\r\n", FALSE); // there is already one CRLF above
		      printf("--->about to send:\n%s", buffer);
		      // usleep(100000);
		      if ((snd = send(pacc, buffer, strlen(buffer), 0)) < 0)
			err_exit("\nsend error in SETUP\n");
		      streamer = 1; // Mark streamer as started
		    }
		  cmdchar = '4';
		  write(fd[1], &cmdchar, 1); // SETUP command to rtpstreamer
		}
	      break;

	    case PLAY:
	      bzero(buffer, sizeof(buffer));
	      add_to_buffer(buffer, "RTSP/1.0 200 OK", TRUE);
	      add_to_buffer(buffer, "Cseq: ", FALSE);
	      if (rtspdata.cseq != NULL)
		add_to_buffer(buffer, rtspdata.cseq, TRUE);
	      else
		add_to_buffer(buffer, "CSEQ_IS_MISSING", TRUE);
	      add_to_buffer(buffer, "Session: 26101992;timeout=5", TRUE);
	      add_to_buffer(buffer, "RTP-Info: url=rtp://127.0.0.1:3010", TRUE);
	      add_to_buffer(buffer, "\r\n", FALSE);
	      printf("--->about to send:\n%s", buffer);
	      // usleep(100000);
	      if((snd = send(pacc, buffer, strlen(buffer), 0)) < 0)
		err_exit("\nsend error in PLAY\n");
	      cmdchar = '5';
	      write(fd[1], &cmdchar, 1); // PLAY command to rtpstreamer
	      break;
	      
	    case TEARDOWN:
	      bzero(buffer,sizeof(buffer));
	      add_to_buffer(buffer, "RTSP/1.0 200 OK", TRUE);
	      add_to_buffer(buffer, "Cseq: ", FALSE);
	      if (rtspdata.cseq != NULL)
		add_to_buffer(buffer, rtspdata.cseq, TRUE);
	      else
		add_to_buffer(buffer, "CSEQ_IS_MISSING", TRUE);
	      add_to_buffer(buffer, "Session: 26101992;timeout=5", TRUE);
	      add_to_buffer(buffer,"\r\n", FALSE);
	      printf("--->about to send:\n%s", buffer);
	      // usleep(100000);
	      if((snd = send(pacc, buffer, strlen(buffer), 0)) < 0)
		err_exit("\nsend error in TEARDOWN\n");
	      cmdchar = '7';
	      write(fd[1], &cmdchar, 1); // TEARDOWN command to rtpstreamer
	      break;

	    case GET_PARAMETER:
	      bzero(buffer, sizeof(buffer));
	      add_to_buffer(buffer, "RTSP/1.0 200 OK", TRUE);
	      add_to_buffer(buffer, "Cseq: ", FALSE);
	      if(rtspdata.cseq != NULL)
		add_to_buffer(buffer, rtspdata.cseq, TRUE);
	      else
		add_to_buffer(buffer, "CSEQ_IS_MISSING", TRUE);
	      add_to_buffer(buffer, "Session: 26101992;timeout=5", TRUE);
	      add_to_buffer(buffer, "\r\n", FALSE);
	      printf("--->about to send:\n%s", buffer);
	      // usleep(100000);
	      if((snd = send(pacc, buffer, strlen(buffer), 0)) < 0)
		err_exit("\nsend error in GET_PARAMETER\n");
	      break;
	    }//switch/case
	}//decide what to do
    } // RTSP infinite loop
} // function
