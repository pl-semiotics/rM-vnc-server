#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <systemd/sd-bus.h>

#include <libqsgepaper-snoop.h>

#include "backend.h"

#define RM2FB_PID_ENV "QSG_RM2FB_PID"
int string_to_nat(char *s) {
  int n = 0;
  for (char *p = s; *p; p++) {
    if ('0' > *p || '9' < *p) { return -1; }
    n *= 10; n += *p-'0';
  }
  return n;
}
#define LIB_PID_ENV "LIBQSGEPAPER_FBCON_PID"
#define LIB_FD_ENV "LIBQSGEPAPER_SNOOP_REQUESTED_FD"
pid_t find_rm2fb_pid(void) {
  /* don't bother if these are already set */
  if (getenv(LIB_PID_ENV) || getenv(LIB_FD_ENV)) { return -1; }
  char *pidn = getenv(RM2FB_PID_ENV);
  if (pidn) { return string_to_nat(pidn); }

  sd_bus *bus = NULL; sd_bus_message *m = NULL;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  char *path = NULL, *path_copy = NULL, *activeState = NULL;
  pid_t ret = -1;
  if (sd_bus_open_system(&bus) < 0) { goto exit; }
  if (sd_bus_call_method(bus,
                         "org.freedesktop.systemd1",
                         "/org/freedesktop/systemd1",
                         "org.freedesktop.systemd1.Manager",
                         "GetUnit",
                         &error,
                         &m,
                         "s",
                         "rm2fb.service") < 0) { goto exit; }
  if (sd_bus_message_read(m, "o", &path) < 0) { goto exit; }
  path_copy = strdup(path); path = NULL;
  sd_bus_message_unref(m); m = NULL;
  if (sd_bus_get_property_string(bus,
                                 "org.freedesktop.systemd1",
                                 path_copy,
                                 "org.freedesktop.systemd1.Unit",
                                 "ActiveState",
                                 &error,
                                 &activeState) < 0) { goto exit; }
  if (strcmp(activeState, "active") < 0) { goto exit; }
  if (sd_bus_call_method(bus,
                         "org.freedesktop.systemd1",
                         path_copy,
                         "org.freedesktop.systemd1.Service",
                         "GetProcesses",
                         &error,
                         &m,
                         "") < 0) { return -1; }
  if (sd_bus_message_enter_container(m, 'a', "(sus)") != 1) { goto exit; }
  if (sd_bus_message_enter_container(m, 'r', "sus") != 1) { goto exit; }
  if (sd_bus_message_skip(m, "s") < 0) { goto exit; }
  if (sd_bus_message_read(m, "u", &ret) < 0) { goto exit; }
  if (sd_bus_message_skip(m, "s") < 0) { goto exit; }
  if (sd_bus_message_exit_container(m) < 0) { goto exit; }
  if (sd_bus_message_exit_container(m) < 0) { goto exit; }

exit:
  sd_bus_error_free(&error);
  if (path_copy) { free(path_copy); }
  if (m) { sd_bus_message_unref(m); }
  sd_bus_flush_close_unref(bus);
  return ret;
}
int find_rm2fb_fd(pid_t pid) {
  char *fdn;
  int ret = -1;
  if (asprintf(&fdn, "/proc/%d/fd/", pid) < 0 || !fdn) { return -1; }
  DIR *fdd = opendir(fdn);
  if (!fdd) { goto exit; }
  int fdfd = dirfd(fdd);
  if (fdfd < 0) { goto exit; }
  struct dirent *fde;
  errno = 0;
  while (fde = readdir(fdd)) {
    char *buf = NULL;
    if (!(fde->d_type&DT_LNK)) { goto next; }
    int fd = string_to_nat(fde->d_name);
    if (!fd) { goto next; }

    struct stat st;
    if (fstatat(fdfd, fde->d_name, &st, AT_SYMLINK_NOFOLLOW)) { goto next; }
    buf = malloc(st.st_size+1);
    int n = readlinkat(fdfd, fde->d_name, buf, st.st_size);
    if (n < 0) { goto next; }
    buf[n] = '\0';
    if (!strcmp(basename(buf), "swtfb.01")) { ret = fd; goto exit; }
 next:
    if (buf) { free(buf); }
    errno = 0;
  }
exit:
  closedir(fdd);
  return ret;
}
void check_for_rm2fb(void) {
  pid_t pid = find_rm2fb_pid();
  if (pid >= 0) {
    int fd = find_rm2fb_fd(pid);
    if (fd >= 0) {
      char *tmp;
      if (asprintf(&tmp, "%d", pid) < 0 || !tmp) { return; }
      setenv(LIB_PID_ENV, tmp, 0);
      free(tmp);
      if (asprintf(&tmp, "%d", fd) < 0 || !tmp) { return; }
      setenv(LIB_FD_ENV, tmp, 0);
      free(tmp);
    }
  }
}

static void *qsg_initialize(void) {
  struct libqsgepaper_snoop_fb *ret = malloc(sizeof(struct libqsgepaper_snoop_fb));
  if (!ret) { exit(2); }
  check_for_rm2fb();
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
