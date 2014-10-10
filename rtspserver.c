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

    int sockfd, portno;
    // int newsockfd;
    socklen_t clilen;
    char buffer[1024], buf[1024], sdpbuff[80],rtp_out[200],rtp_in[200],databuff[5000]; // sdplen[5];
    /**rtsp variables**/
    int result;
    Rtspblock rtspdata;
    struct sockaddr_in serv_addr, cli_addr;
    char* remy_SDP1 = malloc(sizeof(char)*512);
    if (remy_SDP1 == NULL)
        exit(-1);
    //  char remy_SDP1[512] ="\0";
    bzero(remy_SDP1,sizeof(remy_SDP1));
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);
    strcat(remy_SDP1,"v=0 0\r\n");
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);
    strcat(remy_SDP1,"o=remy 123456789 987654321 IN IP4 10.0.2.15\r\n");
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);
    strcat(remy_SDP1,"s=nmps-session\r\n");
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);
    strcat(remy_SDP1,"c=IN IP4 10.0.2.15\r\n");
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);
    strcat(remy_SDP1,"t=0 0\r\n");
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);
    strcat(remy_SDP1,"a=recvonly\r\n");
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);
    strcat(remy_SDP1,"m=video 3010 RTP/AVP 31\r\n");
    //printf("remy_SDP1(%zu chars):%s\n",strlen(remy_SDP1),remy_SDP1);

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
    //int i=0;
    //for(;;){
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd,&readfds);
    rc=select(6,&readfds,NULL,NULL,NULL);
    if (rc < 0)
    {
      free(remy_SDP1);
      exit(-1);
    }
    /***********************************************************
    RTSP connection Incoming
    ************************************************************/
    if(FD_ISSET(sockfd,&readfds)) {
        int acc=accept(sockfd,(struct sockaddr*)&cli_addr,&clilen);
        int fid=fork();
        if(fid>0) {//father
            printf("i'm the father i close acc and keep sockfd%d\n", acc);
            close(acc);
        }
        /*****************************************************
         Intercept RTSP connection
         *************************************************/
        if(fid==0){
            printf("i'm the son i close sockfd and keep acc\n");
            close(sockfd);
            int rcv,snd,rtp_pid=1,fd[2];
            pipe(fd);
            Rtp_packet rtp_packet;
            Rtp_header rtp_header;
            for ( ; ; )
            {
                bzero(buf,sizeof(buf));
                bzero(buffer,sizeof(buffer));
                /**receive instruction message*/
                if((rcv = recvfrom(acc,buffer,sizeof(buffer),0,(struct sockaddr *)&cli_addr,&clilen))<0) err_exit("rcv error\n");
                /****************************************************************
                 Determine the instruction
                 ******************************************************************/
                strcpy(buf,buffer);
                printf("received: \n%s\n",buf);//receive buffer, copy into buf... great variable names!!
                if((result=rtspanalyze(buf,&rtspdata))!=0) err_exit("rtspanalyze error!!\n");
                else {
                /**DECIDE WHAT TO DO*/
                    switch(rtspdata.method){
                    case OPTIONS:
                        bzero(buffer,sizeof(buffer));
                        add_to_buffer(buffer,"RTSP/1.0 200 OK",true);
                        if (rtspdata.cseq != NULL)
                        {
                            add_to_buffer(buffer,"Cseq: ",false);
                            add_to_buffer(buffer,rtspdata.cseq,true);
                        }
                        else
                            printf("\n***Cseq is missing from received method!\n");
                        add_to_buffer(buffer,"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE",true);
                        add_to_buffer(buffer,"Content-Length: 0",true);
                        add_to_buffer(buffer,"\r\n",false); // there is already one CRLF above
                        printf("about to send:\n%s",buffer);
                        if((snd = send(acc,buffer,strlen(buffer),0))<0)
                            err_exit("send error\n");

                    break;

                    case DESCRIBE:
                        bzero(buffer,sizeof(buffer));
                        bzero(sdpbuff,sizeof(sdpbuff));
                        add_to_buffer(buffer,"RTSP/1.0 200 OK",true);
                        add_to_buffer(buffer,"Content-Type: ",false);
                        if (rtspdata.accept != NULL)
                            add_to_buffer(buffer,rtspdata.accept,true);
                        else
                            add_to_buffer(buffer,"ACCEPT_IS_MISSING",true);
                        add_to_buffer(buffer,"Cseq: ",false);
                        if (rtspdata.cseq != NULL)
                            add_to_buffer(buffer,rtspdata.cseq,true);
                        else
                            add_to_buffer(buffer,"CSEQ_IS_MISSING",true);
                        printf("will add SDP:%s of %zu", remy_SDP1, strlen(remy_SDP1));
                        sprintf(sdpbuff,"Content-Length: %zu",strlen(remy_SDP1));
                        add_to_buffer(buffer,sdpbuff,true);
                        add_to_buffer(buffer,"\r\n",false);
                        // SDP:
                        strcat(buffer,remy_SDP1);
                        printf("about to send (SDP length is %zu):\n%s",
                        strlen(remy_SDP1),buffer);
                        if((snd = send(acc,buffer,strlen(buffer),0))<0)
                            err_exit("send error\n");
                    break;
                    case SETUP:
                        /**spawn RTP*/
                        if((rtp_pid=fork())<0)
                            err_exit("RTP fork error");
                        if (rtp_pid>0){//RTSP
                            close(fd[0]);//close input

                            bzero(buffer,sizeof(buffer));
                            add_to_buffer(buffer,"RTSP/1.0 200 OK",true);
                            add_to_buffer(buffer,"Transport: ",false);
                            if (rtspdata.transport != NULL)
                                add_to_buffer(buffer,rtspdata.transport,false);
                            else
                                add_to_buffer(buffer,"TRANSPORT_IS_MISSING",false);
                            add_to_buffer(buffer,";server_port=3010-3011",true);
                            add_to_buffer(buffer,"Cseq: ",false);
                            if(rtspdata.cseq != NULL)
                                add_to_buffer(buffer,rtspdata.cseq,true);
                            else
                                add_to_buffer(buffer,"CSEQ_IS_MISSING",true);
                            add_to_buffer(buffer,"Content-Length: 0",true);
                            add_to_buffer(buffer,"Session: 26101992",true);
                            add_to_buffer(buffer,"\r\n",false); // there is already one CRLF above
                            printf("about to send:\n%s",buffer);
                            if((snd = send(acc,buffer,strlen(buffer),0))<0)
                                err_exit("send error\n");
                            write(fd[1],"4",2);//write on pipe
                        }
                    break;
                    case PLAY:
                        bzero(buffer,sizeof(buffer));
                        add_to_buffer(buffer,"RTSP/1.0 200 OK",true);
                        add_to_buffer(buffer,"Cseq: ",false);
                        if(rtspdata.cseq != NULL)
                            add_to_buffer(buffer,rtspdata.cseq,true);
                        else
                            add_to_buffer(buffer,"CSEQ_IS_MISSING",true);
                        add_to_buffer(buffer,"Session: 26101992;timeout=5",true);
                        add_to_buffer(buffer,"\r\n",false);
                        printf("about to send:\n%s",buffer);
                        if((snd = send(acc,buffer,strlen(buffer),0))<0)
                                err_exit("send error\n");
                        write(fd[1],"5",2);//write on pipe
                    break;
                    case TEARDOWN:
                        write(fd[1],"7",2);
                    break;
                    }//switch/case
                }//decide what to do
                /*********************************************
                 RTP
                 ***************************************/
                if(rtp_pid==0){/**RTP loop*/
                    bzero(rtp_in,sizeof(rtp_in));
                    int nbytes,old_command=-1;
                    close(fd[1]);//close output
                    /**UDP SOCKET*/
                    int dataSocket = socket (AF_INET, SOCK_DGRAM, 0);
                    if(dataSocket<0) {printf("error socket creation\n");exit(-1);}
                    int reuse=1;
                    setsockopt(dataSocket,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

                    struct sockaddr_in dataClient_addr;
                    memset((char*)&dataClient_addr,0,sizeof(dataClient_addr));
                    dataClient_addr.sin_family=AF_INET;

                    int dataPort=portno+10;
                    dataClient_addr.sin_port=htons(dataPort);
                    dataClient_addr.sin_addr.s_addr=INADDR_ANY;
                    socklen_t clientSize = sizeof(dataClient_addr);

                    for(;;){
                        if((nbytes=read(fd[0],&rtp_in,sizeof(rtp_in)))<0){//read from the pipe
                            err_exit("Read error\n");
                            exit(0);}
                            /**TODO ***************************************
                            get the ffmpeg input file* imagebuffer=popen("ffmpeg command",r);
                            ******************************************************/
                            printf("RTP: %d\n",atoi(rtp_in));
                            switch(atoi(rtp_in)){//determine command from RTSP pipe
                                case SETUP:


                                    if(old_command!=SETUP){
                                        /**Construct first rtp packet*/
                                        rtp_header.seq=0;
                                        rtp_header.version=2;
                                        rtp_header.p=0;
                                        rtp_header.x=0;
                                        rtp_header.pt=31;
                                        rtp_header.cc=0;
                                        rtp_header.m=0;
                                        rtp_packet.header=rtp_header;
                                        rtp_packet.payload=databuff;
                                        rtp_packet.payload_len=(long)sizeof(rtp_packet.payload);
                                        printf("DATADATADATA\n");
                                        /** Open what we want to send*/
                                        /*int filesize,steps,sentsize,remaining,j,k;
                                        FILE *image=fopen("my_picture.jpeg","r");

                                        int im=1;
                                        im=fread(databuff,5000,1,image);*/
                                        old_command=SETUP;//set old command
                                    }
                                break;
                                case PLAY:
                                    if((snd=sendto(dataSocket,&rtp_packet,sizeof(rtp_packet),0,(struct sockaddr *)&dataClient_addr,clientSize))==-1)
                                        err_exit("send error\n");
                                    old_command=PLAY;
                                    rtp_header.seq++;
                                break;
                                case PAUSE:
                                break;
                                case TEARDOWN:
                                    exit(0);
                                break;
                            }//RTP switch/case

                    }//RTP loop

                }//RTP second child process
            }//for ()
        }//RTSP child process
    }//FDSET activated
} // m a i n ()

