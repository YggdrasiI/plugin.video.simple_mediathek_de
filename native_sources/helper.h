#pragma once

#include <stdlib.h>
#include <stdio.h>

#include "parser.h"
#include "filmliste.h"

#define min(MIN, X) (((MIN)<(X))?(MIN):(X))
#define max(X, MAX) (((X)>(MAX))?(X):(MAX))
#define clip(MIN, X, MAX) (((MAX)>(X))?(((X)<(MIN))?(MIN):(X)):(MAX))

#define Free(X) if( X ){free(X); (X)=NULL;}

#if OWN_STRTOL > 0
/* Similar to strtol but for char arrays without \0 ending
 * and base 10 only.
 *
 * – Leading spaces will be skipped. All other non-digit characters
 *   abort the parsing of the string.
 * – No overflow detection.
 * – If 'endptr' is NULL, you can not distinct 0 from 'not found' values.
 *   To check if a number was found compare '*endptr' with 'str'.
 *   The 'endptr' value should point after the start value.
 *
 */
int strtoi(const char* str, char** endptr, int maxlen);
#endif

/* Note: Replaced in code due strange behavior*/
# define my_strdupa(s) \
    (__extension__ \
     ({ \
      const char *__old = (s); \
      size_t __len = strlen (__old) + 1; \
      char *__new = (char *) __builtin_alloca (__len); \
      (char *) memcpy (__new, __old, __len); \
      }))


/* Assumed input: String of time_t-value */
time_t parse_starttime(
        const char_buffer_t *p_buf,
        search_pair_t *sp );

/* Assumed input: [0-9]{1,2}.[0-9]{1,2}.[0-9]{2 or 4}
 *
 * start_second is required to evaluate the day light saving time correctly.
 * Seconds will be added to the day time stamp.
 * */
time_t parse_day(
        const char_buffer_t *p_buf,
        filmliste_workspace_t *p_fl_ws,
        search_pair_t *sp);

/* Assumed input: [0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2}
 *
 * Note: If p_fl_ws is not NULL, tm_sec, tm_min, and tm_hour
 * value of work space time stamp will be updated.
 * */
size_t parse_dauer(
        const char_buffer_t *p_buf,
        filmliste_workspace_t *p_fl_ws,
        search_pair_t *sp );

/* Transform search string into same form as
 * content of title_*.index file. Function
 * made following transformations: (TODO)
 * 1. Lower case string
 * 2. Remove some characters like "/'
 * 3. Normalize or remove several multibyte utf-8 chars.
 * 4. Return NULL pointer for empty input strings.
 *
 * Return value: length of *pp_out
 * Notes:
 *   - Utf-8 strings supported.
 *   - Function creates new string. Free it after usage.
 *   - Set *pp_out to NULL if input is NULL.
 * */
size_t transform_search_title(
        const char *p_in,
        char **pp_out)
;

/* Like transform_search_title, but function just
 * set the number of used chars downwards for short input(s).
 * Thus, p_buf_out->p is never returned as NULL.
 *
 */
size_t transform_search_title2(
        const char *p_in,
        char_buffer_t *p_buf_out)
;

#define transform_channel_name transform_search_title

/* Malloc with extra char for '\0' */
char *char_buffer_malloc(
        size_t size)
;

/* Memcpy with resize of destination if required. */
void charcpy(
        char **pp_dest,
        size_t *p_dest_len, /* >= strlen(dest) */
        const char * const p_source,
        const size_t source_len)
;
