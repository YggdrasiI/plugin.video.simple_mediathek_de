#pragma once

//for open(),

/* Note: Do not include in params.c to
 * prevent following error (?!)
 * In file included from /usr/include/features.h:374:0,
 *                  from /usr/include/stdio.h:27,
 *                  from /usr/include/argp.h:23,
 *                  from params.c:1:
 * /usr/include/x86_64-linux-gnu/bits/fcntl2.h: In function ‘open’:
 * /usr/include/x86_64-linux-gnu/bits/fcntl2.h:43:7:
 *      error: invalid use of ‘__builtin_va_arg_pack_len ()’
 *  if (__va_arg_pack_len () > 1)
 *           ^
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
