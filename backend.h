#include <sys/types.h>
#include <stdint.h>
#include <linux/fb.h>

/* this is a lot like a very-stripped-down version of
 * fb_var_screeninfo that omits everything we don't care about
 */
struct fb_info {
  int fd;
  size_t offset;
  size_t len;
  uint32_t xres, yres;
  uint32_t bits_per_pixel;
  uint32_t line_width;
  /* as in fb_var_screeninfo */
  struct fb_bitfield red;
  struct fb_bitfield green;
  struct fb_bitfield blue;
};

struct update { int x1; int y1; int x2; int y2; };

struct backend {
  char *name;
  void *(*initialize)(void);
  struct fb_info (*get_info)(void *);
  int (*notification_fd)(void *);
  struct update (*read_update)(void *, int);
};

void register_backend(struct backend *);
struct backend **get_backends(void);

#define DECLARE_BACKEND(b)                 \
  __attribute__((constructor)) static void register_backend_() { \
    register_backend(&b); \
  }
