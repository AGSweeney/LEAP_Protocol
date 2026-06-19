/*******************************************************************************
 * NetBurner stubs for optional POSIX helpers referenced by OpENer.
 ******************************************************************************/

#include <errno.h>

int mkdir(const char *path, int mode) {
  (void)path;
  (void)mode;
  return 0;
}
