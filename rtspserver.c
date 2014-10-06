/* This is serveur.c modified 2014-10-04 (not in version control):
 * - Jussi's experimental modification concerning DESCRIBE
 * Also, error() renamed in order to not conflict with lib func error(3),
 * now it is err_exit()
 * And, modified second argument of add_to_buffer to be a const array
 */
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

#include "rtspanalyze.h"

void err_exit(const char *msg)
{
    perror(msg);
    exit(1);
}
/**Add string to buffer
bool endline adds an extra "\r\n" in the end
*/
void add_to_buffer(char *dest, const char *src,bool endline){
    strcat(dest,src);
    if (endline) strcat(dest,"\r\n");
}

int main(int argc,char* argv[]){

  int sockfd, newsockfd, portno;
  socklen_t clilen;

  char buffer[5000], sdpbuff[19]; // sdplen[5];

  /**rtsp analyze variables**/
  int result;
  Rtspblock rtspdata;
  char *buf;

  const char remy_SDP1[] =
    "v=0\r\n"
    "o=remy 123456789 987654321 IN IP4 10.0.2.15\r\n"
    "s=nmps-session\r\n"
    "c=IN IP4 10.0.2.15\r\n"
    "t=0 0\r\n"
    "a=recvonly\r\n"
    "m=video 3001 RTP/AVP 31\r\n"; // strings can be broken into several lines


  buf = malloc(sizeof(char)* (strlen(buffer)+1));
  /*************************/
  struct sockaddr_in serv_addr, cli_addr;
  int n;


  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    err_exit("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
           sizeof(serv_addr)) < 0)
    err_exit("ERROR on binding");
  listen(sockfd,5);
  clilen = sizeof(cli_addr);

  int rc;
  //CONNECTION
  int i=0;
  //for(;;){
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(sockfd,&readfds);
  rc=select(6,&readfds,NULL,NULL,NULL);

  if(FD_ISSET(sockfd,&readfds)) {
    int acc=accept(sockfd,(struct sockaddr*)&cli_addr,&clilen);

    int rcv,snd;
    /*if((rcv = recvfrom(acc,buffer,sizeof(buffer),0,(struct sockaddr *)&cli_addr,&clilen))<0) err_exit("rcv error\n");
      printf("%s\n",buffer);
      printf("BUFFER END\n\n");//OPTIONS
      if((snd = send(acc,buffer,sizeof(buffer),0))<0) err_exit("send error\n");
      bzero(buffer,5000);
    */
    int fid=fork();
    if(fid>0) {//father
      printf("i'm the father i close acc and keep sockfd%d\n", acc);
      close(acc);
      //printf("fid=%d\n",fid);
      //printf("acc = %d\n", acc);
      //printf("client IP: %d\nclient Port: %d\n",cli_addr.sin_addr,serv_addr.sin_port);
    }
    if(fid==0){//son
      printf("i'm the son i close sockfd and keep acc\n");
      close(sockfd);
      int i;
      int rcv,snd;

      for ( ; ; )
        {
          /**receive instruction message*/
          if((rcv = recvfrom(acc,buffer,sizeof(buffer),0,(struct sockaddr *)&cli_addr,&clilen))<0) err_exit("rcv error\n");
          /** Determine the instruction*/
          strcpy(buf,buffer);
          printf("received: \n%s\n",buf);//receive buffer, copy into buf... great variable names!!
          if((result=rtspanalyze(buf,&rtspdata))!=0) err_exit("rtspanalyze error!!\n");
          else {
            /**DECIDE WHAT TO DO*/
            switch(rtspdata.method){
            case OPTIONS:
                bzero(buffer,sizeof(buffer));
                add_to_buffer(buffer,"RTSP/1.0 200 OK\n",false);
                add_to_buffer(buffer,"Cseq: ",false);
                add_to_buffer(buffer,rtspdata.cseq,false);
                add_to_buffer(buffer,"\n",false);
                add_to_buffer(buffer,"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n",true);
                printf("about to send:\n%s",buffer);
                if((snd = send(acc,buffer,strlen(buffer),0))<0) err_exit("send error\n");

               break;

            case DESCRIBE:
                bzero(buffer,sizeof(buffer));
                add_to_buffer(buffer,"RTSP/1.0 200 OK",true);

                add_to_buffer(buffer,"Content-Type: ",false);
                add_to_buffer(buffer,rtspdata.accept,false);
                add_to_buffer(buffer,"\n",false);
                sprintf(sdpbuff,"Content-length: %d",(int)strlen(remy_SDP1));//end of header
                add_to_buffer(buffer,"Content-Length: 130",true);
                add_to_buffer(buffer,"Cseq: ",false);
                add_to_buffer(buffer,rtspdata.cseq,true);
                add_to_buffer(buffer,"\r\n",false);
                //SDP
                add_to_buffer(buffer,remy_SDP1,true);
                printf("%d",(int)strlen(remy_SDP1));
                printf("about to send:\n%s",buffer);
                if((snd = send(acc,buffer,strlen(buffer),0))<0)
                err_exit("send error\n");
            break;

            case PLAY:
            break;
            case SETUP:
            break;
            }
            bzero(buf,sizeof(buf));
            bzero(buffer,sizeof(buffer));
          }
        }

    }

  }

} // m a i n ()
