
/* rtsputils.c
 * saksaj1 2014,2015
 * Utilities for RTSP and RTP server
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "rtsputils.h"
#include "rtspconsts.h"

/*
 * start_ffmpeg -- start the FFmpeg program in its own process.
 * 2015-01-20 NOT USED, see module ffmpegexec.c
 */
int start_ffmpeg(void)
{
  int epid;
  char *arg[] = { "ffmpeg",
		  "-f",
		  "x11grab",
		  "-i",
		  ":0.0",
		  "-f",
		  "h264",
		  "myfifo",
		  "-y",
		  (char*)0 }; // argument list for FFmpeg
  char *env[] = { 0 }; // environment variable list

  epid = fork();
  if (epid < 0)
    {
      perror("fork failed");
      return (-1);
    }
  else if (epid == 0)
    {
      sleep(3); // 3 seconds delay to let the main process start
      execve("/usr/bin/ffmpeg", arg, env);
      perror("execve() failed");     // execve() never returns if OK!
      return (-2);
    }
  else
    return 0;
}

size_t min(size_t a, size_t b)
{
  if (a < b)
    return a;
  else
    return b;
}

/*
 * print_hex -- print a binary buffer in hexadecimal.
 * Parameters:
 * pbuf: the buffer to print
 * psize: its length
 * plf: position of linefeeds (every plf:th character)
 * If plf is zero, do not print linefeeds.
 */

void print_hex(char* pbuf, size_t psize, size_t plf)
{
  size_t idx;
  if (!pbuf || psize == 0)
    return;
  for (idx = 1; idx < psize + 1; idx++)
    {
      printf("%02X", pbuf[idx-1]);
      if (plf > 0 && idx % plf == 0)
	printf("\n");
    }
  if(idx % plf != 0) // last newline if needed
    printf("\n");
}
void err_exit(const char *msg)
{
  perror(msg);
  exit(-1);
}
/*
 * buffer_has_binary -- check for nonprintable content
 * Regard the contents of a buffer to be binary if there are
 * byte values less than 0x20 (ASCII SPACE) and not CR or LF
 * Null is not considered binary because a string can contain
 * null characters in its end and text in the beginning.
 * Return 1 if any byte is not printable ASCII
 * Return 0 if contains only printable ASCII
 * Return -1 if an error occurred in the call
 */
int buffer_has_binary(char* b, size_t n)
{
  size_t ix;
  if (b != NULL)
    {
      for (ix = 0; ix < n; ix++)
	{
	  if (b[ix] > 0 && b[ix] < 0x20 &&
	      b[ix] != 0x0a && b[ix] != 0x0d)
	    return 1;
	}
      return 0;
    }
  else
    return -1;
}
