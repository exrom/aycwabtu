#ifndef LOOP_h
#define LOOP_h

#include <stdint.h>
#include <stdbool.h>
#include "ts.h"


bool loop_perform_key_search(
   ts_probe_t probedata,
   uint64_t u64Currentkey, 
   uint64_t u64Stopkey, 
   void (*progress_callback)(uint64_t u64ProgressKey, uint64_t u64Stopkey),
   unsigned char *retcw)
;

#endif
