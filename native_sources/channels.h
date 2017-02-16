#pragma once

#include "hashmap.h"

// For list of channels
#define KEY_MAX_LENGTH (64)
#define KEY_PREFIX ("channelX")
#define KEY_COUNT (128)

typedef struct data_struct_s
{
    char key_string[KEY_MAX_LENGTH];
    int number;
} data_struct_t;


// work space struct
typedef struct channels_ws_s
{
    map_t map;
    char tmp_key[KEY_MAX_LENGTH];
    data_struct_t* tmp_value;
} channels_ws_t;

channels_ws_t channels_ws_create()
;

void channels_ws_destroy(
        channels_ws_t *p_ws)
;

int add_channel(
        channels_ws_t *p_channels,
        const char *chan)
;

/* Unique channel number.
 *
 * Return -1 channel not found.
 */
int get_channel_number(
        channels_ws_t *p_channels,
        const char *chan,
        int add_missing)
;

/* Find key for channel number.
 * (Linear search...)
 *
 * Return NULL if channel not exists.
 */
const char *get_channel_name(
        channels_ws_t *p_channels,
        int channel_number)
;

/* Writes
 * X, [ (C, S), [...]]
 *   with
 * X - length of the following array in bytes,      int32_t
 * C - channel number                               int32_t
 * S - ends with \0 and length <= KEY_MAX_LENGTH    char-array
 *
 * Return value: X or -1
 *
 * Return value -1 indicate write error.
 * Return value  0 indicate an empty list and is also uncommon.
 */
int32_t write_channel_list(
        int fd,
        channels_ws_t *p_ws)
;

/* Counterpart of write_channel_list
 *
 * return X aka length of read channel data.
*/
int32_t read_channel_list(
        int fd,
        channels_ws_t *p_ws)
;
