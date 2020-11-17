#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
  char buf[]="abcdef";
  char buf2[5];
  char buf3[5];
  char* ptr = buf2;

  for (int i=0; i<5; i++)
  {
       printf("%c\n", buf[i]);
  }

  memcpy(ptr, &buf[1], 5);
 //*sizeof(buf[1])
  for (int i=0; i<5; i++)
  {
       printf("%c\n", *(ptr+i));
  }

  memcpy(&buf[0],ptr, 5);

  for (int i=0; i<5; i++)
  {
       printf("%c\n", buf[i]);
  }
  
  return 0;
}
