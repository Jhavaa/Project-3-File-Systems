#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "LibFS.h"

void usage(char *prog)
{
  printf("USAGE: %s [disk] dir\n", prog);
  exit(1);
}

int main(int argc, char *argv[])
{
  char *diskfile, *path;
  int dir_size = 0;
  if(argc != 2 && argc != 3) usage(argv[0]);
  if(argc == 3) { diskfile = argv[1];}
  else { diskfile = "default-disk"; path = argv[1]; }

  if(FS_Boot(diskfile) < 0) {
    printf("ERROR: can't boot file system from file '%s'\n", diskfile);
    return -1;
  }

  char* first_dir = "/test_dir1";
  char* second_dir = "/test_dir2";

  // Create a directory under root
  Dir_Create("/test_dir1");

  // Confirm if directory exist by calling Dir_size (should return 20)
  int dirSize = Dir_Size(first_dir);
  assert(dirSize == 20);
  
  // Create another directory under root 
  Dir_Create("/test_dir2");
  dirSize = Dir_Size(second_dir);

  // Confirm the Dir_size now returns 40 (20 * 2)
  assert(dirSize == 40);

  if(FS_Sync() < 0) {
    printf("ERROR: can't sync disk '%s'\n", diskfile);
    return -3;
  }
  return 0;
}
