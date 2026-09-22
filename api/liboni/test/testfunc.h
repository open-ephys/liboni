#ifndef __ONI_TESTFUNC_H__
#define __ONI_TESTFUNC_H__

#include <stdint.h>
#include <stdlib.h>

int cobs_stuff(uint8_t *dst, const uint8_t *src, size_t size);
double randn(double mu, double sigma);

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>

// Windows stdlib does not have a usleep()
// https://www.c-plusplus.net/forum/topic/109539/usleep-unter-window
void usleep(__int64 usec);

// Windows stdlib does not have a getline()
size_t getline(char **lineptr, size_t *n, FILE *stream);

#else
#include <time.h>

typedef struct timespec timespec_t;
timespec_t timediff(timespec_t start, timespec_t end);

#endif

#endif
