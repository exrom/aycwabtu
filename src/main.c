/*************************************************************************
 * 
 *  aycwabtu main
 * 
 *    Handle command line arguments
 *    transport stream file reading
 *    resume file read/write    
 * 
 * 
 * 
 *************************************************************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>         /* getopt() */

#ifdef __CYGWIN__
#  include <windows.h>
#else
#  include <time.h>
#endif

#include "loop.h"
#include "ts.h"
#include "helper.h"

#define VERSION   "V2.0"

/* bitslice test cases */
#include "bs_testcases.h"


#define UINT48_MAX   (0xFFFFFFFFFFFF)    /* */

#define RESUMEFILENAME  ".resume"
#define FOUNDFILENAME   "keyfound.cwl"



/* Decrypts with correct key 00 11 22 [33] 44 00 00 [44] to
            000001ff11111111aa11111111111155
            000001ff11111111aa11111111111156
            000001ff11111111aa11111111111157  */
/*const ts_probe_t bench_data = {
      { 0xB2, 0x74, 0x85, 0x51, 0xF9, 0x3C, 0x9B, 0xD2,  0x30, 0x9E, 0x8E, 0x78, 0xFB, 0x16, 0x55, 0xA9},
      { 0x25, 0x2D, 0x3D, 0xAB, 0x5E, 0x3B, 0x31, 0x39,  0xFE, 0xDF, 0xCD, 0x84, 0x51, 0x5A, 0x86, 0x4A},
      { 0xD0, 0xE1, 0x78, 0x48, 0xB3, 0x41, 0x63, 0x22,  0x25, 0xA3, 0x63, 0x0A, 0x0E, 0xD3, 0x1C, 0x70} };
const uint64_t u64BenchStartKey = 0x000011222F0000;
const uint64_t u64BenchStopKey  = 0x00001122460000;
*/


/*
void ayc_printhexbytes(unsigned char *c, uint8_t len)
{
   int i;
   for (i = 0; i<len; i++)
   {
      if ( i && !(i%4) ) printf(" ");
      printf("%02X", c[i]);
   }
   printf("\n");
}
*/


/* get system timer ticks in milli seconds */
long int aycw__getTicks_ms(void)
{
#ifdef WIN32
			return GetTickCount();
#else
   typedef struct {
   long    tv_sec;        /* seconds */
   long    tv_usec;    /* microseconds */
   } timeval;

	//get the current number of microseconds since january 1st 1970
	timeval ts;
	gettimeofday(&ts,0);
	return (long int)(ts.tv_sec * 1000 + (ts.tv_usec / 1000));
#endif
}


/* print performance measure to console */
void aycw_perf_show(uint64_t u64Currentkey, uint64_t u64Stopkey)
{
   const char        prop[] = "|/-\\";
   static int        propcnt;
   static uint64_t   totalkeys = 0;    /* number of keys processed since start of program */
   static uint64_t   u64FirstTicks;
   static uint64_t   u64FirstKey;
   static uint64_t   u64PreviousTicks;
   static uint64_t   u64PreviousKey;
   uint64_t          u64Ticks;
   uint64_t          u64Keys;
   uint64_t          u64DeltaTicks;
   uint64_t          u64DeltaKeys;
   static int        divider = 0;
   static int        init_done = 0;


#ifdef DEBUG
#define DIVIDER 1
#else
#define DIVIDER 16      // reduce update frequency for release
#endif

   if (!init_done)
   {
      init_done = 1;
      u64PreviousTicks = u64FirstTicks  = aycw__getTicks_ms();
      u64PreviousKey = u64FirstKey = u64Currentkey;
   }
   else
   {
      if (!divider)
      {
         uint64_t u64CurrentTicks  = aycw__getTicks_ms();
         uint64_t u64DeltaKeys = u64Currentkey - u64PreviousKey;
         uint64_t u64DeltaTicks = u64CurrentTicks - u64PreviousTicks;
         uint64_t u64AllKeys = u64Stopkey - u64FirstKey;

         printf("\r");
         putc(prop[(propcnt++ & 3)],stdout);
         u64Keys   = u64Currentkey - u64FirstKey;
         u64Ticks  = u64CurrentTicks - u64FirstTicks;

         if (u64DeltaTicks)
         {
            printf(" %.3f Mcw/s ", ((float)u64DeltaKeys / u64DeltaTicks / 1000));
         }
         if (u64Ticks)
         {
            printf("avg: %.3f Mcw/s  ", ((float)u64Keys / u64Ticks / 1000));
         }
         if (u64AllKeys)
         {
            printf("%.3f%% ", ((float)u64Keys / u64AllKeys * 100));
         }
         print_key(u64Currentkey, false);

         u64PreviousKey = u64Currentkey;
         u64PreviousTicks = u64CurrentTicks;
         divider = DIVIDER;
      }
      divider--;
   }
}

/* save to the current key to file to remember brute force progress */
void aycw_write_resumefile(uint64_t u64Currentkey)
{
   static int divider = 10;    /* long live the ssd */
   FILE * filehdl;
   char string[64];

   divider++; divider &= 0x1ff;
   if (!divider)
   {
      if (filehdl = fopen(RESUMEFILENAME, "w"))
      {
         //printf("\nwriting resume file\n");
         sprintf(string, "%02X %02X %02X %02X %02X %02X %02X %02X\n",
            (uint8_t)(u64Currentkey >> 24), (uint8_t)(u64Currentkey >> 16), (uint8_t)(u64Currentkey >> 8), 0, (uint8_t)u64Currentkey, 0, 0, 0);
         fwrite(string, 1, strlen(string), filehdl);
         fclose(filehdl);
      }
      else
      {
         printf("error writing resume file\n");
      }
   }
}

void  main_status_update(uint64_t u64Currentkey, uint64_t u64Stopkey)
{
     aycw_perf_show(u64Currentkey, u64Stopkey);

      /*if (!benchmark) */ aycw_write_resumefile(u64Currentkey);
}



void aycw_read_resumefile(uint64_t *key)
{
   FILE * filehdl;
   unsigned char buf[8 * 3 + 2 + 1];
   unsigned char tmp[8+3]; /* visual C writes 4 bytes to pointer though 'hh' specifier was given */

   if (filehdl = fopen(RESUMEFILENAME, "r"))
   {
      fseek(filehdl, 0, SEEK_SET);
      fread(buf, sizeof(buf), 1, filehdl);
      fclose(filehdl);
      if (8 == sscanf(buf, "%02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX\n",
         &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5], &tmp[6], &tmp[7] ) )
      {
         *key = tmp[0] << 24 | tmp[1] << 16 | tmp[2] << 8 | tmp[4];
         printf("resuming at key %08X\n", *key);
      }
   }
   else
   {
      /* just ignore missing resume file */
   }
}

void aycw_write_keyfoundfile(unsigned char *cw, int probeparity, char* tsfile)
{
   FILE * filehdl;
   int i;
   char string[1024];

   printf("writing result to file \"%s\"\n", FOUNDFILENAME);
   if (filehdl = fopen(FOUNDFILENAME, "w"))
   {
      strcpy(string, "# AYCWABTU cw brute forcer\n# found cw for ts file\n#    ");
      fwrite(string, 1, strlen(string), filehdl);
      fwrite(tsfile, 1, strlen(tsfile), filehdl);
      sprintf(string, "\n#\n%d %02X %02X %02X %02X %02X %02X %02X %02X\n",
         probeparity,
         cw[0], cw[1], cw[2], cw[3], cw[4], cw[5], cw[6], cw[7]);
      fwrite(string, 1, strlen(string), filehdl);
      fclose(filehdl);
   }
   else
   {
      printf("error opening file \"%s\" for writing\n", FOUNDFILENAME);
   }
}


void aycw_welcome_banner(void)
{
   printf("AYCWABTU CSA brute forcer %s %s built on %s", VERSION, GITHASH, __DATE__);
#ifdef _DEBUG
   printf(" DEBUG");
#endif
   printf("\nCPU only, single threaded version");
#ifdef USEALLBITSLICE
   printf(" - all bit slice (bool sbox)");
#else
   printf(" - table sbox");
#endif
   printf("\nSIMD mode: %s, batch size: %d\n", AYC_PARALLEL_MODE_STRING, BS_BATCH_SIZE);
   printf("----------------------------------------\n");
   setbuf(stdout, NULL);   // for gcc
}


void usage(void) {
    printf("Usage: aycwabtu [OPTION]\n");
    printf("   -t filename      transport stream file to obtain three packets\n");
    printf("                    for brute force attack\n");
    printf("   -a start cw      cw to start the brute force attack with. Checksum\n");
    printf("                    bytes are omittted, e.g. 112233556677 [000000000000]\n");
    printf("   -o stop cw       when this cw is reached, program terminates [FFFFFFFFFFFF]\n");
    printf("   -b benchmark     start benchmark run with internal demo ts data\n");
    exit(ERR_USAGE);
}

uint64_t ayc_get_key_from_cw_param(const char *string) {
    uint8_t tmp[8];
    
    if ((strlen(string) != 12) || (6 != sscanf(string, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &tmp[0], &tmp[1], &tmp[2], &tmp[4], &tmp[5], &tmp[6])))
    {
        printf("key parameter format incorrect. 6 hex bytes expected.\n");
        usage();
    }
    return /*getKey(tmp);*/
               ((uint64_t)tmp[0] << 40) +
               ((uint64_t)tmp[1] << 32) +
               ((uint64_t)tmp[2] << 24) +
               
               ((uint64_t)tmp[4] << 16) +
               ((uint64_t)tmp[5] << 8) +
               ((uint64_t)tmp[6]);
}

void performSelfTest(void)
{
   dvbcsa_cw_t  cw_enc;
   dvbcsa_cw_t  cw;
   uint64_t    u64Stopkey, u64Currentkey;
   ts_probe2_t    probedata;
   int          i;

   printf("self test ... ");
   for(i=0; i<BS_BATCH_SIZE+1; i++)
   {
      u64Stopkey = u64Currentkey = 0x112233440000;    /* start loop search at aligned batch */
      getCw(u64Currentkey+i, cw_enc);                 /* put to be found key into increasing positions into batch */
      ts_generate_probe_data(&probedata, cw_enc);

      if (!loop_perform_key_search(
         &probedata,
         u64Currentkey,
         u64Stopkey,
         NULL,
         cw))
      {
         printf("failed at %d\n", i);
         exit (1);
      }
   }
   printf("passed.\n");
}

int main(int argc, char *argv[])
{
   int      benchmark = 0;
   int      opt;
   char*    tsfile = NULL;

   ts_probe2_t    probedata;
   int            probeparity;

   unsigned char  cw[8];

   uint64_t       u64Currentkey     = 0;
   uint64_t       u64Stopkey        = UINT48_MAX;

    while((opt = getopt(argc, argv, "t:a:o:b")) != -1) 
    { 
        switch(opt) 
        { 
            case 't': 
                tsfile = optarg;
                break; 
            case 'a': 
                u64Currentkey = ayc_get_key_from_cw_param(optarg);
                break; 
            case 'o': 
                u64Stopkey = ayc_get_key_from_cw_param(optarg);
                break; 
            case 'b': 
                benchmark = 1;
                break; 
            case ':': 
                printf("option needs a value\n"); 
                break; 
            default:
                usage();
                break; 
        } 
    } 
    for(; optind < argc; optind++){     
        printf("unknown command: %s\n", argv[optind]); 
        usage();
    }

    /* check parameter plausibility */
    if ((!benchmark) && (!tsfile))
    {
        printf("Please provide ts filename or run benchmark\n");
        usage();
    }

    aycw_welcome_banner();
    performSelfTest();

    if (benchmark)
    {
       printf("Benchmark mode enabled. Using internal ts data\n");
      u64Currentkey  = 0xCAFE30000000;
      u64Stopkey     = 0xCAFE50000000;
      getCw(           0xCAFE46000000, cw);
      ts_generate_probe_data(&probedata, cw);
    }
    else
    {
        ts_read_file(tsfile, (uint8_t*) &probedata, &probeparity);
        aycw_read_resumefile(&u64Currentkey);
    }


   printf("start key: "); print_key(u64Currentkey, true);
   printf("stop key : "); print_key(u64Stopkey, true);

#ifdef WIN32
   SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif

   if (loop_perform_key_search(
         &probedata,
         u64Currentkey,
         u64Stopkey,
         main_status_update,
         cw)
      )
   {
      printf("\nkey candidate successfully decrypted three packets\n");
      printf("KEY FOUND!!!    %02X %02X %02X [%02X]  %02X %02X %02X [%02X]\n",
         cw[0], cw[1], cw[2], cw[3], cw[4], cw[5], cw[6], cw[7]);
         /*print_cw()*/

      if (tsfile) aycw_write_keyfoundfile(cw, probeparity, tsfile);
      exit(OK);
   }

   printf("\nStop key reached. No key found\n");
   exit(WORKPACKAGEFINISHED);

} // main

