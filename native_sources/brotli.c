/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:expandtab */
#include <errno.h>

#include "brotli.h"
#include "helper.h"

static const size_t kFileBufferSize = 65536;
brotli_encoder_workspace_t brotli_encoder_ws_create(
        int compress_quality)
{
    brotli_encoder_workspace_t brotli;

    brotli.encoder = BrotliEncoderCreateInstance(0, 0, 0);
    brotli._buf_input_and_output = (char *) malloc(kFileBufferSize);
    assert(brotli._buf_input_and_output);

    brotli.buf_output.p = brotli._buf_input_and_output;
    brotli.buf_output.len = kFileBufferSize;
    brotli.buf_output.used = 0;

    //uint32_t compress_quality = 7;
    //uint32_t lgwin = BROTLI_DEFAULT_WINDOW; // BROTLI_MIN_WINDOW_BITS+4;
    //uint32_t lgwin = BROTLI_DEFAULT_WINDOW-2; // this reduces the memory footprint by ~2*6 MB.
    //uint32_t lgwin = BROTLI_DEFAULT_WINDOW-3; // 2 MB less (during indexing)
    uint32_t lgwin = BROTLI_DEFAULT_WINDOW-4; // 1 MB less
    BrotliEncoderSetParameter(brotli.encoder, BROTLI_PARAM_QUALITY, (uint32_t)compress_quality);
    BrotliEncoderSetParameter(brotli.encoder, BROTLI_PARAM_LGWIN, (uint32_t)lgwin);
    BrotliEncoderSetParameter(brotli.encoder, BROTLI_PARAM_MODE, BROTLI_MODE_TEXT);

    brotli.bytes_written = 0;

    return brotli;
}

void brotli_encoder_ws_destroy(
        brotli_encoder_workspace_t *p_brotli)

{
    if( p_brotli->encoder ){
        BrotliEncoderDestroyInstance(p_brotli->encoder);
        p_brotli->encoder = NULL;
    }
    Free(p_brotli->_buf_input_and_output);
}

brotli_decoder_workspace_t brotli_decoder_ws_create()
{
    brotli_decoder_workspace_t brotli;

    brotli.decoder = BrotliDecoderCreateInstance(0, 0, 0);
    brotli._buf_input_and_output = (char *) malloc(kFileBufferSize);
    assert(brotli._buf_input_and_output);

    brotli.buf_input.p = brotli._buf_input_and_output;
    brotli.buf_input.len = kFileBufferSize;
    brotli.buf_input.used = 0;

    brotli.bytes_read = 0;
    brotli.undecoded_start_byte = 0;

    return brotli;
}

void brotli_decoder_ws_destroy(
        brotli_decoder_workspace_t *p_brotli)

{
    if( p_brotli->decoder ){
        BrotliDecoderDestroyInstance(p_brotli->decoder);
        p_brotli->decoder = NULL;
    }
    Free(p_brotli->_buf_input_and_output);
}


void brotli_write_buffer(
        const int fd,
        brotli_encoder_workspace_t *p_brotli,
        char_buffer_t *p_in,
        int finish)
{
    //char_buffer_t *p_in = &p_brotli->buf_input;
    char_buffer_t *p_out = &p_brotli->buf_output;

    size_t available_in_start = p_in->used;
    size_t available_in = available_in_start;
    const uint8_t* in_start = (const uint8_t *)p_in->p;
    const uint8_t* in_next = in_start;
    size_t available_out_start = p_out->len - p_out->used;
    size_t available_out = available_out_start;
    uint8_t* out_start = (uint8_t *)(p_out->p + p_out->used);
    uint8_t* out_next = out_start;

    int is_ok = 1;

    if (!BrotliEncoderCompressStream(
                p_brotli->encoder,
                finish?BROTLI_OPERATION_FINISH:BROTLI_OPERATION_PROCESS,
                &available_in, &in_next, &available_out, &out_next,
                &p_brotli->bytes_written)) {
        is_ok = 0;
    }

    while ( is_ok && /*available_in > 0 &&*/ available_in != available_in_start ){
        // Not all input consumed. Flush output buffer
        // and try to encode the rest of the input.
#if 0
        DEBUG( fprintf(stderr, "%s Number unhanded bytes: %i / %i Avail. bytes :%i is_ok: %i\n", \
                    "Flush brotli output buffer.", \
                    (int)available_in, (int)available_in_start, \
                    (int)available_out, is_ok); \
             );
#endif

        assert( ((char *)out_next - p_out->p) == (p_out->len - available_out) );
        p_out->used = p_out->len - available_out;

        search_array_flush(fd, p_out);

        // Reset the pointers for next round.
        out_start = (uint8_t *)p_out->p;
        out_next = out_start;
        available_out_start = p_out->len - p_out->used;
        available_out = available_out_start;
        available_in_start = available_in;

        if (!BrotliEncoderCompressStream(
                    p_brotli->encoder,
                    finish?BROTLI_OPERATION_FINISH:BROTLI_OPERATION_PROCESS,
                    &available_in, &in_next, &available_out, &out_next,
                    &p_brotli->bytes_written)) {
            is_ok = 0;
        }
    }

    assert( available_in == 0 );
    // We consumed all input chars.
    p_in->used = available_in;
    //in_next = in_start;
    //available_in_start = 0;

    assert( ((char *)out_next - p_out->p) == (p_out->len - available_out) );
    p_out->used = p_out->len - available_out;

    if(finish)
    {
        if (!BrotliEncoderIsFinished(p_brotli->encoder)){
            fprintf(stderr, "%s\n", "Brotli encoder does not finished stream.");
        }
        search_array_flush(fd, p_out);
    }

#if 0
    if (p_out->len - p_out->used < 100){
        search_array_flush(fd, p_out);
    }
#endif
}

int brotli_read_buffer(
        const int fd,
        brotli_decoder_workspace_t *p_brotli,
        char_buffer_t *p_out,
        int *p_finish)
{
    char_buffer_t *p_in = &p_brotli->buf_input;

    /* Pointer/Distance structure:
     * [  in.p      <= in_start / in_next     <= foo    <= bar   ]
     * [    0       <= undecoded_start_byte <= p.used <= p.len ]
     * < decoded   >  < bytes available to decode. If   > < Max  >
     * < in earlier>  < zero available refill the       > < avail>
     * < call      >  < input buffer in                 > < size >
     *
     * available_in = p.used - undecoded_start_byte
     *
     * [out.p    <= out_start / out_next <= out.p + out.len ]
     * [   0     <= out.used             <= out.len         ]
     *
     * Here, the .used member variable store where the latest decoding
     * has stop. Normally, out.used is 0 at start of function
     * and out.len at end of function.
     */

    // Buffer with data to decode.
    // p_in.used is limit of available bytes
    // undecoded_start_byte marks number of already decoded bytes.
    // Fetch new data if undecoded_start_byte == p_in.used.
    assert( p_in->len >= p_in->used );
    assert( p_brotli->undecoded_start_byte <= p_in->used );
    size_t available_in_start = p_in->used - p_brotli->undecoded_start_byte;
    size_t available_in = available_in_start;
    const uint8_t* in_start = (const uint8_t *)(p_in->p + p_brotli->undecoded_start_byte);
    const uint8_t* in_next = in_start;

    // Decoded bytes. Write new data at p_out->p + p_out->used
    assert( p_out->len >= p_out->used );
    size_t available_out_start = p_out->len - p_out->used;
    size_t available_out = available_out_start;
    uint8_t* out_start = (uint8_t *)(p_out->p + p_out->used);
    uint8_t* out_next = out_start;

    BrotliDecoderResult result = BROTLI_DECODER_RESULT_ERROR;
    int end_of_file_reached = 0;

    while (available_out > 0 && !end_of_file_reached) {

        if ( available_in == 0 )
        {
            assert( in_next == (uint8_t *)(p_in->p + p_in->used) );

            if( result == BROTLI_DECODER_RESULT_SUCCESS){
                // Do not try to read further input.
                break;
            }

            p_brotli->undecoded_start_byte = 0;
#ifdef CHECK_ERRNO
            errno = 0;
#endif
            ssize_t new_bytes_in = read(fd, p_in->p, p_in->len);
#ifdef CHECK_ERRNO
            if( errno != 0 ){
                int x = errno;
                fprintf(stderr, "Error: %s\n", strerror(x));
            }
#endif
            if( new_bytes_in == -1 || new_bytes_in == 0){
                // End of file reached, but decoder not finished.
                // It is possible that the decoder
                // had saved data in some internal buffer
                // (Yes, this case really exists!)

                // Try decoding with available_in==0.
                // but omit restart of while-loop.
                end_of_file_reached = 1;
            }else if( new_bytes_in < p_in->len ){
                // incomplete fill of input buffer.
                // Now, available_in != (p_in->len - p_in->used)
                p_in->used = new_bytes_in;
                available_in = new_bytes_in;
                in_next = (uint8_t *)p_in->p;
            }else{
                // Complete usage of input buffer
                p_in->used = new_bytes_in;
                available_in = new_bytes_in;
                in_next = (uint8_t *)p_in->p;
            }
            result = BROTLI_DECODER_RESULT_ERROR;
        }
        // Above if should had correlate with this result type.
        // Thus it should never be available here.
        assert( result != BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT);

        result = BrotliDecoderDecompressStream(
                p_brotli->decoder, &available_in, &in_next, &available_out, &out_next, 0);

#if 0
        fprintf(stderr, "Status: %s\n",
                BrotliDecoderErrorString(
                    BrotliDecoderGetErrorCode(p_brotli->decoder)
                    )
               );
#endif
        if( result == BROTLI_DECODER_RESULT_ERROR ){
            fprintf(stderr, "Brotli decoding error: %s\n",
                    BrotliDecoderErrorString(
                        BrotliDecoderGetErrorCode(p_brotli->decoder)
                        )
                   );
            *p_finish = 0;
            return -1;
        }

        assert( (char *)out_next >= p_out->p );
        assert( (char *)out_next <= p_out->p + p_out->len );
        p_out->used = p_out->len - (size_t)available_out;

        assert( (char *)in_next >= p_in->p );
        assert( (char *)in_next <= p_in->p + p_in->len );
        p_brotli->undecoded_start_byte = p_in->used - (size_t)available_in;

        if( available_out == 0 ){
            // Out buffer completely filled.
            // Note that available_in could be 0, too.

            //search_array_flush(1, p_out);
            break;
        }

        // Above if should had correlate with this result type.
        // Thus it should never be available here.
        assert( result != BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);

        // Allowed result types.
        // (Other types indicate logical error in this loop.)
        assert( result == BROTLI_DECODER_RESULT_SUCCESS
                || result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT );

        if (result == BROTLI_DECODER_RESULT_SUCCESS ){
            break;
        }
    }

    assert( available_in <= p_in->used );
    p_brotli->undecoded_start_byte = p_in->used - available_in;
    *p_finish = (result == BROTLI_DECODER_RESULT_SUCCESS);
    return 0;
}

