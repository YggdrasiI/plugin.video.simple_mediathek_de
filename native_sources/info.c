#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "settings.h"
#include "helper.h"
#include "search.h"
#include "hashmap.h"
#include "channels.h"
#include "open.h"
#include "info.h"

int info_read(
        search_workspace_t *p_i_ws)
{
    // Read first bytes (creation date info, etc)
    if( read_index_header(p_i_ws) ){
      fprintf(stderr, "(info.c) Can not read header of index file. File empty?!\n");
      return -1;
    }

    // Skip the main part
    //linked_list_read(s_ws.index_fd, &s_ws.index);

    // Seek end of file and read length of channel data
#ifdef CHECK_ERRNO
    errno = 0;
#endif
    off_t s = lseek(p_i_ws->index_fd, -sizeof(int32_t), SEEK_END);
#ifdef CHECK_ERRNO
    if( errno != 0 ){
        int x = errno;
        fprintf(stderr, "Error: %s\n", strerror(x));
    }
#endif

    if( (off_t)-1 == s ){
        fprintf(stderr, "(info.c) Seek in index file failed.\n");
        return -1;
    }

    int32_t bytes_to_read = -1;
    ssize_t r = read(p_i_ws->index_fd, &bytes_to_read, sizeof(int32_t));
    assert( r == sizeof(int32_t) );
    _unused(r);

    if( bytes_to_read < 0 ){
        fprintf(stderr, "(info.c) Reading of channel data failed.\n");
        return -1;
    }

    // Seek to begin of channel data and read channels
    // ... | sizeof(int32_t) | X | sizeof(int32_t) | SEEK_END
    //      ^^
    //     Seek to this position
    lseek(p_i_ws->index_fd, -(2 * sizeof(int32_t) + bytes_to_read), SEEK_END);
    int32_t len_channel_data = read_channel_list(
            p_i_ws->index_fd, &p_i_ws->channels);

    assert( len_channel_data == bytes_to_read );
    _unused(len_channel_data);

#if 1
    /* Read length of channel data, again
     * (just to mirror write_index_footer() behavior)
     */
    int32_t bytes_was_read;
    ssize_t n = read(p_i_ws->index_fd, &bytes_was_read, sizeof(int32_t));
    assert( n == sizeof(int32_t) );
    assert( len_channel_data == bytes_was_read );
    _unused(n);
#endif

    return 0;
}

typedef struct {
    int len_names;
    const char **names;
} sort_item_t;

int it_sort_names(any_t item, any_t data){
  data_struct_t* p_channel_val = (data_struct_t *)data;
  sort_item_t *p_out = (sort_item_t *)item;
  int i = p_channel_val->number;
  if( i >= 0 && i < p_out->len_names)
  {
      p_out->names[i] = p_channel_val->key_string;
  }
  return MAP_OK;
}

void _info_print(
        int fd,
        time_t index_creation_day,
        time_t list_creation_day,
        channels_ws_t *p_channels)
{
    dprintf(fd, "{\n");
    // 1. Creation date

    time_t absolute_day = index_creation_day;
    struct tm * timeinfo;
    char absolute_day_str[40];
    timeinfo = localtime (&absolute_day);
    strftime (absolute_day_str, 40, "%d. %b. %Y %R", timeinfo);

    char list_day_str[40];
    timeinfo = localtime (&list_creation_day);
    strftime (list_day_str, 40, "%d. %b. %Y %R", timeinfo);

    dprintf(fd, "  \"icreation\": %li,\n  \"creation\": \"%s\",\n" \
            "  \"ilistcreation\": %li,\n  \"listcreation\": \"%s\",\n",
            absolute_day, absolute_day_str,
            list_creation_day, list_day_str);

    // 2. List of channels
    dprintf(fd, "  \"channels\": {\n");
    int num_chan = hashmap_length(p_channels->map);

    /* Loop over hash map and order names.
     * Assume that channel numbers are [0, num_chan-1]
     */
    const char **names = (const char **)calloc(num_chan, sizeof(const char *));
    sort_item_t item = {num_chan, names};
    int i, ifirst=1;

    hashmap_iterate(p_channels->map, &it_sort_names, (any_t)&item);

    for(i = 0; i<num_chan; ++i){
        if( i == UNKNOWN_CHANNEL ) continue;
        if( names[i] == NULL ) continue;

        // Note: JSON-Data keys had do be strings.
        dprintf(fd, "%s\"%i\": \"%s\"",
                (ifirst)?"    ":",\n    ",
                i, names[i]);
        ifirst = 0;
    }
    dprintf(fd, "\n  }");

    // Closing
    dprintf(fd, "\n}");
    Free(names);
}

void info_print(
        int fd,
        search_workspace_t *p_i_ws)
{
    _info_print(fd,
            p_i_ws->tcreation,
            p_i_ws->list_creation_time,
            &p_i_ws->channels);
}

void info_print2(
        int fd,
        filmliste_workspace_t *p_fl_ws)
{
    _info_print(fd,
            p_fl_ws->tcreation,
            p_fl_ws->list_creation_time,
            &p_fl_ws->channels);
}

