
#include <string.h>
#include "settings.h"
#include "helper.h"
#include "linked_list.h"

#define USE_CALLOC 1

/* To read of serial data in place we need a few more bytes
 * after the content ends.
 */
//#define LINKED_LIST_EXTRA_BYTES (sizeof(int32_t))


linked_list_t linked_list_create(
        time_t date_anchor)
{
    linked_list_t list;
    memset(&list, 0, sizeof(linked_list_t)); // many NULL pointers...

    list.len_nodes = (LINKED_LIST_MAX_LEN/LINKED_LIST_ARR_LEN) + 1;
    // Initialize as NULL pointers required
    list.nodes_start = (index_node_t **) calloc(sizeof(index_node_t *), list.len_nodes);

    list.nodes_current = list.nodes_start;
    list.next_unused_id = LINKED_LIST_FIRST_ID; //1;
    list.mem_allocated = 0;

    DEBUG( fprintf(stderr, "Allocate %i nodes at node_array %i.\n",
                LINKED_LIST_ARR_LEN,
                (int)(list.nodes_current - list.nodes_start) );
         );

#if USE_CALLOC > 0
    *list.nodes_current = (index_node_t *) calloc(
            sizeof(index_node_t), LINKED_LIST_ARR_LEN);
#else
    *list.nodes_current = (index_node_t *) malloc(
            sizeof(index_node_t) * LINKED_LIST_ARR_LEN);
#endif

    if( *list.nodes_current ) list.mem_allocated +=
        LINKED_LIST_ARR_LEN * sizeof(index_node_t);


    list.len_used_dummy_nodes = 0;
    const size_t ld = (NUM_SUM);

    // Allocation as 0/NULL required!
    list.dummy_nodes = (index_node_t *) calloc(1, sizeof(index_node_t) * ld);
    size_t id;
    for(id=0; id<ld; ++id){
        list.dummy_nodes[id].id = UINT_MAX;

        list.first_ids.ids[id] = 0;
        list.p_latests.nodes[id] = NULL;
    }

    // Map to begin of first array
    list.next_unused = *list.nodes_current;

    list.meta.date_anchor = date_anchor;
    extern const int32_t Time_Array[];
    extern const int32_t Duration_Array[];
    memcpy( list.meta.timestamp_borders, Time_Array, sizeof(list.meta.timestamp_borders) );
    memcpy( list.meta.duration_borders, Duration_Array, sizeof(list.meta.duration_borders) );

    return list;
}

void linked_list_destroy(
        linked_list_t *p_list)
{
    int i;
    for( i=0; i<p_list->len_nodes; i++){
        if( p_list->nodes_start[i] != NULL ){
            free(p_list->nodes_start[i]);
        }
    }
    p_list->mem_allocated = 0;
    free( p_list->nodes_start);
    free( p_list->dummy_nodes);
}


/*  */
void linked_list_push_item(
        linked_list_t *p_list,
        linked_list_subgroup_indizes_t *p_indizes,
        index_node_t *p_data)
{
    // Add internal id.
    //p_data->id = p_list->next_unused_id;
    const int32_t id = p_list->next_unused_id;
    p_list->next_unused_id++;

    p_data->id = id;
    assert( LINKED_LIST_ID(p_data->nexts.subgroup_ids[0]) != p_data->id );
    assert( LINKED_LIST_ID(p_data->nexts.subgroup_ids[1]) != p_data->id );
    assert( LINKED_LIST_ID(p_data->nexts.subgroup_ids[2]) != p_data->id );
    assert( LINKED_LIST_ID(p_data->nexts.subgroup_ids[3]) != p_data->id );

    index_node_t *tmp_prev;

    // Shift id on higher bits to left space for subgroup bits.
    const int32_t id_with_shift = id << LINKED_LIST_SUBGROUP_BITS;
#if 0
    const int N[NUM_RELATIONS + 1] = { 0,
        NUM_REALTIVE_DATE,
        NUM_REALTIVE_DATE + NUM_TIME,
        NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS,
        NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS + NUM_CHANNELS,
        };
    int i;
    for( i=0; i<NUM_RELATIONS; ++i){
        tmp_prev = p_list->p_latests.nodes[N[i] + p_indizes->indizes[i]];
        if( tmp_prev ){
            tmp_prev->nexts.subgroup_ids[i] = id_with_shift | p_indizes->indizes[i];
        }else{
            p_list->first_ids.ids[N[i] + p_indizes->indizes[i]] =
                p_data->id; // without shift
        }
        // Required for latest node of chain. Will be overwritten for all others.
        p_data->nexts.subgroup_ids[i] = 0 | p_indizes->indizes[i];

        p_list->p_latests.nodes[N[i] + p_indizes->indizes[i]] = p_data;
    }
#else
    /* Connect element with previous of all four subsets.
     * If subset had no elements, set anchor in p_list->first_ids.
     */
    tmp_prev = p_list->p_latests.relative_dates[p_indizes->i_relative_date];
    if( tmp_prev ){
        tmp_prev->nexts.relative_date = id_with_shift | p_indizes->i_relative_date;
    }else{
        p_list->first_ids.relative_dates[p_indizes->i_relative_date] =
            p_data->id; // without shift
    }

    // Required for latest node of chain. Will be overwritten for all others.
    p_data->nexts.relative_date = 0 | p_indizes->i_relative_date;

    p_list->p_latests.relative_dates[p_indizes->i_relative_date] = p_data;


    tmp_prev = p_list->p_latests.timestamps[p_indizes->i_timestamp];
    if( tmp_prev ){
        tmp_prev->nexts.timestamp = id_with_shift | p_indizes->i_timestamp;
    }else{
        p_list->first_ids.timestamps[p_indizes->i_timestamp] =
            p_data->id; // without shift
    }
    // Required for latest node of chain. Will be overwritten for all others.
    p_data->nexts.timestamp = 0 | p_indizes->i_timestamp;

    p_list->p_latests.timestamps[p_indizes->i_timestamp] = p_data;


    tmp_prev = p_list->p_latests.durations[p_indizes->i_duration];
    if( tmp_prev ){
        tmp_prev->nexts.duration = id_with_shift | p_indizes->i_duration;
    }else{
        p_list->first_ids.durations[p_indizes->i_duration] =
            p_data->id; // without shift
    }
    // Required for latest node of chain. Will be overwritten for all others.
    p_data->nexts.duration = 0 | p_indizes->i_duration;

    p_list->p_latests.durations[p_indizes->i_duration] = p_data;


    tmp_prev = p_list->p_latests.channels[p_indizes->i_channel];
    if( tmp_prev ){
        tmp_prev->nexts.channel = id_with_shift | p_indizes->i_channel;
    }else{
        p_list->first_ids.channels[p_indizes->i_channel] =
            p_data->id; // without shift
    }
    // Required for latest node of chain. Will be overwritten for all others.
    p_data->nexts.channel = 0 | p_indizes->i_channel;

    p_list->p_latests.channels[p_indizes->i_channel] = p_data;
#endif

    // Prepare next step
    if( p_data == p_list->next_unused) {
        // Position consumed. Find/Create new free position
        p_list->next_unused++;

        if( p_list->next_unused - *p_list->nodes_current >= LINKED_LIST_ARR_LEN ){
            p_list->nodes_current++;
            assert( p_list->nodes_current - p_list->nodes_start < p_list->len_nodes );

            if( *p_list->nodes_current == NULL ){
                DEBUG( fprintf(stderr, "Allocate %i nodes at node_array %i.\n",
                            LINKED_LIST_ARR_LEN,
                            (int)(p_list->nodes_current - p_list->nodes_start) );
                     );
#if USE_CALLOC > 0
                *p_list->nodes_current = (index_node_t *) calloc(
                        sizeof(index_node_t), LINKED_LIST_ARR_LEN);
#else
                *p_list->nodes_current = (index_node_t *) malloc(
                        sizeof(index_node_t) * LINKED_LIST_ARR_LEN);
#endif
                if( *p_list->nodes_current ) p_list->mem_allocated +=
                    LINKED_LIST_ARR_LEN * sizeof(index_node_t);
            }

            // Map to begin of next array
            p_list->next_unused = *p_list->nodes_current;
        }
    }
}

void linked_list_write_partial(
        int fd,
        linked_list_t *p_list,
        char finish)
{
    if( !finish && ( p_list->nodes_current == p_list->nodes_start ||
            (*(p_list->nodes_current-1)) == NULL) )
    {
        // Nothing to do.
        return;
    }

    if( p_list->len_used_dummy_nodes ){
        /* Flush dummy nodes of previous call of linked_list_write_partial(...)
         */
        int32_t num_to_write = p_list->len_used_dummy_nodes;
        ssize_t w = write(fd, &num_to_write, sizeof(int32_t));

        size_t bytes_to_write = num_to_write * sizeof(index_node_t);
        w = write(fd, p_list->dummy_nodes, bytes_to_write);
        if( (ssize_t)bytes_to_write > w )
        {
            fprintf(stderr, "(linked_list_write_partial) Write of %zi bytes failed.\n", bytes_to_write);
        }
#if 1
        // Search bug... Reset used next-id of dummy note to put it in initial state
        // should not be necessary.
        // TODO: Remove this or change comment.
        int i;
        for( i=0; i<p_list->len_used_dummy_nodes; ++i){
             index_node_t * tmp_dummy = &p_list->dummy_nodes[i];
             tmp_dummy->nexts.subgroup_ids[tmp_dummy->dummy_data.index] = 0;
        }

#endif
        p_list->len_used_dummy_nodes = 0;
    }


    if( !finish ){
        // Lift pending next-Pointers into unwritten stage
        uint32_t biggest_id_to_write = (*(p_list->nodes_current -1) + LINKED_LIST_ARR_LEN - 1)->id;

        index_node_t *tmp_node;
        index_node_t *tmp_dummy;

#if 0
        int i_group, i_all;
        const int N[NUM_RELATIONS + 1] = { 0,
            NUM_REALTIVE_DATE,
            NUM_REALTIVE_DATE + NUM_TIME,
            NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS,
            NUM_REALTIVE_DATE + NUM_TIME + NUM_DURATIONS + NUM_CHANNELS,
        };
        i_all = 0;
        for( i_group=0; i_group<4; ++i_group){
            for( ; i_all<N[i_group+1]; ++i_all){
                tmp_node = p_list->p_latests.nodes[i_all]; // latest element of subset
                if( tmp_node && tmp_node->id <= biggest_id_to_write )
                {
                    // Latest node had no successor.
                    assert( (LINKED_LIST_ID(
                            tmp_node->nexts.subgroup_ids[i_group])
                          ) >> (LINKED_LIST_SUBGROUP_BITS) == 0 );

                    // Overwrite target of prev-pointer with dummy node
                    tmp_dummy = p_list->dummy_nodes + p_list->len_used_dummy_nodes;

                    /* Set data to allow reconstruction of correct next element
                     * on reading of index file
                     *
                     * previous_node->id.nexts.subgroup_ids[index] := dummy_node.nexts.subgroup_ids[index]
                     * (with index = dummy_data.index)
                     * . */
                    int i_subgroup = i_all - N[i_group];
                    tmp_dummy->dummy_data.previous_id =
                        (LINKED_LIST_ID(tmp_node->id << LINKED_LIST_SUBGROUP_BITS) | i_subgroup;
                    tmp_dummy->dummy_data.index = i_group;

                    p_list->p_latests.nodes[i_all] = tmp_dummy;
                    p_list->len_used_dummy_nodes++;
                }
            }
        }

#else
        int i;
        for( i=0; i<NUM_REALTIVE_DATE; i++){
            tmp_node = p_list->p_latests.relative_dates[i]; // latest element of subset
            if( tmp_node && tmp_node->id <= biggest_id_to_write )
            {
                assert( LINKED_LIST_ID(tmp_node->nexts.relative_date) == 0); // No successor.

                // Overwrite target of prev-pointer on dummy node
                tmp_dummy = p_list->dummy_nodes + p_list->len_used_dummy_nodes;
                tmp_dummy->dummy_data.previous_id = tmp_node->id;
                tmp_dummy->dummy_data.index = 0;

                p_list->p_latests.relative_dates[i] = tmp_dummy;
                p_list->len_used_dummy_nodes++;
            }
        }
        for( i=0; i<NUM_TIME; i++){
            tmp_node = p_list->p_latests.timestamps[i]; // latest element of subset
            if( tmp_node && tmp_node->id <= biggest_id_to_write )
            {
                assert( LINKED_LIST_ID(tmp_node->nexts.timestamp) >> (LINKED_LIST_SUBGROUP_BITS) == 0); // No successor.

                // Overwrite target of prev-pointer on dummy node
                tmp_dummy = p_list->dummy_nodes + p_list->len_used_dummy_nodes;
                tmp_dummy->dummy_data.previous_id = tmp_node->id;
                tmp_dummy->dummy_data.index = 1;

                p_list->p_latests.timestamps[i] = tmp_dummy;
                p_list->len_used_dummy_nodes++;
            }
        }
        for( i=0; i<NUM_DURATIONS; i++){
            tmp_node = p_list->p_latests.durations[i]; // latest element of subset
            if( tmp_node && tmp_node->id <= biggest_id_to_write )
            {
                assert( LINKED_LIST_ID(tmp_node->nexts.duration) >> (LINKED_LIST_SUBGROUP_BITS) == 0); // No successor.

                // Overwrite target of prev-pointer on dummy node
                tmp_dummy = p_list->dummy_nodes + p_list->len_used_dummy_nodes;
                tmp_dummy->dummy_data.previous_id = tmp_node->id;
                tmp_dummy->dummy_data.index = 2;

                p_list->p_latests.durations[i] = tmp_dummy;
                p_list->len_used_dummy_nodes++;
            }
        }
        for( i=0; i<NUM_CHANNELS; i++){
            tmp_node = p_list->p_latests.channels[i]; // latest element of subset
            if( tmp_node && tmp_node->id <= biggest_id_to_write )
            {
                assert( LINKED_LIST_ID(tmp_node->nexts.channel) >> (LINKED_LIST_SUBGROUP_BITS) == 0); // No successor.

                // Overwrite target of prev-pointer on dummy node
                tmp_dummy = p_list->dummy_nodes + p_list->len_used_dummy_nodes;
                tmp_dummy->dummy_data.previous_id = tmp_node->id;
                tmp_dummy->dummy_data.index = 3;

                p_list->p_latests.channels[i] = tmp_dummy;
                p_list->len_used_dummy_nodes++;
            }
        }
#endif

    }

    //Write data
    index_node_t* *pp_i;
    // Skip NULL array at begin
    for( pp_i=p_list->nodes_start; pp_i<p_list->nodes_current; ++pp_i){
        if( *pp_i ) break;
    }

    int n_blocks = (p_list->nodes_current - pp_i); // without last
    // Number of following items, but not the last one.
    int32_t num_to_write = n_blocks * LINKED_LIST_ARR_LEN;
    if( finish ){
        // Add number of used elements of last block (never completely filled.)
        num_to_write += (p_list->next_unused_id - LINKED_LIST_FIRST_ID) % LINKED_LIST_ARR_LEN;
    }
    ssize_t w = write(fd, &num_to_write, sizeof(int32_t));

    for( ; pp_i<p_list->nodes_current; ++pp_i){
        assert(pp_i != p_list->nodes_current);
        assert(*pp_i != NULL);

        size_t bytes_to_write = LINKED_LIST_ARR_LEN * sizeof(index_node_t);
        w = write(fd, *pp_i, bytes_to_write);
        if( (ssize_t)bytes_to_write > w )
        {
            fprintf(stderr, "(linked_list_write_partial) Write of %zi bytes failed.\n", bytes_to_write);
        }

#ifndef NDEBUG
        // No pending links into this array?
        if( !finish ){
            index_node_t *a, *b, *c;
            a = *pp_i;
            c = a + LINKED_LIST_ARR_LEN - 1;
            int j;
            for(j=0; j<(NUM_SUM); ++j){
                b = p_list->p_latests.nodes[j];
                assert( !( a<=b && b<=c) );
            }
        }
#endif

        //Free memory of written array.
        free(*pp_i);
        *pp_i = NULL;
        p_list->mem_allocated -= LINKED_LIST_ARR_LEN * sizeof(index_node_t);
    }

    if( finish ){
        // Write last block. Pretending of length number is already done above
        index_node_t *p_i = *p_list->nodes_current;
        assert( p_i != NULL );
        int32_t num_to_write = (p_list->next_unused_id - LINKED_LIST_FIRST_ID) % LINKED_LIST_ARR_LEN;
        if( num_to_write ){
            size_t bytes_to_write = num_to_write * sizeof(index_node_t);
            w = write(fd, p_i, bytes_to_write);
            if( (ssize_t)bytes_to_write > w )
            {
                fprintf(stderr, "(linked_list_write_partial) Write of %zi bytes failed.\n", bytes_to_write);
            }
        }

        //Free memory of written array.
        free(p_i);
        *p_list->nodes_current = NULL;
        p_list->mem_allocated -= LINKED_LIST_ARR_LEN * sizeof(index_node_t);
    }


    if( finish ){
        //Finally, write start anchors. Pretend negative value to distinct it from the dummy node
        //length's and mark last chunk of data.
        int32_t num_to_write = -(NUM_SUM);
        ssize_t w = write(fd, &num_to_write, sizeof(int32_t));

        size_t bytes_to_write = -num_to_write * sizeof(uint32_t);
        w = write(fd, p_list->first_ids.ids, bytes_to_write);
        if( (ssize_t)bytes_to_write > w )
        {
            fprintf(stderr, "(linked_list_write_partial) Write of %zi bytes failed.\n", bytes_to_write);
        }
    }

}

void linked_list_handle_read_dummies(
        linked_list_t *p_list)
{
    /* use .previous_id and .index to update previously read items. */
    int i;
    for( i=0; i<p_list->len_used_dummy_nodes; ++i){
        index_node_t * tmp_dummy = &p_list->dummy_nodes[i];
        int id = tmp_dummy->dummy_data.previous_id;

        // Use id to evaluate the position of the previous node.
#if 0
        int pos = id - LINKED_LIST_FIRST_ID;
        assert( pos >= 0 );
        index_node_t * block = *(p_list->nodes_start + (pos / LINKED_LIST_ARR_LEN));
        assert( block != NULL );
        index_node_t * target = block + (pos % LINKED_LIST_ARR_LEN);
#else
        index_node_t * target = linked_list_get_node(p_list, id);
#endif

        target->nexts.subgroup_ids[tmp_dummy->dummy_data.index] =
            tmp_dummy->nexts.subgroup_ids[tmp_dummy->dummy_data.index];
    }

    p_list->len_used_dummy_nodes = 0;
}

void linked_list_read(
        int fd,
        linked_list_t *p_list)
{
    /* Fills the p_list arrays.
     *
     * Data had following structure:
     *  [[ m*N {N Items} {N Items } … { N Items } k { k Dummies }] …]
     *  M*N+n {N Items} {N Items} … {N Items} {n Items} -K {K first_ids }
     *
     * 0 <  n <= N := LINKED_LIST_ARR_LEN
     * 0 <= k <= K := NUM_REALTIVE_DATE + ... + NUM_CHANNELS
     * Items - index_node_t-Structs
     * Dummies - index_node_t-Structs
     * first_ids - array of uint32_t, see linked_list_subgroup_indizes_t
     *
     * Note: The sign of k/K decide if dummy array or first_ids data
     * follow.
     *
     * Note: p_latests array will not be filled (not required).
     */

    p_list->nodes_current = p_list->nodes_start;
    p_list->next_unused_id = LINKED_LIST_FIRST_ID;
    p_list->len_used_dummy_nodes = 0;

    int num; // m*N + n or k or -K
    ssize_t bytes_read;

    // read m*N + n
    bytes_read = read(fd, &num, sizeof(int32_t));
    assert( bytes_read == sizeof(int32_t));

    while( num > 0 ){
        if( *p_list->nodes_current == NULL ){
#if USE_CALLOC > 0
            *p_list->nodes_current = (index_node_t *) calloc(
                    sizeof(index_node_t), LINKED_LIST_ARR_LEN);
#else
            *p_list->nodes_current = (index_node_t *) malloc(
                    sizeof(index_node_t) * LINKED_LIST_ARR_LEN);
#endif
            if( *p_list->nodes_current ) p_list->mem_allocated +=
                LINKED_LIST_ARR_LEN * sizeof(index_node_t);
        }

        if( num > LINKED_LIST_ARR_LEN ){
            bytes_read = read(fd, *p_list->nodes_current,
                    LINKED_LIST_ARR_LEN * sizeof(index_node_t));
            ssize_t x = LINKED_LIST_ARR_LEN * sizeof(index_node_t);
            assert( bytes_read == x );
            _unused(x);

            p_list->nodes_current++;
            p_list->next_unused_id += LINKED_LIST_ARR_LEN;
            num -= LINKED_LIST_ARR_LEN;
            assert( num > 0);
            //DEBUG( fprintf(stderr, "Linked list size: %i\n", (int) p_list->next_unused_id-1) );
            continue;
        }

        // Latest repetition of loop. Read 0<n<=N items.
        if( num > 0 ){
            bytes_read = read(fd, *p_list->nodes_current,
                    num * sizeof(index_node_t));
            assert( bytes_read == num * sizeof(index_node_t) );
            if( num == LINKED_LIST_ARR_LEN ){
                // Array completely filled.
                p_list->nodes_current++;
            }
            p_list->next_unused_id += num;
            //DEBUG( fprintf(stderr, "Linked list size: %i\n", (int) p_list->next_unused_id-1) );
        }

        // Read k or -K
        bytes_read = read(fd, &num, sizeof(int32_t));
        assert( bytes_read == sizeof(int32_t));
        if( num < 0 ){
            assert( num == -NUM_SUM );
            bytes_read = read(fd, p_list->first_ids.ids,
                    -num * sizeof(int32_t));
            assert( bytes_read == NUM_SUM * sizeof(int32_t) );

            break;
        }else{
            //DEBUG( fprintf(stderr, "Dummy nodes to read: %i\n", num); );
            //Read dummies
            bytes_read = read(fd, p_list->dummy_nodes,
                    num * sizeof(index_node_t));
            assert( bytes_read == num * sizeof(index_node_t) );
            p_list->len_used_dummy_nodes = num;
            linked_list_handle_read_dummies(p_list);
        }

        // Read m*N + n
        bytes_read = read(fd, &num, sizeof(int32_t));
        assert( bytes_read == sizeof(int32_t));
    }
    _unused(bytes_read); // used for asserts only
}


index_node_t *linked_list_get_node(
        linked_list_t *p_list,
        uint32_t id)
{
        assert( id >= LINKED_LIST_FIRST_ID );
        uint32_t pos = id - LINKED_LIST_FIRST_ID;
        index_node_t * block = *(p_list->nodes_start + (pos / LINKED_LIST_ARR_LEN));
        assert( block != NULL );
        index_node_t * target = block + (pos % LINKED_LIST_ARR_LEN);
        return target;
}

uint32_t linked_list_get_id(
        linked_list_t *p_list,
        index_node_t *p_node)
{
    return p_node->id;
}

uint32_t linked_list_next_in_subgroup_by_id(
        linked_list_t *p_list,
        uint32_t id,
        int subgroup_index)
{
    index_node_t * target = linked_list_get_node(p_list, id);
    uint32_t id_isub = (target->nexts.subgroup_ids[subgroup_index]);
    return (LINKED_LIST_ID(id_isub));
}

