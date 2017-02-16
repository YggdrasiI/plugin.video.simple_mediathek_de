#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "helper.h"
#include "channels.h"
#include "parser.h"

/* Maximal length of serialized data */
#define CHANNEL_MAX_STRING_LEN 0x100000

// Cache variables of get_channel_name() function.
typedef struct {
    const char *name;
    int number;
} it_get_channel_name_item_t;
static it_get_channel_name_item_t  _get_channel_name_cache  = {NULL, -1};

channels_ws_t channels_ws_create()
{
  //channels_ws_t *ws = (channels_ws_t *) malloc(sizeof(channels_ws_t)); // leaks..
  channels_ws_t ws;

  ws.map = hashmap_new();

  // Add channel for empty name to bind that to channel 0.
  int chan0 = get_channel_number(&ws, "", 1);
  assert( chan0 == 0);
  _unused(chan0);

  return ws;
}

void channels_ws_destroy(channels_ws_t *p_ws)
{
  /* Free all of the values we allocated and remove them from the map */
  data_struct_t* channel_value;
  while( MAP_OK ==
      hashmap_get_one(p_ws->map, (void **)&channel_value, 1))
  {
    free(channel_value);
  };

  /* Now, destroy the map */
  hashmap_free(p_ws->map);
}

int add_channel(
    channels_ws_t *p_channels,
    const char *chan)
{
  int error;
  data_struct_t* channel_value = malloc(sizeof(data_struct_t));

  snprintf(channel_value->key_string, KEY_MAX_LENGTH,
      "%s", chan);
  channel_value->number = hashmap_length(p_channels->map);

  error = hashmap_put(p_channels->map, channel_value->key_string, channel_value);
  assert(error==MAP_OK);
  if( error != MAP_OK ){
      fprintf(stderr, "Adding of channel \"%s\" failed! Hashmap error: %i\n.",
              chan, error);
  }

  // Reset cache value of get_channel_name() function
  _get_channel_name_cache.number  = -1;

  return channel_value->number;
}

int get_channel_number(
    channels_ws_t *p_channels,
    const char *chan,
    int add_missing)
{
  int error;
  data_struct_t* channel_value;

  error = hashmap_get(p_channels->map, (char *)chan, (void**)(&channel_value));
  if( error == MAP_MISSING ){
      if(add_missing){
          return add_channel(p_channels, chan);
      }
  }else{
      assert(error==MAP_OK);
      return channel_value->number;
  }
  // not found
  return -1;
}

int it_get_channel_name(any_t item, any_t data){
  data_struct_t* p_channel_val = (data_struct_t *)data;
  it_get_channel_name_item_t *p = (it_get_channel_name_item_t *)item;
  if( p_channel_val->number == p->number ){
      p->name = p_channel_val->key_string;
      return 1; // Abort
  }
  return MAP_OK;
}

const char *get_channel_name(
        channels_ws_t *p_channels,
        int channel_number)
{
    if( channel_number == _get_channel_name_cache.number ){
        return _get_channel_name_cache.name;
    }

    // Linear search
    _get_channel_name_cache.name = NULL;
    _get_channel_name_cache.number = channel_number;
    hashmap_iterate(p_channels->map, &it_get_channel_name, (any_t)&_get_channel_name_cache);

    return _get_channel_name_cache.name;
}


//typedef int (*PFany)(any_t, any_t);
int it_get_num_bytes_to_write(any_t item, any_t data){
  data_struct_t* p_channel_val = (data_struct_t *)data;
  size_t *p_s = (size_t *)item;
  if( p_channel_val->number > 0 ){
    *p_s += sizeof(int32_t) // channel_nr
      + strlen(p_channel_val->key_string) // key_string
      + sizeof(char); // null terminal
  }
  return MAP_OK;
}

int it_write(
    any_t item,
    any_t data)
{
  data_struct_t* p_channel_val = (data_struct_t *)data;
  char_buffer_t *p_buf = (char_buffer_t *)item;

  // Skip channel 0 (empty string)
  if( p_channel_val->number > 0 ){
    assert(p_buf->used < p_buf->len);
    *((int32_t *)(p_buf->p + p_buf->used)) = (int32_t) p_channel_val->number;
    p_buf->used += sizeof(int32_t);

    assert(p_buf->used + 2 < p_buf->len); // char + \0 assumed.
    int ret = snprintf(p_buf->p + p_buf->used,
        p_buf->len - p_buf->used,
        "%s", p_channel_val->key_string);
    assert( ret >= 0 );
    ret += 1; // + 1 for \0, which is excluded in counting of snprintf

    //DEBUG( fprintf(stderr, "%i\t%i\t%s\t%i\n", ret, (int)p_buf->used,
    //      p_channel_val->key_string, (int) p_channel_val->number); );
    assert( ret >= 1 );
    assert( ret + p_buf->used <= p_buf->len);
    p_buf->used += ret;
  }
  return MAP_OK;
}

int32_t write_channel_list(
    int fd,
    channels_ws_t *p_ws)
{
  assert( p_ws && p_ws->map);

  // First, get length to write, excluding space for X
  size_t bytes_to_write = 0;
  hashmap_iterate(p_ws->map, &it_get_num_bytes_to_write,
      (any_t)&bytes_to_write);

  if( bytes_to_write > CHANNEL_MAX_STRING_LEN ){
      // Expected size to high. Abort writing/allocating
      // of big buffer.
      fprintf(stderr, "Skip writing of channel list. Length %i seems too big!\n",
              (int) bytes_to_write);
      int32_t X = 0;
      ssize_t w = write(fd, &X, sizeof(int32_t));
      if( (ssize_t) sizeof(int32_t) == w ){
          return 0; // w-4
      }
      return -1;
  }

  // Allocate space, including space for X
  char_buffer_t buf_out = {
      bytes_to_write + sizeof(uint32_t), 0,
      (char *)malloc( bytes_to_write + sizeof(uint32_t))
  };

  // Write into buffer
  *((uint32_t *)(buf_out.p + buf_out.used)) = (uint32_t) bytes_to_write;
  buf_out.used += sizeof(uint32_t);
  hashmap_iterate(p_ws->map, &it_write, (any_t)&buf_out);
  assert( buf_out.used == buf_out.len );

  //Flush buffer
  ssize_t w = write(fd, buf_out.p, buf_out.used);
  assert( (ssize_t) buf_out.used == w);

  // Clean up
  free(buf_out.p);
  buf_out.p = NULL;

  if( (ssize_t) buf_out.used != w){
      return (ssize_t) -1;
  }

  return bytes_to_write; // = w-4
}

/* Counterpart of write_channel_list
*/
int32_t read_channel_list(
    int fd,
    channels_ws_t *p_ws)
{
  assert( hashmap_length(p_ws->map) == 1); //only empty channel from constructor.
  int32_t bytes_to_read;
  ssize_t n = read(fd, &bytes_to_read, sizeof(int32_t));
  assert( n == sizeof(int32_t));
  _unused(n);

  if( bytes_to_read < 0 || bytes_to_read > CHANNEL_MAX_STRING_LEN){
      fprintf(stderr, "Skip reading of channel list. " \
              "Size of data (%i) not positive or to big.\n", bytes_to_read);
      return -1;
  }

  char_buffer_t buf_in = {
      bytes_to_read, 0,
      (char *)malloc( bytes_to_read + 8)
  };

  n = read(fd, buf_in.p, bytes_to_read);
  assert( n == buf_in.len);
  buf_in.used = buf_in.len;

  // Interpret data if all input is consumed
  size_t pos = 0;
  data_struct_t *p_channel_val = (data_struct_t *)malloc(sizeof(data_struct_t));
  while( pos<buf_in.used && p_channel_val ){
    p_channel_val->number = *((int32_t *)(buf_in.p+pos));
    pos+=sizeof(int32_t);
    int n2 = snprintf(&p_channel_val->key_string[0], KEY_MAX_LENGTH, "%s", buf_in.p + pos);
    pos += n2 + 1; // +1 for \0
    assert( n2 >= 0 );
    //hashmap_add(p_ws

    //check if channel name already in usage
    data_struct_t *b;
    hashmap_get(p_ws->map, p_channel_val->key_string, (void **)&b);
    if( b != NULL ){
      fprintf(stderr, "Can not read channel. Key already exists.\n");
      // Update channel_nr of existing entry(?)
    }else{
      //DEBUG( fprintf(stderr, "Channel %i: %s\n",
      //    (int)p_channel_val->number, p_channel_val->key_string); );

      // Insert new entry and allocate new location
      if( MAP_OK == hashmap_put(p_ws->map, p_channel_val->key_string, p_channel_val)){
        p_channel_val = (data_struct_t *)malloc(sizeof(data_struct_t));
      }
    }
  }
  assert(pos == buf_in.used);

  free(p_channel_val); // Last one not pushed into map.
  Free(buf_in.p);

  return bytes_to_read;
}

