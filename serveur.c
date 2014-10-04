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

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void add_to_buffer(char *dest,char *src,bool endline){
    strcat(dest,src);
    if (endline) strcat(dest,"\r\n");
}

int main(int argc,char* argv[]){

    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[5000];
    /**rtsp analyze variables**/
    int result;
    Rtspblock rtspdata;
    char *buf;
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
       error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
             error("ERROR on binding");
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
            /*if((rcv = recvfrom(acc,buffer,sizeof(buffer),0,(struct sockaddr *)&cli_addr,&clilen))<0) error("rcv error\n");
            printf("%s\n",buffer);
            printf("BUFFER END\n\n");//OPTIONS
            if((snd = send(acc,buffer,sizeof(buffer),0))<0) error("send error\n");
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
                    if((rcv = recvfrom(acc,buffer,sizeof(buffer),0,(struct sockaddr *)&cli_addr,&clilen))<0) error("rcv error\n");
                    /** Determine the instruction*/
                    strcpy(buf,buffer);
                    printf("received: \n%s\n",buf);//receive buffer, copy into buf... great variable names!!
                    if((result=rtspanalyze(buf,&rtspdata))!=0) error("rtspanalyze error!!\n");
                    else {
                    /**DECIDE WHAT TO DO*/
                        switch(rtspdata.method){
                            case OPTIONS:
                                bzero(buffer,sizeof(buffer));
                                add_to_buffer(buffer,"RTSP/1.0 200 OK",true);
                                add_to_buffer(buffer,"Cseq: ",false);
                                add_to_buffer(buffer,rtspdata.cseq,true);
                                add_to_buffer(buffer,"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n",true);
                                printf("about to send:\n%s",buffer);
                                if((snd = send(acc,buffer,sizeof(buffer),0))<0) error("send error\n");
                                /*Response    =     Status-Line
                                             *(    general-header
                                             |     response-header
                                             |     entity-header )
                                                   CRLF
                                                   [ message-body ]

                                Status-Line =   RTSP-Version SP Status-Code SP Reason-Phrase CRLF
                                break;*/
                            case DESCRIBE:
                                break;
                            case PLAY:
                                break;
                        }
                    bzero(buf,sizeof(buf));
                    bzero(buffer,sizeof(buffer));
                    }
                }

			}
		//}
	}

}
