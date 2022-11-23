#include <stdint.h>

void print_key(uint64_t u64key, bool boLineFeed);
void print_cw(uint8_t u8data[]);

uint64_t getKey(const uint8_t cw[]);
void getCw(const uint64_t key, uint8_t cw[]);
