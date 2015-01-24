/* NMPS screencaster
 * 2015-01-09 saksaj1 size of data read added to read_fd()
 */
#ifndef NMPS_FDCOMM
#define NMPS_FDCOMM
int create_fifo(int);
int open_fd(int,int,mode_t,int*);
int close_fd(int);
int write_fd(int,char*,size_t);
int read_fd(int,char*,ssize_t,ssize_t*);
#endif
