#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "dvbcsa.h"


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

uint64_t getKey(const uint8_t cw[])
{
      uint64_t key =
            cw[0] << 40 +
            cw[1] << 32 +
            cw[2] << 24 +
            cw[4] << 16 +
            cw[5] << 8 +
            cw[6];

      return key;
}

void getCw(const uint64_t key, uint8_t cw[])
{
      cw[0] = (key) >> 40;
      cw[1] = (key) >> 32;
      cw[2] = (key) >> 24;
      cw[3] = cw[0] + cw[1] + cw[2];
      cw[4] = (key) >> 16;
      cw[5] = (key) >> 8;
      cw[6] = (key);
      cw[7] = cw[4] + cw[5] + cw[6];
}
