/*! \file */

#ifndef _CUCKOOHASH_CONFIG_H
#define _CUCKOOHASH_CONFIG_H

//! SLOT_PER_BUCKET is the maximum number of keys per bucket
const size_t SLOT_PER_BUCKET = 8;

//! DEFAULT_SIZE is the default number of elements in an empty hash
//! table
const size_t DEFAULT_SIZE = (1U << 16) * SLOT_PER_BUCKET;

//! set LIBCUCKOO_DEBUG to 1 to enable debug output
#define LIBCUCKOO_DEBUG 0

#endif
