#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <mxc_epdc_fb_damage.h>

#include "backend.h"

struct mxc_data {
  int fbfd;
  int notify_fd;
};

#define MXC_EPDC_FB_DAMAGE_KO_ENV "MXC_EPDC_FB_DAMAGE_KO"
char __attribute__((weak)) mxc_epdc_fb_damage_ko_begin = 0;
char __attribute__((weak)) mxc_epdc_fb_damage_ko_end = 0;
static void ensure_have_fbdamage() {
  if (faccessat(AT_FDCWD, "/dev/fbdamage", F_OK, AT_EACCESS) == 0) { return; }

  char *start = &mxc_epdc_fb_damage_ko_begin;
  size_t size = &mxc_epdc_fb_damage_ko_end-&mxc_epdc_fb_damage_ko_begin;

  char *external = getenv(MXC_EPDC_FB_DAMAGE_KO_ENV);
  int fd;
  if (external) {
    fd = open(external, O_RDONLY);
    if (fd < 0) { return; }
    struct stat stat;
    if (fstat(fd, &stat)) { return; }
    start = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (start == MAP_FAILED) { return; }
    size = stat.st_size;
  }

  syscall(__NR_init_module, start, size, "");

  if (external) {
    munmap(start, size);
    close(fd);
  }
}

static void *mxc_initialize(void) {
  struct mxc_data *d = malloc(sizeof(struct mxc_data));
  if (!d) { return NULL; }
  d->fbfd = open("/dev/fb0", O_RDONLY);
  if (d->fbfd < 0) { free(d); return NULL; }
  ensure_have_fbdamage();
  d->notify_fd = open("/dev/fbdamage", O_RDONLY);
  return d;
}

static struct fb_info mxc_get_info(void *d_) {
  struct mxc_data *d = (struct mxc_data *)d_;
  struct fb_var_screeninfo info;
  ioctl(d->fbfd, FBIOGET_VSCREENINFO, &info);
  __u32 bytes_per_pixel = info.bits_per_pixel / 8;
  struct fb_info ret = {
    .fd = d->fbfd,
    .offset = bytes_per_pixel * (info.xoffset + info.yoffset * info.xres_virtual),
    .len = info.xres_virtual * info.yres_virtual * bytes_per_pixel,
    .xres = info.xres,
    .yres = info.yres,
    .bits_per_pixel = info.bits_per_pixel,
    .line_width = info.xres_virtual * bytes_per_pixel,
    .red = info.red,
    .green = info.green,
    .blue = info.blue,
  };
  return ret;
}

static int mxc_notification_fd(void *d_) {
  return ((struct mxc_data *)d_)->notify_fd;
}

static struct update mxc_read_update(void *fb_voidp, int fd) {
  struct mxcfb_damage_update damage;
  size_t to_read = sizeof(struct mxcfb_damage_update);
  char *p = (char*)&damage;
  while (to_read > 0) {
    size_t len = read(fd, p, to_read);
    if (len == -1 && errno == EINTR) { continue; }
    if (len < 0) { struct update ret = {0}; return ret; }
    p += len;
    to_read -= len;
  };
  struct mxcfb_rect *r = &damage.data.update_region;
  struct update ret = {
    .x1 = r->left, .y1 = r->top,
    .x2 = r->left+r->width, .y2 = r->top+r->height
  };
  return ret;
}

struct backend mxc_backend = {
  .name = "epdc_mxc_fb_damage",
  .initialize = mxc_initialize,
  .get_info = mxc_get_info,
  .notification_fd = mxc_notification_fd,
  .read_update = mxc_read_update,
};

DECLARE_BACKEND(mxc_backend)
