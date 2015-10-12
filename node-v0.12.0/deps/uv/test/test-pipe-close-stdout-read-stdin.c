/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef _WIN32

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "uv.h"
#include "task.h"

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t* buf)
{
  static char buffer[1024];

  buf->base = buffer;
  buf->len = sizeof(buffer);
}

void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t* buf)
{
  if (nread < 0) {
    uv_close((uv_handle_t*)stream, NULL);
    return;
  }
}

/*
 * This test is a reproduction of joyent/libuv#1419 .
 */
TEST_IMPL(pipe_close_stdout_read_stdin) {
  int r = -1;
  int pid;
  int fd[2];
  int status;

  r = pipe(fd);
  ASSERT(r == 0);

  if ((pid = fork()) == 0) {
    /*
     * Make the read side of the pipe our stdin.
     * The write side will be closed by the parent process.
    */
    close(fd[1]);
    close(0);
    r = dup(fd[0]);
    ASSERT(r != -1);

    /* Create a stream that reads from the pipe. */
    uv_pipe_t stdin_pipe;

    r = uv_pipe_init(uv_default_loop(), (uv_pipe_t *)&stdin_pipe, 0);
    ASSERT(r == 0);

    r = uv_pipe_open((uv_pipe_t *)&stdin_pipe, 0);
    ASSERT(r == 0);

    r = uv_read_start((uv_stream_t *)&stdin_pipe, alloc_buffer, read_stdin);
    ASSERT(r == 0);

    /*
     * Because the other end of the pipe was closed, there should
     * be no event left to process after one run of the event loop.
     * Otherwise, it means that events were not processed correctly.
     */
    ASSERT(uv_run(uv_default_loop(), UV_RUN_NOWAIT) == 0);
  } else {
    /*
     * Close both ends of the pipe so that the child
     * get a POLLHUP event when it tries to read from
     * the other end.
     */
     close(fd[1]);
     close(fd[0]);

    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#endif /* ifndef _WIN32 */
