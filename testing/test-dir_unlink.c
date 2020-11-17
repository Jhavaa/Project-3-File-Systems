#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "LibFS.h"
#include "LibDisk.h"
#include "LibFS.h"

void usage(char *prog)
{
  printf("USAGE: %s [disk]\n", prog);
  exit(1);
}

int main(int argc, char *argv[])
{
  char *diskfile;
  int dir_size = 0;
  if (argc != 2) usage(argv[0]);
  diskfile = argv[1];

  if(FS_Boot(diskfile) < 0) 
  {
    printf("ERROR: can't boot file system from file '%s'\n", diskfile);
    return -1;
  }

  // assert empty root
  assert(0 == Dir_Read("/", NULL, 0));

  char *first_dir = "/test_dir1";
  char *second_dir = "/test_dir2";
  char *file = "/test_dir1/test_file.text";

  // Create all directories
  Dir_Create(first_dir);
  Dir_Create(second_dir);
  // FIXME: as the File_Write is not ready, 
  // let's just directly call the internal method to create the file for testing purpose.
  create_file_or_directory(0, file);

  char *path = "/";

  // check invalid input
  assert(Dir_Unlink(NULL) == -1);

  // check non-exist path
  assert(Dir_Unlink("/random_path") == -1);
  assert(osErrno == E_NO_SUCH_DIR);

  // check unlink root
  assert(Dir_Unlink(path) == -1);
  assert(osErrno == E_ROOT_DIR);

  // check positive case
  printf("second dir %s size = %d\n", second_dir, Dir_Size(second_dir));
  assert(Dir_Unlink(second_dir) == 0);

  // check non-empty dir
  assert(Dir_Unlink(first_dir) == -1);
  assert(osErrno == E_DIR_NOT_EMPTY);

  // clean up sub dir and delete
  File_Unlink(file);
  printf("first dir %s size = %d\n", first_dir, Dir_Size(first_dir));
  assert(Dir_Unlink(first_dir) == 0);
  assert(Dir_Size(path) == 0);

  printf("######## All Tests Passed ########\n");
  return 0;
}
