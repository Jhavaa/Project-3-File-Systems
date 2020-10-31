#include <stdio.h>

/* Function to convert 8 bits to a byte */
unsigned char bits_to_byte(int *bits)
{
   unsigned char char_byte;
   int power_value=1; 
   char_byte=0;
   for (int i=0;i<8;i++)
   {
     //printf("%d\n",bits[i]);
     if (i>0) {     
     	power_value*=2;
     }
     char_byte+=bits[7-i]*power_value;
   }
   /* Testing byte */
    printf("%d\n", char_byte);
    return char_byte;
}

int main()
{
   int bits[]={1,1,1,1,1,1,1,1};
   unsigned char testc;
   testc=bits_to_byte(bits);
   for (int i=0;i<8;i++)
    {
       printf("%d", !!((testc << i) & 0x80));
    }
    printf("\n");
  // printf("Byte is %c\n",testc);
   return 0;
}
