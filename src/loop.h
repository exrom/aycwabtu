#ifndef LOOP_h
#define LOOP_h

#include <stdint.h>
#include <stdbool.h>
#include "ts.h"


/**
 * Outer loop for key search.
 * Function is single-threaded, blocking and returns to caller if either key was found or end of search range was reached.
 * While running, the progress callback is executed on each outer loop increment.
 * 
 * @param probedata        array[3][16] - three packets with first 16 bytes each
 * @param u64Currentkey    
 */
bool loop_perform_key_search(
   ts_probe_t probedata,
   uint64_t u64Currentkey, 
   uint64_t u64Stopkey, 
   void (*progress_callback)(uint64_t u64ProgressKey, uint64_t u64Stopkey),
   unsigned char *retcw)
;

#endif
