#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "ts.h"
#include "loop.h"
#include "config.h"
#include "bs_stream.h"
#include "bs_block.h"
#include "bs_block_ab.h"
#include "bs_algo.h"
#include "dvbcsa.h"
#include "helper.h"


/****************** definitions ***********************/
/* key bits calculated in inner bs loop - do not change */
#define INNERKEYBITS    16
/* number of keys calculated in inner bs loop - do not change */
#define KEYSPERINNERLOOP 0x10000

#if (KEYSPERINNERLOOP != (1<<INNERKEYBITS))
#error setting of KEYSPERINNERLOOP incorrect
#endif




/*
control word iteration strategy:

        +---+---+-------+---+---+-----  u64Currentkey
        |   |   |       |   |   |

        |   |   |       |   +---+-----  INNERBATCH /  BS_BATCH_SIZE
        |   |   |       |   |   |
        |   |   |       |   |   |
        |   |   |       |   |   |
        |   |   |       |   |   |
byte    00  11  22 [33] 44  55  66 [77]

Control words are iterated starting at highest byte (66, then 55,...)
u64Currentkey holds the 48 bits where its lsb is lsb of byte 66.
Checksum bytes [33] and [77] are recalculated and not stored.

BS_BATCH_SIZE - the "batch run".
Number of keys processed in parallel bitslice manner
by one thread at at time.
uint32      ->  32 keys at once
sse4_2      ->  128 keys at once
                    
INNERBATCH - the "inner loop"
to transpose keys from regular representation (u64Currentkey) into
bit sliced representation (keys_bs) is costly. Therefore transposition
is done only when necessary.
As an intermediate step, the incrementation of keys is done within its
bit sliced representation - aycw_bs_increment_keys_inner() does this.
This is currently fixed for byte 55 and 66. The lsb bits are covered 
by the batch run. The remaining of these 16 bits are covered by 
aycw_bs_increment_keys_inner().

TBD: where to start parallel threads and how many?
                    
u64Currentkey - the "outer loop". 
This loop should be executed as seldom as possible (in order to not throttle
performance) but as often as possible (to not slow down console 
throughput update).
On every outer loop, u64Currentkey is incremented by inner*batch.
The outer loop runs over the whole key space (if not limited by -a/-o).


Test strategy: aycwabtu's self test will check if a key is found on every
batch position (reveals transposition issues).
Also the inner loop shall be covered (aycw_bs_increment_keys_inner issues).
At least two consecutive runs for outer loop shall be done (reveals 
incrementation issues).

*/


/**
 * From the Current key in regular representation generate
 * - an array of keys for each batch slice in regular representation
 * - the keys in bitsliced representation
 * 
 * */
void loop_generate_key_batch(uint64_t u64Currentkey, uint8_t keylist[], dvbcsa_bs_word_t* keys_bs)
{
   uint64_t      i;

   for (i = 0; i < BS_BATCH_SIZE; i++)
   {
      getCw(u64Currentkey+i, &(keylist[i*8]));
      /*keylist[i*8+0] = (u64Currentkey + i) >> 40;
      keylist[i*8+1] = (u64Currentkey + i) >> 32;
      keylist[i*8+2] = (u64Currentkey + i) >> 24;
      keylist[i*8+3] = keylist[i*8+0] + keylist[i*8+1] + keylist[i*8+2];
      keylist[i*8+4] = (u64Currentkey + i) >> 16;
      keylist[i*8+5] = (u64Currentkey + i) >> 8;
      keylist[i*8+6] = (u64Currentkey + i);
      keylist[i*8+7] = keylist[i*8+4] + keylist[i*8+5] + keylist[i*8+6];*/
   }
   aycw_key_transpose(keylist, keys_bs);
   aycw_assert_key_transpose(keylist, keys_bs);
}


bool loop_perform_key_search(
   ts_probe2_t *probedata,
   uint64_t u64Currentkey, 
   uint64_t u64Stopkey, 
   void (*progress_callback)(uint64_t u64ProgressKey, uint64_t u64Stopkey),
   unsigned char *retcw)
{
   uint64_t      i, k;

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

   uint8_t keylist[BS_BATCH_SIZE*8];     /* the list of keys for the batch run in non-bitsliced form */




   aycw_init_block();

   aycw_init_stream((uint8_t*) probedata, bs_data_sb0);

   for (i = 0; i < 8 * 8; i++)
   {
      bs_data_ib0[i] = bs_data_sb0[i];
   }
#ifndef USEALLBITSLICE
   aycw_bit2byteslice(bs_data_ib0, 1);
#endif

   /************* outer loop ******************/
   while (u64Currentkey <= u64Stopkey)
   {

      loop_generate_key_batch(u64Currentkey, keylist, keys_bs);

      /************* inner loop ******************/
      for (k = 0; k < KEYSPERINNERLOOP / BS_BATCH_SIZE; k++)
      {

#ifdef SELFTEST
         /* check if initial (outer) key and subsequent (inner) key batches are correct */
         aycw_assertKeyBatch(keys_bs);
#endif
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

         aycw_assert_decrypt_result(probedata, keylist, r);

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

                  memcpy(&data, &((ts_probe_packet_t*)probedata)[0], 16);
                  dvbcsa_decrypt(&key, data, 16);
                  if (data[0] != 0x00 || data[1] != 0x00 || data[2] != 0x01)
                  {
                     /* bitslice and regular implementations calculated different results - should never happen */
                     printf("\nFatal error: candidate verification failed!\n");
                     printf("last key was: %02X %02X %02X [%02X]  %02X %02X %02X [%02X]\n",
                        cw[0], cw[1], cw[2], cw[3], cw[4], cw[5], cw[6], cw[7]);
                     exit(ERR_FATAL);
                  }

                  memcpy(&data, &((ts_probe_packet_t*)probedata)[1], 16);
                  dvbcsa_decrypt(&key, data, 16);
                  if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)
                  {
                     memcpy(&data, &((ts_probe_packet_t*)probedata)[2], 16);
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

         u64Currentkey += BS_BATCH_SIZE;

#ifdef SELFTEST
         /* re-transposing is only needed in self test, as keylist is evaluated inside inner */
         loop_generate_key_batch(u64Currentkey, keylist, keys_bs);
#endif
      }  // inner loop

      /***********************************************************************************************************************/
      /***********************************************************************************************************************/
      /***********************************************************************************************************************/
      if (progress_callback) (*progress_callback)(u64Currentkey, u64Stopkey);


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
