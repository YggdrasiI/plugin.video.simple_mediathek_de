#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <unistd.h>
#if 0
//for open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include <time.h>

#include "settings.h"

/*
 * Es werden Paare von Marken definiert, die gefunden werden müssen.
 * Falls getroffen, so muss noch das Zeichen davor verglichen werden.
 * Bei einem Treffer kann außerdem ein Offset vorgegeben werden, das die Suche
 * verkürzt (für die Stellen, deren Mindestlänge bekannt ist.
 * Meiste Zeit nimmt aber der "Rattenschwanz" bis zum \n ein (?!)
 * Wenn alle Paare abgearbeitet sind müssen sie kopiert werden.
 * Wird zwischendurch das Ende des Buffers erreicht, müssen die Paare
 * zwischengespeichert werden. (Ein Swapbuffer kann zu klein sein...)
 * Falls nur die Linke Seite eines Paares getroffen wurde muss
 * diese "halb" kopiert werden und die Stelle für den Rest gespeichert werden.
 *
 * Falls zu wenig Platz ist muss neu allokiert werden.
 */

/*
 * TODO: Konstanten definieren für die Statuswerte (-1, -2,...)
 */
#define SEARCH_MATCH_ELEM             0
#define SEARCH_MATCH_PAIR             0
#define SEARCH_MATCH_PAIRS            0
#define SEARCH_FAILED_DUE_ABORT_CHAR -1
#define SEARCH_FAILED_END_OF_BUFFER  -2


typedef struct {
    char c; // Character searched for.

    /*
     * Zero value could lead to inf. loop.
     */
    uint8_t jump;
} pattern_t;

typedef struct {
    pattern_t pattern; 

    /* Ignore next char if this one match, i.e. '\'.
     * Zero value disable mask.
     */
    char mask;  // pattern will be ignored after this char.

    /* List of chars which leads to bigger jump rightwards.
     * Each element will be tested before 'pattern'.
     *
     * NULL value disable jumps.
     */
    pattern_t *jumps;

    /* Abort search if found, i.e to abort search on newlines. 
     * Default value : '\n'
     * Zero disable option.
     *
     * Note: 'abort' = 'pattern.c' disables, too.
     */
    char abort;

} search_elem_t;

typedef struct {
    search_elem_t begin; // Start pattern
    search_elem_t end;   // End pattern

    /* Points into main block of text.
     * If new block will be read, the pointer will be set
     * a) To NULL, if 'hit_end' was already found or
     * b) to the begin of the new block, if 'hit_end' was not found.
     *
     * Moreover, the area between 'hit_begin' and 'hit_end' will be
     * copied into 'buf' before the new block overwrites the old one.
     */
    const char *hit_begin;

    /* Points into main block of text.
     */
    const char *hit_end;

    /* Store to save text before source array will be overwritten.
     */
    char_buffer_t buf;
    /*
    char *buf;
    size_t buf_len;
    size_t buf_used;
    */

    /* Right shift of hit_begin anchor in search_pair_get_chars(). */
    size_t clip_begin;

    /* Left shift of hit_end anchor in search_pair_get_chars(). */
    size_t clip_end;
} search_pair_t;

void char_buffer_clear(
        char_buffer_t *p_buf)
;

search_pair_t *search_pair_create()
;

void search_pair_destroy(
        search_pair_t **p_sp)
;

void search_pair_reset(
        search_pair_t *sp)
;

void search_pair_reset_array(
        search_pair_t **start, search_pair_t **end)
;

/* Helper function to read content of search pair.
 * The returned char pointer (start) maps into cache buffer sp->buf (if sp->buf.used > 0)
 * or directly into current buffer 'p_buf'.
 *
 * Returned value is the length of the pair.
 * end could be NULL.
 *
 */
size_t search_pair_get_chars(
        const char_buffer_t *p_buf,
        search_pair_t *sp,
        const char **start,
        const char **end,
        char close_string)
;

/* Saves string before source will be overwritten.
 *
 * Note: sp->hit_begin will be reset no NULL.
 *       If the search_pair is spanned over two (or more) 'p_buf' arrays
 *       set it to the start of the next array before you call this 
 *       function again for 'sp'.
 *
 */
void search_pair_cache( 
        const char_buffer_t *p_buf,
        search_pair_t *sp)
;

void search_pair_cache_array(
        const char_buffer_t *p_buf,
        search_pair_t **start, search_pair_t **end)
;

void search_pair_dprintf(
        const int fd,
        const search_pair_t *sp)
;

void search_array_dprintf(
        const int fd,
        const search_pair_t **start,
        const search_pair_t **end)
;

ssize_t search_pair_write(
        const int fd,
        const search_pair_t *sp)
;

ssize_t search_array_write(
        const int fd,
        const search_pair_t **start,
        const search_pair_t **end)
;

void search_array_flush(
        const int fd,
        char_buffer_t *p_buf)
;

void search_pair_write_cached(
        const int fd,
        char_buffer_t *p_buf,
        const search_pair_t *sp)
;

void search_array_write_cached(
        const int fd,
        char_buffer_t *p_buf,
        const search_pair_t **start,
        const search_pair_t **end)
;

//============================================

int search_elem_search(
        const char_buffer_t *p_buf,
        size_t start, size_t *p_stop,
        const search_elem_t *el)
;

int search_pair_search(
        const char_buffer_t *p_buf,
        size_t start, size_t *p_stop,
        search_pair_t *sp)
;

/* Try to match array of pairs. 
 *
 * - End of pairs is marked by NULL.
 * - If p_buf.p is too short, backup/cache the founded
 * values and return -1
 */
int parser_search(
        const char_buffer_t *p_buf,
        size_t buf_start, size_t *p_buf_stop,
        search_pair_t **pairs, size_t pair_start, size_t *p_pair_stop )
;

/* Create array of search pattern (of type search_pair*), followed by NULL.
 *
 * Abbreviation: [ (","), NULL ]
 */
search_pair_t **pattern_1_create()
;

/* Create array of search pattern (of type search_pair*), followed by NULL.
 *
 * Abbreviation:
 * 0 [ (","), NULL ]   "X"
 * 1 [ (","), NULL ]   Sender
 * 2 [ (","), NULL ]   Thema
 * 3 [ (","), NULL ]   Titel
 * 4 [ (","), NULL ]   Datum
 * 5 [ (","), NULL ]   Zeit
 * 6 [ (","), NULL ]   Dauer
 * usw. mit "Größe [MB]", "Beschreibung", "Url", "Website", "Untertitel",
 * "UrlRTMP", "Url_Klein", "UrlRTMP_Klein", "Url_HD", "UrlRTMP_HD", 
 * "DatumL", "Url_History", "Geo", "neu" 
 */
search_pair_t **pattern_filmliste_flexibel_create()
;

/* Create array of search pattern (of type search_pair*), followed by NULL.
 *
 * Abbreviation:
 * 0 [ (","), NULL ]   "Filmliste"
 * 1 [ (","), NULL ]   "Date string 1"
 * 2 [ (","), NULL ]   "Date string 2"
 *
 * Input example:   "Filmliste" : [ "11.02.2017, 15:22", "11.02.2017, 14:22",
 * "3", "MSearch [Vers.: 2.5.2]", "2316758754258e37a5c0cf958ae35908" ]
 */
search_pair_t **pattern_filmliste_head()
;

// Deconstructor of pattern_*_create
void pattern_destroy( search_pair_t ***p_pairs)
;
