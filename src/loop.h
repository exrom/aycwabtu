#include <stdint.h>
#include <stdbool.h>

#define PROBE_NUM_PACKETS        3
#define PROBE_BYTES_PER_PACKET   16

typedef struct ts_probe_tag
{
   unsigned char packet[PROBE_BYTES_PER_PACKET];
} ts_probe_t[PROBE_NUM_PACKETS];


bool loop_perform_key_search(
   ts_probe_t probedata,
   uint64_t u64Currentkey, 
   uint64_t u64Stopkey, 
   void (*progress_callback)(uint64_t u64ProgressKey, uint64_t u64Stopkey),
   unsigned char *retcw)
;
