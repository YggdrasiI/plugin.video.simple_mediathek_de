#include <stdlib.h>
#include <stdio.h>

#include "helper.h"

#if OWN_STRTOL > 0
int strtoi(const char* str, char** endptr, int maxlen)
{
    assert( str != NULL);
    assert( maxlen >= 0);

    int ret = 0;
    char sign = 0; // 0 - Begin of number not found, -1 - negative, 1 - positive.
    const char *cur = str;
    while( 0 < maxlen-- ){
        switch( *cur ){
            case '\0': break;
            case ' ': { if( sign ) break; }
            case '-': { if( sign ) break;

                          sign = -1;
                      }
            default: {
                         if( *cur < '0' || *cur > '9' ) break;
                         if( !sign ) sign = 1;

                         ret = 10 * ret + (*cur - '0');
                     }
        }

        ++cur;
    }

    if( endptr != NULL ){ *endptr = (char *)cur; }
    if( sign < 0 ){ ret = -ret; }
    return ret;
}

#endif

time_t parse_starttime(
        const char_buffer_t *p_buf,
        search_pair_t *sp )
{
    const char *start;
    size_t limit;
    time_t starttime = 0;

    limit = search_pair_get_chars(p_buf, sp, &start, NULL, 0);

    if( limit > 0 ){
#if OWN_STRTOL > 0
        starttime = strtoi(start, NULL, limit);
#else
        starttime = strtol(start, NULL, 10);
#endif
    }

    return starttime;
}

time_t parse_day(
        const char_buffer_t *p_buf,
        filmliste_workspace_t *p_fl_ws,
        search_pair_t *sp)
{
    const char *start;
    char *end;
    int y = 0, m = 0, d = 1, limit;

    limit = search_pair_get_chars(p_buf, sp, &start, NULL, 0);

    if( limit > 0 ){
        // Search day, month and year.
#if OWN_STRTOL > 0
        d = strtoi(start, &end, 2);
#else
        d = strtol(start, &end, 10);
#endif
        limit -= (end-start) + 1; // subtract consumed chars.
        if( limit > 0 ) {
            start = end + 1;
#if OWN_STRTOL > 0
            m = strtoi(start, &end, 2) - 1;
#else
            m = strtol(start, &end, 10) - 1;
#endif
            limit -= (end-start) + 1; // subtract consumed chars.

            if( limit > 0 ) {
                start = end + 1;
#if OWN_STRTOL > 0
                y = strtoi(start, &end, 4) - 1900;
#else
                y = strtol(start, &end, 10) - 1900;
#endif
            }
        }
    }else{
        // Empty string
        //memset(&p_fl_ws->tmp_ts, 0, sizeof(p_fl_ws->tmp_ts)) ;
        //p_fl_ws->tmp_ts.tm_isdst = 0;
    }

    // Add 2000 for two digit year.
    if( y < (100 - 1900) ){ y += 2000; }

    // Normalize values
    y = clip(70, y, 200);
    m = clip(0, m, 11);
    d = clip(1, d, 31);

    // Convert into time stamp
    /* Note that parse_dauer() could set
     * tm_sec, tm_min, and tm_hour != 0
     *
     * This affects time stamp (do not add twice) and daylight saving evaluation.
     */

    //memset(&p_fl_ws->tmp_ts, 0, sizeof(p_fl_ws->tmp_ts)) ;
    //
    p_fl_ws->tmp_ts.tm_year = y;
    if( p_fl_ws->tmp_ts.tm_mday != d || p_fl_ws->tmp_ts.tm_mon != m ){
        p_fl_ws->tmp_ts.tm_mday = d;
        p_fl_ws->tmp_ts.tm_mon = m;

        // Toggle re-evaluation of tm_isdst if day changes (Input hopefully sorted?!)
        p_fl_ws->tmp_ts.tm_isdst = -1;
    }

    time_t ts =  mktime(&p_fl_ws->tmp_ts);

    /* Empty strings respect timezone and leads to -3600 (if time_t signed), not 0.
     * To get the same time stamp like parse_starttime() shift
     * it to 0.
     *
     * Affects 'Livestream' entries.
     */
    if( ts < 0 ){
       ts = 0;
    }

    return ts;
}

size_t parse_dauer(
        const char_buffer_t *p_buf,
        filmliste_workspace_t *p_fl_ws,
        search_pair_t *sp )
{
    const char *start;
    char *end;
    int h = 0, m = 0, s = 0, limit;

    limit = search_pair_get_chars(p_buf, sp, &start, NULL, 0);

    // Search hours, minutes and seconds.
    if( limit > 0 ){
#if OWN_STRTOL > 0
        h = strtoi(start, &end, 2);
#else
        h = strtol(start, &end, 10);
#endif
        limit -= (end-start) + 1; // subtract consumed chars.
        if( limit > 0 ) {
            start = end + 1;
#if OWN_STRTOL > 0
            m = strtoi(start, &end, 2);
#else
            m = strtol(start, &end, 10);
#endif
            limit -= (end-start) + 1; // subtract consumed chars.

            if( limit > 0 ) {
                start = end + 1;
#if OWN_STRTOL > 0
                s = strtoi(start, &end, 2);
#else
                s = strtol(start, &end, 10);
#endif
            }
        }
    }

    s = clip(0, s, 61);
    m = clip(0, m, 59);
    h = clip(0, h, 23);

    if( p_fl_ws ){
        p_fl_ws->tmp_ts.tm_sec = s;
        p_fl_ws->tmp_ts.tm_min = m;
        p_fl_ws->tmp_ts.tm_hour = h;
    }

    return (s + 60 * (m + 60 * h));
}

size_t transform_search_title(
        const char *p_in,
        char **pp_out)
{
    if( p_in == NULL ){
        assert(0);
        return 0;
    }

    size_t len_in = strlen(p_in);

    Free(*pp_out);
    /*uint8_t * const*/
    char * const p_out =  (char *) malloc( (1 + len_in) * sizeof(char) );
    *(p_out + len_in) = '\0';
    *pp_out = p_out;

    const char *p_cur_in = p_in; // current char
    char *p_cur_out = p_out; // current output position

    /* Utf-8 character loop. */
    int i = 0, j = 0;
    int d, l;
    unsigned int uc; // 4 byte Unicode code point
    for( l=strlen(p_in), uc=u8_nextchar((char *)p_in,&j), d=j-i;
            i<l;
            i=j, uc=u8_nextchar((char *)p_in,&j), d=j-i )
    {
        p_cur_in = p_in+i;

        switch(uc){
            case 0x1e9e: // char ẞ
                // Insert lower esszet
                *p_cur_out++ = 0x1e; *p_cur_out++ = 0x9e;
                break;
                //case 0x00df: // char ß

            case 0x0022: // char "
            case 0x0027: // char '
            case 0x005c: // char '\\'
                //case 0x002f: // char '/'
                // Skip character
                break;

            case 0x0020: // char ' '
            case 0x0009: // char '\t'
            case 0x002D: // char '-'
            case 0x005F: // char '_'
            case 0x007C: // char '|'
                // Trim space
                if( p_cur_out == p_out || *(p_cur_out-1) != ' '){
                    *p_cur_out++ = ' ';
                }
                break;

            case 0x00c4: // char Ä (utf-8 is C3 84)
            case 0x00d6: // char Ö (utf-8 is C3 96)
            case 0x00dc: // char Ü (utf-8 is C3 9C)
                //uc += 0x20; // lower case umlaut
                *p_cur_out++ = (*p_cur_in++);
                *p_cur_out++ = (*p_cur_in++) + 0x20;
                break;
                //case 0x00e4: // char ä
                //case 0x00f6: // char ö
                //case 0x00fc: // char ü

            default:
                if( uc >= 0x0041 && uc <= 0x005a ) // A-Z
                {
                    //uc += 0x20; // a-z
                    *p_cur_out++ = (*p_cur_in) + 0x20;
                }else{
                    // Copy (multi-byte) char
                    while( d-- ){
                        *p_cur_out++ = *p_cur_in++;
                    }
                }
                break;
        };

        // Shift input pointer by character width (if d was not consumed in switch)
        p_cur_in += d;
    }

    // close (probably shorten) string
    *p_cur_out = '\0';

    return (p_cur_out - p_out); // = sizeof(p_out)
}

char *char_buffer_malloc(
        size_t size)
{
    char *x = (char *)malloc( (size + 1) * sizeof(char));
    if( x ){
        x[size] = '\0';
    }
    return x;
}
