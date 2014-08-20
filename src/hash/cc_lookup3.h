#ifndef _CC_LOOKUP3_H_
#define _CC_LOOKUP3_H_

/*
Excerpt and modified from lookup3.c (http://burtleburtle.net/bob/c/lookup3.c),
originally by Bob Jenkins, May 2006, Public Domain.
*/

#include <stdint.h>     /* defines uint32_t etc */

#include <cc_define.h>

uint32_t hashlittle( const void *key, size_t length, uint32_t initval);

#endif
