#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>


void print_key(uint64_t u64key, bool boLineFeed)
{
      printf("%02X %02X %02X [] %02X %02X %02X [] ",
      (uint8_t)(u64key >> 40),
      (uint8_t)(u64key >> 32),
      (uint8_t)(u64key >> 24),
      (uint8_t)(u64key >> 16),
      (uint8_t)(u64key >> 8),
      (uint8_t)(u64key >> 0));

      if (boLineFeed) {printf("\n");}
}

void print_cw(uint8_t u8data[])
{
      printf("%02X %02X %02X [%02X] %02X %02X %02X [%02X] \n",
      u8data[0],
      u8data[1],
      u8data[2],
      u8data[3],
      u8data[4],
      u8data[5],
      u8data[6],
      u8data[7]);
}
