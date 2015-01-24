#!/bin/sh
gcc -std=c99 -Wall -Wextra -pedantic -g -D_GNU_SOURCE -fno-diagnostics-show-caret -o newserv newrtspserver.c rtspsession.c rtsputils.c rtspanalyze.c rtpstreamer.c videoswitcher.c ffmpegexec.c fdcomm.c

