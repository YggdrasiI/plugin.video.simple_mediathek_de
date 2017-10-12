#pragma once

/* Buffer size for reading of input. */
#define MY_BUFSIZ (3*BUFSIZ)

#ifdef COMPRESS_BROTLI
/* Buffer size for writing compressed. (+ overhead by compress library)
 *
 * Lower values increases number of flushing calls.
 * */
//#define OUT_CACHESIZ (1000000)
#define OUT_CACHESIZ (0x040000)  // 262kb
#define BROTLI_COMPRESS_QUALITY 2
#else
/* Buffer size for writing uncompressed. */
//#define OUT_CACHESIZ (5000000)
#define OUT_CACHESIZ (3000000)
#endif

/* Lower bound where old compressed payload file 
 * will be closed and new one begins.
 *
 * Note that high values of OUT_CACHESIZ affects
 * the final sizes of the compressed files. 
 * The compression starts if (raw) output buffer
 * is full. 
 * (If we would distribute the output buffer
 * on multiple compressed files the indexes had to be updated.
 * Thus, we won't do that.)
 */
#define PAYLOAD_MAX_FILE_SIZE (1024 * 1024)

/* To save memory the file with the 
 * title strings will read part by 
 * part during the search.
 */
#define READ_TITLE_FILE_PARTIAL 

/* Replace é with e and similar (over 300 cases...).
 */
#define REPLACE_COMPOSITE_CHARS

#define NL (0x0a)

#define OWN_STRTOL 1

#define NUM_RELATIONS 4 // duration, time, date, channel
// Following constants should not exceed 256
#define NUM_DURATIONS 6
#define NUM_TIME 4
#define NUM_REALTIVE_DATE 16
#define NUM_CHANNELS 64 // TODO: Made number flexible?!
// ==
#define NUM_SUM (NUM_DURATIONS + NUM_TIME + NUM_REALTIVE_DATE + NUM_CHANNELS)

/* Array with length NUM_DURATIONS which contain the biggest value of 
 * each interval. Use INT_MAX in last entry
 *                      10 Min 30 Min 60 Min 90 Min 2h      >2h */
#define DURATION_ARRAY {10*60, 30*60, 60*60, 90*60, 120*60, INT_MAX}

/* Array with length NUM_TIME. Use INT_MAX in last entry
 *                0-10 Uhr  10-16 Uhr  16-20 Uhr     Nach 20 Uhr */
//#define TIME_ARRAY {10*3600-1, 16*3600-1, 20*3600-1, INT_MAX}
//Shift by 2 Minutes 
#define TIME_ARRAY {10*3600-121, 16*3600-121, 20*3600-121, INT_MAX}


// Disable channel as search criteria
#define NO_CHANNEL -1 

/* For all entries without given channel / for empty channel string */
#define UNKNOWN_CHANNEL 0 

/* Separator between title and topic string.
 * The splitted strings should not contain this character.
 */
#define SPLIT_CHAR '|'

/* Highest byte value used to mark that prefetecd length information 
 * of a short_string_t is invalid. Use len(...), etc to get length info.
 */
#define UNDEFINED_TITLE_LENGTH (0xFF);
#define MAXIMAL_TITLE_LENGTH (UNDEFINED_TITLE_LENGTH - 1);

/* Constant to inform that topic string of previous entry should be used.
 * ( "title|\t" or  "title\t" expands to "title|previous_topic"
 */
#define BACKTRACK_CHAR '\t'

#ifdef NDEBUG
#define DEBUG(X) 
#else
#define DEBUG(X) X;
#endif

/* To mark variablen which will be only used by assert()-checkes.
 * This avoids the unused variable warnings (-Wunused-variable)
 * on certain variables without disable the warning globally.
 */
#define _unused(x) ((void)(x))

// Check errno after certain system calls
//#define CHECK_ERRNO

/* Payload file index for partial updates */
#define FIRST_DIFF_INDEX 30
static const char diff_ext[] = ".diff";

static const char index_folder[] = "/dev/shm/";
static const char index_file_template[] = "%s/main%s.index";
#ifdef COMPRESS_BROTLI
static const char payload_file_template[] = "%s/payload_%04i.br";
static const char strings_file_template[] = "%s/titles_%04i%s.br";
#else
static const char payload_file_template[] = "%s/payload_%04i.index";
static const char strings_file_template[] = "%s/titles_%04i%s.index";
#endif

typedef struct {
  size_t len;
  size_t used;
  char * p;
} char_buffer_t;


//#define memcpy __memcpy_sse2_unaligned
#if 0
#include <string.h>
void *memcpy(void *dest, const void *src, size_t n);
#ifdef _POSIX_C_SOURCE 
void *memcpy(void *dest, const void *src, size_t n){
    int b, L=n;
    char *t=(char *)(dest);
    char *f=(char *)(src);
    for(b=0; b<L; ++b){
        t[b] = f[b];
    }
}
#endif
#endif

