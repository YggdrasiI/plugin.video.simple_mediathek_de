#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include "settings.h"
#include "params.h"
#include "parser.h"
#include "channels.h"
#include "linked_list.h"
#include "filmliste.h"

//#define SEARCH_TITLE_ARR_LEN   0x100000 //=> 27MB at search
//#define SEARCH_TITLE_ARR_LEN   0x010000 // < 25MB at search
#define SEARCH_TITLE_ARR_LEN   0x040000   // < 25, too.

#define MAX_SUB_PATTERN 10 // search pattern can be splited by *

#if 0
typedef struct {
    time_t start_time;
    uint16_t duration;
    uint16_t length; //title_len;
    //const char *title_str; // Title + Topic string (link into buffer, without \0?!)
} title_line_t;
#else
//Same?!
typedef searchable_strings_prelude_t title_line_t;
#endif

#if 0
typedef struct {
    uint8_t len8;
    const char *p;
} short_string_t;
#endif

typedef struct {
    uint32_t len;
    const char *p;
} string_t;

typedef struct {
    linked_list_subgroup_indizes_t groups;
    char *title_pattern;
    char *_title_sub_pattern; // similar to title_pattern, but with '\0' at arbitary positions.
    char *title_sub_pattern[MAX_SUB_PATTERN+1]; // +1 for extra NULL
    uint32_t current_id;
    int K;
    uint32_t used_indizes[4];
    uint32_t start_ids[4];
    /* Force check if 'datum(dayMax/oldest)' <=  'begin' <= 'datum(dayMin/youngest)'
     * This check is only required for pattern of the oldest day subgroup,
     * because it spans over multiple days.
     */
    uint8_t explicit_day_check; // Forces check
    time_t explicit_dayMin; // Translation of dayMax(sic!) arg into begin of current local day.
    time_t explicit_dayMax; // First second after end of local day.

} search_pattern_t;

#if 0
// For seach task/ Reading of titles.index
typedef struct search_entry_s {
    index_node_t *index; // contains seek positions
    title_line_t *title; // title begins at pointer+sizeof(title_line_t)
    struct search_entry_s *next;
} search_entry_t;

typedef struct {
    char_buffer_t buf_main; // main.index
    char_buffer_t buf_titles; // titles.index
    search_entry_t matches[SEARCH_NUM_MAX_MATCHES];
} xx_t;
#endif

typedef struct {
  uint32_t match_limit;
  uint32_t match_found;
  uint32_t match_skip;
  uint32_t *matches;
} search_result_t;

typedef struct {
    /* len is the length of bufs.
     *
     * Latest element of this array will map on same location as
     * the previous element to catch the lookup case
     *     {seek of something} > SEARCH_TITLE_ARR_LEN
     *
     * ( We decide which chunk contains the entry with an expression like
     * (...).seek_pos / SEARCH_TITLE_ARR_LEN )
     */
    size_t len;
    size_t partial_i;
    char_buffer_t *bufs;
    uint32_t *start_seeks; // start_seeks[i] marks offset of element at bufs[i].p.
    uint32_t *start_ids; // first id in at bufs[i].p.
    //uint32_t *end_ids; // latest id in at bufs[i].p. = start_ids[i+1]-1

    /* Position in brotli input buffer after partial reading of file.
     * The next decoding should be start at
     * 'buffer_data_pointer + decoding_start_byte_offset'.
     *
     * (Shifted into brotli decoder struct.)
     */
    //size_t decoding_start_byte_offset;

} title_chunks_t;

#if 0
typedef struct {
  size_t len;
  size_t used;
  char *p;
} output_str_t;
#else
typedef char_buffer_t output_str_t;
#endif

/* Bundle values for sorting by qsort */
typedef struct {
    uint32_t id;
    searchable_strings_prelude_t *p_entry;  // for output_fill()-call
    size_t iChunk;  // just for get_topic()-call
    // Copy of *p_entry => Title string does not follow this struct anymore
    // Required for qsort compare functions.
    searchable_strings_prelude_t entry;
    output_str_t to_print;
} output_candidate_t;

typedef int sort_cmp_handler_t(const void * p_left, const void * p_right);

// Following struct holds output. (Size depends on number of requested return results...)
typedef struct {
  uint32_t N;
  uint32_t Nskip;
  uint32_t Nsearch;
  uint32_t M; // N+Nskip
  uint32_t M2; // Sum of N+Nskip+Nsearch
  output_candidate_t *p_candidates; // length M2

  // To identify unhandled objects in array after sorting.
  uint32_t first_unhandled_id;

  uint32_t found; // Incr. for each added element.
  uint32_t pos_i; // 0 <= pos_i < M2 (== (found-1) % M2)
  uint32_t pos_p; // 0 <= pos_p < M <= M2; Latest index where pp_top_print contain data.
  /* if pos_p = pos_i  Nothing is unhandled.
   * if pos_p < pos_i. [pos_p+1, pos_i] is unhandled.
   * if pos_p > pos_i  [pos_p+1, M-1] and [0, pos_i] is unhandled.
   *
   */
  int first_comma_offset; // Shift for first entry to omit leading comma (+space)
  int reversed_flag;
  int search_whole_index_flag; // If 0, programm stops after N+Nskip matches.
  int overlap_flag;
  sort_cmp_handler_t *sort_handler;
} output_t;

typedef struct search_workspace_s {
    /* Timestamps for index file */
    time_t tcreation; // Creation day of index file;
    time_t ttoday;  // (Not stored in index file, but 86400*itoday.)
    size_t itoday; // creation_day/seconds(day) (Redundant value)
    int today_time_zone_offset; // GTM+1 + opt. day light offset

    time_t list_creation_time; // Creation time of json-input (-1 until fetched from parser)
    time_t search_tnow;  // Time stamp where this program run.
    time_t search_ttoday; // ttoday;  Timestamp of begin of (local) day
    size_t search_itoday; // itoday; Days since 1970-01-01 (localtime) => itoday*86400 <= tcreation
#ifdef WITH_TOPIC_OPTION
    int restrict_string_search_on_topic;
#endif
    file_data_t payload;
    file_data_t searchable_strings; // Title + Topic
    size_t searchable_strings_len;
    char *_buf;
    char_buffer_t search_out_buf;
    struct tm tmp_ts; // time stamp
    channels_ws_t channels;
    linked_list_t index;
    title_chunks_t chunks;
    int index_fd; // Input fd
//    search_result_t search_result;
    buf_string_copy_t prev_topic;
#ifdef COMPRESS_BROTLI
    brotli_decoder_workspace_t brotli_title;
    //  brotli_decoder_workspace_t brotli_payload;
#endif
    output_t output;
    arguments_t *p_arguments;
} search_workspace_t;


#include "search_output.h"

search_workspace_t search_ws_create(
        arguments_t *p_arguments)
;

void search_ws_destroy(
        search_workspace_t *p_s_ws)
;

int open_index_file(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments)
;

int open_title_file(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments)
;

// Meta data about creation time, etc
void read_index_header(
        search_workspace_t *p_s_ws)
;

void read_index_footer(
        search_workspace_t *p_s_ws)
;

int search_do_search(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments)
;

/* Lookup into index to get seek position for
 * title and second lookup to find this entry.
 *
 * Call only after setup with search_read_title_file(...)
 *
 * Return value is the index of chunk.bufs[] where the entry
 *   could be found.
 * The evaluated entry will be stored in pp_entry.
 */
size_t title_entry(
        search_workspace_t *p_s_ws,
        uint32_t entry_id,
        searchable_strings_prelude_t **pp_entry)
;

void search_read_title_file(
        search_workspace_t *p_s_ws)
;

/* Print Json data (without content of payload files)
 * for list of current search */
void print_search_results(
        int fd,
        search_workspace_t *p_s_ws)
;

#if 0 //deprecated
/* Print Json data (without content of payload files)
 * for one id. */
void print_search_result(
        int fd,
        search_workspace_t *p_s_ws,
        uint32_t id,
        int prepend_comma)
;
#endif

/*
 * Find lowest node with
 * index_node_t.link.title_seek >= threshold
 * after an given start node of the index.
 *
 * Used to evaluate size of buffer before this node.
 */
uint32_t find_lowest_seek_over_threshold(
        search_workspace_t *p_s_ws,
        uint32_t threshold,
        //uint32_t start_index, uint32_t *stop_index)
        uint32_t start_id, uint32_t *p_stop_id)
;

/* Return pointer on (transformed) title pattern.
 * (Similar to title_entry()...
 *
 * Pattern could contain '|topic' substring.
 */
const char * get_title_string(
        search_workspace_t *p_s_ws,
        uint32_t id)
;

/* List of derivered search pattern from
 * the input
 * Returned array length of pp_results is [days] x [durations] x ...
 *
 * Note that all pattern point on the same title_pattern. (=> Avoid double free'ing)
 */
int search_gen_patterns_for_partial(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments,
        search_pattern_t **pp_results)
;

/* Search until currently loaded part of
 * title data ends.
 *
 * 1 New entries found in chunk
 * 0 No new entry found in chunk
 * -1 No new entry now and all following parts.
 * */
int search_do_search_partial(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern)
;

/*
 * Only fill one chunk at each run.
 *
 * Return values:
 *  -1 Error during file reading
 *   0 Loading ok
 *   1 Loading ok and end reached.
 */
int search_read_title_file_partial(
        search_workspace_t *p_s_ws)
;

/* Reset workspace
 * to allow opening of next title
 * file.
 * (Called before search in diff file begins.)
 */
void search_reset_workspace(
        search_workspace_t *p_s_ws)
;

/* Assumes string of form aaaXbbbXccc...
 * in array[0] and return array
 * with aaa, bbb, ccc, ..., NULL
 *
 * Return: Number of used celles of array (>= 1).
 *
 * Note: array[i] pointers maping into string title_pattern.
 * Thus, reset array values to NULL, but not free'ing them.
 */
int split_pattern(
        search_pattern_t *p_pattern,
        char split_char)
;

/*
 * Go back over elements until non-empty topic
 * string is reached or begin of block.
 * In the second case the backuped topic of the previous block
 * will be returned.
 *
 * Return value:
 *   0 - Topic appends title string.
 *   1 - Topic appends previous title string, but same chunk.
 *   2 - Topic appends previous title string, but different chunk.
 *   3 - Cached previous topic string pointer returned.
 *  -1 - Can not evaluate topic pointer.
 */
int get_title_and_topic(
        search_workspace_t *p_s_ws,
        uint32_t id,
        const char **pp_title,
        const char **pp_topic
        )
;

/* Same return codes as get_title_and_topic. */
int get_topic(
        search_workspace_t *p_s_ws,
        size_t iChunk_of_entry,
        searchable_strings_prelude_t *p_entry,
        const char **pp_topic)
;
