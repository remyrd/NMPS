#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "rtspconsts.h"
#include "ffmpegexec.h"

int ffmpegexec(int pvid)
{
  char *arg[ARG_LEN];  // execve arguments
  char *env[] = { 0 }; // environment variable list
  int duplo;
  pid_t ffpid = getpid();

 dup2_child:
  duplo = dup2(pvid, STDOUT_FILENO); // redirect stdout to pipe
  if (duplo == -1)
    {
      if (errno == EINTR)
        goto dup2_child;
      else
        return -1;
    }
  close(pvid); // no longer used because of dup2

  for (int mx = 0; mx < ARG_COUNT; mx++)
    {
      arg[mx] = malloc(sizeof(char) * ARG_LEN);
      if (arg[mx] == NULL)
        {
          return -1;
        }
    }
  strcpy(arg[0],"ffmpeg");
  strcpy(arg[1],"-v");
  strcpy(arg[2],"quiet"); // do not want printouts
  strcpy(arg[3],"-f");
  strcpy(arg[4],"x11grab");
  strcpy(arg[5],"-i");
  strcpy(arg[6],":0.0");
  strcpy(arg[7],"-f");
  strcpy(arg[8],"h264");
  strcpy(arg[9],"-y"); // maybe not needed with stdout
  strcpy(arg[10],"pipe:1"); // pipe to stdout
  arg[11] = (char*)NULL;    // list terminator
  fprintf(stderr, "FFmpeg will start in process %zu\n", ffpid);
  execve("/usr/bin/ffmpeg", arg, env);
  perror("execve() failed");
  exit(-1);
}
