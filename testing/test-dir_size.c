#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "LibFS.h"

void usage(char *prog)
{
  printf("USAGE: %s [disk]\n", prog);
  exit(1);
}

int main(int argc, char *argv[])
{
  char *diskfile = NULL;
  int dir_size = 0;
  if (argc != 2) usage(argv[0]);
  diskfile = argv[1];

  if(FS_Boot(diskfile) < 0) 
  {
    printf("ERROR: can't boot file system from file '%s'\n", diskfile);
    return -1;
  }

  char *first_dir = "/test_dir1";
  char *second_dir = "/test_dir2";

  // check root dir
  assert(0 == Dir_Size("/"));

  // Create a directory under root
  Dir_Create(first_dir);

  // Confirm if directory exist by calling Dir_size (should return 20)
  dir_size = Dir_Size("/");
  assert(dir_size == 20);
  
  // Create another directory under root 
  // Now, there are two dir under root - /test_dir1 and /test_dir2
  Dir_Create(second_dir);
  dir_size = Dir_Size("/");

  // Confirm the Dir_size now returns 40 (20 * 2)
  assert(dir_size == 40);

  // Testing subdir
  char *subdir1 = "/test_dir1/test_subdir1";
  char *subdir2 = "/test_dir1/test_subdir2";
  char *subdir3 = "/test_dir1/test_subdir3/";

  Dir_Create(subdir1);
  Dir_Create(subdir2);
  Dir_Create(subdir3);
  assert(60 == Dir_Size("/test_dir1/"));
  assert(40 == Dir_Size("/"));

  // test non-exist path
  assert(-1 == Dir_Size("/invalid_path"));
  assert(-1 == Dir_Size("/test_dir1/invalid_path/")); 

  printf("####### All Tests Passed ######\n");

  if (FS_Sync() < 0)
  {
    printf("ERROR: can't sync disk '%s'\n", diskfile);
    return -3;
  }
  return 0;
}
