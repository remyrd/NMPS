#!/bin/sh
gcc -std=c99 -Wall -Wextra -pedantic -g -D_GNU_SOURCE -fno-diagnostics-show-caret -o a.out singlemain.c ffmpegexec.c singlertsp.c singlertp.c fdcomm.c rtsputils.c rtspanalyze.c
