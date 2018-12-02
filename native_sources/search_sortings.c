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
//#define _GNU_SOURCE
//#include <stdlib.h>
#endif

#include "helper.h"
#include "parser.h"
#include "filmliste.h"
#include "search.h"

static search_workspace_t *_p_s_ws = NULL; // For sortings.

void output_qsort_set_workspace(
        search_workspace_t *p_s_ws)
{
    _p_s_ws = p_s_ws; // Static pointer for qsort...
}

/* Attention: All sorting functions need to be strict. (never equals)
 * to stabilize the sorting.
 * Otherwise, we can not hold the assumption that elements flips
 * its position on the reverse sorting.
 */

#ifdef USE_INLINE_QSORT
#define PREPARE_BY_DATE(p_left, p_right) \
        left = (const output_candidate_t*)p_left; \
        right = (const output_candidate_t*)p_right; \
        left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time); \
        right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time); \

#define COMP_BY_DATE_ASC(p_left, p_right) ((left_day == right_day)?(left->id < right->id):(left_day < right_day))
#define COMP_BY_DATE_DESC(p_left, p_right) ((left_day == right_day)?(left->id > right->id):(left_day > right_day))

int compByDateAsc(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;
    time_t left_day, right_day;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_DATE_ASC, PREPARE_BY_DATE);
    return 0;
}
int compByDateDesc(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;
    time_t left_day, right_day;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_DATE_DESC, PREPARE_BY_DATE);
    return 0;
}
#else
int compByDateAsc (const void * p_left, const void * p_right)
{
    assert( _p_s_ws != NULL );
    //linked_list_t *p_list = &_p_s_ws->index;
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;
    const time_t left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time);
    const time_t right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time);

    const int dist = (left_day - right_day);
    if( dist ) return dist;
    return (left->id - right->id);
}
int compByDateDesc (const void * p_left, const void * p_right)
{
    assert( _p_s_ws != NULL );
    //linked_list_t *p_list = &_p_s_ws->index;
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;
    const time_t left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time);
    const time_t right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time);

    const int dist = (right_day-left_day);
    if( dist ) return dist;
    return (right->id - left->id);
}

#endif


#ifdef USE_INLINE_QSORT
#define PREPARE_BY_TIME(p_left, p_right) \
        left = (const output_candidate_t*)p_left; \
        right = (const output_candidate_t*)p_right; \
        left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time); \
        right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time); \
        left_begin = (_p_s_ws->search_ttoday - left_day) % 86400; \
        right_begin = (_p_s_ws->search_ttoday - right_day) % 86400; \

#define COMP_BY_TIME_ASC(p_left, p_right) ((left_begin == right_begin)?(left->id < right->id):(left_begin < right_begin))
#define COMP_BY_TIME_DESC(p_left, p_right) ((left_begin == right_begin)?(left->id > right->id):(left_begin > right_begin))

int compByTimeAsc(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;
    time_t left_day, right_day;
    int left_begin, right_begin;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_TIME_ASC, PREPARE_BY_TIME);
    return 0;
}
int compByTimeDesc(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;
    time_t left_day, right_day;
    int left_begin, right_begin;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_TIME_DESC, PREPARE_BY_TIME);
    return 0;
}

#else
int compByTimeAsc (const void * p_left, const void * p_right)
{
    assert( _p_s_ws != NULL );
    //linked_list_t *p_list = &_p_s_ws->index;
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;
    const time_t left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time);
    const time_t right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time);

    // Local times.
    const int left_begin = (_p_s_ws->search_ttoday - left_day) % 86400;
    const int right_begin = (_p_s_ws->search_ttoday - right_day) % 86400;
    // TODO: Respect daylight saving hour.

    const int dist = (right_begin-left_begin);
    if( dist ) return dist;
    return (right->id - left->id);
}

int compByTimeDesc (const void * p_left, const void * p_right)
{
    assert( _p_s_ws != NULL );
    //linked_list_t *p_list = &_p_s_ws->index;
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;
    const time_t left_day = (time_t)(_p_s_ws->ttoday + left->entry.relative_start_time);
    const time_t right_day = (time_t)(_p_s_ws->ttoday + right->entry.relative_start_time);

    // Local times.
    const int left_begin = (_p_s_ws->search_ttoday - left_day) % 86400;
    const int right_begin = (_p_s_ws->search_ttoday - right_day) % 86400;
    // TODO: Respect daylight saving hour.

    const int dist = (left_begin-right_begin);
    if( dist ) return dist;
    return (left->id - right->id);
}
#endif

#ifdef USE_INLINE_QSORT
#define PREPARE_BY_CHANNEL(p_left, p_right) \
        left = (const output_candidate_t*)p_left; \
        right = (const output_candidate_t*)p_right; \

#define COMP_BY_CHANNEL_ASC(p_left, p_right) (left->id < right->id)
#define COMP_BY_CHANNEL_DESC(p_left, p_right) (left->id > right->id)

int compByChannelAsc(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_CHANNEL_ASC, PREPARE_BY_CHANNEL);
    return 0;
}
int compByChannelDesc(const void *base, const void *p_nmemb) // sort_cmp_handler_t
{
    output_candidate_t * const p_candidates = (output_candidate_t *)base;
    size_t nmemb = *((size_t*)p_nmemb);
    // Temp variables
    const output_candidate_t *left, *right;

    QSORT_WITH_PREPARE(output_candidate_t, p_candidates, nmemb, COMP_BY_CHANNEL_DESC, PREPARE_BY_CHANNEL);
    return 0;
}
#else
// Note that input list is already sorted by channels...
int compByChannelAsc (const void * p_left, const void * p_right)
{
    assert( _p_s_ws != NULL );
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;

#if 1
    return (left->id - right->id);
#else
    linked_list_t * const p_list = &_p_s_ws->index;
    const index_node_t *p_left_el = linked_list_get_node(p_list, left->id);
    const index_node_t *p_right_el = linked_list_get_node(p_list, right->id);
    const uint32_t left_channel = LINKED_LIST_SUBGROUP(p_left_el->nexts.channel);
    const uint32_t right_channel = LINKED_LIST_SUBGROUP(p_right_el->nexts.channel);

    const int dist = (left_channel-right_channel);
    if( dist ) return dist;
    return (left->id - right->id);
#endif
}
int compByChannelDesc (const void * p_left, const void * p_right)
{
    assert( _p_s_ws != NULL );
    const output_candidate_t *left = (const output_candidate_t*)p_left;
    const output_candidate_t *right = (const output_candidate_t*)p_right;

#if 1
    return (right->id - left->id);
#else
    linked_list_t * const p_list = &_p_s_ws->index;
    const index_node_t *p_left_el = linked_list_get_node(p_list, left->id);
    const index_node_t *p_right_el = linked_list_get_node(p_list, right->id);
    const uint32_t left_channel = LINKED_LIST_SUBGROUP(p_left_el->nexts.channel);
    const uint32_t right_channel = LINKED_LIST_SUBGROUP(p_right_el->nexts.channel);

    const int dist = (right_channel-left_channel);
    if( dist ) return dist;
    return (right->id - left->id);
#endif
}
#endif

void output_select_sorting_function(
        const char* keyword,
        int *p_reversed_in_out,
        sort_cmp_handler_t **p_sort_handler_out)
{
    const char* asc_desc = NULL;

    if( keyword == NULL || *keyword == '\0' ){
        *p_sort_handler_out = NULL;
        return;
    }

    DEBUG( fprintf(stderr, "Selected sorting method: ") );
    if( (strncmp("date", keyword, sizeof("date")-1) == 0
                && (asc_desc = keyword + sizeof("date")-1))
            || (strncmp("day", keyword, sizeof("day")-1) == 0
                && (asc_desc = keyword + sizeof("day")-1))
      )
    {
        // Here, default order is 'Desc'
        *p_reversed_in_out = (*p_reversed_in_out != 0) ^ (strcmp("Asc", asc_desc) == 0);
        if( *p_reversed_in_out ){
            *p_sort_handler_out = compByDateAsc;
            DEBUG( fprintf(stderr, "By date, oldest at top\n") );
        }else{
            *p_sort_handler_out = compByDateDesc;
            DEBUG( fprintf(stderr, "By date, newest at top\n") );
        }
    }

    else if( (strncmp("time", keyword, sizeof("time")-1) == 0
                && (asc_desc = keyword + sizeof("time")-1))
            || (strncmp("begin", keyword, sizeof("begin")-1) == 0
                && (asc_desc = keyword + sizeof("begin")-1))
      )
    {
        *p_reversed_in_out = (*p_reversed_in_out != 0) ^ (strcmp("Desc", asc_desc) == 0);
        if( *p_reversed_in_out ){
            *p_sort_handler_out = compByTimeDesc;
            DEBUG( fprintf(stderr, "By time of day, latest at top\n") );
        }else{
            *p_sort_handler_out = compByTimeAsc;
            DEBUG( fprintf(stderr, "By time of day, earliest at top\n") );
        }
    }

    else if( (strncmp("channel", keyword, sizeof("channel")-1) == 0
                && (asc_desc = keyword + sizeof("channel")-1))
      )
    {
        *p_reversed_in_out = (*p_reversed_in_out != 0) ^ (strcmp("Desc", asc_desc) == 0);
        if( *p_reversed_in_out ){
            *p_sort_handler_out = compByChannelDesc;
            DEBUG( fprintf(stderr, "By channel, N-1\n") );
        }else{
            *p_sort_handler_out = compByChannelAsc;
            DEBUG( fprintf(stderr, "By channel, 1-N\n") );
        }
    }else{
        DEBUG( fprintf(stderr, "None.\n") );
    }
}

void output_sort(
        output_t *p_output)
{
    uint32_t n = min(p_output->pos_i + 1, p_output->found);

    if( p_output->sort_handler == NULL ){
        DEBUG( fprintf(stderr, "Print %u elements\n", n) );
        return;
    }
    DEBUG( fprintf(stderr, "Sorting %u elements\n", n) );

    // This mixed the unprocessed ids into the processed.
    // Every Element behind position M-1 will never be processed.
#ifdef USE_INLINE_QSORT
    // Ugly, handler maps to sort function, but not to compare function.
    p_output->sort_handler(p_output->p_candidates, &n);
#else
    qsort(p_output->p_candidates,
            n,
            sizeof(*p_output->p_candidates),
            p_output->sort_handler);
#endif

#if 0 // Test order
    for( int x=0; x<n; ++x){
        fprintf(stderr, "%i %i\n",x, p_output->p_candidates[x].id);
    }
#endif

}
