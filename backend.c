#include "backend.h"
#include <stdlib.h>
#include <string.h>

static size_t n_alloced = 0;
static size_t n_used = 0;
static struct backend **backends = 0;

void register_backend(struct backend *b) {
  if (n_used >= n_alloced) {
    size_t new_n = n_alloced*2+1;
    struct backend **new_backends = calloc(sizeof(struct backend *), new_n);
    memcpy(new_backends, backends, sizeof(struct backend *)*n_alloced);
    backends = new_backends;
    n_alloced = new_n;
  }
  backends[n_used++] = b;
  backends[n_used] = NULL;
}

struct backend **get_backends(void) { return backends; }
