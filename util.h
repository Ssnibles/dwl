/* See LICENSE.dwm file for copyright and license details. */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
int fd_set_nonblock(int fd);

#endif /* UTIL_H */
