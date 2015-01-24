
/* rtsputils.c
 * saksaj1 2014
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
      printf("%02x", pbuf[idx-1]);
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

/**Add string to buffer   MOVED to the module that uses it
 * and modified to use int instead of bool

   bool endline adds an extra "\r\n" in the end

void add_to_buffer(char *dest, const char *src,bool endline)
{
  strcat(dest, src);
  if (endline) strcat(dest, "\r\n");
}
*/
