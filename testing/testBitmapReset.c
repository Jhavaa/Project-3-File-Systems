#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define dprintf printf

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
   // Testing byte 
    printf("%d\n", char_byte);
   // dprintf("%d\n", char_byte);
    return char_byte;
}

// initialize a bitmap with 'num' sectors starting from 'start'
// sector; all bits should be set to zero except that the first
// 'nbits' number of bits are set to one
static void bitmap_init(int start, int num, int nbits)
{
  /* YOUR CODE */
  char buf[SECTOR_SIZE];
  unsigned char all_ones=255;
  unsigned char all_zeros=0;
  int start_sector;

  // Find the number of sectors and bytes with all the bit values of 1
  int num_full_sectors=(nbits/8)/SECTOR_SIZE;
  printf("full sector number: %d\n",num_full_sectors);
  if(num_full_sectors>num) 
       printf("Error in writing initialized bitmap to disk");
       //dprintf("Error in writing initialized bitmap to disk");

  //for (int i=0; i<SECTOR_SIZE;i++)
  //{
  //   buf[i]=all_ones;
  //}
  memset(buf, all_ones, SECTOR_SIZE);
  for (int j=0; j<num_full_sectors; j++)
  {
     printf("%d\n",j);
     //if(Disk_Write(start+j, buf) < 0) return -1;
  }

  // For the sector with partial full bytes
  // Bytes with all ones
  start_sector=start+num_full_sectors;
  int num_full_bytes=(nbits-num_full_sectors*SECTOR_SIZE*8)/8;
  for (int i=0; i<num_full_bytes;i++)
  {
      buf[i]=all_ones;
  }
  printf("Number of full bytes: %d\n",num_full_bytes);
  // The byte that is between zero and all ones
  int remaining_bits=nbits-num_full_sectors*SECTOR_SIZE*8-num_full_bytes*8;
  int bits_array[8];
  for (int i=0; i<remaining_bits;i++)
  {
    bits_array[i]=1;
  }
  for (int i=remaining_bits; i<8;i++)
  {
    bits_array[i]=0;
  }
  buf[num_full_bytes]=bits_to_byte(bits_array);
  printf("Remaining bits with 1:%d, Partial byte:%c\n",remaining_bits, buf[num_full_bytes]);
   for (int i=0;i<8;i++)
    {
       printf("%d", !!((buf[num_full_bytes] << i) & 0x80));
    }
    printf("\n");

  // Bytes with all zeors
  for (int j=num_full_bytes+1;j<SECTOR_SIZE;j++)
  {
    buf[j]=all_zeros;
  }
  printf("Number of bytes with zeros:%d\n",num_full_bytes+1);
  //if(Disk_Write(start_sector, buf) < 0) return -1;

  // Sectors with all zeros
  start_sector=start_sector+1;
  //for (int i=0; i<SECTOR_SIZE;i++)
  //{
  //    buf[i]=0;
  //}
  memset(buf, all_zeros, SECTOR_SIZE);
  for (int j=0; j<num-num_full_sectors-1; j++)
  {
     //if(Disk_Write(start_sector+j, buf) < 0) return -1;
  }
  printf("Number of sectors with zeros:%d\n",num-num_full_sectors-1);
  //dprintf("... update bitmap sector is done\n");
}


// reset the i-th bit of a bitmap with 'num' sectors starting from
// 'start' sector; return 0 if successful, -1 otherwise
static int bitmap_reset(int start, int num, int ibit)
{
  /* YOUR CODE */

  dprintf("... bitmap_reset\n");
  dprintf("...... bitmap_reset: start=%d, num=%d, ibit=%d\n",start, num, ibit);

  char buf[SECTOR_SIZE];
  // int flag_reset=0;
  int bit_count = 0;
  // find the position of the bit that needs to reset
  int pos = ibit % 8;

  // Loop through all the bitmap sectors
  for (int i = start; i < num; i++)
  {
    // Save the ith sector into buf
    Disk_Read(i, buf);
    
    // Find the i-th bit of the sector
    for(int j = 0; j < SECTOR_SIZE; j++)
    {
      // reach the i-th bit
      // bit_count increments by 8 everytime we loop through another byte in the sector
      bit_count += 8;

      // if the bit_count passes our target or equals it, we've found our byte
      if(bit_count >= ibit)
      {
        // check the jth byte in buf
        unsigned char jth_byte=(unsigned char)buf[j];
        int bits[8];

        // save bits in array
        for(int k = 0; k < 8; k++)
        {
          bits[k] = (jth_byte << k) & 0x80;
        }

        // Reset bit in 'pos' position
        bits[pos] = 0;

        // Write back to disk
        buf[j]=bits_to_byte(bits);
        Disk_Write(i, buf);

        // Successful reset
        return 0;
      }
    }
  }
  // Unsuccessful reset
  return -1;
}

int main()
{
   int nbits=20;
   bitmap_init(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, 1);
      printf("... formatted inode bitmap (start=%d, num=%d)\n",
	     (int)INODE_BITMAP_START_SECTOR, (int)INODE_BITMAP_SECTORS);
      
      // format sector bitmap (reserve the first few sectors to
      // superblock, inode bitmap, sector bitmap, and inode table)
      bitmap_init(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS,
		  DATABLOCK_START_SECTOR);
     printf("... formatted sector bitmap (start=%d, num=%d,datablockstart=%d)\n",
	     (int)SECTOR_BITMAP_START_SECTOR, (int)SECTOR_BITMAP_SECTORS, (int)DATABLOCK_START_SECTOR);
   //printf("%d,%d\n", (nbits/8),(nbits/8/SECTOR_SIZE));
   return 0;
}

