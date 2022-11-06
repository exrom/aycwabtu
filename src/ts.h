/* read transport stream file and get three data blocks back to mount the brute force attack on
@tsfile         filename string
@probedata      array [3][16]
@probeparity    parity needed to write the cwl
*/
unsigned char ayc_read_ts(unsigned char *tsfile, unsigned char *probedata, int *probeparity);
