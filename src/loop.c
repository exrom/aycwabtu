#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "loop.h"
#include "config.h"
#include "bs_stream.h"
#include "bs_block.h"
#include "bs_block_ab.h"
#include "bs_algo.h"
#include "dvbcsa.h"


/****************** definitions ***********************/
/* key bits calculated in inner bs loop - do not change */
#define INNERKEYBITS    16
/* number of keys calculated in inner bs loop - do not change */
#define KEYSPERINNERLOOP (1<<INNERKEYBITS)



/*
control word iteration strategy:

        +---+---+-------+-------------  u64Currentkey
        |   |   |       |
        |   |   |       |   +---------  INNERBATCH
        |   |   |       |   |   
        |   |   |       |   |   +-----  BS_BATCH_SIZE
        |   |   |       |   |   |
        |   |   |       |   |   |
        |   |   |       |   |   |
        |   |   |       |   |   |
byte    00  11  22 [33] 44  55  66 [77]


BS_BATCH_SIZE:
number of keys processed in parallel bitslice manner
by one thread at at time.
== 32 bits for uint32
== 128 bits for sse4_2
                    
INNERBATCH:
inner loop: number of executed threads
thread 0 starts at offset 0
thread 1 starts at offset BS_BATCH_SIZE
thread 2 starts at offset 2*BS_BATCH_SIZE
...
thread INNERBATCH starts at offset INNERBATCH*BS_BATCH_SIZE
                    
                    
u64Currentkey:
outer loop, always 32 bit, 
incremented after all threads are finished


*/

bool loop_perform_key_search(
   ts_probe_t probedata,
   uint64_t u64Currentkey, 
   uint64_t u64Stopkey, 
   void (*progress_callback)(uint64_t u64ProgressKey, uint64_t u64Stopkey),
   unsigned char *retcw)
{
   int      i, k;

   /************** stream ***************/
   dvbcsa_bs_word_t     bs_data_sb0[8 * 16];    // constant scrambled data blocks SB0 + SB1, global init for stream, da_diett.pdf 5.1
   dvbcsa_bs_word_t     bs_data_ib0[8 * 16];    // IB0 is bit/byte sliced block init vector, ib1 is bit sliced stream output
   /************** block ***************/
   dvbcsa_bs_word_t     keys_bs[64];            // bit sliced keys for block
   dvbcsa_bs_word_t     keyskk[448];            // bit sliced scheduled keys (64 bit -> 448 bit)

#ifdef USEBLOCKVIRTUALSHIFT
   dvbcsa_bs_word_t	r[8 * (1 + 8 + 56)];        // working data block
#else                                           //
   dvbcsa_bs_word_t	r[8 * (1 + 8 + 0)];         //
#endif
   dvbcsa_bs_word_t  candidates;       /* 1 marks a key candidate in the batch */

   uint8_t keylist[BS_BATCH_SIZE][8];     /* the list of keys for the batch run in non-bitsliced form */




   aycw_init_block();

   aycw_init_stream((uint8_t*) probedata, &bs_data_sb0);

   for (i = 0; i < 8 * 8; i++)
   {
      bs_data_ib0[i] = bs_data_sb0[i];
   }
#ifndef USEALLBITSLICE
   aycw_bit2byteslice(bs_data_ib0, 1);
#endif

   /************* outer loop ******************/
   // run over whole key search space
   // key bytes incremented: 0 + 1 + 2 + 4 
   while (u64Currentkey <= u64Stopkey)
   {

      /* bytes 5 + 6 belong to the inner loop
         aycw_bs_increment_keys_inner() increments every slice by one starting byte 5 LSB (bit 40) 
         from byte 6 MSB down the slices spread key ranges.
         example: BS_BATCH_SIZE=32  -> topmost 5 bits of byte 6 (2^5==32) contain different values for batches
         
         batch    byte 5   byte 6
         0        00       00
         1        00       08
         2        00       10
         3        00       18
         .....
         31       00       F8         */
#if BS_BATCH_SIZE>256
#error keylist calculation cannot yet handle BS_BATCH_SIZE>256
#endif
      for (i = 0; i < BS_BATCH_SIZE; i++)
      {
         keylist[i][0] = u64Currentkey >> 24;
         keylist[i][1] = u64Currentkey >> 16;
         keylist[i][2] = u64Currentkey >> 8;
         keylist[i][3] = keylist[i][0] + keylist[i][1] + keylist[i][2];
         keylist[i][4] = u64Currentkey;
         keylist[i][5] = 0;
         keylist[i][6] = (0x0100 >> BS_BATCH_SHIFT)*i;
         keylist[i][7] = keylist[i][4] + keylist[i][5] + keylist[i][6];
      }
/***********************************************************************************************************************/
/***********************************************************************************************************************/
/***********************************************************************************************************************/
      aycw_key_transpose(&keylist[0][0], keys_bs);     // transpose BS_BATCH_SIZE keys into bitsliced form

      // check if all keys were transposed correctly
      aycw_assert_key_transpose(&keylist[0][0], keys_bs);

      // inner loop: process 2^16 keys - see aycw_bs_increment_keys_inner()
      for (k = 0; k < KEYSPERINNERLOOP / BS_BATCH_SIZE; k++)
      {

         /* check if initial (outer) key and subsequent (inner) key batches are correct */
         aycw_assertKeyBatch(keys_bs);

         /************** stream ***************/
         aycw_stream_decrypt(&bs_data_ib0[64], 25, keys_bs, bs_data_sb0);    // 3 bytes required for PES check, 25 bits for some reason

         aycw_assert_stream(&bs_data_ib0[64], 25, keys_bs, bs_data_sb0);     // check if first bytes of IB1 output are correct

#ifndef USEALLBITSLICE
         aycw_bit2byteslice(&bs_data_ib0[64], 1);
#endif

         /************** block ***************/
         for (i = 0; i < 8 * 8; i++)
         {
#ifdef USEBLOCKVIRTUALSHIFT
            r[8 * 56 + i] = bs_data_ib0[i];      // r is the input/output working data for block
#else                                            // restore after each block run
            r[i] = bs_data_ib0[i];               // 
#endif
         }

         /* block schedule key 64 bits -> 448 bits */  /* OPTIMIZEME: only the 16 inner bits in inner loop */
         aycw_block_key_schedule(keys_bs, keyskk);

         /* byte transpose */
#ifndef USEALLBITSLICE
         aycw_bit2byteslice(keyskk, 7);    // 448 scheduled key bits / 64 key bits
#endif

         aycw_block_decrypt(keyskk, r);   // r is the generated block output

         {
/*#ifdef USEALLBITSLICE
            uint8_t dump[8];
            aycw_extractbsdata(r, 0, 64, dump);
            printf("%02x %02x %02x %02x  %02x %02x %02x %02x\n",dump[0],dump[1],dump[2],dump[3],dump[4],dump[5],dump[6],dump[7]);
#else
            printf("%02x %02x %02x %02x  %02x %02x %02x %02x\n",(uint8)BS_EXTLS32(r[8 * 0]),(uint8)BS_EXTLS32(r[8 * 1]),(uint8)BS_EXTLS32(r[8 * 2]),(uint8)BS_EXTLS32(r[8 * 3]),(uint8)BS_EXTLS32(r[8 * 4]),(uint8)BS_EXTLS32(r[8 * 5]),(uint8)BS_EXTLS32(r[8 * 6]),(uint8)BS_EXTLS32(r[8 * 7]));
#endif*/
         }

         /************** block xor stream ***************/
         aycw_bs_xor24(r, r, &bs_data_ib0[64]);

         //for (i = 32; i < 64; i++) r[i] = BS_VAL8(55);   // destroy decrypted bytes 4...7 of DB0 shouldnt matter

         aycw_assert_decrypt_result((uint8_t*) probedata, keylist, r);

         if (aycw_checkPESheader(r, &candidates))  /* OPTIMIZEME: return value should be first possible slice number to let the loop below start right there */
         {
            // candidate keys marked with '1' for the last batch run
            //printf("\n %d key candidate(s) found\n", i);
            for (i = 0; i < BS_BATCH_SIZE; i++)
            {
               dvbcsa_cw_t   cw;
               dvbcsa_key_t  key;
               unsigned char data[16];
               memset(cw, 255, 8);
               if (1 == BS_EXTLS32(BS_AND(BS_SHR(candidates, i), BS_VAL8(01))))
               {
                  // candidate bit set, now extract the key bits
                  aycw_extractbsdata(keys_bs, i, 64, cw);

                  dvbcsa_key_set(cw, &key);

                  memcpy(&data, &probedata[0], 16);
                  dvbcsa_decrypt(&key, data, 16);
                  if (data[0] != 0x00 || data[1] != 0x00 || data[2] != 0x01)
                  {
                     /* bitslice and regular implementations calculated different results - should never happen */
                     printf("\nFatal error: candidate verification failed!\n");
                     printf("last key was: %02X %02X %02X [%02X]  %02X %02X %02X [%02X]\n",
                        cw[0], cw[1], cw[2], cw[3], cw[4], cw[5], cw[6], cw[7]);
                     exit(ERR_FATAL);
                  }

                  memcpy(&data, &probedata[1], 16);
                  dvbcsa_decrypt(&key, data, 16);
                  if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)
                  {
                     memcpy(&data, &probedata[2], 16);
                     dvbcsa_decrypt(&key, data, 16);
                     if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)
                     {
                        memcpy(retcw, cw, 8);
                        return(true);   /* key found and return in retcw */
                     }
                  }
               }
            }
         }

         // set up the next BS_BATCH_SIZE keys
         aycw_bs_increment_keys_inner(keys_bs);

      }  // inner loop

      /***********************************************************************************************************************/
      /***********************************************************************************************************************/
      /***********************************************************************************************************************/
      (*progress_callback)(u64Currentkey, u64Stopkey);

      u64Currentkey++;   // prepare for next 2^16 keys

   };  // while (u64Currentkey <= u64Stopkey)

   return false;     /* nothing found */
}

#if 0 //def USE_MEASURE
aycw_tstRegister     stRegister;
   dvbcsa_bs_word_t	r[8 * (1 + 8 + 56)];        // working data block

   dvbcsa_bs_word_t     bs_128[8 * 16];
   dvbcsa_bs_word_t     bs_64_1[64];
   dvbcsa_bs_word_t     bs_64_2[64];
   dvbcsa_bs_word_t     bs_448[448];
void aycw_partsbench(void)
{
   int i;
   const long int max = 1 << 19;
   long int start, diff;

   printf("performance measurement of all algorithmic parts for %d loops\n", max);

   start = aycw__getTicks_ms();
   for (i = 0; i<max; i++) aycw_stream_decrypt(bs_64_2, 25, bs_64_1, bs_128);
   printf("aycw_stream_decrypt()             %.3fs\n", ((float)aycw__getTicks_ms() - start) / 1000);

   start = aycw__getTicks_ms();
   for (i = 0; i<max; i++) aycw__vInitShiftRegister(bs_64_1, &stRegister);
   printf("  aycw__vInitShiftRegister()      %.3fs\n", ((float)aycw__getTicks_ms() - start) / 1000);

   start = aycw__getTicks_ms();
#ifndef USEALLBITSLICE
   for (i = 0; i<max; i++) aycw_bit2byteslice(bs_448, 7);
#endif
   printf("aycw_bit2byteslice(7)             %.3fs\n", ((float)aycw__getTicks_ms() - start) / 1000);

   start = aycw__getTicks_ms();
   for (i = 0; i<max; i++) aycw_block_key_schedule(bs_64_1, bs_448);
   printf("aycw_block_key_schedule           %.3fs\n", ((float)aycw__getTicks_ms() - start) / 1000);

   start = aycw__getTicks_ms();
   for (i = 0; i<max; i++) aycw_block_decrypt(bs_448, r);
   printf("aycw_block_decrypt                %.3fs\n", ((float)aycw__getTicks_ms() - start) / 1000);

   start = aycw__getTicks_ms();
#ifdef USEALLBITSLICE
   for (i = 0; i<56 * max; i++) aycw_block_sbox(r, bs_448);
#else
   for (i = 0; i<56 * max; i++) aycw_block_sbox(r, bs_448);
#endif
   printf("  aycw_block_sbox  (56x)          %.3fs\n", ((float)aycw__getTicks_ms() - start) / 1000);

   start = aycw__getTicks_ms();
   for (i = 0; i<max; i++) aycw_checkPESheader(r, bs_64_1);
   printf("aycw_checkPESheader               %.3fs\n", ((float)aycw__getTicks_ms() - start) / 1000);

   //printf("%d %d %d\n", bs_64_1[0], r[0], bs_448[0]);
}
#else
void aycw_partsbench(void) {}
#endif
