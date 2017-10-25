#define _XOPEN_SOURCE // For strptime
#include <stdlib.h>
#include <time.h>

#include <string.h>
#include "params.h"
#include "filmliste.h"
#include "parser.h"
#include "search.h"
#include "payload.h"
#include "info.h"
#include "open.h"
#include "main.h"

const int32_t Time_Array[] = TIME_ARRAY;
const int32_t Duration_Array[] = DURATION_ARRAY;

int indexing(
        arguments_t *p_arguments)
{

    // Argument handling
    int fdin = 0;
    //int fdout = -1;
    int fdout = 1; // for output after indexing like --info

    if( p_arguments->input_file ){
        fdin = open(p_arguments->input_file, O_RDONLY);
        if( fdin < 0 ){
            fprintf(stderr, "Can not open file '%s'\n", p_arguments->input_file);
            return -1;
        }
    }else{
        fdin = 0; // file descriptor of stdin
    }

#if 0
    if( p_arguments->output_file ){
        fdout = open(p_arguments->output_file, O_CREAT|O_WRONLY|O_TRUNC,
                S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        if( fdout < 0 ){
            fprintf(stderr, "Can not open file '%s'\n", p_arguments->output_file);
            return -1;
        }
    }else{
        fdout = 1; // file descriptor of stdout
    }
#endif


    /* Gen list of search patterns.
    */
    //search_pair_t **pp_pairs = pattern_1_create();
    search_pair_t **pp_pairs = pattern_filmliste_flexibel_create();

    // Pattern for second line to grab creation date of list.
    search_pair_t **pp_pairs_header = pattern_filmliste_head();

    // Initial of further variables
    char *_buf_in = malloc( (MY_BUFSIZ + 1) * sizeof(*_buf_in) );
    _buf_in[MY_BUFSIZ] = '\0';
    char_buffer_t buf_in = {MY_BUFSIZ, 0, _buf_in};

    //char _buf_out[OUT_CACHESIZ + 1];
    char *_buf_out = malloc( (OUT_CACHESIZ + 1) * sizeof(*_buf_out) );
    _buf_out[OUT_CACHESIZ] = '\0';
    char_buffer_t buf_out = {OUT_CACHESIZ, 0, _buf_out};

    size_t buf_start, buf_stop, pair_start, pair_stop;
    buf_start = 0;
    pair_start = 0;
    int ret_search;
    time_t main_list_creation_time = -1;

    filmliste_workspace_t fl_ws = filmliste_ws_create(p_arguments);

    if( p_arguments->diff_update ){
        // Read channels list from main index.
        p_arguments->diff_update = 0;
        search_workspace_t i_ws = search_ws_create(p_arguments);
        if( 0 == open_index_file(&i_ws, i_ws.p_arguments) &&
                0 == info_read(&i_ws) )
        {
            // Swap channels struct of fl_ws and i_ws
            channels_ws_t tmp = fl_ws.channels;
            fl_ws.channels = i_ws.channels;
            i_ws.channels = tmp;

            // Store time stamp of main data to measure distance.
            main_list_creation_time = i_ws.list_creation_time;
        }else{
            fprintf(stderr, "Can not read channel list from main index. Channel numbering will be inconsistent.\n");
        }

        search_ws_destroy(&i_ws);
        p_arguments->diff_update = 1;
    }else{
        remove_old_diff_files(p_arguments);
    }


    init_next_searchable_file(&fl_ws); // Titles, etc
    init_next_payload_file(&fl_ws); // Urls, etc
    create_index_file(&fl_ws); // Date, Time, etc

    // Start
    buf_in.used = read(fdin, buf_in.p,
            buf_in.len); // Negative value indicate read error.

#if 1
    // Search header informations
    ret_search = parser_search(&buf_in, buf_start, &buf_stop,
            pp_pairs_header, pair_start, &pair_stop);
    if( ret_search == SEARCH_MATCH_PAIRS ){
        const char *key, *date1, *date2;
        size_t len[3];
        // Detect substring positions but not add '\0' after them
        len[0] = search_pair_get_chars(&buf_in, *(pp_pairs_header+0), &key, NULL, 0);
        len[1] = search_pair_get_chars(&buf_in, *(pp_pairs_header+1), &date1, NULL, 0);
        len[2] = search_pair_get_chars(&buf_in, *(pp_pairs_header+2), &date2, NULL, 0);

        const char keyw[] = "Filmliste";
        if( len[0] != sizeof(keyw)-1 ||
                len[1] > 30 || len[2] > 30 ||
                strncmp(keyw, key, sizeof(keyw)-1) != 0 ){
            // Header of file not parseable
        }else{
            char *tmp = malloc( (len[1]+1)*sizeof(*tmp) );
            tmp[len[1]] = '\0';
            memcpy(tmp, date1, len[1]);
            struct tm tm;
            memset(&tm, 0, sizeof(struct tm));
            strptime(tmp, "%d.%m.%Y, %H:%M", &tm);
            time_t tdate1 = mktime(&tm);
            Free(tmp);

            // date 2...
            //strptime(date2, "%d.%m.%Y, %H:%M", &tm);
            //time_t tdate2 = mktime(&tm);

            fl_ws.list_creation_time = tdate1;
        }

        search_pair_reset_array(pp_pairs_header, pp_pairs + pair_stop);
    }else{
        // Header of file not parseable
        assert( ret_search == SEARCH_MATCH_PAIRS);
    }

    if( p_arguments->diff_update && fl_ws.list_creation_time > main_list_creation_time
            && (fl_ws.list_creation_time - main_list_creation_time ) > 86400 ){

        char *main_day_str = malloc(40 * sizeof(*main_day_str));
        char *diff_day_str = malloc(40 * sizeof(*diff_day_str));
        struct tm * timeinfo;

        timeinfo = localtime (&main_list_creation_time);
        strftime (main_day_str, 40, "%d. %b. %Y %R", timeinfo);
        timeinfo = localtime (&fl_ws.list_creation_time);
        strftime (diff_day_str, 40, "%d. %b. %Y %R", timeinfo);

        // TODO: Es fehlt das Wissen über die genaue Generierung der Diff-Updates
        // Ein einfacher Check mit dist(..) <= 24 reicht nicht aus.
        fprintf(stderr, "Warning: Differential updates covers only current day, " \
                "but the given data is already older...\n" \
                "Date of main index: %s\n" \
                "Date of diff index: %s\n",
                main_day_str, diff_day_str);
    }

#endif
    write_index_header(&fl_ws);

    // Go back to start
    buf_start = 0;
    pair_start = 0;
    while (buf_in.used > 0){

        // Search for group of tuples
#if 1
        ret_search = parser_search(&buf_in, buf_start, &buf_stop,
                pp_pairs, pair_start, &pair_stop);
#else
        buf_stop = buf_in.used;
        ret_search = SEARCH_FAILED_END_OF_BUFFER;
#endif

        if( ret_search == SEARCH_MATCH_PAIRS){
            // Propagate data and restart
#if 0
            //search_array_dprintf(1, pp_pairs, pp_pairs+pair_stop);
            //dprintf(1, "\n");

            //search_array_write(1, pp_pairs, pp_pairs+pair_stop);
            search_array_write_cached(fdout, &buf_out,
                    (const search_pair_t **) pp_pairs,
                    (const search_pair_t **) pp_pairs+pair_stop);
#else
            filmliste_handle(/*fl_ws.searchable_strings.fd,*/
                    &buf_in,
                    &fl_ws,
                    pp_pairs, pp_pairs + pair_stop,
                    &buf_out);
#endif


            // Next search should start with first pair again.
            search_pair_reset_array(pp_pairs, pp_pairs + pair_stop);
            pair_start = 0;

        }else if( ret_search == SEARCH_FAILED_DUE_ABORT_CHAR ){
            // Pattern not found before search aborted. Here,
            // some chars of the buffer, i.e. after an '\n' are still unprocessed.

            // Go to next char to avoid re-check on abort char.
            ++buf_stop;

            // Next search should start with first pair again.
            search_pair_reset_array(pp_pairs, pp_pairs + pair_stop);
            pair_start = 0;

        }else if( ret_search == SEARCH_FAILED_END_OF_BUFFER ){
            // Save pair to continue after fetch of more data.
            pair_start = pair_stop;
        }else{
            assert(0); // unknown state.
        }

        // All data consumed? Fetch more data until buf_stop < buf_in.used.
        while( buf_stop >= buf_in.used ){
            //DEBUG( fprintf(stderr, "Fetch new buf_in data\n"); )
            filmliste_cache_data_before_buffer_clears(&buf_in, &fl_ws);
            buf_stop -= buf_in.used;
            buf_in.used = read(fdin, buf_in.p, MY_BUFSIZ);
            if( buf_in.used == 0 ){
                // No more data available
                break;
            }

            if( buf_stop >= buf_in.used ){
                search_pair_t *latest_pair = *(pp_pairs+pair_stop);
                if( latest_pair != NULL && latest_pair->buf.used > 0){
                    /* buf.used > 0 indicate that the beginning pattern
                     * was hit, but the end pattern not. Moreover, buf_stop
                     * is so high that the whole buffer will be consumed.
                     * => Rerun cache for the current pair.
                     */
                    latest_pair->hit_begin = buf_in.p;
                    latest_pair->hit_end = buf_in.p + buf_in.used;
                    search_pair_cache(&buf_in, latest_pair);
                }
            }
        }

        // Now, buf_stop is < buf_in.used or buf_in.used = 0.
        buf_start = buf_stop;
    }

    // Flush output buffer (3)
    if( fl_ws.payload.fd > -1 && fl_ws.payload_buf.used ){
#ifdef COMPRESS_BROTLI
        brotli_write_buffer(fl_ws.payload.fd, &fl_ws.brotli_payload, &fl_ws.payload_buf, 1);
#else
        search_array_flush(fl_ws.payload.fd, &fl_ws.payload_buf);
#endif
    }

    // Flush output buffer (2)
    if( fl_ws.index_fd > -1 ){
        linked_list_write_partial( fl_ws.index_fd, &fl_ws.index, 1);
        write_index_footer(&fl_ws);
    }

    // Flush output buffer
    if( fl_ws.searchable_strings.fd > -1 ){
#ifdef COMPRESS_BROTLI
        brotli_write_buffer(fl_ws.searchable_strings.fd, &fl_ws.brotli_title, &buf_out, 1);
#else
        search_array_flush(fl_ws.searchable_strings.fd, &buf_out);
#endif
    }

    // Close input fd if not fd of stdin.
    if( fdin > 0 ){
        close(fdin);
    }
    // Item for output fd
    if( fl_ws.searchable_strings.fd > 2 ){
        close(fl_ws.searchable_strings.fd);
    }

    info_print2(fdout, &fl_ws);

    pattern_destroy( &pp_pairs);
    pattern_destroy( &pp_pairs_header);
    filmliste_ws_destroy(&fl_ws);

    free(_buf_in);
    free(_buf_out);

    return 0;
}

int searching(
        arguments_t *p_arguments)
{
    //int fdin = -1;
    int fdout = 1;

    // Argument handling
    if( p_arguments->max_num_results == 0 ){
        dprintf(fdout, "{ \"found\": []}\n");
        return 0;
    }

    search_workspace_t s_ws = search_ws_create(p_arguments);
    if( open_index_file(&s_ws, s_ws.p_arguments) ){
        fprintf(stderr, "Opening of index file failed.\n");
        return -1;
    }
    if( open_title_file(&s_ws, s_ws.p_arguments) ){
        fprintf(stderr, "Opening of title file failed.\n");
        return -1;
    }

    if( p_arguments->output_file ){
        fdout = open(p_arguments->output_file, O_CREAT|O_WRONLY|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        if( fdout < 0 ){
            fprintf(stderr, "Can not open file '%s'\n", p_arguments->output_file);
            return -1;
        }
    }else{
        fdout = 1; // file descriptor of stdout
    }

    //Read index file
    read_index_header(&s_ws);
    linked_list_read(s_ws.index_fd, &s_ws.index);
    read_index_footer(&s_ws);

    // After index file, title file can be loaded.
#ifdef READ_TITLE_FILE_PARTIAL
    search_pattern_t *p_pattern_list = NULL;
    int len_patterns = search_gen_patterns_for_partial(
        &s_ws, p_arguments, &p_pattern_list);

    dprintf(fdout, "{ \"found\": [\n");

    while( 1 ) { // Loop read normal file, diff file, and then breaks.

        int status = search_read_title_file_partial(&s_ws);
        fprintf(stderr, "Num pattern: %i\n", len_patterns);
        while( status >= 0 ){
            //fprintf( stderr, "Index: %i %i\n", (int) s_ws.chunks.partial_i, status);

            int ip;
            for( ip=0; ip<len_patterns; ++ip){

                if( 0 < search_do_search_partial(&s_ws, &p_pattern_list[ip]) )
                {
                    // At least one new entry found
                    //output_flush(&s_ws, &s_ws.output); // too early
                }

                if( s_ws.output.found >= s_ws.output.M &&
                        !s_ws.output.search_whole_index_flag
                  ){
                    output_flush(&s_ws, &s_ws.output);
                    goto break_both; // Enough data. break pattern, chunk and file loop.
                }
            }

            output_flush(&s_ws, &s_ws.output);

            if( status > 0 ){
                // Latest chunk already read
                break;
            }
            // Load next chunk
            status = search_read_title_file_partial(&s_ws);
        };


        if(s_ws.p_arguments->diff_update){
            break;
        }else{
          // Set flag and open differential files.
          s_ws.p_arguments->diff_update = 1;
          search_reset_workspace(&s_ws);

          if( open_index_file(&s_ws, s_ws.p_arguments) ){
              // No diff files available
              DEBUG(fprintf(stderr, "No diff file available.\n"));
              break;
          }
          if( open_title_file(&s_ws, s_ws.p_arguments) ){
              // No diff files available
              break;
          }
      }
    } // End of "normal|diff" loop

break_both:

    print_search_results(&s_ws);
    dprintf(fdout, "\n]}\n");

    if( 0 < len_patterns ){
        p_pattern_list[0].title_sub_pattern[0] = NULL;
        free(p_pattern_list[0]._title_sub_pattern);
        free(p_pattern_list[0].title_pattern);
        free(p_pattern_list);
    }
#else
    dprintf(fdout, "{ \"found\": [\n");
    while( 1 ) { // Read normal file, diff file, and then breaks.
        search_read_title_file(&s_ws);
        search_do_search(&s_ws, p_arguments);
      if(s_ws.p_arguments->diff_update){
          break;
      }else{
          // Set flag and open differential files.
          s_ws.p_arguments->diff_update = 1;
          if( open_index_file(&s_ws, s_ws.p_arguments) ){
              // No diff files available
              break;
          }
          if( open_title_file(&s_ws, s_ws.p_arguments) ){
              // No diff files available
              break;
          }
      }
    } // End of "normal|diff" loop

    print_search_results(&s_ws);
    dprintf(fdout, "\n]}\n");
#endif

    // Close input fds if not stdin.
    if( s_ws.index_fd > 0 ){
        close(s_ws.index_fd);
        s_ws.index_fd = -1;
    }
    // Flush output buffer
    if( fdout > -1 ){
        // Add flushing here, if output buffered.
        if( fdout > 2 ){
            close(fdout);
        }
    }

    if( s_ws.searchable_strings.fd > 0 ){
        close(s_ws.searchable_strings.fd);
        s_ws.searchable_strings.fd = -1;
    }

    search_ws_destroy(&s_ws);

    return 0;
}

int payload_reading(
        arguments_t *p_arguments)
{

    assert( p_arguments->payload_anchors != NULL );

    if( p_arguments->payload_anchors == NULL ){
        fprintf(stderr, "No payload anchor given.\n");
        return -1;
    }

    payload_workspace_t pay_ws = payload_ws_create(p_arguments);

    int fdout = 1;
    dprintf(fdout, "{\n  ");

    int i, first = 1;
    for(i=0; i<p_arguments->payload_anchors_len; ++i){
        if(!first){
            dprintf(fdout, ",\n  ");
        }
        if( 0 == payload_do_search(&pay_ws, i)){
            print_payload(&pay_ws);
            first = 0;
        }
    }
    dprintf(fdout, "\n}");

    payload_ws_destroy(&pay_ws);
    return 0;
}

int info_reading(
        arguments_t *p_arguments)
{
    // Argument handling
    //int fdin = -1;
    int fdout = 1;

    // Try to open diff file and if not found...
    p_arguments->diff_update = 1;
    search_workspace_t i_ws = search_ws_create(p_arguments);
    if( open_index_file(&i_ws, i_ws.p_arguments) ){
        // ... take normal file.
        i_ws.p_arguments->diff_update = 0;
        // search_reset_workspace(&s_ws); // Not needed during info mode

        if( open_index_file(&i_ws, i_ws.p_arguments) ){
            fprintf(stderr, "Opening of index file failed.\n");
            return -1;
        }
    }

#if 1
    if( p_arguments->output_file ){
        fdout = open(p_arguments->output_file, O_CREAT|O_WRONLY|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        if( fdout < 0 ){
            fprintf(stderr, "Can not open file '%s'\n", p_arguments->output_file);
            return -1;
        }
    }else{
        fdout = 1; // file descriptor of stdout
    }
#endif

    if( 0 == info_read(&i_ws) ){
        info_print(fdout, &i_ws);
    }else{
        fprintf(stderr, "Can not parse index file\n");
    }

    // Close input fds if not stdin.
    if( i_ws.index_fd > 0 ){
        close(i_ws.index_fd);
        i_ws.index_fd = -1;
    }

    // Flush output buffer
    if( fdout > -1 ){
        // Add flushing here, if output buffered.
        if( fdout > 2 ){
            close(fdout);
        }
    }

    search_ws_destroy(&i_ws);

    return 0;
}

int main(int argc, char** argv){

    // Argument handling
    arguments_t arguments = handle_arguments(argc, argv);
    int ret = 0;

    fprint_args(stderr, &arguments);
    if( arguments.mode == INDEXING_MODE ){
      DEBUG( fprintf(stderr, "Start indexing...\n") );
      ret = indexing(&arguments);
    }

    if( arguments.mode == SEARCH_MODE ){
      DEBUG( fprintf(stderr, "Start search...\n") );
      ret = searching(&arguments);
    }

    if( arguments.mode == PAYLOAD_MODE ){
      DEBUG( fprintf(stderr, "Get urls...\n") );
      ret = payload_reading(&arguments);
    }

    if( arguments.mode == INFO_MODE ){
      DEBUG( fprintf(stderr, "Info mode...\n") );
      ret = info_reading(&arguments);
    }

    clear_arguments(&arguments);
    return ret;
}
