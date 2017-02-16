#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include "settings.h"
#include "params.h"
#include "parser.h"
#include "channels.h"
#include "linked_list.h"
#include "filmliste.h"


typedef struct {
  file_data_t payload;
  char_buffer_t buf_in;
  char_buffer_t buf_entry;
  search_pair_t **token_pattern;
  int current_anchor_index;
#ifdef COMPRESS_BROTLI
  brotli_decoder_workspace_t brotli_payload;
#endif
  arguments_t *p_arguments;
} payload_workspace_t;

payload_workspace_t payload_ws_create(
        arguments_t *p_arguments)
;

void payload_ws_destroy(
        payload_workspace_t *p_s_ws)
;

int open_payload_file(
        payload_workspace_t *p_pay_ws,
        int i_num)
;

int payload_do_search(
        payload_workspace_t *p_pay_ws,
        int i_num)
;


/* Print Json data, but unpack urls into complete form.
 * 
 * Assumed string structure (escaped \" allowed):
 * (,)\{0,1}["[^"]*","[^"]*","[^"]*","[^"]*","[^"]*","[^"]*",[^]]*]
 * ([]- Range with at least 6 strings)
 * */
void print_payload(
        payload_workspace_t *p_s_ws)
;

/* Generate pattern for search of token in print_payload()
 */
search_pair_t **pattern_payload_file()
;

/* For sorting */
int payload_anchor_compar(
        const void *p_a,
        const void *p_b)
;
