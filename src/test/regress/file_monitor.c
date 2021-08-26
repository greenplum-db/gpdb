#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <uv.h>

static void fs_cb(uv_fs_event_t *handle, const char *filename, int events, int status)
{
  if (status != 0 || (events & UV_RENAME) == 0)
    return;

  printf("%s/%s\n", (char*)handle->data, filename);
}

static void signal_cb(uv_signal_t *handle, int signo)
{
  uv_signal_stop(handle);
  uv_stop(uv_default_loop());
  fflush(stdout);
}

int main(int ac, const char *av[])
{
  uv_loop_t *loop = uv_default_loop();
  uv_fs_event_t fse[32];
  uv_signal_t sig[2];
  int i, n;
  unsigned flags = 0;
  n = ac - 1;
  if (n > 32) {
    fprintf(stderr, "Too many directories to monitor\n");
    exit(1);
  }
  for (i = 0; i < n; i++) {
    uv_fs_event_init(loop, &fse[i]);
    fse[i].data = (void*)av[i+1];
    uv_fs_event_start(&fse[i], fs_cb, av[i+1], flags);
  }
  uv_signal_init(loop, &sig[0]);
  uv_signal_init(loop, &sig[1]);
  uv_signal_start(&sig[0], signal_cb, SIGINT);
  uv_signal_start(&sig[1], signal_cb, SIGTERM);

  uv_run(loop, UV_RUN_DEFAULT);

  fflush(stdout);
  return 0;

}
