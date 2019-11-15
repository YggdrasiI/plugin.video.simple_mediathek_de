/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:expandtab */
#pragma once

/* Buffer of output which tries to omit unnessary string parsing 
 * operations (a.ka. not parse all elements in the reversed output.)
 *
 * This file should only be included in search.h
 */

//#include "settings.h"

#define OUT_DEFAULT_LEN 300;

void output_qsort_set_workspace(
        search_workspace_t *p_s_ws)
;

void output_select_sorting_function(
        const char* keyword,
        int *p_reversed_in_out,
        sort_cmp_handler_t **p_sort_handler_out)
;

output_t output_init(
        uint32_t N,
        uint32_t Nskip,
        int reversed,
        int comma_offset,
        sort_cmp_handler_t *sort_handler)
;

void output_uninit(
        output_t *p_output)
;

void output_add_id(
        search_workspace_t *p_s_ws,
        output_t *p_output,
        uint32_t id)
;

/* Process all title reading operations before
 * title buffer is flushed. */
void output_flush(
        search_workspace_t *p_s_ws,
        output_t *p_output)
;

void output_prepare_for_sort(
        search_workspace_t *p_s_ws,
        output_candidate_t *p_candidate)
;

/* Generate for a sufficient¹ subset of p_candidates the
 * strings to print.
 *
 * Sufficent subset: Depending on the sorting/reverse flags
 *  we could skip a few element of the
 *  [ Nskip | N | Nsearch ] array.
 */
void output_fill(
        search_workspace_t *p_s_ws,
        output_candidate_t *p_candidate)
;

size_t output_print(
        int fd,
        output_t *p_output)
;

void output_sort(
        output_t *p_output)
;
