#ifndef UTIL_H
#define UTIL_H

#include <pthread.h>
#include "cuckoohash_config.h" // for LIBCUCKOO_DEBUG

#if LIBCUCKOO_DEBUG
#  define LIBCUCKOO_DBG(fmt, args...)  fprintf(stderr, "\x1b[32m""[libcuckoo:%s:%d:%lu] " fmt"" "\x1b[0m",__FILE__,__LINE__, (unsigned long)pthread_self(), ##args)
#else
#  define LIBCUCKOO_DBG(fmt, args...)  do {} while (0)
#endif

#endif
