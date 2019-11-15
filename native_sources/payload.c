/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:expandtab */
#include <string.h>

#include "utf8.h"
#include "helper.h"
#include "parser.h"
#include "open.h"
#include "payload.h"

#define PAYLOAD_LEN 2000 // (Buffer reallocation if to small)
#define BLOCK_END_CHAR ']'

payload_workspace_t payload_ws_create(
        arguments_t *p_arguments)
{
    payload_workspace_t pay_ws = {
        {-1, -1, 0}, // payload
        {OUT_CACHESIZ, 0, char_buffer_malloc(OUT_CACHESIZ)}, // buf_in
        {PAYLOAD_LEN, 0, char_buffer_malloc(PAYLOAD_LEN)}, // buf_entry
        pattern_payload_file(), // token_pattern
        -1, // current_anchor_index
#ifdef COMPRESS_BROTLI
        brotli_decoder_ws_create(),
#endif
        p_arguments
    };

    return pay_ws;
}

void payload_ws_destroy(
        payload_workspace_t *p_pay_ws)
{
    if( p_pay_ws->payload.fd > -1 ){
        close(p_pay_ws->payload.fd);
        p_pay_ws->payload.fd = -1;
    }

    Free(p_pay_ws->buf_in.p);
    Free(p_pay_ws->buf_entry.p);

    pattern_destroy(&p_pay_ws->token_pattern);
#ifdef COMPRESS_BROTLI
    brotli_decoder_ws_destroy(&p_pay_ws->brotli_payload);
#endif
}

int open_payload_file(
        payload_workspace_t *p_pay_ws,
        int i_num)
{
    if( p_pay_ws->payload.fd > -1 ){
        close(p_pay_ws->payload.fd);
        p_pay_ws->payload.fd = -1;

#ifdef COMPRESS_BROTLI
        brotli_decoder_ws_destroy(&p_pay_ws->brotli_payload);
        p_pay_ws->brotli_payload = brotli_decoder_ws_create();
#endif
    }

    p_pay_ws->current_anchor_index = -1;
    p_pay_ws->payload.id = -1;

    if( i_num < 0 || i_num >= p_pay_ws->p_arguments->payload_anchors_len ){
        return -1;
    }

    uint32_t payload_anchor = p_pay_ws->p_arguments->payload_anchors[i_num];

    int id = (payload_anchor >> 24);

    // Construct file name
    assert( p_pay_ws->p_arguments->index_folder != NULL);
    size_t ts = strlen(p_pay_ws->p_arguments->index_folder) + sizeof(payload_file_template) + 10;
    char *tmp = malloc(ts * sizeof(*tmp));
    if( !tmp ){
        return -1;
    }

    snprintf(tmp, ts, payload_file_template,
            p_pay_ws->p_arguments->index_folder, id);  
    DEBUG( fprintf(stderr, "Open '%s'\n", tmp) );

    int fd = open(tmp, O_RDONLY);
    free(tmp);

    //assert( fd > -1 );
    if( fd < 0 ){
        DEBUG( fprintf(stderr, "Opening of payload file failed!\n") );
        return -1;
    }

    p_pay_ws->current_anchor_index = i_num;
    p_pay_ws->payload.fd  = fd;
    p_pay_ws->payload.seek = 0;
    p_pay_ws->payload.id = id;

    // Fill buffer
    char_buffer_t *p_buf_in = &p_pay_ws->buf_in;
    p_buf_in->used = 0;
#ifdef COMPRESS_BROTLI
    int finish_decompressing = 0;
    if( brotli_read_buffer(fd, &p_pay_ws->brotli_payload,
                p_buf_in,
                &finish_decompressing) ){
        fprintf(stderr, "%s Current byte range: [%i,%i)\n",
                "(open_payload_file) decompressing failed.",
                0, (int)p_buf_in->len);
        return -1;
    }
#else
    ssize_t n = read(fd, p_buf_in->p + 0, p_buf_in->len);
    if( n == -1 ){
        fprintf(stderr, "%s Current byte range: [%i,%i). Number read bytes: %zu\n",
                "(open_payload_file) file reading failed.",
                0, (int)p_buf_in->len, n);
        p_buf_in->used = 0;
        return -1;
    }
    if( (ssize_t) p_buf_in->len > n ){
        // Buffer is not complety filled. Probably last slice of file.
        p_buf_in->used = n;
    }else{
        p_buf_in->used = p_buf_in->len;
    }
#endif

    return 0;
}

int payload_do_search(
        payload_workspace_t *p_pay_ws,
        int i_num)
{
    if( i_num < 0 || i_num >= p_pay_ws->p_arguments->payload_anchors_len ){
        return -1;
    }

    if( p_pay_ws->current_anchor_index > -1 ){
        // check if same file
        uint32_t anchor_prev = p_pay_ws->p_arguments->payload_anchors[
            p_pay_ws->current_anchor_index];
        uint32_t anchor_now = p_pay_ws->p_arguments->payload_anchors[i_num];
        if( (anchor_prev >> 24) != (anchor_now >> 24) ){
            // Open other payload file
            if( open_payload_file(p_pay_ws, i_num) < 0){
                return -1;
            }
        }else{
            if( 0 < payload_anchor_compar(&anchor_prev, &anchor_now) ){
                fprintf(stderr, "Bad order of payload anchors.\n");
                //wrong order, re-opening required.
                if( open_payload_file(p_pay_ws, i_num) < 0){
                    return -1;
                }
            }
        }
    }else{
        // No file open
        if( open_payload_file(p_pay_ws, i_num) < 0){
            return -1;
        }
    }
   
    char_buffer_t *p_buf_in = &p_pay_ws->buf_in;
    char_buffer_t *p_buf_entry = &p_pay_ws->buf_entry;

    // Prepare buffer (if not already done).
    if( p_buf_in->p == NULL ){
        p_buf_in->len = OUT_CACHESIZ;
        p_buf_in->used = 0;
        p_buf_in->p = char_buffer_malloc(p_buf_in->len * sizeof(*p_buf_in->p));
    }
    assert( 0 < p_buf_in->len );
    assert( NULL != p_buf_in->p );

    uint32_t payload_anchor = p_pay_ws->p_arguments->payload_anchors[i_num];
    int fd = p_pay_ws->payload.fd;
    size_t anchor_seek = (payload_anchor & 0xFFFFFF);
    size_t *p_current_seek = &p_pay_ws->payload.seek;
    size_t next_seek = *p_current_seek + p_buf_in->used;
    int finish_decompressing = 0;
    p_buf_entry->used = 0; // begin new entry

    assert( p_pay_ws->payload.id == (payload_anchor >> 24) ); // correct file open?
    assert( anchor_seek >= *p_current_seek ); // forward ever, backward never

    // Read next chunk, if anchor not in current.
    while( anchor_seek >= next_seek ){

        // Refill buf_in
        p_buf_in->used = 0; 
        *p_current_seek = next_seek;

        // Fill buffer
#ifdef COMPRESS_BROTLI
        if( brotli_read_buffer(fd, &p_pay_ws->brotli_payload,
                    p_buf_in,
                    &finish_decompressing) ){
            fprintf(stderr, "%s Current byte range: [%i,%i)\n",
                    "(payload_do_search) decompressing failed.",
                    (int)*p_current_seek, (int)next_seek);
            //assert(0);
            //return -1;
        }
#else
        size_t bytes_to_read = p_buf_in->len;
        ssize_t n = read(fd, p_buf_in->p + 0, bytes_to_read);
        if( n == -1 ){
            fprintf(stderr, "%s Current byte range: [%i,%i). Number read bytes: %zu\n",
                    "(payload_do_search) file reading failed.",
                    (int)*p_current_seek, (int)next_seek, n);
            assert(0);
            p_buf_in->used = 0;
            finish_decompressing = 1;
            return -1;
        }
        if( (ssize_t) bytes_to_read > n ){
            // Buffer is not complety filled. Probably last slice of file.
            p_buf_in->used = n;
        }else{
            p_buf_in->used = bytes_to_read;
        }
#endif
        next_seek = *p_current_seek + p_buf_in->used;
        //if(finish_decompressing) break; 
    }

    assert( *p_current_seek <= anchor_seek );

    if( anchor_seek - *p_current_seek >= p_buf_in->used ){
        // While loop does not reached desired slice of data
        return -1;
    }

    /* Copy payload into entry buffer
     * We had no meta information how long the searched substring is. Search for
     * end of the current []-Block in the utf-8 block.
     * char *u8_memchr(char *s, uint32_t ch, size_t sz, int *charn);
     *
     * It could be nessessary to read an other slice of data
     */
    int offset = (anchor_seek - *p_current_seek);
    assert( offset >= 0 );
    assert( offset < p_buf_in->len );
    int block_pos;
    char *block_begin = p_buf_in->p + offset;
    char *block_end = u8_memchr(block_begin, BLOCK_END_CHAR, p_buf_in->len - offset, &block_pos);

    while( block_end == NULL && !finish_decompressing ){
        DEBUG( fprintf(stderr, "Can not find end of payload []-block.\n") );

        // Copy till end of buf_in.
        size_t required_len = p_buf_in->len - offset;
        if( required_len + p_buf_entry->used > p_buf_entry->len ){
            p_buf_entry->len += required_len * 2;
            p_buf_entry->p = (char *)realloc(p_buf_entry->p, p_buf_entry->len);
            if( p_buf_entry->p == NULL ){
                p_buf_entry->len = 0;
                return -1;
            }
        }
        memcpy(p_buf_entry->p + p_buf_entry->used,
                block_begin, required_len);
        p_buf_entry->used += required_len;

        // Read next slice
        p_buf_in->used = 0; 
        *p_current_seek = next_seek;


        // Fill buffer
#ifdef COMPRESS_BROTLI
        if( brotli_read_buffer(fd, &p_pay_ws->brotli_payload,
                    p_buf_in,
                    &finish_decompressing) ){
            fprintf(stderr, "%s Current byte range: [%i,%i)\n",
                    "(payload_do_search) decompressing failed.",
                    (int)*p_current_seek, (int)next_seek);
            assert(0);
            return -1;
        }
#else
        size_t bytes_to_read = (next_seek-*p_current_seek);
        ssize_t n = read(fd, p_buf_in->p + 0, bytes_to_read);
        if( n == -1 ){
            fprintf(stderr, "%s Current byte range: [%i,%i). Number read bytes: %zu\n",
                    "(payload_do_search) file reading failed.",
                    (int)*p_current_seek, (int)next_seek, n);
            p_buf_in->used = 0;
            finish_decompressing = 1;
            assert(0);
            return -1;
        }
        if( (ssize_t) bytes_to_read > n ){
            // Buffer is not complety filled. Probably last slice of file.
            p_buf_in->used = n;
            break;
        }else{
            p_buf_in->used = bytes_to_read;
        }
#endif
        next_seek = *p_current_seek + p_buf_in->used;

        if( p_buf_in->used == 0){
            // Nothing decoded?!
            break;
        }
        *p_current_seek += p_buf_in->used;


        offset = 0; // anchor_seek is in previous slice.
        block_begin = p_buf_in->p + offset;
        block_end = u8_memchr(block_begin, BLOCK_END_CHAR, p_buf_in->len - offset, &block_pos);

        if( block_end == NULL ){
            fprintf(stderr, "An []-Block over more than two slices indicates a problem." \
                    "with the selection of buffer sizes\n");
        }
    }

    if( block_end == NULL ){
        return -1;
    }

    assert(block_end > block_begin);
    assert( block_end - block_begin <= p_buf_in->used );

    // Copy till block_end
    size_t required_len = (block_end - block_begin + 1);
    if( required_len + p_buf_entry->used > p_buf_entry->len ){
        p_buf_entry->len += required_len * 2;
        p_buf_entry->p = (char *)realloc(p_buf_entry->p, p_buf_entry->len);
        if( p_buf_entry->p == NULL ){
            p_buf_entry->len = 0;
            return -1;
        }
    }
    memcpy(p_buf_entry->p + p_buf_entry->used,
            block_begin, required_len);
    p_buf_entry->used += required_len;
    p_buf_entry->p[p_buf_entry->used] = '\0';

    p_pay_ws->current_anchor_index = i_num;
    return 0;
}

/* If stub begins with [number]|, return new string
 * base_url[:number] + stub[stub.find("|")+1:]
 *
 * Otherwise strdup of stub.
 */
char *expand_url(
        size_t base_len, const char *base_url, 
        size_t stub_len, const char *stub_url)
{
    //size_t stub_len = strlen(stub_url);
    //size_t base_len = strlen(base_url);

    int charn;
    if( u8_memchr((char *)stub_url, '|', min(stub_len, 20), &charn) ){
        char *_end;
        long int parsed_number = strtoul(stub_url, &_end, 10);
        assert( *_end == '|' );
        if( parsed_number <= base_len ){
            size_t expanded_len = parsed_number + stub_len - (charn+1);
            char *expanded_url = char_buffer_malloc(expanded_len);
            memcpy(expanded_url, base_url, parsed_number);
            memcpy(expanded_url + parsed_number, stub_url+(charn+1), stub_len-(charn+1));
            return expanded_url;
        }
    }

    //return my_strdupa(stub_url); //bug?!
    char *expanded_url = char_buffer_malloc(stub_len);
    memcpy(expanded_url, stub_url, stub_len);
    return expanded_url;
}

void print_payload(
        payload_workspace_t *p_pay_ws)
{
    int i_num = p_pay_ws->current_anchor_index;
    char_buffer_t *p_buf_entry = &p_pay_ws->buf_entry;
    int fd = 1;
    uint32_t payload_anchor = 0;

    if( i_num < 0 || i_num >= p_pay_ws->p_arguments->payload_anchors_len   
            || p_buf_entry->len == 0
      )
    {
        dprintf(fd, "\"-1\": []");
        return;
    }
    payload_anchor = p_pay_ws->p_arguments->payload_anchors[i_num];
 
#define NUM_URLS 6
    size_t urls_len[NUM_URLS];
    const char *urls_str[NUM_URLS];
    char *urls_expanded[NUM_URLS];

    size_t buf_start, buf_stop, pair_start, pair_stop;
    int ret_search;
    search_pair_t **token = p_pay_ws->token_pattern;

    buf_start = 0;
    pair_start = 0;
    ret_search = parser_search(p_buf_entry, buf_start, &buf_stop,
            token, pair_start, &pair_stop);

    //if( pair_stop - pair_start < NUM_URLS )
    if( ret_search < 0 )
    {
        fprintf(stderr, "Not all %i url token found in payload.", NUM_URLS);
        dprintf(fd, "\"%u\": []",
                payload_anchor);
        search_pair_reset_array(token, token + pair_stop);
        return;
    }

    int i;
    for(i=0; i<NUM_URLS; ++i){
        urls_len[i] = search_pair_get_chars(
                p_buf_entry, *(token+i),
                &urls_str[i], NULL, 1);
    }

    urls_expanded[0] = NULL; // urls_str[0]; // do not free'ing;
    for(i=1; i<NUM_URLS; ++i){
        urls_expanded[i] = expand_url(
                urls_len[0], urls_str[0], urls_len[i], urls_str[i]);
    }

    dprintf(fd, "\"%u\": [\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",
            payload_anchor,
            urls_str[0], urls_expanded[1], urls_expanded[2],
            urls_expanded[3], urls_expanded[4], urls_expanded[5]);

    for(i=1; i<NUM_URLS; ++i){
        Free( (urls_expanded[i]) );
    }
    search_pair_reset_array(token, token + pair_stop);
}

search_pair_t **pattern_payload_file()
{
    const int NP = 6;
    int i; 

    search_pair_t **pairs = (search_pair_t **) malloc((NP+1) * sizeof(search_pair_t*));
    pairs[NP] = NULL;

    pairs[0] = search_pair_create();
    // "..."-Pair
    pairs[0]->begin.pattern.c = '"';
    pairs[0]->begin.mask = '\\';
    pairs[0]->begin.abort = '\n';
    pairs[0]->end.pattern.c = '"';
    pairs[0]->end.mask = '\\';
    pairs[0]->end.abort = '\n';
    pairs[0]->clip_begin = 1;
    pairs[0]->clip_end = 1;

    for( i=1; i<NP; ++i){
        pairs[i] = search_pair_create();
        memcpy(pairs[i], pairs[0], sizeof(search_pair_t));
    }

    return pairs;
}

int payload_anchor_compar(
        const void *p_a,
        const void *p_b)
{
    const uint32_t *a = (uint32_t *)p_a;
    const uint32_t *b = (uint32_t *)p_b;
    int i_file = (int)( ((*a)>>24) - ((*b)>>24) );
    if( i_file ) return i_file;

    return (int)((*a & 0xFFFFFF) - (*b & 0xFFFFFF));
}

