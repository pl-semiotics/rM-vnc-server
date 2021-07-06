#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <linux/input.h>

#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <rM-input-devices.h>

#include "backend.h"

/* Used by the signal handler */
int fbfd;
void* fb;
size_t fb_len;
long pagesize; /* not strictly necessary */
struct rM_input_devices idevs;
int current_contact = -1;

static void sigsegv_handler(int sig, siginfo_t *si, void *unused) {
  if (si->si_addr >= fb && si->si_addr < fb + fb_len) {
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

void process_ptr_event(int buttonMask, int x, int y, rfbClientPtr cl) {
  rfbDefaultPtrAddEvent(buttonMask, x, y, cl);
  struct rM_coord where = { .coord_kind = RM_COORD_DISPLAY, .x = x, .y = y };
  if (buttonMask & 4) {
    /* touch event */
    if (current_contact < 0) { current_contact = touch_begin_contact(&idevs); }
    submit_touch_contact(&idevs, current_contact,
                         (struct rM_coord){ RM_COORD_DISPLAY, x, y },
                         WHICH_TOUCH_X|WHICH_TOUCH_Y);
  } else {
    if (current_contact >= 0) {
      touch_end_contact(&idevs, current_contact); current_contact = -1;
    }
  }
  submit_wacom_event(&idevs,
                     (buttonMask & 2) || (buttonMask & 1), buttonMask & 1,
                     (struct rM_coord){ RM_COORD_DISPLAY, x, y },
                     4095,
                     WHICH_WACOM_PEN|WHICH_WACOM_TOUCH|WHICH_WACOM_X|
                     WHICH_WACOM_Y|WHICH_WACOM_PRESSURE);
}

void process_kbd_event(rfbBool down, rfbKeySym sym, rfbClientPtr cl) {
  int evdev_keysym;
  int found_known_key = 1;
  switch (sym) {
    case XK_Left:
    case XK_Up:
    case XK_Page_Up:
      evdev_keysym = KEY_LEFT; break;
    case XK_Right:
    case XK_Down:
    case XK_Page_Down:
      evdev_keysym = KEY_RIGHT; break;
    case XK_Home:
    case XK_Escape:
      evdev_keysym = KEY_HOME; break;
    default:
      found_known_key = 0;
  }
  if (found_known_key) { submit_key_event(&idevs, evdev_keysym, !!down); }
}

void handle_wacom_event(void *data, int pen_down, int touch_down,
                        int abs_x, int abs_y, int abs_pressure) {
  rfbScreenInfoPtr srv = (rfbScreenInfoPtr)data;
  if (abs_x == srv->cursorX && abs_y == srv->cursorY) { return; }
  pthread_mutex_lock(&srv->cursorMutex);
  srv->cursorX = abs_x;
  srv->cursorY = abs_y;
  pthread_mutex_unlock(&srv->cursorMutex);
  rfbClientIteratorPtr i = rfbGetClientIterator(srv);
  rfbClientPtr c;
  while ((c = rfbClientIteratorNext(i))) {
    if (c->enableCursorPosUpdates) { c->cursorWasMoved = TRUE; }
  }
  rfbMarkRectAsModified(srv, abs_x,abs_y,abs_x+1,abs_y+1); /* trigger an update */
}

int main(int argc, char *argv[]) {
  /* Work out framebuffer parameters */
  struct backend **backends = get_backends();
  if (!backends || !backends[0]) { return 1; }
  printf("Using backend %s\n", backends[0]->name);
  void *b = backends[0]->initialize();

  struct fb_info info = backends[0]->get_info(b);
  fbfd = info.fd;
  fb_len = info.len;

  /* Set up server parameters */
  rfbScreenInfoPtr server = rfbGetScreen(&argc, argv,
                                         info.xres, info.yres,
                                         info.bits_per_pixel / 3, 3,
                                         info.bits_per_pixel / 8);
  /* RGB565, little endian */
  server->serverFormat.redMax = (1 << info.red.length) - 1;
  server->serverFormat.redShift = info.red.offset;
  server->serverFormat.greenMax = (1 << info.green.length) - 1;
  server->serverFormat.greenShift = info.green.offset;
  server->serverFormat.blueMax = (1 << info.blue.length) - 1;
  server->serverFormat.blueShift = info.blue.offset;

  server->paddedWidthInBytes = info.line_width;

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

  server->frameBuffer = fb + info.offset;

  idevs = find_rm_input_devices(1);
  on_wacom_event(&idevs, RM_COORD_DISPLAY, handle_wacom_event, server);
  enable_input_event_listening(&idevs);
  server->ptrAddEvent = process_ptr_event;
  server->kbdAddEvent = process_kbd_event;

  /* Start serving */
  rfbInitServer(server);
  rfbRunEventLoop(server, -1, 1);

  /* Listen for damage events to pass along */
  int notifyfd = backends[0]->notification_fd(b);
  while (1) {
    struct update u = backends[0]->read_update(b, notifyfd);
    if (mmap(fb, fb_len, PROT_READ, MAP_SHARED|MAP_FIXED, fbfd, 0) != fb) {
      return 6;
    }
    rfbMarkRectAsModified(server, u.x1, u.y1, u.x2, u.y2);
  }

  return 0;
}
