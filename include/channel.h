#ifndef _CHANNELS_H
#define _CHANNELS_H

#include "raii.h"

typedef _channel_t *channel_t;

/* Creates an unbuffered channel, similar to golang channels. */
C_API channel_t channel(void);

/* Creates an buffered channel of given element count,
similar to golang channels. */
C_API channel_t channel_buf(int);

/* Send data to the channel. */
C_API int chan_send(channel_t, void_t);

/* Receive data from the channel. */
C_API values_type chan_recv(channel_t);
C_API bool chan_ready(channel_t);
C_API void chan_ready_reset(channel_t);
C_API void channel_print(channel_t);
C_API void channel_free(channel_t);
C_API void channel_destroy(void);

/* The `for_select` macro sets up a coroutine to wait on multiple channel
operations. Must be closed out with `selected`,
and if no `_send(channel, data)`, `_recv(channel, data)`, `_default` provided,
an infinite loop is created.

This behaves same as GoLang `select {}` statement. */
#define for_select              \
  bool $##__FUNCTION__##_fs;    \
  while (true)  {               \
      $##__FUNCTION__##_fs = false;

/* Ends an `for_select` condition block.
Will `yield` execution, if no target ~channel~ is selected as ready. */
#define selected          \
  if ($##__FUNCTION__##_fs == false)    \
      yielding();           \
  }

/* Will send `data` and execute block, only if `ch` is target and ready.
Must use `_else_if` to end block, or `_break` to end an `_else_if` chain. */
#define _send(ch, data) \
  if (chan_ready(ch) && $##__FUNCTION__##_fs == false) {    \
      chan_ready_reset(ch); chan_send(ch, data);

#define _else_if   _break else

/* Will receive `data` and execute block, only if `ch` is target and ready.
Must use `_else_if` to end block, or `_break` to end an `_else_if` chain. */
#define _recv(ch, data) \
  if (chan_ready(ch) && $##__FUNCTION__##_fs == false) {    \
      chan_ready_reset(ch); values_type data = chan_recv(ch);

#define _break          \
  $##__FUNCTION__##_fs = true;  \
  }

/* The `_default` is run only if no `channel` is ready.
Must also closed out with `_break()`. */
#define _default        \
  _else_if if ($##__FUNCTION__##_fs == false) {

#endif