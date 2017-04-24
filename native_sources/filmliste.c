#include "helper.h"
#include "open.h"
#include "filmliste.h"

#define _POSIX_C_SOURCE 200112L // for setenv on gcc

filmliste_workspace_t filmliste_ws_create(
        arguments_t *p_arguments)
{

    setenv("TZ", "/usr/share/zoneinfo/Europe/Berlin", 1); // POSIX-specific, for localtime()
    time_t tnow = time(NULL);
    struct tm tm_now = *localtime(&tnow);

    int clock_offset = 3600 + (1 == tm_now.tm_isdst?3600:0); // GTM+1 + daylight saving
    //tnow = (tnow / 86400) * 86400; // Rounding nearby begin of day
    //size_t nlocalday = (tnow + clock_offset) / 86400;

    size_t nlocalday = (tnow + clock_offset) / 86400;  // NDay relative to current time zone
    time_t tlocalday_begin = nlocalday * 86400 - clock_offset; // Begin of day in current time zone

    filmliste_workspace_t fl_ws = {
        tnow, // tcreation
        tlocalday_begin, // ttoday
        nlocalday, // itoday
        clock_offset,

        -1, // list_creation_time
        {-1, -1, 0}, // payload
        {-1, -1, 0}, // searchable_strings
        //0, // searchable_strings_len
        (char *) malloc(OUT_CACHESIZ),
        {OUT_CACHESIZ, 0, NULL}, // payload_buf
        tm_now, // tmp_ts
        channels_ws_create(), // channels
        0,
        linked_list_create(tlocalday_begin),
        -1, // index_fd
        NULL, // index_folder
#ifdef COMPRESS_BROTLI
        brotli_encoder_ws_create(BROTLI_COMPRESS_QUALITY),
        brotli_encoder_ws_create(BROTLI_COMPRESS_QUALITY),
#endif
        p_arguments,
    };

    fl_ws.payload_buf = (char_buffer_t){OUT_CACHESIZ, 0, fl_ws._buf};

    // Due leap-seconds, etc required
    fl_ws.tmp_ts.tm_sec = 0;
    fl_ws.tmp_ts.tm_min = 0;
    fl_ws.tmp_ts.tm_hour = 0;

    // Toggle evaluation of daylight saving time at next call of mktime
    // This sets the flag to 0 or 1 and will be used as base for all other
    // evaluations. The flag should be reset to -1 in some cases...
    fl_ws.tmp_ts.tm_isdst = -1;

    if( p_arguments ){
      fl_ws.index_folder = p_arguments->index_folder;

      if( p_arguments->diff_update ){
          fl_ws.payload.id = (FIRST_DIFF_INDEX-1);
          fl_ws.searchable_strings.id = (FIRST_DIFF_INDEX-1);
      }
    }else{
      fl_ws.index_folder = index_folder; // Fallback value
    }

    return fl_ws;
}

void filmliste_ws_destroy(
        filmliste_workspace_t *p_fl_ws)
{
    p_fl_ws->payload_buf.used = 0;
    p_fl_ws->payload_buf.p = NULL;
    free(p_fl_ws->_buf);
    p_fl_ws->_buf = NULL;

    //free(p_fl_ws->tmp);
    //p_fl_ws->tmp = NULL;

    channels_ws_destroy(&p_fl_ws->channels);
    //index_data_destroy(&p_fl_ws->index);
    linked_list_destroy(&p_fl_ws->index);

#ifdef COMPRESS_BROTLI
    brotli_encoder_ws_destroy(&p_fl_ws->brotli_title);
    brotli_encoder_ws_destroy(&p_fl_ws->brotli_payload);
#endif
}

int create_index_file(
        filmliste_workspace_t *p_fl_ws)
{
    if( p_fl_ws->index_fd > -1 ){
        close(p_fl_ws->index_fd);
        p_fl_ws->index_fd = -1;
    }

    assert(p_fl_ws->index_folder != NULL);

    //Currently, fixed name.
    size_t ts = strlen(p_fl_ws->index_folder) + sizeof(index_file_template) + 10;
    char *tmp = (char *) malloc(ts * sizeof(char));
    const int diff = p_fl_ws->p_arguments->diff_update;
    if( !tmp ) return -1;

    snprintf(tmp, ts, index_file_template,
            p_fl_ws->index_folder, (diff?diff_ext:""));
    DEBUG( fprintf(stderr, "Open '%s'\n", tmp); )

    p_fl_ws->index_fd = open(tmp, O_CREAT|O_WRONLY|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

    free(tmp);
    assert( p_fl_ws->index_fd > -1 );

    return (p_fl_ws->index_fd > 0)?0:-1;
}

void write_index_header(
        filmliste_workspace_t *p_fl_ws)
{
    assert( p_fl_ws && p_fl_ws->index_fd > 0);
    /* Note: sizeof(time_t) could be 4 or 8 */
    uint32_t data[] = {(uint32_t)p_fl_ws->tcreation,
        (uint32_t)p_fl_ws->itoday,
        (uint32_t)p_fl_ws->list_creation_time,
    };
    if( p_fl_ws->index_fd > 0 ){
        ssize_t w = write(p_fl_ws->index_fd, &data, sizeof(data));
        assert( (ssize_t)sizeof(data) == w);
        _unused(w);
    }
}

// Data which is only available after indexing finished (channel names)
void write_index_footer(
        filmliste_workspace_t *p_fl_ws)
{
    // Write seek position of title file after end (uncompressed file size).
    uint32_t seek = (uint32_t)p_fl_ws->searchable_strings.seek; //size_t -> uint32_t
    ssize_t w = write( p_fl_ws->index_fd, &seek, sizeof(uint32_t));
    assert( sizeof(seek) == w );
    _unused(w);

    assert( p_fl_ws && p_fl_ws->index_fd > 0);

    int32_t chan_data_length = write_channel_list( p_fl_ws->index_fd, &p_fl_ws->channels);
    assert( chan_data_length >= 0 );

    /* Write length of channel data, again.
     * Required to seek the channel list begin from the end of the file.
     */
    ssize_t end = write( p_fl_ws->index_fd, &chan_data_length, sizeof(int32_t));
    assert( (size_t)sizeof(int32_t) == end );
    _unused(end);

}


int update_index2(
        filmliste_workspace_t *p_fl_ws,
        int channel_nr,
        time_t day_ctime,
        size_t start,
        size_t duration)
{
  assert( channel_nr < NUM_CHANNELS);
  assert( channel_nr >= 0 );

  linked_list_t *p_index = &p_fl_ws->index;

  //int time = (int) start_time % 86400;
  //int day = (int) start_time / 86400;
  //int index_day = p_fl_ws->itoday - (day_ctime / 86400); //GTM Day border.
  int index_day = p_fl_ws->itoday - ( (day_ctime+3600) / 86400); //GTM+1 Day border.
  index_day = clip(0, index_day, NUM_REALTIVE_DATE-1);

  linked_list_subgroup_indizes_t indizes = {{{
      index_day,
      NUM_TIME-1,
      NUM_DURATIONS-1,
      channel_nr
  }}};

  int i;
  // Search array indices
  for( i=0; i<NUM_TIME-1; ++i ){
      if( start <= p_index->meta.timestamp_borders[i] ){
          indizes.i_timestamp = i;
          break;
      }
  }

  for( i=0; i<NUM_DURATIONS-1; ++i ){
      if( duration <= p_index->meta.duration_borders[i] ){
          indizes.i_duration = i;
          break;
      }
  }

  index_node_t *p_data = p_index->next_unused;
  // Info about target position (value)
  p_data->link.payload_file_id = p_fl_ws->payload.id;
  p_data->link.payload_seek = p_fl_ws->payload.seek;
  p_data->link.title_seek = p_fl_ws->searchable_strings.seek;
  //p_data->link.index  = p_fl_ws->payload.item;

  linked_list_push_item(p_index, &indizes, p_data);

  return 0;
}

int init_next_payload_file(
        filmliste_workspace_t *p_fl_ws)
{
    int fd = p_fl_ws->payload.fd;
    if( fd > -1 ){
        if( fd <= 2 ){
            //stdin, stdout or stderr should not be closed
            fprintf(stderr, "Close of file descriptor %i requested, but ignored\n", fd);
            return -1;
        }
        close(fd);
        fd = -1;
        p_fl_ws->payload.fd = fd;
    }

    // Construct file name
    p_fl_ws->payload.id++;
    assert( p_fl_ws->index_folder != NULL);
    size_t ts = strlen(p_fl_ws->index_folder) + sizeof(payload_file_template) + 10;
    char *tmp = (char *) malloc(ts * sizeof(char));
    const int diff = p_fl_ws->p_arguments->diff_update;
    assert( diff == 0 || p_fl_ws->payload.id >= FIRST_DIFF_INDEX );
    _unused(diff);

    if( !tmp ){
        return -1;
    }

    snprintf(tmp, ts, payload_file_template,
            p_fl_ws->index_folder, p_fl_ws->payload.id); //, (diff?diff_ext:""));
    DEBUG( fprintf(stderr, "Open '%s'\n", tmp); )

    fd = open(tmp, O_CREAT|O_WRONLY|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

    p_fl_ws->payload.fd = fd;
    //p_fl_ws->payload.item = 0;
    p_fl_ws->payload.seek = 0;

    assert( p_fl_ws->payload.fd > -1 );

    free(tmp);
    return (p_fl_ws->payload.fd > 0)?0:-1;
}

/* (Only used once ) */
int init_next_searchable_file(
        filmliste_workspace_t *p_fl_ws)
{
    int fd = p_fl_ws->searchable_strings.fd;
    if( fd > -1 ){
        if( fd <= 2 ){
            //stdin, stdout or stderr should not be closed
            fprintf(stderr, "Close of file descriptor %i requested, but ignored\n", fd);
            return -1;
        }
        close(fd);
        fd = -1;
        p_fl_ws->searchable_strings.fd = fd;
    }

    // Construct file name
    p_fl_ws->searchable_strings.id++;
    assert( p_fl_ws->index_folder != NULL);
    assert( p_fl_ws->index_folder != NULL);
    size_t ts = strlen(p_fl_ws->index_folder) + sizeof(strings_file_template) + 10;
    char *tmp = (char *) malloc(ts * sizeof(char));
    const int diff = p_fl_ws->p_arguments->diff_update;
    if( !tmp ){
        return -1;
    }

    snprintf(tmp, ts, strings_file_template,
            p_fl_ws->index_folder, p_fl_ws->searchable_strings.id, (diff?diff_ext:""));
    DEBUG( fprintf(stderr, "Open '%s'\n", tmp); )

    fd = open(tmp, O_CREAT|O_WRONLY|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

    p_fl_ws->searchable_strings.fd = fd;
    //p_fl_ws->searchable_strings.item = 0;
    p_fl_ws->searchable_strings.seek = 0;

    assert( p_fl_ws->searchable_strings.fd > -1 );

    free(tmp);
    return (p_fl_ws->searchable_strings.fd > 0)?0:-1;
}

void filmliste_handle(
//        const int fd,
        const char_buffer_t *p_buf_in,
        filmliste_workspace_t *p_fl_ws,
        search_pair_t **start,
        search_pair_t **end,
        char_buffer_t *p_buf_out)
{
    assert( end-start >= 21 );
    assert( p_fl_ws != NULL );
    assert( p_fl_ws->payload_buf.p != NULL );

    // skip if first key is not "X"
    const char *first_group;
    size_t len = search_pair_get_chars(p_buf_in, *(start+INDEX_KEY), &first_group, NULL, 1);

    if( len != 1 || *(first_group + 0) != 'X' ){
        DEBUG( \
                *((char*)first_group + len) = '\0'; \
                fprintf(stderr, "Key skipped: len=%i s=%s\n", (int)len, first_group); \
             )

        return;
    }

    size_t channel_len, topic_len, title_len;
    const char *channel_str;
    const char *topic_str;
    const char *title_str;
    //const char *description_str;

    // #Channel
    channel_len = search_pair_get_chars(p_buf_in, *(start+INDEX_SENDER),
            &channel_str, NULL, 1);
    int channel_nr;
    if(channel_len == 0){
        channel_nr = p_fl_ws->previous_channel;
    }else{
        // Normalize channel name. Cheap operation because
        // channel name is mostly an empty string.
        char *channel_str2 = NULL;
        transform_channel_name(channel_str, &channel_str2);
        channel_nr = get_channel_number(&p_fl_ws->channels, channel_str2, 1);
        Free(channel_str2);
        p_fl_ws->previous_channel = channel_nr;
    }

    // #Thema
    topic_len = search_pair_get_chars(p_buf_in, *(start+INDEX_THEMA),
            &topic_str, NULL, 1);

    // #Titel
    title_len = search_pair_get_chars(p_buf_in, *(start+INDEX_TITEL),
            &title_str, NULL, 1);

    // #Day and #Start
    time_t start_time = parse_starttime(p_buf_in, *(start+INDEX_DATUML) );

    // Other approach for same value
#if 0
    size_t start_second = parse_dauer(p_buf_in, p_fl_ws, *(start+INDEX_ZEIT) );
    time_t start_time2 = parse_day(p_buf_in, p_fl_ws, *(start+INDEX_DATUM));
    if( start_time != start_time2 ){
        search_pair_t *tmp_sp = *(start+INDEX_DATUML);

        fprintf(stderr, " Inconsistent timestamp: %s, day light saving flag: %i\n",
                title_str, p_fl_ws->tmp_ts.tm_isdst);
        fprintf(stderr, " Starttime 1: %i ( %s",
                (int)start_time, ctime(&start_time));
        fprintf(stderr, " Starttime 2: %i ( %s",
                (int)start_time2, ctime(&start_time2));
    }
    //assert( start_time == start_time2 );
#else
    /* Do not parse given timestamp string, but use ctime string
     * We need to respect GTM+1 and daylight saving hour.
     *
     * Nearby clock change this estimates the wrong time/day.
     */
    size_t start_second = (start_time + p_fl_ws->today_time_zone_offset) % 86400;
    size_t start_time2 = start_time;
#endif

    // Subtract base offset to shorten string of number
    int32_t relative_start_time = start_time - p_fl_ws->ttoday;

    // #Start
    size_t duration = parse_dauer(p_buf_in, p_fl_ws, *(start+INDEX_DAUER) );


    // Required
    /* Save following columns:
      "Url", "UrlRTMP",
      "Url_Klein", "UrlRTMP_Klein",
      "Url_HD", "UrlRTMP_HD",

      But not
      "DatumL", "Beschreibung",
      */
    size_t urls_len[6];
    const char *urls_str[6];

    urls_len[0] = search_pair_get_chars(p_buf_in, *(start+INDEX_URL), &urls_str[0], NULL, 1);
    urls_len[1] = search_pair_get_chars(p_buf_in, *(start+INDEX_URLRTMP), &urls_str[1], NULL, 1);
    urls_len[2] = search_pair_get_chars(p_buf_in, *(start+INDEX_URL_KLEIN), &urls_str[2], NULL, 1);
    urls_len[3] = search_pair_get_chars(p_buf_in, *(start+INDEX_URLRTMP_KLEIN), &urls_str[3], NULL, 1);
    urls_len[4] = search_pair_get_chars(p_buf_in, *(start+INDEX_URL_HD), &urls_str[4], NULL, 1);
    urls_len[5] = search_pair_get_chars(p_buf_in, *(start+INDEX_URLRTMP_HD), &urls_str[5], NULL, 1);
    _unused(urls_len);

#if NORMALIZED_STRINGS > 0
    // Normalize title and topic string
    char *title_str_norm = NULL;
    char *topic_str_norm = NULL;
    title_len = transform_search_title(title_str, &title_str_norm);
    topic_len = transform_search_title(topic_str, &topic_str_norm);
#endif

    update_index2(p_fl_ws,
            channel_nr,
            start_time2 - start_second,
            start_second,
            duration
            );

    if( p_fl_ws->searchable_strings.fd > 0 ){
        /* Write searchable data into first buffer */
#if 1
        //Prepend with raw data
        searchable_strings_prelude_t start_dur_len= {
            (int32_t)relative_start_time,
            //(int32_t)duration,
            //(int32_t)(topic_len + title_len + 1)
            /* Packing reduces filesize by approx. 10% */
            (uint16_t)(duration),
            (uint16_t)(topic_len + 1 + title_len) /* %s|%s */
        };
        p_fl_ws->searchable_strings.seek += buf_write(
                p_fl_ws->searchable_strings.fd,
                p_buf_out,
#ifdef COMPRESS_BROTLI
                &p_fl_ws->brotli_title,
#endif
                sizeof(start_dur_len), (void *)&start_dur_len);
#endif

        p_fl_ws->searchable_strings.seek += buf_snprintf(
                p_fl_ws->searchable_strings.fd,
                p_buf_out,
#ifdef COMPRESS_BROTLI
                &p_fl_ws->brotli_title,
#endif
#if NORMALIZED_STRINGS > 0
                "%s|%s%c%s%c",
                title_str_norm, topic_str_norm, '\0',
                title_str, '\0'
#else
                "%s|%s%c",
                title_str, topic_str, '\0'
#endif
                );
        //p_fl_ws->searchable_strings.item += 1;
    }

#if NORMALIZED_STRINGS > 0
    Free(title_str_norm);
    Free(topic_str_norm);
#endif

    // Write payload in second buffer
    if( p_fl_ws->payload.fd > 0 )
    {
        //p_fl_ws->payload.item += 1;
        p_fl_ws->payload.seek += buf_snprintf(
                p_fl_ws->payload.fd,
                &p_fl_ws->payload_buf,
#ifdef COMPRESS_BROTLI
                &p_fl_ws->brotli_payload,
#endif
                ",[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]\n",
                urls_str[0], urls_str[1], urls_str[2], urls_str[3], urls_str[4], urls_str[5]);


#ifdef COMPRESS_BROTLI
        if( p_fl_ws->brotli_payload.bytes_written > PAYLOAD_MAX_FILE_SIZE ){
            brotli_write_buffer(p_fl_ws->payload.fd, &p_fl_ws->brotli_payload, &p_fl_ws->payload_buf, 1);
            assert( 0 == p_fl_ws->payload_buf.used );
            brotli_encoder_ws_destroy(&p_fl_ws->brotli_payload);
            p_fl_ws->brotli_payload = brotli_encoder_ws_create(BROTLI_COMPRESS_QUALITY);
            init_next_payload_file(p_fl_ws);
        }
#else
        if( p_fl_ws->payload.seek > PAYLOAD_MAX_FILE_SIZE ){
            search_array_flush(p_fl_ws->payload.fd, &p_fl_ws->payload_buf);
            init_next_payload_file(p_fl_ws);
        }
#endif
    }

    // Write into index file if enough data available
    if( p_fl_ws->index_fd > 0 &&
            p_fl_ws->index.mem_allocated > ((LINKED_LIST_ARR_LEN*sizeof(index_node_t)) << 2) ){
        linked_list_write_partial( p_fl_ws->index_fd, &p_fl_ws->index, 0);
    }

}

size_t buf_snprintf(
        int fd,
        char_buffer_t *p_buf,
#ifdef COMPRESS_BROTLI
        brotli_encoder_workspace_t *brotli,
#endif
        const char *format, ...)
{
    va_list ap;

    int try = 2; // First try without flush of buffer, then with flush.
    while( try-- > 0 ){
        int len_available = p_buf->len - p_buf->used;
        assert( len_available >= 0 );
        va_start(ap, format);
        int len_required = vsnprintf(p_buf->p + p_buf->used, len_available, format, ap);
        va_end(ap);

        if( len_required < 0 ){
            // Encoding error.
            fprintf(stderr, "Encoding error during write of string. ERRNO: %i.\n", len_required);
            return 0;
        }else if( len_required < len_available){
            // Write was successful.
            p_buf->used += len_required;
            return (size_t) len_required;
        }else{
            /* Not enough space in buffer. Flush buffer and try again
             * (if empty buffer long enough.) */
#ifdef COMPRESS_BROTLI
            brotli_write_buffer(fd, brotli, p_buf, 0);
#else
            search_array_flush(fd, p_buf);
#endif
            if( len_required > p_buf->len ){
                fprintf(stderr, "Caching of %i bytes failed. Buffer too small!\n", len_required);
                return 0;
            }
        }
    }
  return 0; //should never be reached.
}

size_t buf_write(
        int fd,
        char_buffer_t *p_buf,
#ifdef COMPRESS_BROTLI
        brotli_encoder_workspace_t *brotli,
#endif
        size_t len_required, void *arr)
{

    int len_available = p_buf->len - p_buf->used;
    assert( len_available >= 0 );
    if( len_required > len_available){
        /* Not enough space in buffer. Flush buffer and try again
         * (if empty buffer long enough.) */
#ifdef COMPRESS_BROTLI
        brotli_write_buffer(fd, brotli, p_buf, 0);
#else
        search_array_flush(fd, p_buf);
#endif
        len_available = p_buf->len - p_buf->used;
        if( len_required > p_buf->len ){
            fprintf(stderr, "Caching of %u bytes failed. Buffer too small!\n", (uint32_t) len_required);
            return 0;
        }
    }

    memcpy(p_buf->p + p_buf->used, arr, len_required);
    p_buf->used += len_required;

    return len_required;
}

int remove_old_diff_files(
        arguments_t *p_arguments)
{
    int n_del = 0;
    int di; // index of current file

    const char *folder = (p_arguments->index_folder != NULL)
        ?(p_arguments->index_folder):(index_folder);

    assert(folder != NULL);
    if( folder == NULL ) return -1;

    // 1. Delete index file
    size_t ts = strlen(folder) + sizeof(index_file_template) + 10;
    char *tmp = (char *) malloc(ts * sizeof(char));
    if( !tmp ) return -1;

    snprintf(tmp, ts, index_file_template,
            folder, diff_ext);

    DEBUG( fprintf(stderr, "Delete '%s'\n", tmp); )
    if( 0 == unlink(tmp) ){
        ++n_del;
    }
    Free(tmp);

    // 2. Delete title file(s)
    di = FIRST_DIFF_INDEX;
    ts = strlen(folder) + sizeof(strings_file_template) + 10;
    tmp = (char *) malloc(ts * sizeof(char));
    if( !tmp ) return -1;
    while(1){
        snprintf(tmp, ts, strings_file_template,
                folder, di, diff_ext);
        DEBUG( fprintf(stderr, "Delete '%s'\n", tmp); )

            if( unlink(tmp) ){ // No further files...
                break;
            }else{
                ++di;
                ++n_del;
            }
    }
    Free(tmp);


    // 3. Delete payload file(s)
    di = FIRST_DIFF_INDEX;
    ts = strlen(folder) + sizeof(payload_file_template) + 10;
    tmp = (char *) malloc(ts * sizeof(char));
    if( !tmp ) return -1;

    while(1){
        snprintf(tmp, ts, payload_file_template,
                folder, di);
        DEBUG( fprintf(stderr, "Delete '%s'\n", tmp); )

            if( unlink(tmp) ){ // No further files...
                break;
            }else{
                ++di;
                ++n_del;
            }
    }
    Free(tmp);
    return n_del;
}
