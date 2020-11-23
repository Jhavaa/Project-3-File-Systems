#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LibFS.h"

#define BFSZ 1024

void usage(char *prog)
{
  printf("USAGE: %s [disk] file from_unix_file\n", prog);
  exit(1);
}

int main(int argc, char *argv[])
{
  char *diskfile, *path, *fname;
  char *coffset, *cfrsz;
  int offset=0;
  int frsz=BFSZ;
  if(argc != 3 && argc != 4 && argc != 5 && argc != 6) usage(argv[0]);
  if(argc == 4) { diskfile = argv[1]; path = argv[2]; fname = argv[3]; }
  else if(argc == 5)
  { diskfile = argv[1]; path = argv[2]; fname = argv[3]; coffset=argv[4]; offset=atoi(coffset); printf("Offset is %d\n",offset);}
  else if(argc == 6)
  { diskfile = argv[1]; path = argv[2]; fname = argv[3]; coffset=argv[4]; offset=atoi(coffset); 
    cfrsz=argv[5]; frsz=atoi(cfrsz);
    printf("Offset is %d and file size is %d\n",offset, frsz);}
  else 
  { diskfile = "default-disk"; path = argv[1]; fname = argv[2]; }

  if(FS_Boot(diskfile) < 0) {
    printf("ERROR: can't boot file system from file '%s'\n", diskfile);
    return -1;
  }
  
  if(File_Create(path) < 0) {
    printf("ERROR: can't create file '%s'\n", path);
    return -2;
  }

  int fd = File_Open(path);
  if(fd < 0) {
    printf("ERROR: can't open file '%s'\n", path);
    return -2;
  }
  
  FILE* fptr = fopen(fname, "r");
  if(!fptr) {
    printf("ERROR: can't open file '%s' to import\n", fname);
    return -3;
  }

  char buf[BFSZ]; 
  while(!feof(fptr)) {
    int rsz = fread(buf, 1, BFSZ, fptr);
    if(rsz < 0) {
      printf("ERROR: can't read file '%s' to import\n", fname);
      return -4;
    } else if(rsz > 0) {
      int wsz = File_Write(fd, buf, rsz);
      if(wsz < 0) {
	printf("ERROR: can't write file '%s'\n", path);
	return -5;
      }
    }
  }

  fclose(fptr);

  int test_location=File_Seek(fd, offset);
  printf("test location is %d\n", test_location);

  FILE* fptr2 = fopen(fname, "r");
  if(!fptr2) {
    printf("ERROR: can't open file '%s' to import\n", fname);
    return -3;
  }

  //int frsz=BFSZ;
  char buf2[frsz]; 
  //while(!feof(fptr2)) {
    int rsz2 = fread(buf2, 1, frsz, fptr2);
      if(rsz2 < 0) {
      printf("ERROR: can't read file '%s' to import\n", fname);
      return -4;
    } else if(rsz2 > 0) {
      for(int ii=0; ii<frsz; ii++)
      {
        printf("%c", buf2[ii]);
      }
      printf("\n");
      int wsz2 = File_Write(fd, buf2, rsz2);
      if(wsz2 < 0) {
	printf("ERROR: can't write file '%s'\n", path);
	return -5;
      }
    }
  //}
  fclose(fptr2);

  File_Close(fd);
  
  if(FS_Sync() < 0) {
    printf("ERROR: can't sync disk '%s'\n", diskfile);
    return -3;
  }
  return 0;
}
