#ifndef RTSPCONSTS
#define RTSPCONSTS

/* S-38.3152 NMPS screencaster (R.Rojas/J.Saksa)
 * rtspconsts.h
 * Common constants
 * 2015-01-16 Merged here the consts.h from experiments
 */
#ifdef FALSE
#error "FALSE already defined"
#else
#define FALSE 0
#endif
#ifdef TRUE
#error "TRUE already defined"
#else
#define TRUE 1
#endif

#define ARG_COUNT 12
#define ARG_LEN 20
#define CMDSTRLEN 16
#define COMMBUFSIZE 1024
#define COMMLINESIZE 80
#define MAX_CLIENTS 4
#define MAX_CMDLEN 4
#define MAX_NAMELEN 256
#define RTP_PACKET_SIZE 65536
#define RTP_PAYLOAD_SIZE 65536
#define VIDEOBUFSIZE 1024
#define VIDEOBUF_LEN 1024
#endif
