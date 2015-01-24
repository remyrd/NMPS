#ifndef RTSPANALYZE
#define RTSPANALYZE
#include <netinet/in.h>
/*
 * Our numeric presentation of the RTSP methods, in the order of the RFC
 */
#define OPTIONS 1
#define DESCRIBE 2
#define ANNOUNCE 3
#define SETUP 4
#define PLAY 5
#define PAUSE 6
#define TEARDOWN 7
#define GET_PARAMETER 8
#define SET_PARAMETER 9
#define REDIRECT 10
#define RECORD 11

/*
 * The RTSP protocol header structure.
 * String length calculation is left for the user of the struct.
 * The headers end with a newline, so it will be easy to measure.
 * Header pointers are NULL until they are found in analysis.
 */
typedef struct rtspblock {
  int method;            // The method, as an integer (see above)
  char* url;             // The URL after the method (add string length)
  char* accept;          // Accept:
  char* conn;            // Connection:
  char* contenc;         // Content-Encoding:
  char* contlang;        // Content-Language:
  char* contlen;         // Content-Length:
  char* conttype;        // Content-Type:
  char* cseq;            // CSeq:
  char* proxyrequire;    // Proxy-Require:
  char* require;         // Require:
  char* rtpinfo;         // RTP-Info:
  char* session;         // Session:
  char* transport;       // Transport:
  char* unsupp;          // Unsupported:
} Rtspblock;

typedef struct rtpheader{
    unsigned int version:2;
    unsigned int p:1;
    unsigned int x:1;
    unsigned int cc:4;
    unsigned int m:1;
    unsigned int pt:7;

    unsigned int seq:16;
    unsigned int timestamp:32;
    unsigned int ssrc:32;
    unsigned int csrc:32;
} Rtp_header;

typedef struct rtppacket
{
  Rtp_header header;
  char* payload;
  uint32_t payload_len;
} Rtp_packet;
/*
 * Function prototypes
 */
int rtspanalyze(char*,Rtspblock*);

#endif

