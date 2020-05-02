#include <rfb/rfb.h>
#include <mxc_epdc_fb_damage.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>

/* Used by the signal handler */
int fbfd;
void* fb;
size_t fb_len;
long pagesize; /* not strictly necessary */

static void sigsegv_handler(int sig, siginfo_t *si, void *unused) {
  if (si->si_addr >= fb && si->si_addr < fb + fb_len) {
    printf("in fb touch %p %p %ld\n", si->si_addr, fb, fb_len);
    /* Find the nearest page boundary */
    long buf_off = si->si_addr - fb;
    void *page_base = (void*)((uintptr_t)si->si_addr & ~(pagesize - 1));
    if (mmap(page_base, pagesize, PROT_WRITE,
      MAP_PRIVATE|MAP_FIXED, fbfd, 0) != page_base) { exit(6); }
  } else {
    /* reset the handler, we can't help here */
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_DFL;
    sigaction(SIGSEGV, &sa, NULL);
  }
}
int setup_discard_writes(void) {
  struct sigaction sa;

  pagesize = sysconf(_SC_PAGE_SIZE);

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = sigsegv_handler;
  if (sigaction(SIGSEGV, &sa, NULL) == -1) {
    return 4;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  /* Work out framebuffer parameters */
  struct fb_var_screeninfo info;
  fbfd = open("/dev/fb0", O_RDONLY);
  if (fbfd < 0) { return 1; }
  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &info) == -1) { return 2; }
  __u32 bytes_per_pixel = info.bits_per_pixel / 8;
  fb_len = info.xres_virtual * info.yres_virtual * bytes_per_pixel;

  /* Set up server parameters */
  rfbScreenInfoPtr server = rfbGetScreen(&argc, argv,
                                         info.xres, info.yres,
                                         info.bits_per_pixel / 3, 3,
                                         bytes_per_pixel);
  /* RGB565, little endian */
  server->serverFormat.redMax = (1 << info.red.length) - 1;
  server->serverFormat.redShift = info.red.offset;
  server->serverFormat.greenMax = (1 << info.green.length) - 1;
  server->serverFormat.greenShift = info.green.offset;
  server->serverFormat.blueMax = (1 << info.blue.length) - 1;
  server->serverFormat.blueShift = info.blue.offset;
  /* 1408x3840 layout, with 1404x1872 as the visible portion */
  server->paddedWidthInBytes = bytes_per_pixel * info.xres_virtual;

  /* Hook up the actual framebuffer */
  fb = mmap(NULL, fb_len, PROT_READ, MAP_SHARED, fbfd, 0);
  if (!fb) { return 3; }
  /* We don't want the VNC server writing to the actual screen and
   * causing issues. Admittedly, it does seem to only do that in the
   * cursor handling code, but that's still a problem. So, we map with
   * PROT_READ and have a SIGSEGV handler that quietly replaces any
   * written pages with private ones on writes, and then reset the
   * mapping whenever we receive an update.
   */
  int ret = setup_discard_writes();
  if (ret) { return ret; };
  /* 1408x3840 layout, with 1404x1872 as the visible portion */
  server->frameBuffer = fb + bytes_per_pixel * (info.xoffset + info.yoffset * info.xres_virtual);

  /* Start serving */
  rfbInitServer(server);
  rfbRunEventLoop(server, -1, 1);

  /* Listen for damage events to pass along */
  int damagefd = open("/dev/fbdamage", O_RDONLY);
  struct mxcfb_damage_update damage;
  while (1) {
    int len = read(damagefd, &damage, sizeof(struct mxcfb_damage_update));
    if (len == -1 && errno == EINTR) { continue; }
    if (len != sizeof(struct mxcfb_damage_update)) { return 5; }
    struct mxcfb_rect *r = &damage.data.update_region;
    if (mmap(fb, fb_len, PROT_READ, MAP_SHARED|MAP_FIXED, fbfd, 0) != fb) {
      return 6;
    }
    rfbMarkRectAsModified(
        server, r->left, r->top, r->left + r->width, r->top + r->height);
  }

  return 0;
}
