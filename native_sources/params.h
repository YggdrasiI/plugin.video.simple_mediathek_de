#pragma once

#include <string.h>
#include <limits.h>
#include <stdint.h>

#define WITH_TOPIC_OPTION

/* Maximal day to go behind.
 *
 * Restrictions:
 * - Multiplication with seconds(day) should not lead to overflows of time_t
 * - Current day - MAX_DAY_OFFSET should map below day 0.
 * * */
#define MAX_DAY_OFFSET ((INT_MAX-1)/86400)

typedef struct arguments {
    enum { UNDEFINED_MODE, INDEXING_MODE, SEARCH_MODE, PAYLOAD_MODE, INFO_MODE } mode;
    const char *input_file;
    const char *output_file;
    const char *index_folder;
    const char *title;
#ifdef WITH_TOPIC_OPTION
    const char *topic;
#endif
    int32_t  beginMin;
    int32_t  beginMax;
    int32_t  dayMin;
    int32_t  dayMax;
    int32_t  durationMin;
    int32_t  durationMax;
    int32_t  channelNr;
    /*const*/ char *channelName;
    uint32_t *payload_anchors;
    uint32_t payload_anchors_len;
    uint32_t max_num_results;
    uint32_t skiped_num_results;
    int32_t reversed_results;
    int32_t diff_update;
    const char *sort_by;
} arguments_t;

/* Initialize argument list */
arguments_t default_arguments()
;

/* Uninitialize argument list.
 * Call this to free some interal string buffer.*/
void clear_arguments(
 arguments_t *p_arguments)
;

/* Normalization rules:
 * - [begin, end)-pairs both -1 or both != -1
 * - [begin, end)-pairs sorted.
 * - Unset values replaced by -1
 * - Unset strings replaced by NULL pointer.
*/
void normalize_args(
        arguments_t *p_arguments)
;

/* Sort entries by file number to avoid re-opening
 * of the same file. */
void sort_payload_anchors(
        arguments_t *p_arguments)
;

arguments_t handle_arguments(
        int argc,
        char *argv[])
;
