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
  char *subdir1 = "/test_dir1/test_subdir1";
  char *subdir2 = "/test_dir1/test_subdir2";
  char *subdir3 = "/test_dir1/test_subdir3/";
  char *file1 = "/test_file1.txt";
  char *file2 = "/test_dir1/test_file2.text";

  // Create all directories
  Dir_Create(first_dir);
  Dir_Create(second_dir);
  Dir_Create(subdir1);
  Dir_Create(subdir2);
  Dir_Create(subdir3);
  // FIXME: as the File_Write is not ready, 
  // let's just directly call the internal method to create the file for testing purpose.
  create_file_or_directory(0, file1);
  create_file_or_directory(0, file2);

  char *path = "/";
  printf("directory '%s':\n     %-15s\t%-s\n", path, "NAME", "INODE");
  printf("you should see 2 folders 'test_dir1' 'test_dir2' and 1 file 'test_file1.txt'");

  dir_size = Dir_Size(path);
  printf("dir_size from Dir_Size = %d", dir_size);
  char* buf = malloc(dir_size);
  int entries = Dir_Read(path, buf, dir_size);
  assert(3 == entries);
  int idx = 0;
  for(int i=0; i < entries; i++) {
      printf("%-4d %-15s\t%-d\n", i, &buf[idx], *(int*)&buf[idx+16]);
      idx += 20;
  }
  free(buf);

  // Now, let's check subdir
  path = "/test_dir1/";
  printf("directory '%s':\n     %-15s\t%-s\n", path, "NAME", "INODE");
  printf("you should see 3 folders 'test_subdir1' 'test_subdir2' 'test_subdir3' and 1 file 'test_file2.txt'");

  dir_size = Dir_Size(path);
  printf("dir_size from Dir_Size = %d", dir_size);
  buf = malloc(dir_size);
  entries = Dir_Read(path, buf, dir_size);
  assert(4 == entries);
  idx = 0;
  for(int i = 0; i < entries; i++) {
      printf("%-4d %-15s\t%-d\n", i, &buf[idx], *(int*)&buf[idx+16]);
      idx += 20;
  }
  free(buf);

  // test non-exist path
  assert(-1 == Dir_Read("/invalid_path", NULL, 0));
  assert(-1 == Dir_Read("/test_dir1/invalid_path/", NULL, 0)); 

  // now check not big enough size buffer 
  assert(-1 == Dir_Read("/test_dir1", NULL, 1));
  assert(E_BUFFER_TOO_SMALL == osErrno);

  // now check large number of entries that can take multiple sectors 
  printf("## Testing large number of entries\n");
  int num_dir = 20 + 1; // 20 = 512 (SECTOR_SIZE) / 20 (sizeof(dirent_t))
  char* test_dir_path = malloc(8);
  strcpy(test_dir_path, "/test/");
  test_dir_path[7] = '/';
  Dir_Create("/test/");
  for (int i = 0; i < num_dir; i++) 
  {
    test_dir_path[6] = i + 'a';
    Dir_Create(test_dir_path);
  }
  dir_size = Dir_Size("/test/");
  buf = malloc(dir_size);
  assert(num_dir == Dir_Read("/test/", buf, dir_size));

  free(test_dir_path);
  free(buf);
  printf("######## All Tests Passed ########\n");
  return 0;
}
