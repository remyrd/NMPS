/* rtsputils.h
 * modified: saksaj1 2014,2015
 * Utilities for RTSP and RTP server
 */
#ifndef RTSPUTILS
#define RTSPUTILS
#include <stddef.h>
int start_ffmpeg(void);
size_t min(size_t a, size_t b);
void print_hex(char* pbuf, size_t psize, size_t plf);
void err_exit(const char *msg);
int buffer_has_binary(char*,size_t);
/* void add_to_buffer(char *dest, const char *src,bool endline); */
#endif
