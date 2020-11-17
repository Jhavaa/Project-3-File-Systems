#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{  
  char buf[3000];
  char* ptr = buf;
  char letters[26]="Abcdefghijklmnopqrstuvwxyz";
  char* fname=argv[1];
  int size=0;
  int sz=0;

  for(int i=0; i<50; i++)
  {
    for (int j=0; j<26; j++)
    {
        //if(j==26)
        //{
        //  size=sizeof("\n");
        //  sprintf(ptr, "%s", "\n");                 
        //}
        //else
        //{      
          size=1;
          sprintf(ptr, "%c", letters[j]); 
        //}
        ptr+=size;
        sz=sz+size;
        //if(ptr >= buf+sz) break;
    }
  }
  printf("size is %d\n", sz);
  //fname="/test_input2.txt"
  FILE* fptr = fopen(fname, "w");
  if(!fptr) {
    printf("ERROR: can't open file '%s' to export\n", fname);
    return -3;
  }
  fwrite(buf, 1, sz, fptr);
  fclose(fptr);

  return 0;
}

