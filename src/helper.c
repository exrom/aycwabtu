#include <stdint.h>
#include <stdio.h>


void print_key(uint64_t u64key)
{
      printf("%02X %02X %02X [] %02X %02X %02X []\n",
      (uint8_t)(u64key >> 40),
      (uint8_t)(u64key >> 32),
      (uint8_t)(u64key >> 24),
      (uint8_t)(u64key >> 16),
      (uint8_t)(u64key >> 8),
      (uint8_t)(u64key >> 0));

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
