#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <libqsgepaper-snoop.h>

#include "backend.h"

static void *qsg_initialize(void) {
  struct libqsgepaper_snoop_fb *ret = malloc(sizeof(struct libqsgepaper_snoop_fb));
  if (!ret) { exit(2); }
  *ret = libqsgepaper_snoop();
  return ret;
}
#define XRES 1404
#define YRES 1872
#define LINE_PADDING 0
#define BITS_PER_PIXEL 16
static struct fb_info qsg_get_info(void *fb_voidp) {
  struct libqsgepaper_snoop_fb *fb = (struct libqsgepaper_snoop_fb *)fb_voidp;
  struct fb_info ret = {
    .fd = fb->fb_fd,
    .offset = fb->offset,
    .len = XRES*YRES*BITS_PER_PIXEL/8,
    .xres = XRES,
    .yres = YRES,
    .bits_per_pixel = BITS_PER_PIXEL,
    .line_width = XRES*BITS_PER_PIXEL/8,
    .red = { .length = 5, .offset = 11, },
    .green = { .length = 6, .offset = 5, },
    .blue = { .length = 5, .offset = 0, },
  };
  return ret;
}

static int qsg_notification_fd(void *fb_voidp) {
  struct libqsgepaper_snoop_fb *fb = (struct libqsgepaper_snoop_fb *)fb_voidp;
  return fb->socket_fd;
}

static struct update qsg_read_update(void *fb_voidp, int fd) {
  struct libqsgepaper_snoop_fb *fb = (struct libqsgepaper_snoop_fb *)fb_voidp;
  uint upd[4];
  char *ptr = (char*)&upd;
  size_t to_read = 16;
  while (to_read > 0) {
    int ret = read(fd, ptr, to_read);
    if (ret < 0) {
      if (errno == EINTR) { continue; }
      else { exit(5); }
    }
    to_read -= ret;
    ptr += ret;
  }
  struct update ret;
  ret.x1 = upd[0];
  ret.y1 = upd[1];
  ret.x2 = upd[2];
  ret.y2 = upd[3];
  return ret;
}

struct backend qsg_backend = {
  .name = "libqsgepaper-snoop",
  .initialize = qsg_initialize,
  .get_info = qsg_get_info,
  .notification_fd = qsg_notification_fd,
  .read_update = qsg_read_update,
};

DECLARE_BACKEND(qsg_backend)
