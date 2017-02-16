#pragma once

/* Buffer of output which tries to omit unnessary string parsing 
 * operations (a.ka. not parse all elements in the reversed output.)
 *
 * This file should only be included in search.h
 */

//#include "settings.h"

#define OUT_DEFAULT_LEN 300;


output_t output_init(
        uint32_t N,
        uint32_t Nskip,
        int reversed,
        int comma_offset)
;

void output_uninit(
        output_t *p_output)
;

void output_add_id(
        output_t *p_output,
        uint32_t id)
;

/* Process all title reading operations before
 * title buffer is flushed. */
void output_flush(
        search_workspace_t *p_s_ws,
        output_t *p_output)
;

void output_fill(
        search_workspace_t *p_s_ws,
        output_str_t *p_out,
        uint32_t id)
;

size_t output_print(
        int fd,
        output_t *p_output)
;
