
/* This omit cmp-function calls during the sorting 
 * and gives a marginal speed improvement. 
 * It' would be more efficient to omit the sorting at
 * each flush, but this require more memory.
 */
#define USE_INLINE_QSORT
#ifdef USE_INLINE_QSORT
// Inline qsort
#include "qsort.h"
#else
// For gcc's qsort_r
#define _GNU_SOURCE
#include <stdlib.h>
#endif

#include "helper.h"
#include "parser.h"
#include "filmliste.h"
#include "search.h"

#define INC_MODULO(A, B) A = ((++(A) == B)?0:(A));
#define DEC_MODULO(A, B) A = ((A == 0)?(B-1):(--A));

/* Number of extra slots to collect output entries. If 
 * full, a sort will be triggerd. After this the slots are
 * free again. 
 * Only use a value > 0.
 * Maximal useful value: Maximal number of elements in each search-chunk.
 */
#define NSEARCH_SIZE(NSKIP, N) (10000)

static search_workspace_t *_p_s_ws = NULL; // For sortings.

void output_qsort_set_workspace(
        search_workspace_t *p_s_ws)
{
    _p_s_ws = p_s_ws; // Static pointer for qsort...
}

#ifdef USE_INLINE_QSORT 
#define PREPARE_BY_DATE(p_left, p_right) \
        left = (const output_candidate_t*)p_left; \
        right = (const output_candidate_t*)p_right; \
        left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time); \
        right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time); \
        
#define COMP_BY_DATE(p_left, p_right) (left_day < right_day)
#define COMP_BY_DATE_REV(p_left, p_right) (right_day < left_day)

int compByDate(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;
    time_t left_day, right_day;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_DATE, PREPARE_BY_DATE)
}
int compByDateRev(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;
    time_t left_day, right_day;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_DATE_REV, PREPARE_BY_DATE)
}
#else
int compByDate (const void * p_left, const void * p_right) 
{
    assert( _p_s_ws != NULL );
    //linked_list_t *p_list = &_p_s_ws->index;
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;
    const time_t left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time);
    const time_t right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time);

    return (left_day-right_day);
}
int compByDateRev (const void * p_left, const void * p_right) 
{
    assert( _p_s_ws != NULL );
    //linked_list_t *p_list = &_p_s_ws->index;
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;
    const time_t left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time);
    const time_t right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time);

    return (right_day-left_day);
}
#endif

int compByTime (const void * p_left, const void * p_right) 
{
    assert( _p_s_ws != NULL );
    //linked_list_t *p_list = &_p_s_ws->index;
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;
    const time_t left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time);
    const time_t right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time);

    // Local times. 
    const int left_begin = (left_day - _p_s_ws->search_ttoday - left_day) % 86400;
    const int right_begin = (right_day - _p_s_ws->search_ttoday - right_day) % 86400;
    // TODO: Respect daylight saving hour.

    return (left_begin-right_begin);
}

// Note that input list is already sorted by channels...
int compByChannel (const void * p_left, const void * p_right) 
{
    assert( _p_s_ws != NULL );
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;

#if 0
    return (left->id - right->id);
#else
    linked_list_t * const p_list = &_p_s_ws->index;
    const index_node_t *p_left_el = linked_list_get_node(p_list, left->id);
    const index_node_t *p_right_el = linked_list_get_node(p_list, right->id);
    const uint32_t left_channel = LINKED_LIST_SUBGROUP(p_left_el->nexts.channel);
    const uint32_t right_channel = LINKED_LIST_SUBGROUP(p_right_el->nexts.channel);

    return (left_channel-right_channel);
#endif
}

void output_select_sorting_function(
        const char* keyword,
        int *p_reversed_in_out,
        sort_cmp_handler_t **p_sort_handler_out)
{
    const char* asc_desc = NULL;
    int reversed2 = 0; // Implict reversed by keyword

    if( keyword == NULL || *keyword == '\0' ){
        *p_sort_handler_out = NULL;
        return;
    }

    if( (strncmp("date", keyword, sizeof("date")-1) == 0
                && (asc_desc = keyword + sizeof("date")-1)) 
            || (strncmp("day", keyword, sizeof("day")-1) == 0
                && (asc_desc = keyword + sizeof("day")-1))
      )
    {
        *p_reversed_in_out = (*p_reversed_in_out != 0) ^ (strcmp("Desc", asc_desc) == 0);
        if( *p_reversed_in_out ){
            *p_sort_handler_out = compByDateRev;
            DEBUG( fprintf( stderr, "Rev\n"); )
        }else{
            *p_sort_handler_out = compByDate;
        }
    }

    else if( (strncmp("time", keyword, sizeof("time")) == 0
                && (asc_desc = keyword + sizeof("time")))
            || (strncmp("begin", keyword, sizeof("begin")) == 0
                && (asc_desc = keyword + sizeof("begin")))
      )
    {
        *p_reversed_in_out ^= (strcmp("Desc", asc_desc) == 0);
        if( *p_reversed_in_out ){
            *p_sort_handler_out = compByTime;
        }else{
            *p_sort_handler_out = compByTime;
        }
    }

    else if( (strncmp("channel", keyword, sizeof("channel")) == 0
                && (asc_desc = keyword + sizeof("channel")))
      )
    {
        *p_reversed_in_out ^= (strcmp("Desc", asc_desc) == 0);
        if( *p_reversed_in_out ){
            *p_sort_handler_out = compByChannel;
        }else{
            *p_sort_handler_out = compByChannel;
        }
    }
}


output_t output_init(
        uint32_t N,
        uint32_t Nskip,
        int reversed,
        int comma_offset,
        sort_cmp_handler_t *sort_handler)
{
    output_t o;

    o.first_comma_offset = comma_offset;
    o.reversed_flag = reversed;
    o.sort_handler = sort_handler;

    o.search_whole_index_flag = o.reversed_flag|(o.sort_handler != NULL);

    o.N = N;
    o.Nskip = Nskip;
    o.M = o.N + o.Nskip;
    o.Nsearch = (o.sort_handler != NULL )?NSEARCH_SIZE(Nskip, N):0;
    o.M2 = o.M + o.Nsearch;

    o.found = 0;
    o.pos_i = o.M2-1;
    o.pos_p = o.M2-1;
    o.overlap_flag = 0;
    o.first_unhandled_id = 0;

    assert( o.M2 > 0 );

    o.p_candidates = calloc( max(o.M2, 1), sizeof(*o.p_candidates));  // (output_candidate_t  *)
    if( o.p_candidates == NULL ){
        Free(o.p_candidates);
        fprintf(stderr, "Allocation of mem for %u elements failed.\n", o.M);
        if( N != 1 || Nskip != 1 ){
            return output_init(1, 1, reversed, comma_offset, sort_handler);
        }
    }

    return o;
}

void output_uninit(
        output_t *p_output)
{
    uint32_t i;
    for( i=0; i<p_output->M; ++i){
        Free(p_output->p_candidates[i].to_print.p);
    }
    Free(p_output->p_candidates);
}

uint32_t dist(
        uint32_t i,
        uint32_t d,
        uint32_t M)
{
    assert( M>i && M>d );
    if( i<=d ) return (d-i);
    // d < i
    return (M-i-1) + d;
}

void output_add_id(
        search_workspace_t *p_s_ws,
        output_t *p_output,
        uint32_t id)
{
  int i = p_output->pos_i;
  INC_MODULO(i, p_output->M2);

  if( p_output->sort_handler && i == 0 ){
      // Buffer full -> toggle sorting to free Nsearch slots.
      output_flush(p_s_ws, p_output, 0);
      i = p_output->pos_i;
      INC_MODULO(i, p_output->M2);
  }

  p_output->p_candidates[i].id = id;
  p_output->pos_i = i;
  p_output->found++;
  if( p_output->pos_p == p_output->pos_i){
      // Push pos_p one position further. We can not lap it.
      //p_output->pos_p = (p_outp->pos_p+1) % (p_output->M);
      p_output->overlap_flag = 1;
      //INC_MODULO(p_output->pos_p, p_output->M2);
  }
}

void output_flush(
        search_workspace_t *p_s_ws,
        output_t *p_output,
        int b_last_flush)
{
    uint32_t i = p_output->pos_i;
    uint32_t p = p_output->pos_p;
    const uint32_t M = p_output->M;
    const uint32_t M2 = p_output->M2;
    const uint32_t prev_first_unhandled_id = p_output->first_unhandled_id;
    output_candidate_t * const p_candidates = p_output->p_candidates;

    if( p_output->overlap_flag == 1){
        p = i;
    }else if(i == p){
        // No new entry available to flush
        return;
    }
    INC_MODULO(p, M2);

    if( !p_output->search_whole_index_flag ){
        // Go fast-forward to last N entries because the other ones
        // would not be needed in any case.
        assert( M == M2 );
        uint32_t d = dist(p, i, M2);
        if( d > p_output->N){
            p = (i - p_output->N + M2) % M2;
        }
    }

    if( p_output->sort_handler ){
        assert( p <= i ); // Buffer not used 'cyclic' if sorting is enabled
        if( i >= M || b_last_flush){
            int p2 = p;
            while( p2 <= i ){
                assert( p_candidates[p2].id >= prev_first_unhandled_id );
                output_prepare_for_sort(p_s_ws, &p_candidates[p2]);
                ++p2;
            }

            // Store highest id for next call.
            p_output->first_unhandled_id = p_candidates[i].id + 1;

            //DEBUG( fprintf(stderr, "Sorting %u elements\n", i); )

            // This mixed the unprocessed ids into the processed.
            // Every Element behind position M-1 will never be processed.
#ifdef USE_INLINE_QSORT
            // Ugly, but handler maps to sort function but not to compare function.
            p_output->sort_handler(p_candidates, &i);
#else
            qsort(p_candidates,
                    i,
                    sizeof(*p_candidates),
                    p_output->sort_handler);
#endif

            p = 0;  // To force output_fill-call for all M entries.
            p_output->pos_p = p;
            i = M-1; // min(i, M-1);
            p_output->pos_i = i;

        }else{
            // Handling of unsorded is ok.
            int p2 = p;
            while( p2 <= i ){
                output_prepare_for_sort(p_s_ws, &p_candidates[p2]);
                ++p2;
            } 
            p_output->first_unhandled_id = p_candidates[i].id + 1;
        }

        while( p < i ){
            // Handle [0, pos_i-1]
            if( p_candidates[p].id >= prev_first_unhandled_id ){
                output_fill(p_s_ws, &p_candidates[p]);
            }
            ++p;
        }
        assert( p == i );

        // Handle pos_i
        if( p_candidates[p].id >= prev_first_unhandled_id ){
            output_fill(p_s_ws, &p_candidates[p]);
        }
        p_output->pos_p = p;
        p_output->overlap_flag = 0;

    }else{ //  Without sorting

        if( p > i ){
            // Handle [pos_p+1, M-1]. p will be 0 after this step
            while( p > 0 ){
                // Setup p_entry/iChunk (required for output_fill).
                output_prepare_for_sort(p_s_ws, &p_candidates[p]);

                output_fill(p_s_ws, &p_candidates[p]);
                INC_MODULO(p, M2);
            }
        }
        while( p < i ){
            // Handle [0, pos_i-1]
            output_prepare_for_sort(p_s_ws, &p_candidates[p]);
            output_fill(p_s_ws, &p_candidates[p]);
            ++p;
        }
        assert( p == i );

        // Handle pos_i
        output_prepare_for_sort(p_s_ws, &p_candidates[p]);
        output_fill(p_s_ws, &p_candidates[p]);
        p_output->pos_p = p;
        p_output->overlap_flag = 0;
    }
}

void output_prepare_for_sort(
        search_workspace_t *p_s_ws,
        output_candidate_t *p_candidate)
{
    const uint32_t id = p_candidate->id;
    p_candidate->iChunk = title_entry(p_s_ws, id, &p_candidate->p_entry);
    // Copy prelude into candidates array
    p_candidate->entry = *p_candidate->p_entry;
}

void output_fill(
        search_workspace_t *p_s_ws,
        output_candidate_t *p_candidate)
{
    const uint32_t id = p_candidate->id;
    output_str_t *p_out = &p_candidate->to_print;
    if( p_out->p == NULL ){
        p_out->len = OUT_DEFAULT_LEN;
        p_out->used = 0;
        p_out->p = malloc(p_out->len * sizeof(*p_out->p));  // (char *)
    }

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

    searchable_strings_prelude_t *p_entry = p_candidate->p_entry;

    //int title_len = p_entry->length; //followed by \0
    const char *title, *topic;
    uint32_t title_len;
#if NORMALIZED_STRINGS > 0
    const char *title_norm;
    uint32_t title_norm_len;
    title_norm = (const char *)(p_entry+1);  // Normalized 'title + topic' string...
    title_norm_len = strlen(title_norm); //followed by \0
    title = title_norm + title_norm_len + 1; // Original title
    title_len = strlen(title);
#else
    title = (const char *)(p_entry+1);

    /* Restrict on characters before SPLIT_CHAR to cut of 'topic' substring*/
#ifdef _GNU_SOURCE
    title_len = strchrnul(title, SPLIT_CHAR) - title;
#else
    char *sep = strchr(title, SPLIT_CHAR);
    if( sep ){
        title_len = sep - title;
    }else{
        title_len = strlen(title);
    }
#endif
    assert(title_len < -10000U); // Underflow check
#endif

    if( 0 > get_topic(p_s_ws, p_candidate->iChunk, p_entry, &topic) ){
        fprintf( stderr,
                "Error: Unable to evaluate topic string for id=%i, title=%s)\n",
                id, title);
        topic = NULL;

    }else if( *topic == '\0'){
        fprintf( stderr,
                "Warning: Empty topic string for id=%i, title=%s)\n",
                id, title);
        assert(0);
    }

    // Convert channel id to string
    const char *channel_str = get_channel_name(&p_s_ws->channels, channel);

    time_t absolute_day_begin = (time_t)(p_s_ws->ttoday + p_entry->relative_start_time);
    // Convert time to string
#if 0
    const char *absolute_day_str = ctime(&absolute_day_begin);
#else
    struct tm * p_timeinfo;
    char absolute_day_str[80];

    p_timeinfo = localtime (&absolute_day_begin);
    strftime(absolute_day_str, 80, "%d. %b. %Y %R", p_timeinfo);
#endif
    int retry;
    const char _format[] = ",\n\t{\"id\": %i, \"topic\": \"%s\", \"title\": \"%.*s\", " \
                          "\n\t\t\"ibegin\": %u, \"begin\": \"%s\", " \
                          "\"iduration\": %u, " \
                          "\"ichannel\": %u, \"channel\": \"%s\", " \
                          "\"anchor\": %u}";
    for(retry=0; retry<2; ++retry){
        int len_needed = snprintf(p_out->p, p_out->len,
                _format, id,
                topic, // p_entry->topic_string_offset,
                title_len, title,
                (uint32_t) absolute_day_begin, absolute_day_str,
                (uint32_t) p_entry->duration,
                channel, channel_str,
                payload_anchor
                );
        if( len_needed < p_out->len ){
            // buffer was long enough for output + '\0'
            p_out->used = len_needed;
            break;
        }
        // Increase buffer length
        Free(p_out->p);
        p_out->len = len_needed;
        p_out->used = 0;
        p_out->p = malloc(p_out->len * sizeof(*p_out->p));
    }
}

size_t output_print(
        int fd,
//        search_workspace_t *p_s_ws,
        output_t *p_output)
{
    const uint32_t M = p_output->M;
    const uint32_t M_1 = M-1;
    //const uint32_t N = p_output->N;
    const uint32_t Nskip = p_output->Nskip;
    const int R = p_output->reversed_flag && (p_output->sort_handler == NULL);
    uint32_t i = p_output->pos_i;
    uint32_t p = p_output->pos_p;
    uint32_t f = p_output->found;
    size_t written = 0;
    int offset_left = p_output->first_comma_offset;

    f = min(f,M);
    assert( i == p );
    _unused(i);

    if( R ){ // Backward...
        // p is on last element. Skip latests 'Nskip' values.
        if( f > Nskip){
            f -= Nskip;
            p = (p - Nskip + M) % M;

            // Limit offset
            if( f > 0 && ( 0 > offset_left
                        || offset_left > p_output->p_candidates[p].to_print.used ))
            {
                offset_left = 0;
            }

            while( f > 0 ){
                const output_str_t * p_out = &p_output->p_candidates[p].to_print;

                ssize_t w = write(fd, p_out->p + offset_left, p_out->used - offset_left);
                assert( (ssize_t)(p_out->used - offset_left) == w);
                if( (ssize_t)-1 == w) return written;
                written += w;
                offset_left = 0;

                p = (p==0)?(M_1):(p-1);
                --f;
            }
        }

    }else{ // Forward...
        // p is on last element. Go back to first and skip 'Nskip' elements.
        p = (p - (f-1) + M) % M;
        if( f > Nskip ){
            f -= Nskip;
            p = (p + Nskip) % M;

            // Limit offset
            if( f > 0 && ( 0 > offset_left
                        || offset_left > p_output->p_candidates[p].to_print.used ))
            {
                offset_left = 0;
            }

            while( f > 0 ){
                const output_str_t * p_out = &p_output->p_candidates[p].to_print;

                ssize_t w = write(fd, p_out->p + offset_left, p_out->used - offset_left);
                assert( (ssize_t)(p_out->used - offset_left) == w);
                if( (ssize_t)-1 == w) return written;
                written += w;
                offset_left = 0;

                p = (p==M_1)?0:(p+1);
                --f;
            }
        }
    }

    return written;
}
