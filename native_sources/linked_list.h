#pragma once

/* Data struct to put data into one single array, but
 * arrange data in several subsets.
 *
 * Background: Putting the data into different arrays increases
 * the memory footprint and/or increases calls of realloc().
 *
 */

/* Overview
 *
 *Id*B 0*B                    1*B                      n*B
 *     | 1st index_node_t| | 2nd index_node_t| … |n.th index_node_t| …
 *     | data|g⁰|g¹|g²|g³| | data|g⁰|g¹|g²|g³| … | data|g⁰|g¹|g²|g³| …
 *            │  …     │   ▵      │   …          ▵
 *            │        │   │      │              │
 *            └────────┼───┘      └───────── …   │
 *                     └────────────────────── … ┘
 *                         ▵                     │
 * ┅┅┅┅┅┅┅┅┅┅┓             │                       ┏┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅
 * List of   ┋             │                     │ ┋  Pointer to latest
 * first     ┋             │                     │ ┋  element of subgroup
 * id of     ┋             │                     │ ┋  (During indexing the id 
 * subgroups ┋             │                     │ ┋  does not encode the position)
 *     s⁰₀ ────────────────┘                     └───▻   p⁰₀
 * g⁰  s⁰₁   ┋                                     ┋     p⁰₁
 *     …     ┋                                     ┋     …
 * …         ┋                                     ┋  …
 *     s³₀   ┋                                     ┋     p³₀
 * g³  s³₁   ┋                                     ┋     p³₁
 *     …     ┋                                     ┋     …
 * ┅┅┅┅┅┅┅┅┅┅┛                                     ┗┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅
 * Σ|Gⁱ| entries                                    Σ|Gⁱ| entries
 * linked_list_group_ids_t                          linked_list_group_anchors_t
 * 
 * g^i - Partition of nodes into subsets s^i_1,…,s^i_j  
 * B   - sizeof(index_node_t)
 * The nodes are placed into arrays A_b of length B*L. To find an entry
 * over it's id use A_[id / L][ id % L].
 * (In code:
 *  index_node_t * block = *(p_list->nodes_start + (pos / LINKED_LIST_ARR_LEN));
 *  index_node_t * target = block + (pos % LINKED_LIST_ARR_LEN);
 * )
 */

#include <limits.h>
#include <stdint.h>
#include "settings.h"
#include "parser.h"

/* Allocation size of internal blocks */
#define LINKED_LIST_MAX_LEN (0x100000) 
//#define LINKED_LIST_ARR_LEN (0x008000)
#define LINKED_LIST_ARR_LEN (0x004000)
#define LINKED_LIST_FIRST_ID 1

typedef struct {
  uint8_t payload_file_id; // payload file number
  uint32_t payload_seek;  // seek position in dummy file
  uint32_t title_seek;  // seek position in title file
} anchor_t;

typedef struct {
  uint8_t _unused; 
  uint32_t previous_id; 
  uint32_t index; // used like linked_list_subgroup_ids_t.anchor[index]
} dummy_info_t;

/* Note about 'subgroup named types':
 * The set of elements partitioned between four different relations
 * (i.e. the channel).
 *
 * The relations split the set into subsets (called subgroups) and we want
 * iterate over the elements of this subgroups.
 * Thus we need a link to the next element for each relation.
 *
 * On the other hand we need a complete list of anchors for all subgroups 
 * of all relations. This types contain the substring 'group'. 
 *
 * Moreover, to avoid repetition of code for each relation,
 * the structs contain some unions. They allow us the iteration over
 * the group of relations with one variable name.
 *
 * Note: The 32 byte were used to packed an other variable into:
 *       The lowest byte encodes the subgroup id.
 *       Upper bytes encode the id.
 *       
 */
#define LINKED_LIST_SUBGROUP_BITS 8

/* Extract bits from subgroup */
#define LINKED_LIST_ID(ID_SUBGROUP) ( (ID_SUBGROUP) >> LINKED_LIST_SUBGROUP_BITS )

/* Extract bits from subgroup */
#define LINKED_LIST_SUBGROUP(ID_SUBGROUP) ( (ID_SUBGROUP) & 0xFF )

typedef struct {
    union {
        struct {
            uint32_t relative_date;
            uint32_t timestamp;
            uint32_t duration;
            uint32_t channel;
        };
        uint32_t subgroup_ids[4]; //access on above stuff per index.
    };
} linked_list_subgroup_ids_t;

/* Index numbers for linked_list_group_anchors_t
 *
 * Note: i_ prefix indicate members as index variables, i.e
 * 0 <= i_timestamp < NUM_TIME
 * */
typedef struct {
    union {
        struct {
            int32_t i_relative_date;
            int32_t i_timestamp;
            int32_t i_duration;
            int32_t i_channel;
        };
        int32_t indizes[4];
    };
} linked_list_subgroup_indizes_t;

typedef struct index_node_s {
    /* Pointer to next element for each group.
     *
     * (incl. encoding of subgroup id)
     * */
    linked_list_subgroup_ids_t nexts;
    uint32_t id;
    union {
        anchor_t link; 
        dummy_info_t dummy_data;
    };
} index_node_t;

typedef struct {
    union {
        struct {
            index_node_t * relative_dates[NUM_REALTIVE_DATE];
            index_node_t * timestamps[NUM_TIME];
            index_node_t * durations[NUM_DURATIONS];
            index_node_t * channels[NUM_CHANNELS];
        };
        // For easier looping
        index_node_t *nodes[NUM_SUM];
    };
} linked_list_group_anchors_t;

typedef struct {
    union {
        struct {
    uint32_t relative_dates[NUM_REALTIVE_DATE];
    uint32_t timestamps[NUM_TIME];
    uint32_t durations[NUM_DURATIONS];
    uint32_t channels[NUM_CHANNELS];
        };
        // For easier looping
        uint32_t ids[NUM_SUM];
    };
} linked_list_group_ids_t;

typedef struct {
    /* Split duration into ranges 
     * [0, duration_borders[0]],
     * [duration_borders[0] + 1, duration_borders[1]],
     * ...
     * [duration_borders[N] + 1, INT_MAX ]
     *
     * (in seconds)
     * */
    time_t date_anchor; // time stamp of day 0 (newest), day 1 is previous day and so on...
    int32_t timestamp_borders[NUM_TIME];
    int32_t duration_borders[NUM_DURATIONS];
} index_metadata_t;

typedef struct {
    // Length of array nodes (Not all entries initialized.)
    size_t len_nodes;

    /* Sum of sizes of all allocated arrays in nodes_start[0, ..., len_nodes-1] 
     */
    size_t mem_allocated;
    /* Links to N arrays of size LINKED_LIST_ARR_LEN
    */
    index_node_t* *nodes_start;
    index_node_t* *nodes_current; // [nodes_start, ..., nodes_start+len_nodes-1]

    size_t len_used_dummy_nodes;
    index_node_t *dummy_nodes;    // Place for some nodes which will not be serialized with the rest.

    /* Pointer to next free entry for data.
     * Store your next element here and call then
     * push_item(...)
     * */
    index_node_t *next_unused;
    /* Used to enumerate the index_node_t entries.
     * Starts 1 because 0 marks unset values. Moreover,
     * 'id-1' is the distance to the begin in the serialized data.
     */
    uint32_t next_unused_id; // starts with 1. 0 = uset values EDIT: Now, starts with 0...

    /* Hold pointer to all previous elements of subsets.
     * Required to update index_node_t->nexts anchors.
     * */
    linked_list_group_anchors_t p_latests;

    /* Links to first entry of each subgroup.
     */
    linked_list_group_ids_t first_ids;

    index_metadata_t meta;
} linked_list_t;

linked_list_t linked_list_create(
        time_t date_anchor)
;

void linked_list_destroy(
        linked_list_t *p_list)
;

/*  */
void linked_list_push_item(
        linked_list_t *p_list,
        linked_list_subgroup_indizes_t *p_indizes,
        index_node_t *p_data)
;

/* Flush data array up to *(p_list->nodes_current - 1).
 * To omit broken .next-pointer the function lift up all
 * pending links to p_list->nodes_current (and above).
 *
 * The flushed arrays will be deleted to hold memory footprint
 * low.
 */
void linked_list_write_partial(
        int fd,
        linked_list_t *p_list,
        char finish)
;


void linked_list_read(
        int fd,
        linked_list_t *p_list)
;

/* Translate id into node. */
index_node_t *linked_list_get_node(
        linked_list_t *p_list,
        uint32_t id)
;

/* Inversion of linked_list_get_node(...) */
uint32_t linked_list_get_id(
        linked_list_t *p_list,
        index_node_t *p_node)
;

/* Follow .next[subgroup_index] of element
 */
uint32_t linked_list_next_in_subgroup_by_id(
        linked_list_t *p_list,
        uint32_t id,
        int subgroup_index)
;
