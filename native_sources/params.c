#include <argp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "settings.h"
#include "helper.h"
#include "params.h"
#include "payload.h"

#define SEARCH_NUM_MAX_MATCHES 0x20

#ifndef VERSION
#define VERSION "unknown version"
#endif

/* Begin of argp setup */
const char *argp_program_version = VERSION;
const char *argp_program_bug_address = "<funwithkinect@googlemail.com>";
static char doc[] = "Strip and indexing json files will low memory footprint.";
static char args_doc[] = "--index [-i FILE NAME] [-f FOLDER]" \
                          "\n--search [FILTER ARGS] [-f FOLDER] [-o FILE NAME]" \
                          "\n--payload [ANCHOR,[...]] [-f FOLDER]" \
                          "\n--info";

static struct argp_option options[] = {
    //group 1
    {"index", 'a', 0, OPTION_NO_USAGE,
        "Toggle indexing mode. Require json file with data on stdin or -i argument",
        1},
    {"search", 's', 0, OPTION_NO_USAGE,
        "Toggle search mode. Use existing index file to search matching entries",
        1},
    {"payload", 'p', "ANCHOR(S)", OPTION_NO_USAGE,
        "Toggle fetch of data for entries. Assumes as argument comma separated " \
            "list of 'anchors'. Sufficient values could be extracted from the " \
            "output of the search mode (anchor-value).",
        1},
    {"info", 'I', 0, OPTION_NO_USAGE,
        "Print meta information about current film list.",
        1},
    // group 2
    { "input", 'i', "FILE NAME", OPTION_NO_USAGE,
        "Input from FILE instead of standard input\nTest",
        2},
    {"output", 'o', "FILE NAME", OPTION_NO_USAGE,
        "Output to FILE instead of standard output",
        2},
    {"folder", 'f', "FOLDER", OPTION_NO_USAGE,
        "Output directory for payload and index files",
        2},
    // group 3
    {"title", 't', "TITLE", OPTION_NO_USAGE,
        "For search mode. Restrict output on entries containing this title/topic. Use * to separate multiple keywords.",
        3},
#ifdef WITH_TOPIC_OPTION
    {"topic", 'T', "TOPIC", OPTION_NO_USAGE,
        "For search mode. Like --title, but search in topic string, only. --title value overrides this option.",
        3},
#endif
    {"beginMin", 'b', "TIME MIN", OPTION_NO_USAGE,
        "For search mode. Min begin time of entries in seconds",
        3},
    {"beginMax", 'B', "TIME MAX", OPTION_NO_USAGE,
        "For search mode. Max begin time of entries in seconds",
        3},
    {"dayMin", 'd', "RELATIVE DAY MIN", OPTION_NO_USAGE,
        "For search mode. Day relative to today:=0<=dayMin<dayMax",
        3},
    {"dayMax", 'D', "RELATIVE DAY MAX", OPTION_NO_USAGE,
        "For search mode. Day relative to today:=0<=dayMin<dayMax",
        3},
    {"durationMin", 'm', "DURATION MIN", OPTION_NO_USAGE,
        "For search mode. Minimal duration of entry in seconds",
        3},
    {"durationMax", 'M', "DURATION MAX", OPTION_NO_USAGE,
        "For search mode. Maximal duration of entry in seconds",
        3},
    {"channel", 'c', "CHANNEL NUMBER", OPTION_NO_USAGE,
        "For search mode. Restrict on channel. Use -I to fetch channel " \
            "number for channel name",
        3},
    {"channelName", 'C', "CHANNEL NAME", OPTION_NO_USAGE,
        "For search mode. Restrict on channel. Require exact name. " \
            "Use -I to fetch channel number for channel name",
        3},
    {"num", 'n', "NUM RESULTS", OPTION_NO_USAGE,
        "For search mode. {num,[skip]} Maximal number of results and " \
            "optional second number for number of skiped entries",
        3},
    {"reverse", 'r', 0, OPTION_NO_USAGE,
        "For search mode. Invert order of entries in output",
        3},
    {"diff", 'x', 0, OPTION_NO_USAGE,
        "In combination with --index: The input will be processed as differential update. " \
            "Differential updates (hourly) updates to the (bigger) daily files.\n" \
            "In combination with --search: Search only in differential index.",
        3},
    // group 4
    {"anchor", -1, "ANCHOR(S)", OPTION_NO_USAGE,
        "Synonym for --payload",
        4},
    { 0 }
};

arguments_t default_arguments()
{
    arguments_t arguments;

    arguments.mode = UNDEFINED_MODE;
    arguments.input_file = NULL;
    arguments.output_file = NULL;
    arguments.index_folder = index_folder;
    arguments.title = NULL;
#ifdef WITH_TOPIC_OPTION
    arguments.topic = NULL;
#endif
    arguments.beginMin = -1;
    arguments.beginMax = -1;
    arguments.dayMin = -1;
    arguments.dayMax = -1;
    arguments.durationMin = -1;
    arguments.durationMax = -1;
    arguments.channelNr = NO_CHANNEL; // 0 - empty/unset channel name
    arguments.channelName = NULL;
    arguments.payload_anchors = NULL;
    arguments.payload_anchors_len = 0;
    arguments.max_num_results = SEARCH_NUM_MAX_MATCHES;
    arguments.skiped_num_results = 0;
    arguments.reversed_results = 0;
    arguments.diff_update = 0;

    return arguments;
}

void clear_arguments(arguments_t *p_arguments)
{
    Free((p_arguments->payload_anchors));
    Free(p_arguments->channelName);
}


/* If a and b != 'unset' sort them.
 * If both 'unset', do not changes them
 * Otherwise replace the unsetted element with the extremal
 * value. */
void normalize_range(int min, int max, int unset_val,
        int *a, int *b){
#define SWAP(A, B) t=(A); (A)=(B); (B)=t
    int t;
    if( *a != unset_val && *b != unset_val && *a > *b){
        SWAP(*a, *b); // [b, a]
    }else if( *a == unset_val && *b == unset_val){
        // [-1, -1]
    }else if( *a == unset_val){
        *a = min; // [min, b]
    }else if( *b == unset_val){
        *b = max; // [a, max]
    }
}

void normalize_args(
        arguments_t *p_arguments)
{
    if( p_arguments->input_file &&
            0 == strcmp("-", p_arguments->input_file) ){
        p_arguments->input_file = NULL;
    }
    if( p_arguments->output_file &&
            0 == strcmp("-", p_arguments->output_file) ){
        p_arguments->output_file = NULL;
    }

    normalize_range(0, INT_MAX, -1,
            &p_arguments->beginMin, &p_arguments->beginMax);
    normalize_range(0, INT_MAX, -1,
            &p_arguments->durationMin, &p_arguments->durationMax);
    normalize_range(0, MAX_DAY_OFFSET, -1,
            &p_arguments->dayMin, &p_arguments->dayMax);

    if( p_arguments->channelName != NULL && p_arguments->channelNr != NO_CHANNEL ){
        fprintf(stderr, "Channel number and channel string both set. " \
                "Named argument will be prefered.\n");
    }

    sort_payload_anchors(p_arguments);

    if( p_arguments->mode != INDEXING_MODE && p_arguments->mode != SEARCH_MODE ){
        p_arguments->diff_update = 0;
    }

    // Omit very high values for --num input because its sum
    // directy affects the size of an array.
#define MAX_ALLOWED_NUMBER_OF_RESULTS 300000
    if( p_arguments->skiped_num_results > MAX_ALLOWED_NUMBER_OF_RESULTS ){
        fprintf(stderr, "Number of skiped values (%u) to high! Search aborted\n",
                p_arguments->skiped_num_results);
        p_arguments->skiped_num_results = 0;
        p_arguments->max_num_results = 0;
    }
    if( p_arguments->skiped_num_results + p_arguments->max_num_results
            > MAX_ALLOWED_NUMBER_OF_RESULTS ){
        assert(MAX_ALLOWED_NUMBER_OF_RESULTS > p_arguments->skiped_num_results);
        p_arguments->max_num_results = MAX_ALLOWED_NUMBER_OF_RESULTS - p_arguments->skiped_num_results;
        fprintf(stderr, "Number of requested values reduced on %u!\n",
                p_arguments->skiped_num_results
               );
    }
}

void convertInt(const char *in, int *out)
{
    const char format[] = "%i%c";
    char unit;
    int backup = *out;
    int status = sscanf(in, format, out, &unit);
    if( status < 1 || status > 2 ){ /* 0 or EOF */
        fprintf(stderr, "Conversion of '%s' failed.\n", in);
        *out = backup;
    }else if(status == 2){
        // Interpret char after number as unit.
        switch(unit){
            case 'H':
            case 'h':
                *out *= 3600;
                break;
            case 'M':
            case 'm':
                *out *= 60;
                break;
            case 'S':
            case 's':
            default:
                break;
        }
    }
}

/* Convert list of ints into array */
void parseInts(
        const char *in,
        uint32_t u8_sep_char,
        uint32_t *size_results,
        uint32_t **pp_results)
//        arguments_t *p_arguments)
{
    Free(*pp_results);

    // 1. Count comma to estimate requirred array length.
    char *p_c = (char *)in;
    size_t len = (strlen(in)>0)?1:0;
    while( NULL != (p_c = strchr(p_c, u8_sep_char)) ){
        ++len;
        ++p_c;
    }
    if( len == 0 ){
        return;
    }

    uint32_t *new_anchors = calloc(len, sizeof(*new_anchors));
    size_t i = 0;

    // 2. Parse until error/end.
    unsigned long int parsed_number;
    const char *_in=in;
    char *_end;
    errno = 0;
    parsed_number = strtoul(_in, &_end, 10);
    while( _end > _in ){
        if ((errno == ERANGE && parsed_number == ULONG_MAX )
                || (errno != 0 && parsed_number == 0)) {
            fprintf(stderr, "An error occurred during " \
                    "the parsing of the --info anchors.");
            Free(new_anchors);
            break;
        }

        new_anchors[i] = parsed_number;
        ++i;

        if( *_end != u8_sep_char){
            // go forward to next separator (prevents i>=len).
            if( NULL == (_end = strchr(_end, u8_sep_char))){
                break;
            }
        }
        if( *_end == '\0' ) break;

        _in = _end+1;
        parsed_number = strtoul(_in, &_end, 10);
    }

    *size_results = i;
    *pp_results = new_anchors;
}

/* Sort entries by file number to avoid re-opening
 * of the same file. */
void sort_payload_anchors(
        arguments_t *p_arguments)
{
    if( p_arguments->payload_anchors == NULL ){
        return;
    }
    // Disabled
    return;

    qsort(p_arguments->payload_anchors,
            p_arguments->payload_anchors_len,
            sizeof(uint32_t), payload_anchor_compar);
}

void fprint_args(FILE *stream, arguments_t *args){
    switch( args->mode ){
        case SEARCH_MODE:
            fprintf(stream, "Mode: Search entry\n");
            break;
        case INDEXING_MODE:
            fprintf(stream, "Mode: Indexing entries\n");
            break;
        case PAYLOAD_MODE:
            fprintf(stream, "Mode: Get data for entry.\n");
            break;
        case INFO_MODE:
            fprintf(stream, "Mode: General infos\n");
            break;
        default:
            fprintf(stream, "Mode: Undefined\n");
            break;
    }

    // For all modes
    fprintf(stream, "Payload/Index folder: %s\n", args->index_folder?args->index_folder:"???");

    if( args->mode == INDEXING_MODE ){
        fprintf(stream, "Input file: %s\n",
                args->input_file?args->input_file:"-");
        fprintf(stream, "%s\n",
                (args->diff_update?"Input stored as diff-update."
                 :"Input stored as full update. Existing diff files will be removed.")
               );
    }

    if( args->mode == SEARCH_MODE ){
        fprintf(stream, "Output file: %s\n", args->output_file?args->output_file:"-");
        fprintf(stream, "Title/Topic: %s\n", args->title?args->title:"Not set");
#ifdef WITH_TOPIC_OPTION
        if( !args->title ){
            fprintf(stream, "Topic: %s\n", args->topic?args->topic:"Not set");
        }
#endif
        fprintf(stream, "Begin range: [%i, %i]\n", args->beginMin, args->beginMax);
        if( args->dayMax < MAX_DAY_OFFSET ){
            fprintf(stream, "Day range: [%i, %i]\n", args->dayMin, args->dayMax);
        }else{
            fprintf(stream, "Day range: [%i, max]\n", args->dayMin);
        }
        fprintf(stream, "Duration range: [%i, %i]\n", args->durationMin, args->durationMax);
        fprintf(stream, "Channel number: %i\t", args->channelNr);
        fprintf(stream, "Channel name: %s\n", args->channelName);
        fprintf(stream, "Output dims: %u,%u (%s)\n",
                args->max_num_results,
                args->skiped_num_results,
                (args->reversed_results?"backward":"forward")
               );
    }

    if( args->mode == PAYLOAD_MODE ){
        if( args->payload_anchors != NULL ){
            fprintf(stream, "Payload anchors: ");
            int i;
            for(i=0; i<args->payload_anchors_len; ++i){
                fprintf(stream, "%s%i", (i==0)?"":", ",
                        args->payload_anchors[i]);
            }
            fprintf(stream, "\n");
        }
    }
    if( args->mode == INFO_MODE ){
        // None
    }
}

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    arguments_t *p_arguments = state->input;
    switch (key) {
        case 'a': p_arguments->mode = INDEXING_MODE; break;
        case 's': p_arguments->mode = SEARCH_MODE; break;
        case 'I': p_arguments->mode = INFO_MODE; break;
        case 'i': p_arguments->input_file = arg; break;
        case 'o': p_arguments->output_file = arg; break;
        case 'f': p_arguments->index_folder = arg; break;
        case 't': p_arguments->title = arg; break;
        case 'T': p_arguments->topic = arg; break;
        case 'b': convertInt(arg, &p_arguments->beginMin); break;
        case 'B': convertInt(arg, &p_arguments->beginMax); break;
        case 'd': convertInt(arg, &p_arguments->dayMin); break;
        case 'D': convertInt(arg, &p_arguments->dayMax); break;
        case 'm': convertInt(arg, &p_arguments->durationMin); break;
        case 'M': convertInt(arg, &p_arguments->durationMax); break;
        case 'c': convertInt(arg, &p_arguments->channelNr); break;
        case 'C': {
                      size_t l = strlen(arg);
                      Free(p_arguments->channelName);
                      p_arguments->channelName = char_buffer_malloc(l);
                      memcpy(p_arguments->channelName, arg, l);
                      break;
                  }
        case -1:
        case 'p': {
                      p_arguments->mode = PAYLOAD_MODE;
                      parseInts(arg, ',',
                              &p_arguments->payload_anchors_len,
                              &p_arguments->payload_anchors);
                      break;
                  }
        case 'n': {
                      uint32_t tmp_len = 0;
                      uint32_t *tmp = NULL;
                      parseInts(arg, ',', &tmp_len, &tmp);
                      if( tmp_len>1 ){
                          p_arguments->skiped_num_results = tmp[1];
                      }
                      if( tmp_len>0 ){
                          p_arguments->max_num_results = tmp[0];
                      }
                      Free(tmp);
                      break;
                  }
        case 'r': {
                      p_arguments->reversed_results = 1;
                      break;
                  }
        case 'x': {
                      p_arguments->diff_update = 1;
                      break;
                  }
        case ARGP_KEY_ARG:
                  return 0;
        case ARGP_KEY_END:
                  if( p_arguments->mode == UNDEFINED_MODE ){
                      // Explicit selection of mode required.
                      fprintf(stderr, "Missing mode option: --index, --search, --payload, or --info required\n");
                      argp_usage (state);
                      return ARGP_KEY_ERROR;
                  }
                  return 0;
        default: return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };
/* End of argp setup */

arguments_t handle_arguments(
        int argc,
        char *argv[])
{
    arguments_t arguments = default_arguments();
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    normalize_args(&arguments);

    return arguments;
}

#ifdef STANDALONE
int main(
        int argc,
        char *argv[])
{
    arguments_t arguments = handle_arguments(argc, argv);

    fprint_args(stderr, &arguments);

    clear_arguments(&arguments);
    return 0;
}

#endif
