#include <stdlib.h>
#include <stdio.h>

#include "helper.h"
#include "utf8.h"

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
    Free(*pp_out);

    if( p_in == NULL ){
        *pp_out = NULL;
        return 0;
    }

    size_t len_in = strlen(p_in);
    if( len_in == 0 ){
        *pp_out = NULL;
        return 0;
    }

    /*uint8_t * const*/
    unsigned char * const p_out =  (unsigned char *) malloc( (1 + len_in) * sizeof(unsigned char) );
    *(p_out + len_in) = '\0';
    *pp_out = (char *)p_out;

    const char *p_cur_in = p_in; // current char
    unsigned char *p_cur_out = p_out; // current output position

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
#if SPLIT_CHAR != '|'
            case SPLIT_CHAR:
#endif
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

#ifdef REPLACE_COMPOSITE_CHARS
                             if( uc >= 0xC0 ){
                                 /* Smalest value in switch is 0xC0 */
                                 switch(uc) {
                                     case 0x00C0: /* À => a */
                                     case 0x00C1: /* Á => a */
                                     case 0x00C2: /* Â => a */
                                     case 0x00C3: /* Ã => a */
                                     case 0x00C5: /* Å => a */
                                     case 0x00E0: /* à => a */
                                     case 0x00E1: /* á => a */
                                     case 0x00E2: /* â => a */
                                     case 0x00E3: /* ã => a */
                                     case 0x00E5: /* å => a */
                                     case 0x0100: /* Ā => a */
                                     case 0x0101: /* ā => a */
                                     case 0x0102: /* Ă => a */
                                     case 0x0103: /* ă => a */
                                     case 0x01CD: /* Ǎ => a */
                                     case 0x01CE: /* ǎ => a */
                                     case 0x0226: /* Ȧ => a */
                                     case 0x0227: /* ȧ => a */
                                     case 0x023A: /* Ⱥ => a */
                                     case 0x1EA0: /* Ạ => a */
                                     case 0x1EA1: /* ạ => a */
                                     case 0x2C65: /* ⱥ => a */
                                         { *p_cur_out++ =  97; break;}
                                     case 0x0180: /* ƀ => b */
                                     case 0x0243: /* Ƀ => b */
                                     case 0x1E02: /* Ḃ => b */
                                     case 0x1E03: /* ḃ => b */
                                     case 0x1E04: /* Ḅ => b */
                                     case 0x1E05: /* ḅ => b */
                                         { *p_cur_out++ =  98; break;}
                                     case 0x00C7: /* Ç => c */
                                     case 0x00E7: /* ç => c */
                                     case 0x0106: /* Ć => c */
                                     case 0x0107: /* ć => c */
                                     case 0x0108: /* Ĉ => c */
                                     case 0x0109: /* ĉ => c */
                                     case 0x010A: /* Ċ => c */
                                     case 0x010B: /* ċ => c */
                                     case 0x010C: /* Č => c */
                                     case 0x010D: /* č => c */
                                     case 0x023B: /* Ȼ => c */
                                     case 0x023C: /* ȼ => c */
                                         { *p_cur_out++ =  99; break;}
                                     case 0x010E: /* Ď => d */
                                     case 0x010F: /* ď => d */
                                     case 0x0110: /* Đ => d */
                                     case 0x0111: /* đ => d */
                                     case 0x1E0A: /* Ḋ => d */
                                     case 0x1E0B: /* ḋ => d */
                                     case 0x1E0C: /* Ḍ => d */
                                     case 0x1E0D: /* ḍ => d */
                                     case 0x1E10: /* Ḑ => d */
                                     case 0x1E11: /* ḑ => d */
                                         { *p_cur_out++ = 100; break;}
                                     case 0x00C8: /* È => e */
                                     case 0x00C9: /* É => e */
                                     case 0x00CA: /* Ê => e */
                                     case 0x00CB: /* Ë => e */
                                     case 0x00E8: /* è => e */
                                     case 0x00E9: /* é => e */
                                     case 0x00EA: /* ê => e */
                                     case 0x00EB: /* ë => e */
                                     case 0x0112: /* Ē => e */
                                     case 0x0113: /* ē => e */
                                     case 0x0114: /* Ĕ => e */
                                     case 0x0115: /* ĕ => e */
                                     case 0x0116: /* Ė => e */
                                     case 0x0117: /* ė => e */
                                     case 0x011A: /* Ě => e */
                                     case 0x011B: /* ě => e */
                                     case 0x0228: /* Ȩ => e */
                                     case 0x0229: /* ȩ => e */
                                     case 0x0246: /* Ɇ => e */
                                     case 0x0247: /* ɇ => e */
                                     case 0x1EB8: /* Ẹ => e */
                                     case 0x1EB9: /* ẹ => e */
                                     case 0x1EBC: /* Ẽ => e */
                                     case 0x1EBD: /* ẽ => e */
                                         { *p_cur_out++ = 101; break;}
                                     case 0x1E1E: /* Ḟ => f */
                                     case 0x1E1F: /* ḟ => f */
                                         { *p_cur_out++ = 102; break;}
                                     case 0x011C: /* Ĝ => g */
                                     case 0x011D: /* ĝ => g */
                                     case 0x011E: /* Ğ => g */
                                     case 0x011F: /* ğ => g */
                                     case 0x0120: /* Ġ => g */
                                     case 0x0121: /* ġ => g */
                                     case 0x0122: /* Ģ => g */
                                     case 0x0123: /* ģ => g */
                                     case 0x01E4: /* Ǥ => g */
                                     case 0x01E5: /* ǥ => g */
                                     case 0x01E6: /* Ǧ => g */
                                     case 0x01E7: /* ǧ => g */
                                     case 0x01F4: /* Ǵ => g */
                                     case 0x01F5: /* ǵ => g */
                                     case 0x1E20: /* Ḡ => g */
                                     case 0x1E21: /* ḡ => g */
                                         { *p_cur_out++ = 103; break;}
                                     case 0x0124: /* Ĥ => h */
                                     case 0x0125: /* ĥ => h */
                                     case 0x0126: /* Ħ => h */
                                     case 0x0127: /* ħ => h */
                                     case 0x021E: /* Ȟ => h */
                                     case 0x021F: /* ȟ => h */
                                     case 0x1E22: /* Ḣ => h */
                                     case 0x1E23: /* ḣ => h */
                                     case 0x1E24: /* Ḥ => h */
                                     case 0x1E25: /* ḥ => h */
                                     case 0x1E26: /* Ḧ => h */
                                     case 0x1E27: /* ḧ => h */
                                     case 0x1E28: /* Ḩ => h */
                                     case 0x1E29: /* ḩ => h */
                                         { *p_cur_out++ = 104; break;}
                                     case 0x00CC: /* Ì => i */
                                     case 0x00CD: /* Í => i */
                                     case 0x00CE: /* Î => i */
                                     case 0x00CF: /* Ï => i */
                                     case 0x00EC: /* ì => i */
                                     case 0x00ED: /* í => i */
                                     case 0x00EE: /* î => i */
                                     case 0x00EF: /* ï => i */
                                     case 0x0128: /* Ĩ => i */
                                     case 0x0129: /* ĩ => i */
                                     case 0x012A: /* Ī => i */
                                     case 0x012B: /* ī => i */
                                     case 0x012C: /* Ĭ => i */
                                     case 0x012D: /* ĭ => i */
                                     case 0x0130: /* İ => i */
                                     case 0x0131: /* ı => i */
                                     case 0x0197: /* Ɨ => i */
                                     case 0x01CF: /* Ǐ => i */
                                     case 0x01D0: /* ǐ => i */
                                     case 0x0268: /* ɨ => i */
                                     case 0x1ECA: /* Ị => i */
                                     case 0x1ECB: /* ị => i */
                                         { *p_cur_out++ = 105; break;}
                                     case 0x0134: /* Ĵ => j */
                                     case 0x0135: /* ĵ => j */
                                     case 0x01F0: /* ǰ => j */
                                     case 0x0237: /* ȷ => j */
                                     case 0x0248: /* Ɉ => j */
                                     case 0x0249: /* ɉ => j */
                                         { *p_cur_out++ = 106; break;}
                                     case 0x0136: /* Ķ => k */
                                     case 0x0137: /* ķ => k */
                                     case 0x01E8: /* Ǩ => k */
                                     case 0x01E9: /* ǩ => k */
                                     case 0x1E30: /* Ḱ => k */
                                     case 0x1E31: /* ḱ => k */
                                     case 0x1E32: /* Ḳ => k */
                                     case 0x1E33: /* ḳ => k */
                                         { *p_cur_out++ = 107; break;}
                                     case 0x0139: /* Ĺ => l */
                                     case 0x013A: /* ĺ => l */
                                     case 0x013B: /* Ļ => l */
                                     case 0x013C: /* ļ => l */
                                     case 0x013D: /* Ľ => l */
                                     case 0x013E: /* ľ => l */
                                     case 0x013F: /* Ŀ => l */
                                     case 0x0140: /* ŀ => l */
                                     case 0x0141: /* Ł => l */
                                     case 0x0142: /* ł => l */
                                     case 0x1E36: /* Ḷ => l */
                                     case 0x1E37: /* ḷ => l */
                                         { *p_cur_out++ = 108; break;}
                                     case 0x1E3E: /* Ḿ => m */
                                     case 0x1E3F: /* ḿ => m */
                                     case 0x1E40: /* Ṁ => m */
                                     case 0x1E41: /* ṁ => m */
                                     case 0x1E42: /* Ṃ => m */
                                     case 0x1E43: /* ṃ => m */
                                         { *p_cur_out++ = 109; break;}
                                     case 0x00D1: /* Ñ => n */
                                     case 0x00F1: /* ñ => n */
                                     case 0x0143: /* Ń => n */
                                     case 0x0144: /* ń => n */
                                     case 0x0145: /* Ņ => n */
                                     case 0x0146: /* ņ => n */
                                     case 0x0147: /* Ň => n */
                                     case 0x0148: /* ň => n */
                                     case 0x01F8: /* Ǹ => n */
                                     case 0x01F9: /* ǹ => n */
                                     case 0x1E44: /* Ṅ => n */
                                     case 0x1E45: /* ṅ => n */
                                     case 0x1E46: /* Ṇ => n */
                                     case 0x1E47: /* ṇ => n */
                                         { *p_cur_out++ = 110; break;}
                                     case 0x00D4: /* Ô => o */
                                     case 0x00D5: /* Õ => o */
                                     case 0x00D8: /* Ø => o */
                                     case 0x00F2: /* ò => o */
                                     case 0x00F3: /* ó => o */
                                     case 0x00F4: /* ô => o */
                                     case 0x00F5: /* õ => o */
                                     case 0x00F8: /* ø => o */
                                     case 0x014C: /* Ō => o */
                                     case 0x014D: /* ō => o */
                                     case 0x014E: /* Ŏ => o */
                                     case 0x014F: /* ŏ => o */
                                     case 0x0150: /* Ő => o */
                                     case 0x0151: /* ő => o */
                                     case 0x01D1: /* Ǒ => o */
                                     case 0x01D2: /* ǒ => o */
                                     case 0x022E: /* Ȯ => o */
                                     case 0x022F: /* ȯ => o */
                                     case 0x1ECC: /* Ọ => o */
                                     case 0x1ECD: /* ọ => o */
                                         { *p_cur_out++ = 111; break;}
                                     case 0x1D7D: /* ᵽ => p */
                                     case 0x1E54: /* Ṕ => p */
                                     case 0x1E55: /* ṕ => p */
                                     case 0x1E56: /* Ṗ => p */
                                     case 0x1E57: /* ṗ => p */
                                     case 0x2C63: /* Ᵽ => p */
                                         { *p_cur_out++ = 112; break;}
                                     case 0x0154: /* Ŕ => r */
                                     case 0x0155: /* ŕ => r */
                                     case 0x0156: /* Ŗ => r */
                                     case 0x0157: /* ŗ => r */
                                     case 0x0158: /* Ř => r */
                                     case 0x0159: /* ř => r */
                                     case 0x024C: /* Ɍ => r */
                                     case 0x024D: /* ɍ => r */
                                     case 0x1E58: /* Ṙ => r */
                                     case 0x1E59: /* ṙ => r */
                                     case 0x1E5A: /* Ṛ => r */
                                     case 0x1E5B: /* ṛ => r */
                                         { *p_cur_out++ = 114; break;}
                                     case 0x015A: /* Ś => s */
                                     case 0x015B: /* ś => s */
                                     case 0x015C: /* Ŝ => s */
                                     case 0x015D: /* ŝ => s */
                                     case 0x015E: /* Ş => s */
                                     case 0x015F: /* ş => s */
                                     case 0x0160: /* Š => s */
                                     case 0x0161: /* š => s */
                                     case 0x1E60: /* Ṡ => s */
                                     case 0x1E61: /* ṡ => s */
                                     case 0x1E62: /* Ṣ => s */
                                     case 0x1E63: /* ṣ => s */
                                         { *p_cur_out++ = 115; break;}
                                     case 0x0162: /* Ţ => t */
                                     case 0x0163: /* ţ => t */
                                     case 0x0164: /* Ť => t */
                                     case 0x0165: /* ť => t */
                                     case 0x0166: /* Ŧ => t */
                                     case 0x0167: /* ŧ => t */
                                     case 0x1E6A: /* Ṫ => t */
                                     case 0x1E6B: /* ṫ => t */
                                     case 0x1E6C: /* Ṭ => t */
                                     case 0x1E6D: /* ṭ => t */
                                     case 0x1E97: /* ẗ => t */
                                         { *p_cur_out++ = 116; break;}
                                     case 0x00D9: /* Ù => u */
                                     case 0x00DA: /* Ú => u */
                                     case 0x00DB: /* Û => u */
                                     case 0x00F9: /* ù => u */
                                     case 0x00FA: /* ú => u */
                                     case 0x00FB: /* û => u */
                                     case 0x0168: /* Ũ => u */
                                     case 0x0169: /* ũ => u */
                                     case 0x016A: /* Ū => u */
                                     case 0x016B: /* ū => u */
                                     case 0x016C: /* Ŭ => u */
                                     case 0x016D: /* ŭ => u */
                                     case 0x016E: /* Ů => u */
                                     case 0x016F: /* ů => u */
                                     case 0x0170: /* Ű => u */
                                     case 0x0171: /* ű => u */
                                     case 0x01D3: /* Ǔ => u */
                                     case 0x01D4: /* ǔ => u */
                                     case 0x01D5: /* Ǖ => u */
                                     case 0x01DA: /* ǚ => u */
                                     case 0x01DB: /* Ǜ => u */
                                     case 0x01DC: /* ǜ => u */
                                     case 0x0244: /* Ʉ => u */
                                     case 0x0289: /* ʉ => u */
                                     case 0x1EE4: /* Ụ => u */
                                     case 0x1EE5: /* ụ => u */
                                         { *p_cur_out++ = 117; break;}
                                     case 0x1E7C: /* Ṽ => v */
                                     case 0x1E7D: /* ṽ => v */
                                     case 0x1E7E: /* Ṿ => v */
                                     case 0x1E7F: /* ṿ => v */
                                         { *p_cur_out++ = 118; break;}
                                     case 0x0174: /* Ŵ => w */
                                     case 0x0175: /* ŵ => w */
                                     case 0x1E80: /* Ẁ => w */
                                     case 0x1E81: /* ẁ => w */
                                     case 0x1E82: /* Ẃ => w */
                                     case 0x1E83: /* ẃ => w */
                                     case 0x1E84: /* Ẅ => w */
                                     case 0x1E85: /* ẅ => w */
                                     case 0x1E86: /* Ẇ => w */
                                     case 0x1E87: /* ẇ => w */
                                     case 0x1E88: /* Ẉ => w */
                                     case 0x1E89: /* ẉ => w */
                                     case 0x1E98: /* ẘ => w */
                                         { *p_cur_out++ = 119; break;}
                                     case 0x1E8A: /* Ẋ => x */
                                     case 0x1E8B: /* ẋ => x */
                                     case 0x1E8C: /* Ẍ => x */
                                     case 0x1E8D: /* ẍ => x */
                                         { *p_cur_out++ = 120; break;}
                                     case 0x00DD: /* Ý => y */
                                     case 0x00FD: /* ý => y */
                                     case 0x00FF: /* ÿ => y */
                                     case 0x0176: /* Ŷ => y */
                                     case 0x0177: /* ŷ => y */
                                     case 0x0178: /* Ÿ => y */
                                     case 0x0232: /* Ȳ => y */
                                     case 0x0233: /* ȳ => y */
                                     case 0x024E: /* Ɏ => y */
                                     case 0x024F: /* ɏ => y */
                                     case 0x1E8E: /* Ẏ => y */
                                     case 0x1E8F: /* ẏ => y */
                                     case 0x1E99: /* ẙ => y */
                                     case 0x1EF2: /* Ỳ => y */
                                     case 0x1EF3: /* ỳ => y */
                                     case 0x1EF4: /* Ỵ => y */
                                     case 0x1EF5: /* ỵ => y */
                                     case 0x1EF8: /* Ỹ => y */
                                     case 0x1EF9: /* ỹ => y */
                                         { *p_cur_out++ = 121; break;}
                                     case 0x0179: /* Ź => z */
                                     case 0x017A: /* ź => z */
                                     case 0x017B: /* Ż => z */
                                     case 0x017C: /* ż => z */
                                     case 0x017D: /* Ž => z */
                                     case 0x017E: /* ž => z */
                                     case 0x01B5: /* Ƶ => z */
                                     case 0x01B6: /* ƶ => z */
                                     case 0x1E90: /* Ẑ => z */
                                     case 0x1E91: /* ẑ => z */
                                     case 0x1E92: /* Ẓ => z */
                                     case 0x1E93: /* ẓ => z */
                                         { *p_cur_out++ = 122; break;}
                                     case 0x01D6: /* ǖ => ü */
                                     case 0x01D7: /* Ǘ => ü */
                                     case 0x01D8: /* ǘ => ü */
                                     case 0x01D9: /* Ǚ => ü */
                                         { *p_cur_out++ = 195; *p_cur_out++ = 188; break;}
                                     default:
                                         // Copy (multi-byte) char
                                         while( d-- ){
                                             *p_cur_out++ = *p_cur_in++;
                                         }
                                 }
                             }else{
#endif
                                 // Copy (multi-byte) char
                                 while( d-- ){
                                     *p_cur_out++ = *p_cur_in++;
                                 }
#ifdef REPLACE_COMPOSITE_CHARS
                             }
#endif
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

void charcpy(
        char **pp_dest,
        size_t *p_dest_len, /* >= strlen(dest) */
        const char * const p_source,
        const size_t source_len)
{
    char *dest = *pp_dest;
    if( source_len > *p_dest_len ){
        free(dest);
        *p_dest_len = source_len + 16;  // A few more bytes to avoid reallocations later; optional.
        dest = char_buffer_malloc(*p_dest_len);
        *pp_dest = dest;
    }
    if( dest ){
        memcpy(dest, p_source, source_len);
        dest[source_len] = '\0';
    }
}
