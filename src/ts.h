#ifndef TS_H
#define TS_H

#include "dvbcsa.h"


#define PROBE_NUM_PACKETS        3
#define PROBE_BYTES_PER_PACKET   16
/** Contains excerpt from transport stream (three packets with its 16 bytes each) needed for brute force attack.
 */

typedef struct ts_probe_packet_tag
{
   unsigned char packet[PROBE_BYTES_PER_PACKET];
} ts_probe_packet_t;

typedef struct ts_probe2_tag
{
   ts_probe_packet_t probe[PROBE_NUM_PACKETS];
} ts_probe2_t;



/** read transport stream file and get three data blocks back to mount the brute force attack on
@tsfile         filename string
@probedata      array [3][16]
@probeparity    parity needed to write the cwl
*/
unsigned char ts_read_file(unsigned char *tsfile, unsigned char  *probedata, int *probeparity);

void ts_generate_probe_data(ts_probe2_t *data, const dvbcsa_cw_t cw);

#endif
