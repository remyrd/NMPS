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
#include "rtpstreamer.h"
#include "rtspsession.h"

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
 * rtspsession -- process an RTSP session
 * Parameters:
 *  pacc the accepted socket
 * Returns:
 * 0 if session exited OK, -1 in case of an error
 */
int rtspsession(int pacc, int portno, int pcli)
{
  int fd[2], rtp_pid;
  int rcv, snd, result;
  struct sockaddr_in cli_addr;
  socklen_t clilen;
  Rtspblock rtspdata;
  char* remy_SDP1;
  char buf[COMMBUFSIZE], buffer[COMMBUFSIZE], sdpbuff[COMMLINESIZE];

  if (pipe2(fd, O_NONBLOCK) == -1)
    err_exit("pipe2");

  remy_SDP1 = malloc(sizeof(char)*512);
  if (remy_SDP1 != NULL)
    init_sdp1(remy_SDP1);
  else
    err_exit("malloc");
  
  for ( ; ; )
    {
      bzero(buf, sizeof(buf));
      bzero(buffer, sizeof(buffer));
      /**receive instruction message*/
      if ( (rcv = recvfrom(pacc, buffer, sizeof(buffer), 0,
			   (struct sockaddr *)&cli_addr, &clilen)) < 0 )
	{
	  fprintf(stderr,"*** recvfrom error: %s(%d)\n",
		  strerror(errno), errno);
	  exit(-1);
	}
      strcpy(buf, buffer);
      printf("--->received: \n%s\n",buf);
      //receive buffer, copy into buf... great variable names!!
      if ( (result = rtspanalyze(buf, &rtspdata)) != 0 )
	{
	  err_exit("rtspanalyze error!!\n");
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
	      /**spawn RTP*/
	      if ((rtp_pid = fork()) < 0)
		err_exit("RTP fork error");
	      else if (rtp_pid > 0)
		{ //RTSP
		  close(fd[0]);//close input
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
		  write(fd[1], "4", 2); //write on pipe
		}
	      else
		{
		  //
		result = rtpstreamer(pcli,fd[0],3010);
		
		  //
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
	      write(fd[1], "5", 2); //write on pipe
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
	      write(fd[1], "7", 2);
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
