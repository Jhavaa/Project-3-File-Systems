#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// set to 1 to have detailed debug print-outs and 0 to have none
#define FSDEBUG 1

#if FSDEBUG
#define dprintf printf
#else
#define dprintf noprintf
void noprintf(char* str, ...) {}
#endif

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

// Function to convert 8 bits to a byte 
// based on the corresponding numerical value
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
   dprintf("...... bits to byte: %d\n", char_byte);
   return char_byte;
}

// Set the n bits of bitmap to be one based on input locations
static int bitmap_set_nbits(int start, int max_nbit, int nbits, int* bit_address)
{
  /* YOUR CODE */
  dprintf("... bitmap_set_nbits: start %d, total number of sector bits %d, total number of bits %d\n", start, max_nbit, nbits);
  
  // Initialize parameters
  int remaining_bit=nbits;
  int ibit;
  int isector;
  int ibyte;

  char buf[SECTOR_SIZE];
  int bits[8];
  int flag_flip=0;
  int flag_sector_write=0;
  int ibyte_end_bit=8;
  unsigned char byte_128=128;
  unsigned char all_zeros=0;

  for (int i=0; i<SECTOR_SIZE; i++)
  {
     buf[i]=all_zeros;
  }

  // Check if the input bits are between 0 and maximum bits
  for (int i=0; i<nbits; i++)
  {
    if(bit_address[i]>max_nbit || bit_address[i]<0)
    {
      dprintf("...... %d input bit of %d is out of bound\n", i, bit_address[i]);
      return -1;
    }
  }

  while(remaining_bit>0)
  {

    // Find the sector and byte of the bit
    ibit=bit_address[nbits-remaining_bit];
    isector = start+ibit/(SECTOR_SIZE*8);
    ibyte= (ibit-(isector-start)*(SECTOR_SIZE*8))/8;
    dprintf("...... the bit of %d is located at %d sector, %d byte\n", ibit, isector, ibyte);

    // Read the sector
    //Disk_Read(isector, buf); 

    flag_sector_write=0;
    // Check each byte and flip the corresponding bits
    while (remaining_bit>0 && ibyte<SECTOR_SIZE)
    {
      flag_flip=0;

      // Find the end bit in a byte
      if(remaining_bit<8)
      {
        ibyte_end_bit=remaining_bit;
      }
      else
      {
        ibyte_end_bit=8;
      }
      //dprintf("...... the ibyte_end_bit of %d\n", ibyte_end_bit);
      dprintf("...... starting bit %d, ending bit %d, remaining bit %d %d %d\n", bit_address[nbits-remaining_bit], bit_address[nbits-1], remaining_bit,(ibyte+1)*8+(isector-start)*SECTOR_SIZE*8, ibyte*8+(isector-start)*SECTOR_SIZE*8);
      if(bit_address[nbits-remaining_bit]<=(ibyte+1)*8+(isector-start)*SECTOR_SIZE*8 && bit_address[nbits-1]>=ibyte*8+(isector-start)*SECTOR_SIZE*8)
      {
        dprintf("...... test\n");
        // Do the following only when there are some bits in this byte
        for(int k=0;k<8;k++)
        {
          bits[k]=!!(((unsigned char)buf[ibyte]<< k) & byte_128);  
          dprintf("...... %d bit of %d byte is %d\n", k, ibyte, bits[k]);

          for (int l=0; l<ibyte_end_bit; l++)
          {
            //dprintf("....... location of bit %d is %d\n", l, bit_address[nbits-remaining_bit+l]);
            if(bit_address[nbits-remaining_bit+l]==k+ibyte*8+(isector-start)*SECTOR_SIZE*8)
            {
              // Set up the bit
              bits[k]=1;
              flag_flip++; 
              dprintf("...... %d bit of %d byte is changed\n", k, ibyte);
            }  
          }
        }
        if(flag_flip>0)
        {
          dprintf("...... byte before flip:");
          for (int m=0;m<8;m++)
          {
            dprintf("%d", !!((((unsigned char)buf[ibyte])<< m) & byte_128));
          }
          dprintf("\n"); 

          buf[ibyte]=bits_to_byte(bits);
          dprintf("...... byte after flip:");
          for (int m=0;m<8;m++)
          {
            dprintf("%d", !!((((unsigned char)buf[ibyte])<< m) & byte_128));
          }
          dprintf("\n"); 
          remaining_bit=remaining_bit-flag_flip; 
          flag_sector_write=1;
        }
      }
      ibyte++;
    }
    //if(flag_sector_write==1)
    //{
    //  // Write the flipped sector back to disk
    //  Disk_Write(isector, buf);
    //}
  }
  return -1;
}

int main()
{
   int newsec = 0;
   int n=20;
   int start=250+512*2;
   int bit_addr[n];  
   for(int i=0; i<n; i++)
   {
     bit_addr[i]=i*3+start;
   }
   
   newsec=bitmap_set_nbits(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SIZE, n,bit_addr);
   printf("... formatted sector bitmap (start=%d, size=%d)\n",
	     (int)SECTOR_BITMAP_START_SECTOR, (int)n);
   printf("ibit: %d\n", newsec);   
   return 0;
}

