#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SECTOR_SIZE 512
#define TOTAL_SECTORS 10000 

#define SECTOR_SIZE 512
#define SUPERBLOCK_START_SECTOR 0

// the total number of files and directories in the file system has a
// maximum limit of 1000
#define MAX_FILES 1000

// each file can have a maximum of 30 sectors; we treat the data
// blocks of the file/director the same as sectors
#define MAX_SECTORS_PER_FILE 30

// the size of a file or directory is limited
#define MAX_FILE_SIZE (MAX_SECTORS_PER_FILE*SECTOR_SIZE)

// the magic number chosen for our file system
#define OS_MAGIC 0xdeadbeef

// 2. the inode bitmap (one or more sectors), which indicates whether
// the particular entry in the inode table (#4) is currently in use
#define INODE_BITMAP_START_SECTOR 1

// the total number of bytes and sectors needed for the inode bitmap;
// we use one bit for each inode (whether it's a file or directory) to
// indicate whether the particular inode in the inode table is in use
#define INODE_BITMAP_SIZE ((MAX_FILES+7)/8)
#define INODE_BITMAP_SECTORS ((INODE_BITMAP_SIZE+SECTOR_SIZE-1)/SECTOR_SIZE)

// 3. the sector bitmap (one or more sectors), which indicates whether
// the particular sector in the disk is currently in use
#define SECTOR_BITMAP_START_SECTOR (INODE_BITMAP_START_SECTOR+INODE_BITMAP_SECTORS)

// the total number of bytes and sectors needed for the data block
// bitmap (we call it the sector bitmap); we use one bit for each
// sector of the disk to indicate whether the sector is in use or not
#define SECTOR_BITMAP_SIZE ((TOTAL_SECTORS+7)/8)
#define SECTOR_BITMAP_SECTORS ((SECTOR_BITMAP_SIZE+SECTOR_SIZE-1)/SECTOR_SIZE)

// 4. the inode table (one or more sectors), which contains the inodes
// stored consecutively
#define INODE_TABLE_START_SECTOR (SECTOR_BITMAP_START_SECTOR+SECTOR_BITMAP_SECTORS)

// an inode is used to represent each file or directory; the data
// structure supposedly contains all necessary information about the
// corresponding file or directory
typedef struct _inode {
  int size; // the size of the file or number of directory entries
  int type; // 0 means regular file; 1 means directory
  int data[MAX_SECTORS_PER_FILE]; // indices to sectors containing data blocks
} inode_t;

// the inode structures are stored consecutively and yet they don't
// straddle accross the sector boundaries; that is, there may be
// fragmentation towards the end of each sector used by the inode
// table; each entry of the inode table is an inode structure; there
// are as many entries in the table as the number of files allowed in
// the system; the inode bitmap (#2) indicates whether the entries are
// current in use or not
#define INODES_PER_SECTOR (SECTOR_SIZE/sizeof(inode_t))
#define INODE_TABLE_SECTORS ((MAX_FILES+INODES_PER_SECTOR-1)/INODES_PER_SECTOR)

// 5. the data blocks; all the rest sectors are reserved for data
// blocks for the content of files and directories
#define DATABLOCK_START_SECTOR (INODE_TABLE_START_SECTOR+INODE_TABLE_SECTORS)

// other file related definitions

// max length of a path is 256 bytes (including the ending null)
#define MAX_PATH 256

// max length of a filename is 16 bytes (including the ending null)
#define MAX_NAME 16

// max number of open files is 256
#define MAX_OPEN_FILES 256

// each directory entry represents a file/directory in the parent
// directory, and consists of a file/directory name (less than 16
// bytes) and an integer inode number
typedef struct _dirent {
  char fname[MAX_NAME]; // name of the file
  int inode; // inode of the file
} dirent_t;

// the number of directory entries that can be contained in a sector
#define DIRENTS_PER_SECTOR (SECTOR_SIZE/sizeof(dirent_t))

// global errno value here
int osErrno;

// the name of the disk backstore file (with which the file system is booted)
static char bs_filename[1024];

unsigned char bits_to_byte(int *bits)
{
   unsigned char char_byte;   
   // The powers of 2 is stored in the variable power_value
   int power_value=1; 
   // The variable char_byte is used for the converted byte value
   char_byte=0;
   for (int i=0;i<8;i++)
   {
     //dprintf("%d\n",bits[i]);
     if (i>0) {     
     	power_value*=2;
     }
     char_byte+=bits[7-i]*power_value;
   }
   // Testing byte 
   printf("...Bits to byte: %d\n", char_byte);
   return char_byte;
}


// set the first unused bit from a bitmap of 'nbits' bits (flip the
// first zero appeared in the bitmap to one) and return its location;
// return -1 if the bitmap is already full (no more zeros)
static int bitmap_first_unused(int start, int num, int nbits)
{
  /* YOUR CODE */

  printf("...bitmap_first_unused: start=%d, num=%d, nbits=%d\n",start, num, nbits);

  char buf[SECTOR_SIZE];
  unsigned char all_ones=255;
  // ending_byte is used to determine the ending location of bitmap
  // a bitmap may not occupy the whole sector
  int ending_byte=SECTOR_SIZE;
  int flag_flip=0;
  int ibit=-1;

  for (int i=0; i<SECTOR_SIZE; i++)
  {
     buf[i]=all_ones;
  }
 
 // Loop through all the bitmap sectors
  for (int i=0; i<num; i++)
  {
    //Disk_Read(start+i, buf);
    if (i==1) 
    {
       buf[230]=127;
    }
    // Find the ending location of bitmap in each sector
    if (i<num-1)
    {
      ending_byte=SECTOR_SIZE;
      printf("...Ending byte is %d\n",ending_byte);
    }
    else
    {
      ending_byte=nbits-i*SECTOR_SIZE;
      printf("...Ending byte is %d\n",ending_byte);
    }
    for (int j=0; j<ending_byte; j++)
    {
       //printf("i=%d, j=%d\n",i, j);
       //if (i==1 && j==230)
       //{
       //for (int m=0;m<8;m++)
       //{
       //   printf("%d", !!((buf[j] << m) & 0x80));
       //  }
       //   printf("test1%d\n",j);
       //}
       unsigned char unfull_byte=(unsigned char)buf[j];
       if(unfull_byte!=all_ones)
       {
         // Not full byte
         //unsigned char unfull_byte=buf[j];
         for (int m=0;m<8;m++)
         {
             printf("%d", !!((unfull_byte<< m) & 0x80));
         }
          printf("\n");

         int bits[8];
         for (int k=0; k<8; k++)
         {
            bits[k]=!!((unfull_byte<< k) & 0x80);            
            if(bits[k]==0 && flag_flip==0)
            {
               // Flip the bit
               bits[k]=1; 
               flag_flip=1; 
               // Bit location
               ibit=i*SECTOR_SIZE*8+j*8+k;
               printf("...First unused bit: %d\n", ibit);       
            }
            //printf("%d",bits[k]);
            //printf("\n");
         }
         if(flag_flip==1)
         {
           buf[j]=bits_to_byte(bits);
           printf("...Byte after flip:");
           for (int m=0;m<8;m++)
           {
             printf("%d", !!((((unsigned char)buf[j])<< m) & 0x80));
           }
           printf("\n");           
         }
       }
     if (flag_flip==1)
     {         
       break;
     }
    }
    if(flag_flip==1)
    {
       // Write the flipped sector back to disk
       //Disk_Write(start+i, buf);
       printf("flipped\n");
       return ibit;
    }
  }
  return -1;
}


int main()
{
   int child_inode=0;
   child_inode= bitmap_first_unused(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, INODE_BITMAP_SIZE);
   printf("... formatted inode bitmap (start=%d, num=%d, size=%d)\n",
	     (int)INODE_BITMAP_START_SECTOR, (int)INODE_BITMAP_SECTORS, (int)INODE_BITMAP_SIZE);
   printf("ibit: %d\n", child_inode);   

   int newsec = 0;
   newsec=bitmap_first_unused(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE);
   printf("... formatted sector bitmap (start=%d, num=%d, size=%d)\n",
	     (int)SECTOR_BITMAP_START_SECTOR, (int)SECTOR_BITMAP_SECTORS, (int)SECTOR_BITMAP_SIZE);
   printf("ibit: %d\n", newsec);   
   return 0;
}

