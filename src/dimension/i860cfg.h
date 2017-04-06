//
//  i860cfg.h
//  Previous
//
//  Created by Simon Schubiger on 14/01/16.
//
//

#ifndef i860cfg_h
#define i860cfg_h

#include "configuration.h"

/* Emulator configurations - keep in sync with i860_cpu_device::init() */

#define CONF_I860_SPEED     1
#define CONF_I860_DEV       2
#define CONF_I860_NO_THREAD 3
#define CONF_STR(CONF) #CONF

#if ENABLE_TESTING
#define CONF_I860 CONF_I860_DEV
#else
#define CONF_I860 CONF_I860_SPEED
#endif

/* Emulator configurations */

#if CONF_I860==CONF_I860_DEV
#define TRACE_RDWR_MEM       0
#define TRACE_PAGE_FAULT     0
#define TRACE_UNDEFINED_I860 1
#define TRACE_UNALIGNED_MEM  1
#define TRACE_EXT_INT        0
#define ENABLE_I860_DB_BREAK 0
#define ENABLE_PERF_COUNTERS 1
#define ENABLE_DEBUGGER      1

#elif CONF_I860==CONF_I860_SPEED
#define TRACE_RDWR_MEM       0
#define TRACE_PAGE_FAULT     0
#define TRACE_UNDEFINED_I860 0
#define TRACE_UNALIGNED_MEM  0
#define TRACE_EXT_INT        0
#define ENABLE_I860_DB_BREAK 0
#define ENABLE_PERF_COUNTERS 0
#define ENABLE_DEBUGGER      0


#elif CONF_I860==CONF_I860_NO_THREAD
#define TRACE_RDWR_MEM       0
#define TRACE_PAGE_FAULT     0
#define TRACE_UNDEFINED_I860 0
#define TRACE_UNALIGNED_MEM  0
#define TRACE_EXT_INT        0
#define ENABLE_I860_DB_BREAK 0
#define ENABLE_PERF_COUNTERS 0
#define ENABLE_DEBUGGER      0

#endif

#endif /* i860cfg_h */
