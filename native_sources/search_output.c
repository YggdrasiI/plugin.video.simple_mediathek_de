#include "helper.h"
#include "parser.h"
#include "filmliste.h"
#include "search.h"

#define INC_MODULO(A, B) A = ((++(A) == B)?0:(A));
#define DEC_MODULO(A, B) A = ((A == 0)?(B-1):(--A));

output_t output_init(
        uint32_t N,
        uint32_t Nskip,
        int reversed,
        int comma_offset,
        void (*sort_handler))
{
    output_t o;
    o.N = N;
    o.Nskip = Nskip;
    o.M = o.N + o.Nskip;
    o.found = 0;
    o.pos_i = o.M-1;
    o.pos_p = o.M-1;
    o.first_comma_offset = comma_offset;
    o.reversed_flag = reversed;
    o.overlap_flag = 0;
    o.sort_handler = sort_handler;
    o.search_whole_index_flag = o.reversed_flag|(o.sort_handler != NULL);

    o.p_ids = calloc(o.M, sizeof(*o.p_ids)); // (uint32_t *)
    o.p_to_print = calloc(o.M, sizeof(*o.p_to_print));
    if( o.p_ids == NULL || o.p_to_print == NULL || o.M == 0 ){
        Free(o.p_ids);
        Free(o.p_to_print);
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
    Free(p_output->p_ids);
    uint32_t i;
    for( i=0; i<p_output->M; ++i){
        Free(p_output->p_to_print[i].p);
    }
    Free(p_output->p_to_print);
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
        output_t *p_output,
        uint32_t id)
{
  int i = p_output->pos_i;
  //i = (i+1) % (p_output->M);
  INC_MODULO(i, p_output->M);
  p_output->p_ids[i] = id;
  p_output->pos_i = i;
  p_output->found++;
  if( p_output->pos_p == p_output->pos_i){
      // Push pos_p one position further. We can not lap it.
      //p_output->pos_p = (p_outp->pos_p+1) % (p_output->M);
      p_output->overlap_flag = 1;
      //INC_MODULO(p_output->pos_p, p_output->M);
  }
}

void output_flush(
        search_workspace_t *p_s_ws,
        output_t *p_output)
{
    const uint32_t i = p_output->pos_i;
    uint32_t p = p_output->pos_p;
    const uint32_t M = p_output->M;

    if( p_output->overlap_flag == 1){
        p = i;
    }else if(i == p){
        // No new entry available to flush
        return;
    }
    INC_MODULO(p, M);

    if( !p_output->search_whole_index_flag ){
        // Go fast-forward to last N entries because the other ones
        // would not be needed in any case.
        uint32_t d = dist(p, i, M);
        if( d > p_output->N){
            p = (i - p_output->N + M) % M;
        }
    }

    if( p > i ){
        // Handle [pos_p+1, M-1]. p will be 0 after this step
        while( p > 0 ){
            uint32_t id = p_output->p_ids[p];
            output_fill(p_s_ws, p_output->p_to_print+p, id);
            //p = (p+1) % M;
            INC_MODULO(p, M);
        }
    }
    while( p < i ){
        // Handle [0, pos_i-1]
        uint32_t id = p_output->p_ids[p];
        output_fill(p_s_ws, p_output->p_to_print+p, id);
        p = (p+1);
    }
    assert( p == i );

    // Handle pos_i
    uint32_t id = p_output->p_ids[p];
    output_fill(p_s_ws, p_output->p_to_print+p, id);
    p_output->pos_p = p;
    p_output->overlap_flag = 0;
}

void output_fill(
        search_workspace_t *p_s_ws,
        output_str_t *p_out,
        uint32_t id)
{
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

    searchable_strings_prelude_t *p_entry;
    size_t iChunk = title_entry(p_s_ws, id, &p_entry);

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

    if( 0 > get_topic(p_s_ws, iChunk, p_entry, &topic) ){
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
    const int R = p_output->reversed_flag;
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
                        || offset_left > p_output->p_to_print[p].used ))
            {
                offset_left = 0;
            }

            while( f > 0 ){
                const output_str_t * p_out = &p_output->p_to_print[p];

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
                        || offset_left > p_output->p_to_print[p].used ))
            {
                offset_left = 0;
            }

            while( f > 0 ){
                const output_str_t * p_out = &p_output->p_to_print[p];

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
