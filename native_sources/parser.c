/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:expandtab */
#include "parser.h"
#include "filmliste.h"
#include "channels.h"
#include "helper.h"

void char_buffer_clear(
        char_buffer_t *p_buf)
{
    p_buf->used = 0;
    p_buf->len = 0;
    free(p_buf->p);
    p_buf->p = NULL;
}

search_pair_t *search_pair_create(){
    static const search_pair_t sp = {
        {{'\0', 1 }, 0, NULL, 0}, /* {{pattern, offset}, mask, jumps, abort} */
        {{'\0', 1 }, 0, NULL, 0}, /* {{pattern, offset}, mask, jumps, abort} */
        NULL, NULL,
        {0, 0, NULL}, /* internal buffer */
        0, 0, /* Offsets */
    };
    search_pair_t *ret = malloc(sizeof(*ret));
    if( ret ){
        memcpy(ret, &sp, sizeof(sp));
    }
    return ret;
}

void search_pair_destroy(
        search_pair_t **pp_sp)
{
    free((*pp_sp)->buf.p);
    free(*pp_sp);
    *pp_sp = NULL;
}

void search_pair_reset(
        search_pair_t *sp)
{
    sp->hit_begin = NULL;
    sp->hit_end = NULL;
    sp->buf.used = 0;
}

void search_pair_reset_array(
        search_pair_t **start, search_pair_t **end)
{
    while( start<end ){
        search_pair_reset(*start);
        ++start;
    }
}

size_t search_pair_get_chars(
        const char_buffer_t *p_buf,
        search_pair_t *sp,
        const char **begin,
        const char **end,
        char close_string)
{
    size_t len;
    const char *_begin, *_end;

    assert( p_buf != NULL );
    assert( sp != NULL );
    assert( begin != NULL );

    if( sp->buf.used > 0 ){
        // Combine split strings if required.
        search_pair_cache(p_buf, sp);

        _begin = sp->buf.p;
        _end = _begin + sp->buf.used;
    }else{
        _begin = sp->hit_begin;
        _end = sp->hit_end;
    }

    if( sp->clip_begin ){
        _begin = clip( \
                _begin, \
                _begin + sp->clip_begin, \
                _end - 1);
    }

    if( sp->clip_end ){
        _end = clip( \
                _begin /*+ 1*/, \
                _end - sp->clip_end, \
                _end);
    }

    *begin = _begin;
    len = _end - _begin;
    if( end != NULL ){
        *end = _end;
    }

    if( close_string )
    {
        // Note: This ignores the 'const' definition and alters the input buffer.
        char *tmp = (char *) (_end);
        *tmp = '\0';
    }

    return len;
}

void search_pair_cache(
        const char_buffer_t *p_buf,
        search_pair_t *sp)
{
    if( sp->hit_begin == NULL && sp->hit_end == NULL){
        return;
    }

    const char *begin = (sp->hit_begin == NULL)?p_buf->p:sp->hit_begin;
    const char *end = (sp->hit_end == NULL)?(p_buf->p + p_buf->used):sp->hit_end;

#if 1
    if( (begin - p_buf->p) > p_buf->used ||
            (end - p_buf->p) > p_buf->used  ||
            end <= begin ){
        fprintf(stderr, "Error: hit range not valid!\n");
        return;
    }
#else
    assert( (begin - p_buf->p) <= p_buf->used &&
            (end - p_buf->p) <= p_buf->used &&
            end > begin );
#endif
    size_t more_buf_used = end - begin;
    if( sp->buf.len < sp->buf.used + more_buf_used ){
        // Allocate bigger buffer
        sp->buf.len = sp->buf.used + more_buf_used + 8;
        if( sp->buf.used ){
            sp->buf.p = (char*) realloc((void *)sp->buf.p, sp->buf.len + 1); /* + 1 for '\0' */
        }else{
            free(sp->buf.p);
            // exccedes the written string in some cases.
            sp->buf.p = (char*) malloc(sp->buf.len + 1); /* + 1 for '\0' */
            // Made valgrind happy. Utf-8 lib reads 4 words wide, which
            *((uint32_t*)(sp->buf.p + sp->buf.len - 8)) = 0;
            *((uint32_t*)(sp->buf.p + sp->buf.len - 4)) = 0;
            // or
            //sp->buf.p = calloc(sp->buf.len + 1, sizeof(*sp->buf.p));  // char
        }
    }

    //fprintf(stdout, "used %i more %i\n", sp->buf.used, more_buf_used);
#if 1
    memcpy(sp->buf.p + sp->buf.used, begin, more_buf_used);
#else
    int b;
    for(b=0; b<more_buf_used; ++b){
        sp->buf.p[sp->buf.used + b] = begin[b];
    }
#endif
    sp->buf.used +=  more_buf_used;
    sp->hit_begin = NULL;
    sp->hit_end = NULL;
}

void search_pair_cache_array(
        const char_buffer_t *p_buf,
        search_pair_t **start, search_pair_t **end)
{
    while( start<end ){
        search_pair_cache(p_buf, *start);
        ++start;
    }
}

void search_pair_dprintf(
        const int fd,
        const search_pair_t *sp)
{
    // Attention: Use of printf syntax for non-null terminated strings!
    // printf("%.*s", stringLength, pointerToString);

    if( sp->buf.used > 0 ){
        dprintf(fd, "%.*s", (int)(sp->buf.used), sp->buf.p);
    }
    if( sp->hit_begin != NULL && sp->hit_end != NULL){
        assert(sp->hit_end > sp->hit_begin);
        dprintf(fd, "%.*s", (int)(sp->hit_end - sp->hit_begin), sp->hit_begin);
    }
}

void search_array_dprintf(
        const int fd,
        const search_pair_t **start,
        const search_pair_t **end)
{
    while( start<end ){
        search_pair_dprintf(fd, *start);
        ++start;
    }
}

ssize_t search_pair_write(
        const int fd,
        const search_pair_t *sp)
{
    ssize_t w1 =0, w2 = 0;

    if( sp->buf.used > 0 ){
        w1 = write(fd, sp->buf.p, sp->buf.used);
        if( (ssize_t)sp->buf.used > w1 ){
            fprintf(stderr, "Write of %zd bytes failed.\n", sp->buf.used);
            return -1;
        }
    }
    if( sp->hit_begin != NULL && sp->hit_end != NULL){
        assert(sp->hit_end > sp->hit_begin);
        size_t l = (sp->hit_end - sp->hit_begin);
        w2 = write(fd, sp->hit_begin, l);
        if( (ssize_t)l > w2 ){
            fprintf(stderr, "Write of %zu bytes failed.\n", l);
            return -1;
        }
    }
    return w1+w2;
}

ssize_t search_array_write(
        const int fd,
        const search_pair_t **start,
        const search_pair_t **end)
{
    ssize_t wsum = 0, w;
    while( start<end ){
        w = search_pair_write(fd, *start);
        if( w == -1 ) return -1;
        wsum += w;
        ++start;
    }

    w = write(fd, "\n", 1);
    if( w == -1 ) return -1;
    wsum += w;
    return wsum;
}

void search_array_flush(
        const int fd,
        char_buffer_t *p_buf)
{
    ssize_t w = write(fd, p_buf->p, p_buf->used);
    if( (ssize_t)p_buf->used > w )
    {
        fprintf(stderr, "Write of %zi bytes failed.\n", p_buf->used);
    }
    assert( w == p_buf->used );
    p_buf->used = 0;
}

void fsearch_array_flush(
        FILE *stream,
        char_buffer_t *p_buf)
{
    size_t w = fwrite(p_buf->p, 1, p_buf->used, stream);
    if( (size_t)p_buf->used > w )
    {
        fprintf(stderr, "Write of %zi bytes failed.\n", p_buf->used);
    }
    assert( w == p_buf->used );
    p_buf->used = 0;

}

void search_pair_write_cached(
        const int fd,
        char_buffer_t *p_buf,
        const search_pair_t *sp)
{

    assert( sp->hit_begin == NULL
            || sp->hit_end == NULL
            || sp->hit_end > sp->hit_begin );


    // Evaluate required size
    size_t required = sp->buf.used;

    if( sp->hit_begin != NULL && sp->hit_end != NULL){
        required += (sp->hit_end - sp->hit_begin);
    }

    // Compare with available size and flush cache
    if( required > (p_buf->len - p_buf->used)){ // (*)
        search_array_flush(fd, p_buf);

        // Re-check if buffer is still to small.
        if( required > (p_buf->len - p_buf->used)){
            fprintf(stderr, "Caching of %zu bytes failed. Buffer too small!\n", required);
        }
    }

    // Copy data into buffer
    if( sp->buf.used > 0 ){
        memcpy(p_buf->p + p_buf->used, sp->buf.p, sp->buf.used);
        p_buf->used += sp->buf.used; // No overflow due (*)
        required -= sp->buf.used; // No underflow due construction of required.
    }

    if( required ){
        memcpy(p_buf->p + p_buf->used, sp->hit_begin, required);
        p_buf->used += required;
    }
}

void search_array_write_cached(
        const int fd,
        char_buffer_t *p_buf,
        const search_pair_t **start,
        const search_pair_t **end)
{
    while( start<end ){
        search_pair_write_cached(fd, p_buf, *start);
        ++start;
    }

    // Split texts by newline
    if( 1 > (p_buf->len -  p_buf->used) ){
        search_array_flush(fd, p_buf);

        if( 1 > (p_buf->len -  p_buf->used) ){
            fprintf(stderr, "Hey, buffer of length %zi is too small!\n", p_buf->len);
            return;
        }
    }
    p_buf->p[p_buf->used] = '\n';
    ++p_buf->used;
}

// ============================================

int search_elem_search(
        const char_buffer_t *p_buf,
        size_t start, size_t *p_stop,
        const search_elem_t *el)
{
    const char * const end = p_buf->p + p_buf->used;
    const char *pos = p_buf->p + start;

    while( pos<end ){
        // 1. Handle jumps
        if( el->jumps ){
            pattern_t *j = el->jumps;
            while(j){
                if( *pos == j->c ){
                    pos += j->jump;
                    break;
                }
                ++j;
            }
            // If above break was used j is not NULL
            if( j ) continue; // Restart main loop
        }

        // 2. Handle char
        if( *pos == el->pattern.c ){

            // Handle mask
            if( el->mask  &&
               pos > p_buf->p  && *(pos-1) == el->mask){
                // Ignore this match because element is masks.
            }else{
                *p_stop = (pos - p_buf->p); // < p_buf.used;
                return SEARCH_MATCH_ELEM;
            }
        }else if( *pos == el->abort ){
            *p_stop = (pos - p_buf->p);
            return SEARCH_FAILED_DUE_ABORT_CHAR;
        }
        ++pos;
    }

    // Mask check on last char of p_buf
    if( pos > p_buf->p  && *(pos-1) == el->mask){
        // Force skip first char on next buffer.
        ++pos;
    }

    *p_stop = (pos - p_buf->p); // >= p_buf->used;
    return SEARCH_FAILED_END_OF_BUFFER;
}

int search_pair_search(
        const char_buffer_t *p_buf,
        size_t start, size_t *p_stop,
        search_pair_t *sp)
{
    const search_elem_t *el = (sp->hit_begin || sp->buf.used > 0)?&sp->end:&sp->begin;
    //const char *pos;
    int status;

    assert( sp->hit_end == NULL ); /* If false, begin and end are already found. */

    status = search_elem_search(p_buf, start, p_stop, el);
    if( status == SEARCH_MATCH_ELEM ){
        // Save position
        if( el == &sp->end ){
            sp->hit_end = p_buf->p + *p_stop + 1;
        }else{
            sp->hit_begin = p_buf->p + *p_stop;
        }

        // Add jump offset of element
        *p_stop += el->pattern.jump;

        return SEARCH_MATCH_PAIR;
    }
    return status;
}

int parser_search(
        const char_buffer_t *p_buf,
        size_t buf_start, size_t *p_buf_stop,
        search_pair_t **pp_pairs, size_t pair_start, size_t *p_pair_stop )
{
    size_t cur_buf_offset = buf_start;
    //size_t next_buf_offset;
    int status;
    search_pair_t **cur_pair = pp_pairs + pair_start;

    while( *cur_pair != NULL ){
        status = search_pair_search(
                p_buf,
                cur_buf_offset, &cur_buf_offset,
                *cur_pair);

        if( status == SEARCH_FAILED_DUE_ABORT_CHAR ){
            // current search failed
            *p_buf_stop = cur_buf_offset;
            *p_pair_stop = (cur_pair - pp_pairs);
            return status;
        }

        //if( cur_buf_offset >= p_pub->used )
        if( status == SEARCH_FAILED_END_OF_BUFFER )
        {
            // We need more input data to continue.
            search_pair_cache_array(p_buf,
                    pp_pairs + pair_start, (cur_pair+1));

            // Not all pairs found
            *p_buf_stop = cur_buf_offset;
            *p_pair_stop = (cur_pair - pp_pairs);
            return status;
        }

        if( (*cur_pair)->hit_end ){
            // End token found. Continue with next search_pair
            ++cur_pair;
        }else{
            // Continue with same search_pair, but search end token at next round.
            assert( (*cur_pair)->hit_begin != NULL );
        }
    }

    // All pairs found
    *p_buf_stop = cur_buf_offset;
    *p_pair_stop = (cur_pair - pp_pairs);
    return SEARCH_MATCH_PAIRS;
}

search_pair_t **pattern_1_create()
{
    const int NP = 1;
    int i;

    search_pair_t **pp_pairs = (search_pair_t **) malloc((NP+1) * sizeof(search_pair_t*));
    pp_pairs[NP] = NULL;
    for( i=0; i<NP; ++i){
        pp_pairs[i] = search_pair_create();
    }

    // "..."-Pair
    pp_pairs[0]->begin.pattern.c = '"';
    pp_pairs[0]->begin.mask = '\\';
    pp_pairs[0]->begin.abort = '\n';
    pp_pairs[0]->end.pattern.c = '"';
    pp_pairs[0]->end.mask = '\\';
    //pp_pairs[0]->end.abort = '\n';

    return pp_pairs;
}

search_pair_t **pattern_filmliste_flexibel_create()
{
    const int NP = 21;
    int i;

    search_pair_t **pp_pairs = (search_pair_t **) malloc((NP+1) * sizeof(search_pair_t*));
    pp_pairs[NP] = NULL;

    pp_pairs[0] = search_pair_create();
    // "..."-Pair
    pp_pairs[0]->begin.pattern.c = '"';
    pp_pairs[0]->begin.mask = '\\';
    pp_pairs[0]->begin.abort = ']';
    pp_pairs[0]->end.pattern.c = '"';
    pp_pairs[0]->end.mask = '\\';
    pp_pairs[0]->end.abort = '\n';
    pp_pairs[0]->clip_begin = 1;
    pp_pairs[0]->clip_end = 1;

    for( i=1; i<NP; ++i){
        pp_pairs[i] = search_pair_create();
        memcpy(pp_pairs[i], pp_pairs[0], sizeof(search_pair_t));
    }
    pp_pairs[0]->begin.abort = ':';
    pp_pairs[0]->end.abort = ':';
    pp_pairs[NP-1]->end.pattern.c = ']';

    return pp_pairs;
}

search_pair_t **pattern_filmliste_head()
{
    // "Filmliste" [ "Date string 1", "Date string 2" [...]]
    const int NP = 3;
    int i;

    search_pair_t **pp_pairs = (search_pair_t **) malloc((NP+1) * sizeof(search_pair_t*));
    pp_pairs[NP] = NULL;

    pp_pairs[0] = search_pair_create();
    // "..."-Pair
    pp_pairs[0]->begin.pattern.c = '"';
    pp_pairs[0]->begin.mask = '\\';
    pp_pairs[0]->begin.abort = ']'; //not '\n' to search over multiple lines
    pp_pairs[0]->end.pattern.c = '"';
    pp_pairs[0]->end.mask = '\\';
    pp_pairs[0]->end.abort = '\n';
    pp_pairs[0]->clip_begin = 1;
    pp_pairs[0]->clip_end = 1;

    for( i=1; i<NP; ++i){
        pp_pairs[i] = search_pair_create();
        memcpy(pp_pairs[i], pp_pairs[0], sizeof(search_pair_t));
    }

    return pp_pairs;
}

void pattern_destroy( search_pair_t ***ppp_pairs)
{
    search_pair_t **pp_pairs = *ppp_pairs;
    while( *pp_pairs != NULL ){
        search_pair_destroy(pp_pairs);
        ++pp_pairs;
    }
    free(*ppp_pairs);
    *ppp_pairs = NULL;
}

// ============================================

