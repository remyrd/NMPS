/*
 * videoswitcher.c
 * The FFmpeg output switching process for the NMPS assignment 2014 
 * FFmpeg executes in an own process and outputs to pipe:1 i.e. stdout.
 * stdout from FFmpeg is dup2'ed and read by the videoswitcher.
 * It then distributes FFmpeg output to FIFOs that use standard names.
 *
 * 2015-01-05 saksaj1 New structure using fdcomm -functions
 * 2015-01-09 saksaj1 open, read, write flags changed (works now?)
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "rtspconsts.h"
#include "fdcomm.h"
#include "ffmpegexec.h"
#include "videoswitcher.h"

int read_cmd(int p, int* prc);

/* videoswitcher -- start and read from FFmpeg and send it to
 * clients. Client numbers are received from the pipe 'pc'. To be started
 * in a child process of the screencaster server that sends the add/remove
 * commands via the pipe. Infinite loop (does not return unless error).
 * Zero in the fifolist array indicates an unused FIFO.
 * Parameters:
 *  pc: pipe read end from parent for receiving client numbers
 * Returns:
 *  0 if exited without problems (e.g. Ctrl-C interrupt of FFmpeg)
 * -1 in case of an error
 */
int videoswitcher(int pc)
{

  char videobuf[VIDEOBUF_LEN];
  int fifolist[MAX_CLIENTS];
  int videofd[2];
  int switcherpid;
  int fdop, status;
  int ix = 0;
  ssize_t ncmd, readgot;

  if (pipe(videofd))
    {
      perror("pipe");
      exit(-1);
    }

  switcherpid = fork();
  if (switcherpid == 0)
    {
      if (close_fd(videofd[0]) == -1)
        exit(-1);
      if (ffmpegexec(videofd[1]) == -1)
        {
          fprintf(stderr, "Error: videoswitcher: ffmpegexec failed\n");
          exit(-1);
        }
    }
  else if (switcherpid > 0)
    {
      if (close_fd(videofd[1]) == -1)
        exit(-1);
      // Initialize the FIFO list with an inactive (-1) indicator
      for (ix = 0; ix < MAX_CLIENTS; ix++)
        {
          fifolist[ix] = -1;
        }

      fprintf(stderr,
              "DEBUG(videoswitcher:%zu): Loop will start\n",
              switcherpid); // The switching loop
      while (1)
        {
          // Check for added (+) or removed (-) clients
          fdop = read_cmd(pc, &ncmd);
          if (fdop == 0)
            {
              if (ncmd > 0)
                {
                  fprintf(stderr,
                          "DEBUG(videoswitcher:%zu):"
                          " received %d from pipe %d\n",
                          switcherpid, ncmd, pc);
                  // fdop = open_fd(ncmd, O_RDWR|O_NONBLOCK, 0644, &fifolist[ncmd - 1]);
                  fdop = open_fd(ncmd, O_WRONLY, 0644, &fifolist[ncmd - 1]);
                  if (fdop == -1)
                    {
                      fprintf(stderr,
                              "Error: videoswitcher: fifo %d open failed (%d)\n",
                              ncmd, fdop);
                      exit(-1);
                    }
                  else if (fdop == 0)
                    {
                      fprintf(stderr, "DEBUG(videoswitcher:%zu):"
                             " fifo %d open OK\n",
                             switcherpid, ncmd);
                    }
                  else
                    fprintf(stderr, "DEBUG(videoswitcher:%zu): open_fd returned %d\n",
                            switcherpid, fdop);
                }
              else if (ncmd < 0)
                {
                  ncmd = abs(ncmd);
                  close(fifolist[ncmd - 1]);
                  fifolist[ncmd - 1] = -1;
                }
            }
          else if (fdop == -1)
            {
              fprintf(stderr, "Error: videoswitcher: pipe failed\n");
              exit(-1);
            } // note: fdop == 1 just skips this stage

          // Distribute video to client FIFOs
          fdop = read_fd(videofd[0], videobuf, VIDEOBUF_LEN, &readgot);
          if (fdop == 0)
            {
              for(ix = 0; ix < MAX_CLIENTS - 1; ix++)
                if (fifolist[ix] != -1)
                  {
                    fdop = write_fd(fifolist[ix], videobuf, readgot);
                    if (fdop)
                      fprintf(stderr,
                              "Error: videoswitcher: write to client%d.fifo\n",
                              ix + 1);
                  }
            }
          else
            {
              fprintf(stderr, "Error: videoswitcher: pipe failed\n");
              exit(-1);
            }
        }
      while (wait(&status))
        sleep(1);
    } // parent
  else
    return -1;
  return 0;
}

int read_cmd(int p, int* prc)
{
  char cmdstr[MAX_CMDLEN];
  ssize_t rc;
  int apu;
  pid_t mypid = getpid();
 read_cmd_try:
  rc = read(p, cmdstr, MAX_CMDLEN - 1);
  if (rc > 0)
    {
      cmdstr[rc] = '\0';
      fprintf(stderr, "DEBUG(read_cmd:%zu): cmd pipe read OK\n",
              mypid);
      apu = atoi(cmdstr);
      if (abs(apu) == 0 || abs(apu) > MAX_CLIENTS)
        {
          fprintf(stderr, "Error: read_cmd: received bad number: %d\n",
                  apu);
          return -1;
        }
      *prc = apu;
      fprintf(stderr, "DEBUG(read_cmd:%zu): got client number %d\n",
              mypid, *prc);
      return 0;
    }
  else if (rc == -1)
    {
      if (errno == EINTR)
        goto read_cmd_try;
      else
        {
          if (errno == EWOULDBLOCK)
            {
              fprintf(stderr, "DEBUG(read_cmd:%zu): "
                      "cmd pipe read would block\n",
                      mypid);
              return 1;
            }
          else
            {
              fprintf(stderr, "Error: read_cmd: "
                      "cmd pipe read failed: %s(%d)\n",
                      strerror(errno), errno);
              return -1;
            }
        }
    }
  else
    {
      fprintf(stderr, "Error: read_cmd: cmd pipe read zero\n");
      return -1;
    }
  return 0;
}
//----------------------------------------------------------------------------
