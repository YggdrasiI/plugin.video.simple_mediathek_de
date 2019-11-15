/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:expandtab */
#pragma once

#include "brotli/encode.h"
#include "brotli/decode.h"
#include "parser.h"

typedef struct { 
    BrotliEncoderState *encoder;
    char *_buf_input_and_output;
    //char_buffer_t buf_input;
    char_buffer_t buf_output;
    size_t bytes_written;
} brotli_encoder_workspace_t;

typedef struct { 
    BrotliDecoderState *decoder;
    char *_buf_input_and_output;
    char_buffer_t buf_input;
    size_t bytes_read;
    /* Needed to save state between two calls
     * of brotli_read_buffer(...)
     */
    size_t undecoded_start_byte;
} brotli_decoder_workspace_t;

brotli_encoder_workspace_t brotli_encoder_ws_create(
        int compress_quality)
;

void brotli_encoder_ws_destroy(
        brotli_encoder_workspace_t *p_brotli)
;

brotli_decoder_workspace_t brotli_decoder_ws_create()
;

void brotli_decoder_ws_destroy(
        brotli_decoder_workspace_t *p_brotli)
;

/* Compress content of p_in into p_brotli->buf_output.
 *
 * Set finish = 1 to encode in BROTLI_OPERATION_FINISH
 * mode and force flushing of the output buffer.
 */
void brotli_write_buffer(
        const int fd,
        brotli_encoder_workspace_t *p_brotli,
        char_buffer_t *p_in,
        int finish)
;

/* Read not the whole file, but (p_out->len - p_out.used) bytes.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
int brotli_read_buffer(
        const int fd,
        brotli_decoder_workspace_t *p_brotli,
        char_buffer_t *p_out,
        int *p_finish)
;
