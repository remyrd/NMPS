/* NMPS screencaster fd communications (pipe and FIFO file descriptor)
 * fdcomm.c
 * 2015-01-09 saksaj1 open, read, write flags changed (works now?)
 */
#include <stdio.h>

// memset(3)
#include <string.h>

// open(2), mkfifo(3), getpid():
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//  read(2), write(2), getpid();
#include <unistd.h>

// assert(3)
#include <assert.h>

// errno
#include <errno.h>
#include "rtspconsts.h"

/* create_fifo
 *  Creates a named pipe (FIFO) if it does not exist
 */
int create_fifo(int pcli)
{
  char fifoname[MAX_NAMELEN];

  assert(pcli > 0);
  assert(pcli <= MAX_CLIENTS);

  memset(fifoname, 0, MAX_NAMELEN);
  snprintf(fifoname, MAX_NAMELEN - 1, "client%d.fifo",
           pcli);
  if (mkfifo(fifoname,
             S_IRUSR |
             S_IWUSR |
             S_IRGRP |
             S_IWGRP |
             S_IROTH |
             S_IWOTH) == -1)
    {
      if (errno == EEXIST)
        {
          fprintf(stderr,
                  "Note: create_fifos:"
                  " FIFO exists already: %s\n", fifoname);
        }
      else
        {
          fprintf(stderr,
                  "Error: create_fifos failed to create FIFO %s\n",
                  fifoname);
          return -1;
        }
    }
  return 0;
}

/* open_fd
 * Open the given client's fd (FIFO) with the given flags and mode.
 * If successful, the fd is returned through pointer pfd and
 * the function will return 0. In case of blocking, 1 will be
 * returned, -1 in case of an error.
 */
int open_fd(int pcli, int pflags, mode_t pmode, int *pfd)
{
  int fd;
  char fn[MAX_NAMELEN];
  //  pid_t mypid = getpid();

  assert(pcli > 0);
  assert(pcli <= MAX_CLIENTS);
  assert(pfd);

  memset(fn, 0, MAX_NAMELEN);
  snprintf(fn, MAX_NAMELEN - 1,
           "client%d.fifo", pcli);
  fprintf(stderr, "open_fd: try to open %s\n", fn);
 open_fd_try:
  fd = open(fn, pflags, pmode);
  if (fd == -1)
    {
      if (errno == EINTR)
        goto open_fd_try;
      else if (errno == EWOULDBLOCK)
        {
          fprintf(stderr, "open_fd: open of %s would block\n", fn);
          return 1;
        }
      else
        {
          fprintf(stderr, "Error: open_fd: opening of %s failed: %s(%d)\n",
		  fn,strerror(errno),errno);
          return -1;
        }
    }
  else
    {
      *pfd = fd;
      fprintf(stderr, "open_fd: open of %s OK, fd is %d\n", fn, *pfd);
      return 0;
    }
}

/*
 * close_fd
 *   closes the given (FIFO) fd
 */
int close_fd(int pfd)
{
 close_fd_try:
  if (close(pfd) == -1)
    {
      if (errno == EINTR)
        goto close_fd_try;
      else
        {
          fprintf(stderr,
                  "Error: close_fd closing of pfd %d failed: %s(%d)\n",
                  pfd, strerror(errno), errno);
          return -1;
        }
    }
  return 0;
}

/*
 * write_fd
 *   writes the given buffer to the given fd (FIFO or pipe)
 *   returns 0 if OK, -1 if not OK
 *   or 1 if write would block
 */
int write_fd(int pfd, char* pbuf, ssize_t psiz)
{
  ssize_t nfd;
  int retval = 0;
  pid_t mypid = getpid();
 write_fd_try:
  nfd = write(pfd, pbuf, psiz);
  if (nfd == psiz)
    {/*
      fprintf(stderr,
              "write_fd(%zu): Wrote OK to %d\n",
              mypid, pfd);
     */
      retval = 0;
    }
  else if (nfd == -1)
    {
      if (errno == EINTR)
        goto write_fd_try;
      else if (errno == EWOULDBLOCK)
        {
          fprintf(stderr,
                  "write_fd(%zu): %d would block\n",
                  mypid, pfd);
          retval = 1;
        }
      else
        {
          fprintf(stderr,
                  "Error: write_fd: %d error %s\n",
                  pfd, strerror(errno));
          retval = -1;
        }
    }
  else
    {
      fprintf(stderr,
              "Error: write_fd: Wrote different amount (%zu of %zu) to %d\n",
              nfd, psiz, pfd);
      retval = -1;
    }
  return retval;
}

/*
 * read_fd -- read from a FIFO or pipe
 * Data is read from fd to pbuf at max pbufsize
 * If reading fails, -1 will be returned
 * If OK, 0 will be returned. Return value 1
 * indicates that the reading would block if
 * the fd was opened with O_NONBLOCK. Return
 * value 2 indicates that the FIFO or pipe
 * read returned 0 bytes (other end closed).
 * The amount of data successfully read is
 * returned through the pointer prd.
 */
int read_fd(int pfd, char* pbuf, ssize_t pbufsize, ssize_t *prd)
{
  ssize_t res;
  pid_t mypid = getpid();
  assert(pbuf);
 read_fd_try:  
  res = read(pfd, pbuf, pbufsize);
  if (res > 0)
    {
      /*      
      fprintf(stderr,"read_fd(%zu): read %zu from %d\n",mypid,res,pfd);
      */
      *prd = res;
    }
  else if (res == -1)
    {
      if (errno == EINTR)
        goto read_fd_try;
      else if (errno == EWOULDBLOCK)
        {
	  /*
          fprintf(stderr,"read_fd(%zu): reading would block\n", mypid);
	  */
          return 1;
        }
      else
        {
          fprintf(stderr,"Error: read_fd(%zu): reading failed: %s(%d)\n",
                  mypid, strerror(errno), errno);
          return -1;
        }
    }
  else
    {
      return 2;
    }
  return 0;
}

