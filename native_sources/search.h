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

typedef struct {
    linked_list_subgroup_indizes_t groups;
    char *title_pattern;
    char *_title_sub_pattern; // similar to title_pattern, but with '\0' at arbitary positions.
    char *title_sub_pattern[MAX_SUB_PATTERN+1]; // +1 for extra NULL
    uint32_t current_id;
    int K;
    uint32_t used_indizes[4];
    uint32_t start_ids[4];

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

// Following struct holds output. (Size depends on number of requested return results...)
typedef struct {
  uint32_t N;
  uint32_t Nskip;
  uint32_t M; // Sum of N+Nskip
  uint32_t found; // Incr. for each added element.
  uint32_t pos_i; // 0 <= pos_i < M (== found % M)
  uint32_t pos_p; // 0 <= pos_p < M; Latest index where pp_top_print contain data.
  /* if pos_p = pos_i  Nothing is unhandled.
   * if pos_p < pos_i. [pos_p+1, pos_i] is unhandled.
   * if pos_p > pos_i  [pos_p+1, M-1] and [0, pos_i] is unhandled.
   *
   */

  uint32_t *p_ids; // length M
  output_str_t *p_to_print; // Candidates to print, length M

  int first_comma_offset; // Shift for first entry to omit leading comma (+space)
  int reversed_flag;
  int overlap_flag;
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
 */
void *title_entry(
        search_workspace_t *p_s_ws,
        uint32_t entry_id
        )
;

void search_read_title_file(
        search_workspace_t *p_s_ws)
;

/* Print Json data (without content of payload files)
 * for list of current search */
void print_search_results(
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
 */
const char * get_title_string(
        search_workspace_t *p_s_ws,
        uint32_t id)
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
