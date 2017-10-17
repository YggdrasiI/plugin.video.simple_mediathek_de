#pragma once

#include <stdarg.h>
#include <limits.h>
#include "settings.h"
#include "params.h"
#include "parser.h"
#include "channels.h"
#include "linked_list.h"

// Brotli
#ifdef COMPRESS_BROTLI
#include "brotli.h"
#endif

/* Convert 'title|topic'-string at indexing time into lower case and
 * remove subset of special characters like -,_,|,\,".
 *
 * Moreover, the original title will added to the title_XXXX.br files
 * which increased its size (currently from 3.3MB to 4.5MB)
 */
#define NORMALIZED_STRINGS 1


/* Current indices of input. (Index 0 is "X"-key of json-dict) */
#define INDEX_KEY 0
#define INDEX_SENDER 1
#define INDEX_THEMA 2
#define INDEX_TITEL 3
#define INDEX_DATUM 4
#define INDEX_ZEIT 5
#define INDEX_DAUER 6
#define INDEX_GROESSE 7
#define INDEX_BESCHREIBUNG 8
#define INDEX_URL 9
#define INDEX_WEBSITE 10
#define INDEX_UNTERTITEL 11
#define INDEX_URLRTMP 12
#define INDEX_URL_KLEIN 13
#define INDEX_URLRTMP_KLEIN 14
#define INDEX_URL_HD 15
#define INDEX_URLRTMP_HD 16
#define INDEX_DATUML 17
#define INDEX_URL_HISTORY 18
#define INDEX_GEO 19
#define INDEX_NEU 20

#if 0
/* For indexing files lists of anchors will be stored. */
typedef struct {
  uint8_t file; // file number
  uint32_t index;  // entry index in file
  uint32_t seek;  // seek position in file
} anchor_t;

typedef struct {
    size_t len;
    size_t used;
    anchor_t *first;
} range_t;

typedef struct {
    /* Split duration into ranges
     * [0, duration_borders[0]],
     * [duration_borders[0] + 1, duration_borders[1]],
     * ...
     * [duration_borders[N] + 1, INT_MAX ]
     *
     * (in seconds)
     * */
    int duration_borders[NUM_DURATIONS];
    range_t durations[NUM_DURATIONS];
    int timestamp_borders[NUM_TIME];
    range_t timestamps[NUM_TIME];
    size_t date_anchor; // time stamp of day 0 (newest), day 1 is previous day and so on...
    range_t relative_dates[NUM_REALTIVE_DATE];

    //size_t num_channels;
    range_t channels[NUM_CHANNELS];
} index_data_t_old;

typedef struct {
    int duration_borders[NUM_DURATIONS];
    range_t durations[NUM_DURATIONS];
    int timestamp_borders[NUM_TIME];
    range_t timestamps[NUM_TIME];
    size_t date_anchor; // time stamp of day 0 (newest), day 1 is previous day and so on...
    range_t relative_dates[NUM_REALTIVE_DATE];

    //size_t num_channels;
    range_t channels[NUM_CHANNELS];
} index_data_t;
#endif

typedef struct {
    int fd;
    int id;  // used for payload file index
    //size_t item; // TODO: Remove because never used
    size_t seek;
} file_data_t;

/* Raw data written before each entry of title_*.index */
typedef struct {
    int32_t relative_start_time;
#if 0
    int32_t duration;
    //int32_t UNUSED_length; // (topic_len + title_len + 1)
    int32_t topic_string_offset;
#else
    uint16_t duration;
    //uint16_t UNUSED_length; // TODO: Not required due '\0' after string?!
    /* Offset is (title_len + 1) if topic string set or
     * distance to topic_string of previous entry.
     *
     * - Handling of offsets which can not be represended within 16 bits:
     *     Force inserting of topic string.
     * - All entries between 'this' entry and the 'offset entry' share
     *   the same topic string. Thus at buffer flushing only one string 
     *   had to be cached...
     * */
    int16_t topic_string_offset;
#endif
} searchable_strings_prelude_t;

/* Pointer into buffer string. If buffer is deleted, a copy of the substring
 * should be saved for further usage. */
typedef struct {
    size_t target_len;
    const char *target;
    size_t _copy_size;  // called 'size' because it holds 'strlen(_copy)<= _copy_size'
    char *_copy;
} buf_string_copy_t;

typedef struct filmliste_workspace_s {
    /* Timestamps for index file */
    time_t tcreation;  // Time stamp where this program run.
    time_t ttoday;  // Time stamp for which will be stored (relative).
    size_t itoday; // Days since 1970-01-01 (localtime) => itoday*86400 <= tcreation
    int today_time_zone_offset; // GTM+1 + opt. day light offset

    time_t list_creation_time; // Creation time of json-input (-1 until fetched from parser)
    file_data_t payload;
    file_data_t searchable_strings; // Title + Topic
    //size_t searchable_strings_len;
    char *_buf;
    char_buffer_t payload_buf;
    struct tm tmp_ts; // time stamp
    channels_ws_t channels;
    /* Entries with empty channel name
     * inherit channel from previous entry.
     */
    int previous_channel;
    //index_data_t index;
    linked_list_t index;
    int index_fd;
    const char *index_folder;
    buf_string_copy_t prev_topic;
    // Seek position of topic string MINUS sizeof(start_dur_len)
    // (The adding of sizeof(...) was stripped out.)
    uint32_t prev_topic_seek;
#ifdef COMPRESS_BROTLI
    brotli_encoder_workspace_t brotli_title;
    brotli_encoder_workspace_t brotli_payload;
#endif
    arguments_t *p_arguments;
} filmliste_workspace_t;

/* Create and init main work space struct.
 * p_arguments - Optional handler with parsed program arguments.
 */
filmliste_workspace_t filmliste_ws_create(
        arguments_t *p_arguments)
;

void filmliste_ws_destroy(filmliste_workspace_t *p_fl_ws)
;

#if 0
index_data_t index_data_create(
        size_t date_anchor)
;

void index_data_destroy(
        index_data_t *p_index)
;

void add_anchor(
        const char *debug, int debug_index,
        range_t *p_r,
        anchor_t *a,
        size_t init_val)
;

/* Add entry to several index arrays. */
int update_index(
        filmliste_workspace_t *p_fl_ws,
        int channel_nr,
        time_t day_ctime,
        size_t start,
        //time_t start_time, /* exact start time  (thus, date + time)*/
        size_t duration)
;
#endif

int create_index_file(
        filmliste_workspace_t *p_fl_ws)
;

// Meta data about creation time, etc
void write_index_header(
        filmliste_workspace_t *p_fl_ws)
;

void write_index_footer(
        filmliste_workspace_t *p_fl_ws)
;

int update_index2(
        filmliste_workspace_t *p_fl_ws,
        int channel_nr,
        time_t day_ctime,
        size_t start,
        size_t duration)
;

/* Close old file handler and start new one. Returns 0 on success.*/
int init_next_payload_file(
        filmliste_workspace_t *p_fl_ws)
;

/* Close old file handler and start new one. Returns 0 on success.*/
int init_next_searchable_file(
        filmliste_workspace_t *p_fl_ws)
;

/* Parsing of content
 * and storage into output struct.
 */
void filmliste_handle(
//        const int fd,
        const char_buffer_t *p_buf_in,
        filmliste_workspace_t *p_fl_ws,
        search_pair_t **start,
        search_pair_t **end,
        char_buffer_t *p_buf_out)
;

/* Cached write into fd.
 *
 * Uses snprintf to print into buffer 'buf' and flush it
 * if not enough space is available.
 * The buffer should be big enough that snprintf never fails
 * for an empty buffer.
 *
 * Note that multiple calls of buf_snprintf concatenate the output
 * strings. Add an explicit '\0' as last argument to omit this behavior.
 */
size_t buf_snprintf(
        int fd,
        char_buffer_t *buf,
#ifdef COMPRESS_BROTLI
        brotli_encoder_workspace_t *brotli,
#endif
        const char *format, ...)
;

size_t buf_write(
        int fd,
        char_buffer_t *p_buf,
#ifdef COMPRESS_BROTLI
        brotli_encoder_workspace_t *brotli,
#endif
        size_t len_required, void *arr)
;

/* Delete differential files to prevent mixing
 * of new and outdated files.
 *
 * Return number of deleted files or
 * -1 on error.
 */
int remove_old_diff_files(
        arguments_t *p_arguments)
;

void filmliste_cache_data_before_buffer_clears(
        const char_buffer_t *p_buf_in,
        filmliste_workspace_t *p_fl_ws)
;
