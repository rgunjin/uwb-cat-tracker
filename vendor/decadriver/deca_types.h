/*! ----------------------------------------------------------------------------
 *  @file   deca_types.h
 *  @brief  Decawave general type definitions
 *
 * @attention
 *
 * Copyright 2013 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * Local change: rewritten on top of <stdint.h>. The original defined
 * uint32 as unsigned long, which is 64-bit on a host build.
 */

#ifndef _DECA_TYPES_H_
#define _DECA_TYPES_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;

#ifdef __cplusplus
}
#endif

#endif /* _DECA_TYPES_H_ */
