#define _GNU_SOURCE // for strcasestr

#include "utf8.h"
#include "helper.h"
#include "open.h"
#include "search.h"

/* Adaption of filmliste_ws_create */
search_workspace_t search_ws_create(
        arguments_t *p_arguments)
{
    assert( p_arguments != NULL );

    setenv("TZ", "/usr/share/zoneinfo/Europe/Berlin", 1); // POSIX-specific, for localtime()
    time_t tnow = time(NULL);
    struct tm tm_now = *localtime(&tnow);

    int clock_offset = 3600 + (1 == tm_now.tm_isdst?3600:0); // GTM+1 + daylight saving
    //tnow = (tnow / 86400) * 86400; // Rounding nearby begin of day
    //size_t nlocalday = (tnow + clock_offset) / 86400;
    //DEBUG( fprintf(stderr, "DST: %i, Clock offset: %i\n", tm_now.tm_isdst, clock_offset); )

    size_t nlocalday = (tnow + clock_offset) / 86400;  // NDay relative to current time zone
    time_t tlocalday_begin = nlocalday * 86400 + clock_offset; // Begin of day in current time zone

    output_t output = output_init(
            p_arguments->max_num_results,
            p_arguments->skiped_num_results,
            p_arguments->reversed_results,
            2 /* offset for ",\n" */);


    search_workspace_t s_ws = {
        -1, // tcreation Will be overwritten after load of title file.(*)
        -1, // ttoday (*)
        nlocalday, // itoday (*)
        clock_offset, // today_time_zone_offset

        -1, // list_creation_time (*)
        tnow, // search_tnow
        tlocalday_begin, // search_ttoday
        nlocalday, // search_itoday
#ifdef WITH_TOPIC_OPTION
        p_arguments->title?0:1, //restrict_string_search_on_topic
#endif
        {-1, -1, 0}, // payload
        {-1, -1, 0}, // searchable_strings
        0, // searchable_strings_len
        (char *) malloc(OUT_CACHESIZ),
        {OUT_CACHESIZ, 0, NULL}, // search_out_buf
        tm_now, // tmp_ts
        channels_ws_create(), // channels
        linked_list_create(tlocalday_begin),
        {0, 0, NULL, NULL, NULL}, // chunks
        -1, // index_fd
//        {p_arguments->max_num_results, 0,
//            p_arguments->skiped_num_results, NULL}, // search_result_t
        {0, NULL, 0, NULL}, // prev_topic
#ifdef COMPRESS_BROTLI
        brotli_decoder_ws_create(),
//        brotli_decoder_ws_create(),
#endif
        output,
        p_arguments
    };

    s_ws.search_out_buf = (char_buffer_t){OUT_CACHESIZ, 0, s_ws._buf};

    // Due leap-seconds, etc. required
    s_ws.tmp_ts.tm_sec = 0;
    s_ws.tmp_ts.tm_min = 0;
    s_ws.tmp_ts.tm_hour = 0;

    // Toggle evaluation of daylight saving time at next call of mktime
    // This sets the flag to 0 or 1 and will be used as base for all other
    // evaluations. The flag should be reset to -1 in some cases...
    s_ws.tmp_ts.tm_isdst = -1;

//    s_ws.search_result.matches = (uint32_t *)calloc(
//            s_ws.search_result.match_limit, sizeof(uint32_t));

    if( p_arguments && p_arguments->diff_update ){
          s_ws.payload.id = (FIRST_DIFF_INDEX-1);
          s_ws.searchable_strings.id = (FIRST_DIFF_INDEX-1);
      }

    return s_ws;
}

void init_chunks(
        search_workspace_t *p_s_ws)
{
    title_chunks_t *p_chunks = &p_s_ws->chunks;

    assert( p_chunks->bufs == NULL );
    assert( p_chunks->start_seeks == NULL );
    assert( p_chunks->start_ids == NULL );

    /* Second + 1 for carry during array index evaluation */
    p_chunks->len = 1 + 1 + p_s_ws->searchable_strings_len / SEARCH_TITLE_ARR_LEN;

    p_chunks->bufs = (char_buffer_t *) calloc(
            p_chunks->len, sizeof(char_buffer_t));
    /* Here, the last element is an copy of the previous, too.*/
    p_chunks->start_seeks = (uint32_t *) calloc(
            p_chunks->len, sizeof(uint32_t));

    p_chunks->start_ids = (uint32_t *) calloc(
            p_chunks->len, sizeof(uint32_t));
    p_chunks->start_ids[0] = LINKED_LIST_FIRST_ID;
}

void uninit_chunks(
        search_workspace_t *p_s_ws)
{
    title_chunks_t *p_chunks = &p_s_ws->chunks;
    if( p_chunks->len == 0 )
        return;

    assert( p_chunks->len > 1 );

    /* Clear chunks
     * In general, not all chunks were used and last pointer equals previous one.
     */
    int i;
    for( i=0; i<p_chunks->len - 1; ++i){
        char_buffer_t *p_x = &p_chunks->bufs[i];
        char_buffer_t *p_x2 = &p_chunks->bufs[i+1];
        if( p_x->p && p_x->p != p_x2->p ){
            // Above check omit double free'ing.
            char_buffer_clear(p_x);
        }
    }
    char_buffer_clear(&p_chunks->bufs[p_chunks->len-1]);

    Free(p_chunks->bufs);
    Free(p_chunks->start_seeks);
    Free(p_chunks->start_ids);
    p_chunks->len = 0;
    p_chunks->partial_i = 0;
}

void search_ws_destroy(
        search_workspace_t *p_s_ws)
{
    p_s_ws->search_out_buf.used = 0;
    p_s_ws->search_out_buf.p = NULL;
    Free(p_s_ws->_buf);

    //Free(p_s_ws->tmp);
    Free(p_s_ws->prev_topic._copy);

    channels_ws_destroy(&p_s_ws->channels);
    //index_data_destroy(&p_s_ws->index);
    linked_list_destroy(&p_s_ws->index);
    output_uninit(&p_s_ws->output);

//    Free(p_s_ws->search_result.matches);

    uninit_chunks(p_s_ws);

#ifdef COMPRESS_BROTLI
    brotli_decoder_ws_destroy(&p_s_ws->brotli_title);
//    brotli_decoder_ws_destroy(&p_s_ws->brotli_payload);
#endif
}

int open_index_file(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments)
{
    if( p_s_ws->index_fd > -1 ){
        close(p_s_ws->index_fd);
        p_s_ws->index_fd = -1;
    }

    assert(p_arguments->index_folder != NULL);

    //Currently, fixed name.
    size_t ts = strlen(p_arguments->index_folder) + sizeof(index_file_template) + 10;
    char *tmp = (char *) malloc(ts * sizeof(char));
    const int diff = p_s_ws->p_arguments->diff_update;
    if( !tmp ) return -1;

    snprintf(tmp, ts, index_file_template,
            p_arguments->index_folder, (diff?diff_ext:""));
    DEBUG( fprintf(stderr, "Open '%s'\n", tmp); )

        p_s_ws->index_fd = open(tmp, O_RDONLY);

    free(tmp);

    //assert( p_s_ws->index_fd > -1 );
    return (p_s_ws->index_fd > 0)?0:-1;
}

int open_title_file(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments)
{
    if( p_s_ws->searchable_strings.fd > -1 ){
        close(p_s_ws->searchable_strings.fd);
        p_s_ws->searchable_strings.fd = -1;
    }

    assert(p_arguments->index_folder != NULL);
    p_s_ws->searchable_strings.id++;
    p_s_ws->searchable_strings.seek = 0; //unused
    //p_s_ws->searchable_strings.= 0; //unused

    //Currently, fixed name.
    size_t ts = strlen(p_arguments->index_folder) + sizeof(strings_file_template) + 10;
    char *tmp = (char *) malloc(ts * sizeof(char));
    const int diff = p_s_ws->p_arguments->diff_update;
    if( !tmp ) return -1;

    snprintf(tmp, ts, strings_file_template,
            p_arguments->index_folder,
            p_s_ws->searchable_strings.id, (diff?diff_ext:""));
    DEBUG( fprintf(stderr, "Open '%s'\n", tmp); )

    p_s_ws->searchable_strings.fd = open(tmp, O_RDONLY);

    assert( p_s_ws->searchable_strings.fd > -1 );

    free(tmp);
    return (p_s_ws->searchable_strings.fd > 0)?0:-1;
}

void read_index_header(
        search_workspace_t *p_s_ws)
{
    assert( p_s_ws && p_s_ws->index_fd > 0);
    /* Note: sizeof(time_t) could be 4 or 8 */
    uint32_t data[3];
    if( p_s_ws->index_fd > 0 ){
        ssize_t n = read(p_s_ws->index_fd, &data, sizeof(data));
        assert( (ssize_t)sizeof(data) == n);
        _unused(n);
        p_s_ws->tcreation = (time_t)data[0];
        p_s_ws->itoday = (size_t)data[1];
        // TODO: today_time_zone_offset could be differ from value during creation of list...
        p_s_ws->ttoday = 86400 * p_s_ws->itoday - p_s_ws->today_time_zone_offset;
        p_s_ws->list_creation_time = (time_t)data[2];
    }
}

void read_index_footer(
        search_workspace_t *p_s_ws)
{
    uint32_t seek;
    ssize_t w = read( p_s_ws->index_fd, &seek,
            sizeof(uint32_t));
            //sizeof(p_s_ws->searchable_strings.seek));
    assert( (ssize_t)sizeof(seek) == w );
    _unused(w);
    p_s_ws->searchable_strings_len = seek;

    assert( p_s_ws && p_s_ws->index_fd > 0);
    int32_t len_channel_data = read_channel_list(
            p_s_ws->index_fd, &p_s_ws->channels);

    /* Read length of channel data, again
     * (just to mirror write_index_footer() behavior)
     */
    int32_t bytes_was_read;
    ssize_t n = read(p_s_ws->index_fd, &bytes_was_read, sizeof(int32_t));
    assert( n == sizeof(int32_t));
    assert( len_channel_data == bytes_was_read);
    _unused(n); _unused(len_channel_data);
}

/*
 * The programm arguments could describe search
 * pattern which can not handled with one
 * run of the search loop.
 * Use this function to iterate over the
 * different pattern.
 *
 * For each releation R and range borders [a_0,a_1,...]
 * (which describes the intervals [[0, a_0]_0, [a_0+1, a_1]...]),
 * and an interaval [m, M]
 * r_min and r_max are the extremal indizies with
 * r_min = max{r | a_r <  m }
 * r_max = min{r | a_r >= M }
 *
 * In the code, the condition 'a_r < M' will be checked.
 * If true, index r' := r+1 fulfill r' <= r_max.
 *
 * Return 0 on sucess.
 * Return -1 if no further tuple is available.
 *
 * Note: Function modify p_arguments.
 */
int next_search_tuple(
        search_workspace_t *p_s_ws,
        const arguments_t *p_arguments,
        const search_pattern_t *p_pattern_first,
        search_pattern_t * p_pattern)
{
    int m, M, iNow;
    m = p_arguments->durationMin;
    M = p_arguments->durationMax;
    if( m < M ){
        iNow = p_pattern->groups.i_duration;
        if( iNow < NUM_DURATIONS-1 &&
                M > p_s_ws->index.meta.duration_borders[iNow])
        {
            p_pattern->groups.i_duration++;
            return 0;
        }else{
            // Restore start value of 'first numeral'.
            p_pattern->groups.i_duration =
                p_pattern_first->groups.i_duration;
        }
    }

    m = p_arguments->beginMin;
    M = p_arguments->beginMax;
    if( m < M ){
        iNow = p_pattern->groups.i_timestamp;
        if( iNow < NUM_TIME-1 &&
                M > p_s_ws->index.meta.timestamp_borders[iNow])
        {
            p_pattern->groups.i_timestamp++;
            return 0;
        }else{
            // Restore start value of 'second numeral'.
            p_pattern->groups.i_timestamp =
                p_pattern_first->groups.i_timestamp;
        }
    }

    m = p_arguments->dayMin;
    M = p_arguments->dayMax;
    if( m < M ){
        iNow = p_pattern->groups.i_relative_date;
        iNow += (M - p_pattern_first->groups.i_relative_date);
        // Day order reversed -> Decrement
        if( iNow > 0 && iNow > m )
        {
            p_pattern->groups.i_relative_date--;
            return 0;
        }else{
            // Restore start value of 'third numeral'.
            p_pattern->groups.i_relative_date =
                p_pattern_first->groups.i_relative_date;
        }
    }

    // If channel could be changed, too, extend iterator here...

    // No next pattern found
    return -1;
}

/* The do_search-function search
 * in relation to coarsed criteria.
 * This function returns 0 if the arguments
 * are really fulfilled and
 * -1 otherwise.
 */
int _all_arguments_fulfilled(
        search_workspace_t *p_s_ws,
        index_node_t *p_el)
{
    arguments_t *p_arguments = p_s_ws->p_arguments;

    searchable_strings_prelude_t *p_entry;
    title_entry(p_s_ws, p_el->id, &p_entry);

    uint32_t duration = (uint32_t) p_entry->duration;
    if( p_arguments->durationMin > -1 &&
            p_arguments->durationMin > duration){
        return -1;
    }

    if( p_arguments->durationMax > -1 &&
            p_arguments->durationMax < duration){
        return -1;
    }

    time_t absolute_time = (time_t)(p_s_ws->ttoday + p_entry->relative_start_time);

    // TODO: Fix issue at daylight changes (require summer time flag for earch entry)
    // THE GTM+1 had not to be appied because it is an relative value.
    int32_t entry_summer_offset_s = 0;
    int32_t begin = (absolute_time + p_s_ws->today_time_zone_offset
            + entry_summer_offset_s) % (86400);

    if( p_arguments->beginMin > -1 &&
            p_arguments->beginMin > begin){
        return -1;
    }

    if( p_arguments->beginMax > -1 &&
            p_arguments->beginMax < begin){
        return -1;
    }

#if 0 // Currently, this checks should never be true...
    uint32_t channel = LINKED_LIST_SUBGROUP(p_el->nexts.channel);
    if( p_arguments->channelNr != NO_CHANNEL &&
            p_arguments->channelNr != channel ){
        return -1;
    }
    //TODO: Wrong condition
    time_t absolute_day_begin = absolute_time - begin;
    int32_t relative_day = (p_s_ws->search_ttoday - p_s_ws->ttoday)
        / (24*3600); // seconds -> days

    if( p_arguments->dayMin > -1 &&
            p_arguments->dayMin > relative_day ){
        return -1;
    }
    if( p_arguments->dayMax > -1 &&
            p_arguments->dayMax < relative_day ){
        return -1;
    }
#endif

    return 0;
}



/* Return number of used relations (!= -1)
 * and fill array (with length >= NUM_RELATIONS) with
 * first ids
 */
int _num_used_pattern_old(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern,
        uint32_t *used_indizes,
        uint32_t *start_ids)
{
    const int N[NUM_RELATIONS + 1] = { 0,
        NUM_REALTIVE_DATE,
        NUM_REALTIVE_DATE + NUM_TIME,
        NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS,
        NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS + NUM_CHANNELS,
    };
    int n=0, i;
    for(i=0; i<NUM_RELATIONS; ++i){
      if( p_pattern->groups.indizes[i] > -1 ){
          used_indizes[n] = i;
          start_ids[n] = p_s_ws->index.first_ids.ids[
              N[i] + p_pattern->groups.indizes[i]];
          ++n;
      }
    }
    return n;
}

void _num_used_pattern(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern)
{
    const int N[NUM_RELATIONS + 1] = { 0,
        NUM_REALTIVE_DATE,
        NUM_REALTIVE_DATE + NUM_TIME,
        NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS,
        NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS + NUM_CHANNELS,
    };
    int n=0, i;
    for(i=0; i<NUM_RELATIONS; ++i){
      if( p_pattern->groups.indizes[i] > -1 ){
          p_pattern->used_indizes[n] = i;
          p_pattern->start_ids[n] = p_s_ws->index.first_ids.ids[
              N[i] + p_pattern->groups.indizes[i]];
          ++n;
      }
    }
    p_pattern->K = n;
}

/* Return 0 on match */
int _search_compare_title_(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern,
        index_node_t *p_el)
{
    assert( p_pattern->title_pattern != NULL );
    assert( p_pattern->title_sub_pattern[0] != NULL );
    //linked_list_t *p_list = &p_s_ws->index;
    
#define get_title_len(X) (strchrnul(X, SPLIT_CHAR)-X)

    // Split pattern by '*' and search each substring separately.
    const char *string_to_search = NULL, *prev_topic = NULL;
    //const char *string_to_search = get_title_string(p_s_ws, p_el->id);
    int b_use_prev_topic = (0 < get_title_and_topic(
            p_s_ws, p_el->id, &string_to_search, &prev_topic));

    /*
     * Structure of above byte array: "{TITLE}{SPLIT_CHAR}[{TOPIC}]"
     * where: 
     * TITLE : Title string followed by | 
     * TOPIC : Topic string followed by \0. Only set if topic differs from
     *         previous entry.
     * SPLIT_CHAR: Defined in settings.h)
     *
     * Invalid input data could miss the '\0', but the complete buffer
     * is limited by '\0'.
     */

#ifdef WITH_TOPIC_OPTION
     if( p_s_ws->restrict_string_search_on_topic ){
         // Start directly with topic string
         string_to_search = prev_topic;
         b_use_prev_topic = 0;
     }
#endif

    /* Loop through sub pattern...
     * Firstly , string_to_search maps on the title|topic string.
     * Secondly it maps to the prev_topic string.
     *
     * Return 0 if all subpattern could be found.
     */
    const char **sub_pat = (const char **)p_pattern->title_sub_pattern;
    while( *sub_pat != NULL ){

#if 1
        if( string_to_search == NULL ){
            int b_use_prev_topic2 = (0 < get_title_and_topic(
                        p_s_ws, p_el->id, &string_to_search, &prev_topic));
        }
#endif

#if NORMALIZED_STRINGS > 0
        const char *hit = strstr(string_to_search, *sub_pat);
#else
#ifdef _GNU_SOURCE
        const char *hit = strcasestr(string_to_search, *sub_pat);
#else
        const char *hit = strstr(string_to_search, *sub_pat);
#endif
#endif
        if( hit == NULL ){
            if( b_use_prev_topic ){
                string_to_search  = prev_topic;
                b_use_prev_topic = 0;
                continue;
            }
            return -1;
        }
        string_to_search = hit + strlen(*sub_pat);
        sub_pat++;
    }
    return 0;
}

/* Linear search by title pattern only (slow).*/
int _search_by_title_(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern)
{
    linked_list_t *p_list = &p_s_ws->index;
    const int found_max = p_s_ws->output.M;
    const uint32_t found_before = p_s_ws->output.found;
    uint32_t *p_found = &p_s_ws->output.found;

    assert( p_pattern->title_sub_pattern[0] != NULL );

    // Loop over all arrays of nodes.
    int i, ichunk_begin, ichunk_end;
    index_node_t *p_el, *p_end;
    ichunk_begin = 0;
    ichunk_end = (p_list->next_unused_id - LINKED_LIST_FIRST_ID - 1)
        / LINKED_LIST_ARR_LEN;

    assert( ichunk_end <= p_list->len_nodes - 1);

    for( i=ichunk_begin; i<=ichunk_end; i++){
        if( p_list->nodes_start[i] != NULL ){
            p_el = p_list->nodes_start[i];
            p_end = p_el;
            p_end =(i==ichunk_end)?
                ( p_el + ( (p_list->next_unused_id-1)%LINKED_LIST_ARR_LEN) ):
                ( p_el + LINKED_LIST_ARR_LEN );

            while( p_el < p_end ){
                if( _search_compare_title_(p_s_ws, p_pattern, p_el) == 0 &&
                    _all_arguments_fulfilled(p_s_ws, p_el) == 0 )
                {
                    output_add_id(&p_s_ws->output, p_el->id);
                    if( *p_found >= found_max
                            && p_s_ws->output.reversed_flag == 0
                      ){
                        return 0;
                    }
                }
                ++p_el;
            }
        }
    }

    // End of list reached. Return if at least one element was found.
    return ((*p_found > found_before)?0:-1);
}

/* Inner function of search_do_search */
int _search_do_search_(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern)
{
    linked_list_t *p_list = &p_s_ws->index;

    // Find start points for search. One for each relation
    //const int K = _num_used_pattern_old(p_s_ws,
    //        p_pattern, &used_indizes[0], &ids[0]);
    _num_used_pattern(p_s_ws, p_pattern);
    const int K = p_pattern->K;
    const uint32_t *used_indizes = p_pattern->used_indizes;
    const uint32_t *ids = p_pattern->start_ids;

    const uint32_t found_max = p_s_ws->output.M;
    const uint32_t found_before = p_s_ws->output.found;
    uint32_t *p_found = &p_s_ws->output.found;

    if( K == 0 ){
        if( p_pattern->title_sub_pattern[0] != NULL ){
            return _search_by_title_(p_s_ws, p_pattern);
        }else{
            fprintf(stderr, "Search aborted. It was no criteria set.\n");
            return -1;
        }
    }

    assert( K >= 1 );
    int k = 0, k2;
    int num_matched = 1;
    int i;

    // Find element with highest id. This will be the start point of the search.
    for( i=1; i<K; ++i){
        if( ids[k] < ids[i] ){
            k=i;
            num_matched = 1;
        }else if( ids[k] == ids[i]){
            ++num_matched;
        }
    }
    uint32_t id=ids[k], id2;
    if( id == 0 ){
        return -1;
    }
    index_node_t *p_el = linked_list_get_node(p_list, id);
    const int r = p_s_ws->output.reversed_flag;
    while( r || ( *p_found < found_max ))
    {
        while( num_matched < K ){
            // Check if current element fulfill next releation
            k2 = (k + 1) % K;
            uint32_t next_id_isub = (p_el->nexts.subgroup_ids[used_indizes[k2]]);
            if( LINKED_LIST_SUBGROUP(next_id_isub) == p_pattern->groups.indizes[used_indizes[k2]] )
            {
                // Yes, relation match
                ++num_matched;
                k = k2;

            }else{
                // No, relation is wrong
                // Go to next element in currenty selected (by used_indizes[k]) releation.
                // TODO: Tippelschritte...
                id2 = linked_list_next_in_subgroup_by_id(p_list, id, used_indizes[k]);
                if( id2 == 0 ){
                    return ((*p_found > found_before)?0:-1);
                }

                id = id2;
                p_el = linked_list_get_node(p_list, id);
                num_matched = 1;
            }

        }

        // Id fulfill all releations. Now, compare the title string
        if(( p_pattern->title_sub_pattern[0] == NULL ||
                    _search_compare_title_(p_s_ws, p_pattern, p_el) == 0 ) &&
                _all_arguments_fulfilled(p_s_ws, p_el) == 0 )
        {
            output_add_id(&p_s_ws->output, id);
        }

        // Generate start-id for next round as biggest successor
        int j;
        id = LINKED_LIST_ID(p_el->nexts.subgroup_ids[used_indizes[0]]);
        k = 0;
        for( j=1; j<K; ++j ){
            id2 = LINKED_LIST_ID(p_el->nexts.subgroup_ids[used_indizes[j]]);
            if( id2 > id ){ id = id2; k = j; }
        }
        if( id == 0 ){
            return ((*p_found > found_before)?0:-1);
        }
        p_el = linked_list_get_node(p_list, id);
        num_matched = 1;
    }

    // found_max elements found or end of list reached
    return 0;
}

void print_search_results(
        search_workspace_t *p_s_ws)
{
    int fd = 1; //p_s_ws->?;

    output_print(fd, &p_s_ws->output);
}

#if 0
// Deprecated/Unused function?!
void print_search_result(
        int fd,
        search_workspace_t *p_s_ws,
        uint32_t id,
        int not_prepend_comma)
{
    linked_list_t *p_list = &p_s_ws->index;
    index_node_t *p_el;

    p_el = linked_list_get_node(p_list, id);
    uint32_t channel = LINKED_LIST_SUBGROUP(p_el->nexts.channel);

    /* Construct anchor by
     * file id (upper 8 byte) + (local) seek position.
     */
    assert( p_el->link.payload_seek <= 0xFFFFFF );
    assert( p_el->link.payload_file_id <= 0xFF );
    uint32_t payload_anchor = ( (p_el->link.payload_seek & 0xFFFFFF)
        | p_el->link.payload_file_id << 24) ;

    searchable_strings_prelude_t *p_entry;
    title_entry(p_s_ws, id, &p_entry);

    int title_len = p_entry->length; //followed by \0
#if NORMALIZED_STRINGS > 0
    const char *title, *title_norm;
    title_norm = (const char *)(p_entry+1);  // Normalized title + topic string...
    title = title_norm + title_len + 1; // Original title
    if( title < title_norm || (title_norm - title) > 4096){ // title_len wrong?
        assert(0);
        title = title_norm + strlen(title_norm) + 1;
    }
    title_len = strlen(title);
#else
    const char *title;
    title = (const char *)(p_entry+1);

    /* Restrict on characters before '|' to cut of 'topic' substring*/
#ifdef _GNU_SOURCE
    title_len = strchrnul(title, '|') - title;
#else
    char *sep = strchr(title, '|');
    if( sep ){ title_len = sep - title; }
#endif
#endif

    // Convert channel id to string
    const char *channel_str = get_channel_name(&p_s_ws->channels, channel);

    time_t absolute_day_begin = (time_t)(p_s_ws->ttoday + p_entry->relative_start_time);
    // Convert time to string
#if 0
    const char *absolute_day_str = ctime(&absolute_day_begin);
#else
    struct tm * timeinfo;
    char absolute_day_str[80];

    timeinfo = localtime (&absolute_day_begin);
    strftime(absolute_day_str, 80, "%d. %b. %Y %R", timeinfo);
#endif

    dprintf(fd, "%s{\"id\": %i, \"title\": \"%.*s\", \n\t\t\"ibegin\": %u, " \
            "\"begin\": \"%s\", \"iduration\": %u, " \
            "\"ichannel\": %u, \"channel\": \"%s\", " \
            "\"anchor\": %u}",
            (not_prepend_comma?"\t":",\n\t"), id,
            title_len, title,
            (uint32_t) absolute_day_begin,
            absolute_day_str,
            (uint32_t) p_entry->duration,
            channel, channel_str,
            payload_anchor
           );
}
#endif

int search_do_search(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments)
{
    // 1. Convert arguments into searchable form
    search_pattern_t pattern = {
        //{-1, NUM_TIME-1, NUM_DURATIONS-1, NO_CHANNEL}, /* linked_list_subgroup_indizes_t */
        {{{-1, -1, -1, NO_CHANNEL}}}, /* linked_list_subgroup_indizes_t */
        NULL, NULL, {NULL}, // title_pattern + _title_sub_pattern  Array of NULL values...
        LINKED_LIST_FIRST_ID, // current_id
        0,                    // K/num_used_indizes
        {-1, -1, -1, -1},     // used_indizes
        {0, 0, 0, 0}          // start_ids
    };
#ifdef WITH_TOPIC_OPTION
    transform_search_title(p_s_ws->restrict_string_search_on_topic?
            p_arguments->topic:p_arguments->title, &pattern.title_pattern);
#else
    transform_search_title(p_arguments->title, &pattern.title_pattern);
#endif
    split_pattern(&pattern, '*');
    DEBUG( fprintf(stderr, "Search pattern: '%s'\n", pattern.title_pattern) );

    //p_s_ws->search_result.match_found = 0;

    if( p_arguments->dayMin > -1 ){
        int relative_day_to_now = p_arguments->dayMax + 1; // Begin with oldest.
        int relative_day_to_creation = relative_day_to_now -
            (p_s_ws->search_itoday - p_s_ws->itoday);
        if( relative_day_to_creation < 0 ){
            // This day is to young to be found in this index file.
            // This holds for all days in the range [dayMin, dayMax],
            // thus it exists no matching entry at all.
            return -1;
        }

        int relative_day_to_creation2 = p_arguments->dayMin + 1
            - (p_s_ws->search_itoday - p_s_ws->itoday);

        // Cut ranges
        relative_day_to_creation = clip(0, relative_day_to_creation,
                NUM_REALTIVE_DATE-1);
        relative_day_to_creation2 = clip(0, relative_day_to_creation2,
                NUM_REALTIVE_DATE-1);

        assert( relative_day_to_creation >= relative_day_to_creation2 );

        // Apply range cut on input args to keep range width on same value.
        // This is important for the next_search_tuple() function.
        p_arguments->dayMax = relative_day_to_creation
            + (p_s_ws->search_itoday - p_s_ws->itoday);
        p_arguments->dayMin = relative_day_to_creation2
            + (p_s_ws->search_itoday - p_s_ws->itoday);

        pattern.groups.i_relative_date = relative_day_to_creation;
    }

    int i;
    if( p_arguments->beginMin > -1 ){
        for( i=0; i<NUM_TIME; ++i ){
            if( p_arguments->beginMin <=
                    p_s_ws->index.meta.timestamp_borders[i] ){
                pattern.groups.i_timestamp = i;
                break;
            }
        }
    }

    if( p_arguments->durationMin > -1 ){
        for( i=0; i<NUM_DURATIONS; ++i ){
            if( p_arguments->durationMin <=
                    p_s_ws->index.meta.duration_borders[i] ){
                pattern.groups.i_duration = i;
                break;
            }
        }
    }

    // Channel name preferred over channel number
    if( p_arguments->channelName != NULL){
        // First, try untransformed channel name
        int ic = get_channel_number(&p_s_ws->channels, p_arguments->channelName, 0);
        if( ic == -1){ // Try transformed name
            char *norm_chan_name = NULL;
            transform_channel_name(p_arguments->channelName, &norm_chan_name);
            ic = get_channel_number(&p_s_ws->channels, norm_chan_name, 0);
            Free(norm_chan_name);
        }
        if( ic == -1){
            // Name not found. Select invalid channel number
            // if -c argument is unset
            if( pattern.groups.i_channel < 0 ){
                pattern.groups.i_channel  = NUM_CHANNELS;
            }
        }else{
            pattern.groups.i_channel  = ic;
        }
    }
    if( pattern.groups.i_channel < 0 ){
        pattern.groups.i_channel = p_arguments->channelNr;
    }

    if( pattern.groups.i_channel < NO_CHANNEL ||
            pattern.groups.i_channel >= NUM_CHANNELS){
        // The channel string was not found (or something other
        // went wrong. No valid entry could be found for this input.
        return -1;
    }

    assert( pattern.groups.i_channel >= NO_CHANNEL );
    assert( pattern.groups.i_channel < NUM_CHANNELS );

    //arguments_t args_copy = *p_arguments;
    const search_pattern_t pattern_first = pattern;
    while( 1 ){
        _search_do_search_(p_s_ws, &pattern);

        if( p_s_ws->output.found >= p_s_ws->output.M
                && p_s_ws->output.reversed_flag == 0
                ){
            break;
        }
        if( next_search_tuple(p_s_ws, p_arguments,
                    &pattern_first, &pattern) ){
            break;
        }
    }

    if( pattern.title_pattern){
        Free(pattern.title_pattern);
        pattern.title_sub_pattern[0] = NULL;
        Free(pattern._title_sub_pattern);
    }

    // Transfer data into output buffer
    // (If reversed flag is not set, it doubles
    // the required memcpy opereations...)
    output_flush(p_s_ws, &p_s_ws->output);
    return 0;
}

uint32_t find_lowest_seek_over_threshold(
        search_workspace_t *p_s_ws,
        uint32_t threshold,
        uint32_t start_id, uint32_t *p_stop_id)
{
    /* Informal description of minimization target:
     *
     * p_stop_id := min_id({x | title_seek(x) >= threshold})
     * return_value := title_seek(p_stop_id)
     *
     * Note that title_seek(p_stop_id-1) != return_value-1
     */

    linked_list_t *p_index = &p_s_ws->index;
    uint32_t start_index = start_id - LINKED_LIST_FIRST_ID;

    // Check if begin of search is not too late.
    assert( threshold >
                (*(p_index->nodes_start + ( start_index / LINKED_LIST_ARR_LEN))
                + start_index % LINKED_LIST_ARR_LEN)->link.title_seek );

    const size_t last_index = p_index->nodes_current - p_index->nodes_start;
    size_t node_index = (start_index / LINKED_LIST_ARR_LEN);
    int offset_last_node = (node_index == last_index)?
        (((p_index->next_unused_id-1)%LINKED_LIST_ARR_LEN) - 1):
        (LINKED_LIST_ARR_LEN - 1);

    index_node_t **block =  p_index->nodes_start + node_index;
    while( (*block + offset_last_node)->link.title_seek < threshold ){
        // All elements of the block are too small.
        block++;
        node_index++;
        if( node_index == last_index ){
            offset_last_node = ((p_index->next_unused_id-1)%LINKED_LIST_ARR_LEN) - 1;
        }

        if( *block == NULL || node_index > last_index ){
            // No node exceeded the given threshold.
            if( p_stop_id != NULL ){
                *p_stop_id = p_index->next_unused_id;
            }
            return p_s_ws->searchable_strings_len;
        }
    }

    assert( block >= p_index->nodes_start );
    assert( block < p_index->nodes_start + p_index->len_nodes );

    /* The searched element is in
     * [*block, *(block+END-1)]
     *
     * Use bisect search under the assumption
     * (*block)[END-1] => threshold
     *
     * with END=LINKED_LIST_ARR_LEN or lower (at last block)
     */

    const uint32_t T = threshold;
    int M = offset_last_node+1; //LINKED_LIST_ARR_LEN;
    index_node_t *a = *block;
    index_node_t *c = a+M-1;
    index_node_t *b;

    /*          Index    0       M       N-1 N | M = N-1-floor(N/2)
     *    N even case   [a       b][       ]   |
     *     N odd case   [a      ]b [      c]   |
     * []-range width   <-- M -->  <-- M -->   |
     *
     * New bounds after [a      c'][a'    c]   |  with c' = a + M -1 and
     * (*b >= T)-check: [a   c'] b [a'    c]   |  a' = b + 1 in both cases
     *
     * (Sketch inaccurate for (N=1,2)
     */
    while( M ){
        M >>= 1;
        b = c - M;
        if( b->link.title_seek >= T ){
            c = a + M - 1; // could be < T
        }else{
            a = b + 1;
        }
    }
    // Now, a is the smallest element d with d->link.title_seek >= threshold

    if( p_stop_id != NULL ){
        *p_stop_id = (block - p_index->nodes_start) * LINKED_LIST_ARR_LEN
            + (a - *block)
            + LINKED_LIST_FIRST_ID;
    }

    return a->link.title_seek;
}

const char * get_title_string(
        search_workspace_t *p_s_ws,
        uint32_t id)
{
    searchable_strings_prelude_t *p_entry;
    title_entry(p_s_ws, id, &p_entry);

    const char *title = (const char *)(p_entry+1);
    return title;
}

size_t title_entry(
        search_workspace_t *p_s_ws,
        uint32_t entry_id,
        searchable_strings_prelude_t **pp_entry)
{
    assert( entry_id >= LINKED_LIST_FIRST_ID );
    assert( entry_id < p_s_ws->index.next_unused_id );

    // Normalize
    entry_id -= LINKED_LIST_FIRST_ID;

    index_node_t **block =  p_s_ws->index.nodes_start + (entry_id / LINKED_LIST_ARR_LEN);
    index_node_t *p_node = *block + (entry_id % LINKED_LIST_ARR_LEN);
    size_t seek = p_node->link.title_seek;

    //2. Seek allows evaluation of title in chunks struct.
    int iChunk = (seek / SEARCH_TITLE_ARR_LEN);

#if 0
    /* Bugfix: The latest chunk 'chunks.bufs[partial_i]'
     *     is already copied into 'chunks.bufs[partial_i + 1]', but this
     *     seems still not enough and (seek / SEARCH_TITLE_ARR_LEN) reaches
     *     the index partial_i + 2...
     *
     * (Update) Bugfix removed because reason for bug was found and eliminated.
     */
    if( iChunk > p_s_ws->chunks.partial_i ){
        fprintf(stderr, "Requested chunk index to high! Requested: %i, available: %i\n",
                iChunk,
                (int) p_s_ws->chunks.partial_i);
        iChunk = p_s_ws->chunks.partial_i;
    }
#endif
    char_buffer_t *p_b = &p_s_ws->chunks.bufs[iChunk];

    assert( p_b->p != NULL );

    // searchable_strings_prelude_t*
    *pp_entry = (searchable_strings_prelude_t *)(
            p_b->p + seek - p_s_ws->chunks.start_seeks[iChunk]);
    return iChunk;
}


void search_read_title_file(
        search_workspace_t *p_s_ws)
{
    /*
     * To avoid the allocation of one big memory area
     * the title file will be split into chunks (of flexible size).
     *
     * The chunks suits the following condition:
     *   [title.begin, title.end] is part of chunk N
     *   <=>
     *   N = title.seek / LINKED_LIST_ARR_LEN
     *
     * Note that the length of the latest title in a chunk
     * affects its length.
     *
     * To create create buffers of acceptable size
     * the index data about the starting position of
     * each title string will be used.
     *
     *
     */

    assert( p_s_ws->index.next_unused_id > LINKED_LIST_FIRST_ID );
    assert( p_s_ws->searchable_strings_len > 0 );
    assert( p_s_ws->searchable_strings.fd > -1 );
    title_chunks_t *p_chunks = &p_s_ws->chunks;
    int const fd = p_s_ws->searchable_strings.fd;

    uninit_chunks(p_s_ws);
    init_chunks(p_s_ws);

    //p_chunks->decoding_start_byte_offset = 0;

    //
    char_buffer_t *p_current_chunk = &p_chunks->bufs[0];
    uint32_t *p_current_seek = &p_chunks->start_seeks[0];
    uint32_t *p_current_chunk_start_id = &p_chunks->start_ids[0];
    uint32_t *p_next_chunk_start_id = p_current_chunk_start_id + 1;
    uint32_t threshold = SEARCH_TITLE_ARR_LEN;
    uint32_t current_seek, next_seek;
#ifdef COMPRESS_BROTLI
    int finish_decompressing = 0;
#endif

    current_seek = 0;
    next_seek = 0;
    while( next_seek != p_s_ws->searchable_strings_len ){
        // Evaluate buffer length
        next_seek = find_lowest_seek_over_threshold(p_s_ws,
                threshold,
                *p_current_chunk_start_id, p_next_chunk_start_id);

        // Allocate buffer
        if( p_current_chunk->p != NULL ){
            free(p_current_chunk->p);
            fprintf(stderr, "%s\n",
                    "(search_read_title_file) buffer already alloc'd.");
        }
        assert( next_seek > current_seek );
        assert( p_current_chunk->p == NULL );

        p_current_chunk->len = (next_seek - current_seek);
        //DEBUG( fprintf(stderr, "Allocate buffer of len %zi for [%i, %i)\n",
        //            p_current_chunk->len, current_seek, next_seek) );
        p_current_chunk->p = (char *)malloc(
                p_current_chunk->len * sizeof(char));
        p_current_chunk->used = 0;

        // Fill buffer
#ifdef COMPRESS_BROTLI
        if( brotli_read_buffer(fd, &p_s_ws->brotli_title,
                    p_current_chunk,
            //        &p_chunks->decoding_start_byte_offset,
                    &finish_decompressing) ){
            fprintf(stderr, "%s Current byte range: [%i,%i)\n",
                    "(search_read_title_file) decompressing failed.",
                    current_seek, next_seek);
            break;
        }
#else
        ssize_t n = read(fd, p_current_chunk->p, p_current_chunk->len);
        if( (ssize_t) p_current_chunk->len > n ){
            fprintf(stderr, "%s Current byte range: [%i,%i). Number read bytes: %zu\n",
                    "(search_read_title_file) file reading failed.",
                    current_seek, next_seek, n);
        }
#endif

        // Prepare next loop step
        *(p_current_seek+1) = *(p_current_seek) + p_current_chunk->len;
        p_current_seek++;
        p_current_chunk++;
        p_current_chunk_start_id++;
        p_next_chunk_start_id++;
        current_seek = next_seek;
        threshold += SEARCH_TITLE_ARR_LEN;
    }

    // Repeat latest element for carry-over.
    *p_current_chunk = *(p_current_chunk-1);
    *p_current_seek = *(p_current_seek-1);
    //*p_current_chunk_start_id = *(p_current_chunk_start_id-1);

#ifdef COMPRESS_BROTLI
    assert( finish_decompressing );
#endif
#ifndef NDEBUG
    /* Print text of first entry */
    searchable_strings_prelude_t *p_entry;
    title_entry(p_s_ws, LINKED_LIST_FIRST_ID, &p_entry);

#if NORMALIZED_STRINGS > 0
    const char *title, *title_norm;
    title_norm = (const char *)(p_entry+1);  // Normalized title + topic string...
    title = title_norm + strlen(title_norm) + 1; // Original title
    if( title < title_norm || (title_norm - title) > 4096){ // p_entry->length wrong?
        assert(0);
        title = title_norm + strlen(title_norm) + 1;
    }
#else
    const char *title;
    title = (const char *)(p_entry+1);
#endif
    fprintf(stderr, "First title: %s\n", title);

    /* Print text of latest entry */
    // 1. Find position of last entry.
    int last_elem = (p_s_ws->index.next_unused_id - 1 - LINKED_LIST_FIRST_ID);
    index_node_t **block =  p_s_ws->index.nodes_start + ( last_elem / LINKED_LIST_ARR_LEN);
    index_node_t *p_last_node = *block + (last_elem % LINKED_LIST_ARR_LEN);
    size_t last_seek = p_last_node->link.title_seek;

    //2. Seek allows evaluation of title in chunks struct.
    int iChunk = (last_seek / SEARCH_TITLE_ARR_LEN);
    char_buffer_t *p_b = &p_s_ws->chunks.bufs[iChunk];

    assert( p_s_ws->chunks.len - 2 == iChunk);
    assert( p_b->p != NULL );
    assert( p_b == p_current_chunk-1 );

    p_entry = (searchable_strings_prelude_t *)(p_b->p + last_seek
            - p_s_ws->chunks.start_seeks[iChunk]);
#if NORMALIZED_STRINGS > 0
    title_norm = ((const char *)p_entry) + sizeof(searchable_strings_prelude_t);
    //title = title_norm + p_entry->length  + 1; // Original title
    title = title_norm + strlen(title_norm) + 1; // Original title
    if( title < title_norm || (title_norm - title) > 4096){ // p_entry->length wrong?
        assert(0);
        title = title_norm + strlen(title_norm) + 1;
    }
#else
    title = ((const char *)p_entry) + sizeof(searchable_strings_prelude_t);
#endif
    fprintf(stderr, "Last title: %s\n", title);
#endif
}


// ############## Begin of function with partial file loading

int search_read_title_file_partial(
        search_workspace_t *p_s_ws)
{
    /*
     * Just fills the first, second, ... chunk
     * of search_read_title_file(...).
     *
     * This reduces the memory footprint by approx.
     * size(file)-SEARCH_TITLE_ARR_LEN.
     */

    assert( p_s_ws->index.next_unused_id > LINKED_LIST_FIRST_ID );
    assert( p_s_ws->searchable_strings_len > 0 );
    assert( p_s_ws->searchable_strings.fd > -1 );
    title_chunks_t *p_chunks = &p_s_ws->chunks;
    int const fd = p_s_ws->searchable_strings.fd;

    if( p_chunks->len > 0 ){

        // Save latest topic string.
        buf_string_copy_t *p_topic = &p_s_ws->prev_topic;
        if( p_topic->target && p_topic->target != p_topic->_copy){
            charcpy(&p_topic->_copy, &p_topic->_copy_size,
                    //p_topic->target, p_topic->target_len);
                    p_topic->target, strlen(p_topic->target));
            p_topic->target = p_topic->_copy;
        }

        // Clear chunks of previous run.
        int i;
        //for( i=0; i<p_chunks->len; ++i)
        for( i=p_chunks->partial_i; i<=p_chunks->partial_i; ++i) // it's only one filled...
        {
            char_buffer_t *p_x = &p_chunks->bufs[i];
            if( p_x->p ){
                //printf("Remove chunk %i\n", i);
                char_buffer_clear(p_x);
            }
        }
        ++p_chunks->partial_i;
    }else{
        // First call of this function.
        init_chunks(p_s_ws);
        //p_chunks->decoding_start_byte_offset = 0;
    }

    //
    char_buffer_t *p_current_chunk = &p_chunks->bufs[p_chunks->partial_i];
    uint32_t *p_current_seek = &p_chunks->start_seeks[p_chunks->partial_i];
    uint32_t *p_current_chunk_start_id = &p_chunks->start_ids[p_chunks->partial_i];
    uint32_t *p_next_chunk_start_id = p_current_chunk_start_id + 1;
    uint32_t threshold = SEARCH_TITLE_ARR_LEN * (1 + p_chunks->partial_i);
    uint32_t current_seek, next_seek;
    int finish_decompressing = 0;

    current_seek = *p_current_seek;
    next_seek = current_seek;


    if( next_seek >= p_s_ws->searchable_strings_len ){
        fprintf(stderr, "Read after end of title file. File to short?!\n");
        return -1;
    }

    // Evaluate buffer length
    next_seek = find_lowest_seek_over_threshold(p_s_ws,
            threshold,
            *p_current_chunk_start_id, p_next_chunk_start_id);

    // Allocate buffer
    if( p_current_chunk->p != NULL ){
        free(p_current_chunk->p);
        fprintf(stderr, "%s\n",
                "(search_read_title_file) buffer already alloc'd.");
    }
    assert( next_seek > current_seek );
    assert( p_current_chunk->p == NULL );

    p_current_chunk->len = (next_seek - current_seek);
    //DEBUG( fprintf(stderr, "Allocate buffer of len %zi for [%i, %i)\n",
    //            p_current_chunk->len, current_seek, next_seek) );
    p_current_chunk->p = (char *)malloc(
            p_current_chunk->len * sizeof(char));
    p_current_chunk->used = 0;

    // Fill buffer
#ifdef COMPRESS_BROTLI
    if( brotli_read_buffer(fd, &p_s_ws->brotli_title,
                p_current_chunk,
        //        &p_chunks->decoding_start_byte_offset,
                &finish_decompressing) ){
        fprintf(stderr, "%s Current byte range: [%i,%i)\n",
                "(search_read_title_file) decompressing failed.",
                current_seek, next_seek);
        return -1;
    }
#else
    ssize_t n = read(fd, p_current_chunk->p, p_current_chunk->len);
    if( (ssize_t) p_current_chunk->len > n ){
        fprintf(stderr, "%s Current byte range: [%i,%i). Number read bytes: %zu\n",
                "(search_read_title_file) file reading failed.",
                current_seek, next_seek, n);
    }
    finish_decompressing = (next_seek >= p_s_ws->searchable_strings_len);
#endif

    // Prepare next loop step
    *(p_current_seek+1) = *(p_current_seek) + p_current_chunk->len;

    // Repeat latest element for carry-over.
    if( finish_decompressing ){
        ++p_current_chunk;
        ++p_current_seek;
        *p_current_chunk = *(p_current_chunk-1);
        *p_current_seek = *(p_current_seek-1);
        //*p_current_chunk_start_id = *(p_current_chunk_start_id-1);
    }

    return finish_decompressing?1:0;
}

/* Linear search by title pattern only (slow).*/
int _search_by_title_partial_(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern)
{
    linked_list_t *p_list = &p_s_ws->index;
    const uint32_t found_max = p_s_ws->output.M;
    const uint32_t found_before = p_s_ws->output.found;
    uint32_t *p_found = &p_s_ws->output.found;

    assert( p_pattern->title_pattern != NULL );
    assert( p_pattern->title_sub_pattern[0] != NULL );

    // This id is the first in the currently loaded chunk(s) (of title file)
    uint32_t chunk_start_id = p_s_ws->chunks.start_ids[
        p_s_ws->chunks.partial_i];
    // This id is the first after the currently loaded chunk(s) (of title file)
    uint32_t chunk_stop_id = p_s_ws->chunks.start_ids[
        p_s_ws->chunks.partial_i + 1];

    // Loop over affected nodes in the index blocks.
    int i, iblock_begin, iblock_end;
    index_node_t *p_el, *p_end;
    iblock_begin = (chunk_start_id - LINKED_LIST_FIRST_ID) / LINKED_LIST_ARR_LEN; // Falsch?!
    iblock_end = (chunk_stop_id - LINKED_LIST_FIRST_ID - 1) / LINKED_LIST_ARR_LEN;

    assert( iblock_begin <= iblock_end );
    assert( iblock_end <= p_list->len_nodes - 1);

    for( i=iblock_begin; i<=iblock_end; i++){
        if( p_list->nodes_start[i] != NULL ){
            p_el = p_list->nodes_start[i];
            p_end =(i==iblock_end)?
                ( p_el + ( (chunk_stop_id - LINKED_LIST_FIRST_ID - 1)%LINKED_LIST_ARR_LEN) + 1 ):
                ( p_el + LINKED_LIST_ARR_LEN );
            if( i==iblock_begin ){
                p_el += ( (chunk_start_id - LINKED_LIST_FIRST_ID )%LINKED_LIST_ARR_LEN);
            }

            while( p_el < p_end ){
                if( _search_compare_title_(p_s_ws, p_pattern, p_el) == 0  &&
                        _all_arguments_fulfilled(p_s_ws, p_el) == 0 )
                {
                    output_add_id(&p_s_ws->output, p_el->id);
                    if( *p_found >= found_max ){
                        p_pattern->current_id = p_el->id + 1;
                        // ( + 1 because variable stores first unhandled.)

                        return 1;
                    }
                }
                ++p_el;
            }
        }
    }

    // End of list reached. Return 1 if at least one new element was found.
    p_pattern->current_id = chunk_stop_id;
    return ((*p_found > found_before)?1:0);
}

/* List of derivered search pattern from
 * the input
 * Returned array length of pp_results is [days] x [durations] x ...
 *
 * Note that all pattern point on the same title_pattern. (=> Avoid double free'ing)
 */
int search_gen_patterns_for_partial(
        search_workspace_t *p_s_ws,
        arguments_t *p_arguments,
        search_pattern_t **pp_results
        )
{
    if( *pp_results != NULL ){
        Free(*pp_results);
    }

    search_pattern_t first_pattern = {
        {{{-1, -1, -1, NO_CHANNEL}}}, /* linked_list_subgroup_indizes_t */
        NULL, NULL, {NULL}, // title_pattern + _title_sub_pattern  Array of NULL values...
        LINKED_LIST_FIRST_ID, // current_id
        0,                    // K/num_used_indizes
        {-1, -1, -1, -1},     // used_indizes
        {0, 0, 0, 0}          // start_ids
    };
#ifdef WITH_TOPIC_OPTION
    transform_search_title(p_s_ws->restrict_string_search_on_topic?
            p_arguments->topic:p_arguments->title, &first_pattern.title_pattern);
#else
    transform_search_title(p_arguments->title, &first_pattern.title_pattern);
#endif
    split_pattern(&first_pattern, '*');

    // 0. Eval (or over-estimate) number of pattern and init first entry
    int len_results = 1;
    if( p_arguments->dayMin > -1 ){
        // This would be to big if dayMax >> NUM_REALTIVE_DATE ...
        //len_results *= (p_arguments->dayMax - p_arguments->dayMin + 1);

        // ... better is
        int relative_day_to_creation = p_arguments->dayMax + 1 // Begin with oldest.
            - (p_s_ws->search_itoday - p_s_ws->itoday);
        if( relative_day_to_creation < 0 ){
            // This day is to young to be found in this index file.
            // This holds for all days in the range [dayMin, dayMax],
            // thus it exists no matching entry at all.
            return 0;
        }

        int relative_day_to_creation2 = p_arguments->dayMin + 1
            - (p_s_ws->search_itoday - p_s_ws->itoday);

        // Cut ranges
        relative_day_to_creation = clip(0, relative_day_to_creation,
                NUM_REALTIVE_DATE-1);
        relative_day_to_creation2 = clip(0, relative_day_to_creation2,
                NUM_REALTIVE_DATE-1);

        assert( relative_day_to_creation >= relative_day_to_creation2 );

        // Apply range cut on input args to keep range width on same value.
        // This is important for the next_search_tuple() function.
        p_arguments->dayMax = relative_day_to_creation
            + (p_s_ws->search_itoday - p_s_ws->itoday);
        p_arguments->dayMin = relative_day_to_creation2
            + (p_s_ws->search_itoday - p_s_ws->itoday);

        first_pattern.groups.i_relative_date = relative_day_to_creation;
        len_results *= (relative_day_to_creation - relative_day_to_creation2 + 1);
    }

    if( p_arguments->beginMin > -1 ){
        int i1, i2;
        for( i1=0; i1<NUM_TIME; ++i1 ){
            if( p_arguments->beginMin <=
                    p_s_ws->index.meta.timestamp_borders[i1] ){
                first_pattern.groups.i_timestamp = i1;
                break;
            }
        }
        for( i2=i1; i2<NUM_TIME; ++i2 ){
            if( p_arguments->beginMax <=
                    p_s_ws->index.meta.timestamp_borders[i2] ){
                break;
            }
        }
        len_results *= (i2 - i1 + 1);
    }
    if( p_arguments->durationMin > -1 ){
        int i1, i2;
        for( i1=0; i1<NUM_DURATIONS; ++i1 ){
            if( p_arguments->durationMin <=
                    p_s_ws->index.meta.duration_borders[i1] ){
                first_pattern.groups.i_duration = i1;
                break;
            }
        }
        for( i2=i1; i2<NUM_DURATIONS; ++i2 ){
            if( p_arguments->durationMax <=
                    p_s_ws->index.meta.duration_borders[i2] ){
                break;
            }
        }
        len_results *= (i2 - i1 + 1);
    }
    if( p_arguments->channelName != NULL){
        // First, try untransformed channel name
        int ic = get_channel_number(&p_s_ws->channels, p_arguments->channelName, 0);
        if( ic == -1){ // Try transformed name
            char *norm_chan_name = NULL;
            transform_channel_name(p_arguments->channelName, &norm_chan_name);
            ic = get_channel_number(&p_s_ws->channels, norm_chan_name, 0);
            Free(norm_chan_name);
        }
        if( ic == -1){
            // Name not found. Select invalid channel number
            // if -c argument is unset
            if( first_pattern.groups.i_channel < 0 ){
                first_pattern.groups.i_channel  = NUM_CHANNELS;
            }
        }else{
            first_pattern.groups.i_channel  = ic;
        }
    }
    if( first_pattern.groups.i_channel < 0 ){
        first_pattern.groups.i_channel = p_arguments->channelNr;
    }

    assert( len_results > 0 );
    assert( len_results <= NUM_REALTIVE_DATE * NUM_TIME * NUM_DURATIONS );

    if( first_pattern.groups.i_channel < NO_CHANNEL ||
            first_pattern.groups.i_channel >= NUM_CHANNELS){
        // The channel string was not found (or something other
        // went wrong. No valid entry could be found for this input.
        // Here, *pp_results is NULL.
        assert( *pp_results == NULL );
        return 0;
    }

    *pp_results = (search_pattern_t *) malloc( len_results * sizeof(search_pattern_t) );
    search_pattern_t *first = *pp_results;
    search_pattern_t *cur = *pp_results;
    *cur = first_pattern;

    // 1. Loop to generate other elements of pp_results
    search_pattern_t *next = cur + 1;
    //arguments_t args_copy = *p_arguments;

    int i=1;
    while( i<len_results ){
        *next = *cur;
        ++i;
        if( next_search_tuple(p_s_ws, p_arguments,
                    first, next) )
        {
            break;
        }
        ++cur;
        ++next;
    }
    assert( i == len_results ); // Estimated length reached.

    return  i;
}

int search_do_search_partial(
        search_workspace_t *p_s_ws,
        search_pattern_t *p_pattern)
{
    linked_list_t *p_list = &p_s_ws->index;

    if( p_pattern->current_id == 0){
        // Search chain for this pattern already reached it's end.
        return -1;
    }
    // Find start points for search. One for each relation
    if( p_pattern->current_id == LINKED_LIST_FIRST_ID){
        // First run -> Init interal arrays
        _num_used_pattern(p_s_ws, p_pattern);

        // Highest of the releation ids is first possible match.
        int i;

        /* Set current_id to 0. If K=0, then the call of _search_by_title_partial_()
         * will be update this variable before this function will be called again.
         */
        p_pattern->current_id = 0;
        for( i=0; i<p_pattern->K; ++i){
            if( p_pattern->current_id < p_pattern->start_ids[i] ){
                p_pattern->current_id = p_pattern->start_ids[i];
            }
        }
    }

    const int K = p_pattern->K;
    const uint32_t *used_indizes = p_pattern->used_indizes;
    const uint32_t *ids = p_pattern->start_ids;

    if( K == 0 ){
        if( p_pattern->title_pattern != NULL ){
            return _search_by_title_partial_(p_s_ws, p_pattern);
        }else{
            // Search without criteria
            return -1;
        }
    }

    // This id is the first in the currently loaded chunk(s)
    uint32_t chunk_start_id = p_s_ws->chunks.start_ids[
        p_s_ws->chunks.partial_i];
    // This id is the first after the currently loaded chunk(s)
    uint32_t chunk_stop_id = p_s_ws->chunks.start_ids[
        p_s_ws->chunks.partial_i + 1];

    if( p_pattern->current_id < chunk_start_id ||
            p_pattern->current_id >= chunk_stop_id)
    {
        // Can not search in this chunk
        return 0;
    }

    const int found_max = p_s_ws->output.M;
    uint32_t found_before = p_s_ws->output.found;
    uint32_t *p_found = &p_s_ws->output.found;

    assert( K >= 1 );
    int k = 0, k2;
    int num_matched = 1;
    int i;

    // Find element with highest id. This will be the start point of the search.
    for( i=1; i<K; ++i){
        if( ids[k] < ids[i] ){
            k=i;
            num_matched = 1;
        }else if( ids[k] == ids[i]){
            ++num_matched;
        }
    }
    uint32_t id=ids[k], id2;
    if( id == 0 ){
        assert( 0 ); // p_pattern->current_ids was not 0, but had to.
        return -1;
    }

    index_node_t *p_el = linked_list_get_node(p_list, id);

    while( *p_found < found_max ){
        while( num_matched < K ){
            // Check if current element fulfill next releation
            k2 = (k + 1) % K;
            uint32_t next_id_isub = (p_el->nexts.subgroup_ids[used_indizes[k2]]);
            if( LINKED_LIST_SUBGROUP(next_id_isub) == p_pattern->groups.indizes[used_indizes[k2]] )
            {
                // Yes, relation match
                ++num_matched;
                k = k2;

            }else{
                // No, relation is wrong
                // Go to next element in currenty selected (by used_indizes[k]) releation.
                // TODO: Tippelschritte...
                id2 = linked_list_next_in_subgroup_by_id(p_list, id, used_indizes[k]);
                if( id2 == 0 || id2 >= chunk_stop_id ){
                    // update start_id at position k.
                    // Thus, the algorithm selects this index, k, at the next call.
                    p_pattern->start_ids[k] = id2;
                    p_pattern->current_id = id2;
                    return ((*p_found > found_before)?1:0);
                }

                id = id2;
                p_el = linked_list_get_node(p_list, id);
                num_matched = 1;
            }

        }

        // Id fulfill all releations. Now, compare the title string
        if(( p_pattern->title_pattern == NULL ||
                    _search_compare_title_(p_s_ws, p_pattern, p_el) == 0 ) &&
                _all_arguments_fulfilled(p_s_ws, p_el) == 0 )
        {
            output_add_id(&p_s_ws->output, p_el->id);
        }

        // Generate start-id for next round as biggest successor
        int j;
        id = LINKED_LIST_ID(p_el->nexts.subgroup_ids[used_indizes[0]]);
        k = 0;
        for( j=1; j<K; ++j ){
            id2 = LINKED_LIST_ID(p_el->nexts.subgroup_ids[used_indizes[j]]);
            if( id2 > id ){ id = id2; k = j; }
        }
        if( id >= chunk_stop_id || id == 0){
            // update start_id at position k.
            // Thus, the algorithm selects this index, k, at the next call.
            p_pattern->start_ids[k] = id;
            p_pattern->current_id = id;
            return ((*p_found > found_before)?1:0);
        }
        p_el = linked_list_get_node(p_list, id);
        num_matched = 1;
    }

    // found_max elements found or end of list reached
    return ((*p_found > found_before)?1:0);
}

// ############## End of function with partial file loading

void search_reset_workspace(
        search_workspace_t *p_s_ws)
{
#ifdef COMPRESS_BROTLI
    brotli_decoder_ws_destroy(&p_s_ws->brotli_title);
    p_s_ws->brotli_title = brotli_decoder_ws_create();
#endif
    uninit_chunks(p_s_ws);

    if( p_s_ws->p_arguments->diff_update ){
        p_s_ws->payload.id = FIRST_DIFF_INDEX - 1;
        p_s_ws->searchable_strings.id = FIRST_DIFF_INDEX - 1;
    }else{
        p_s_ws->payload.id = -1;
        p_s_ws->searchable_strings.id = -1;
    }

}

int split_pattern(
        search_pattern_t *p_pattern,
        char split_char)
{
    char **array  = p_pattern->title_sub_pattern;

    if( MAX_SUB_PATTERN < 1 ){
        assert(0); // Hey, at least two elements required in title_sub_pattern
        *array = NULL;
        return 0;
    }

    //Clear previous copy of title_pattern
    Free(p_pattern->_title_sub_pattern);
    p_pattern->_title_sub_pattern = strndup(p_pattern->title_pattern, 1000);

    // Map on new
    *array = p_pattern->_title_sub_pattern;

    char **pp_cur = array;

    char *p_star = strchr(*pp_cur, split_char);
    while( p_star != NULL && (pp_cur-array) < MAX_SUB_PATTERN ){
        *p_star = '\0'; // cut string
        if( *pp_cur == p_star ){
            // * found on first character, which indicates an empty word between two *'s
            // or between * and begin/end of title_pattern.
            // Just set begin of current token on next char.
            *pp_cur = p_star+1;
        }else{
            // Begin next token
            ++pp_cur;
            *pp_cur = p_star+1;
        }
        p_star = strchr(*pp_cur, split_char);
    }
    if( **pp_cur != '\0' ){
        ++pp_cur;
    }
    *pp_cur = NULL; // mark end of array

#ifndef NDEBUG
    fprintf(stderr, "Transformed title/topic pattern: ");
    char **l  = p_pattern->title_sub_pattern;
    while( *l != NULL ){
        fprintf(stderr, "'%s' ", *l);
        ++l;
    }
    fprintf(stderr, "\n");
#endif

    return (pp_cur-array);
}


int get_title_and_topic(
        search_workspace_t *p_s_ws,
        uint32_t id,
        const char **p_title,
        const char **p_topic
        )
{
    searchable_strings_prelude_t *p_entry;
    size_t iChunk;
    iChunk = title_entry(p_s_ws, id, &p_entry);

    // Limits
    title_chunks_t *p_chunks = &p_s_ws->chunks;
#ifdef READ_TITLE_FILE_PARTIAL 
    assert(iChunk == p_chunks->partial_i);
    char_buffer_t *p_current_chunk = &p_chunks->bufs[p_chunks->partial_i];
#else
    char_buffer_t *p_current_chunk = &p_chunks->bufs[iChunk];
#endif
    char * const buf_start = p_current_chunk->p;
    char * const buf_stop = buf_start + p_current_chunk->len;

    const char * const title = (const char *)(p_entry+1);
    assert(title+strlen(title) < buf_stop);
    *p_title = title;

    const char * const topic_candidate = title + p_entry->topic_string_offset;
    if( topic_candidate < buf_start ){
        *p_topic = p_s_ws->prev_topic.target;
        return 2;
    }else if( topic_candidate < buf_stop){
        *p_topic = topic_candidate;
        p_s_ws->prev_topic.target = topic_candidate;
        //p_s_ws->prev_topic.target_len = strlen(topic_candidate); // Unused

        return p_entry->topic_string_offset<0?1:0;
    }else{
        assert(topic_candidate < buf_stop);
        // Offset to high!
        *p_topic = NULL;
        return -1;
    }

    return 0;
}
