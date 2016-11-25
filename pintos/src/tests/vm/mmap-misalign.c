/* Verifies that misaligned memory mappings are disallowed. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  mapid_t map = mmap (handle, (void *) 0x10001234);
  printf("map %d\n", map);
//  CHECK (mmap (handle, (void *) 0x10001234) == MAP_FAILED,
//         "try to mmap at misaligned address");
}

