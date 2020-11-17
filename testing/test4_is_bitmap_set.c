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

// Check if a sepcific bit (either inode or sector number) in a bitmap is used or not
int is_bitmap_set(int start, int nbits, int bit_num)
{
  dprintf("... is_bitmap_set: start %d, nbits %d, bit number %d\n", start, nbits, bit_num);
  
  unsigned char all_ones=255;

  // invalid inode or sector number
  if(bit_num<0 || bit_num>=nbits*8)
  {
    dprintf("...... invalid bit number\n");
    return -1;
  }
 
  // Find the sector that this bit is located
  int bit_sector = start+bit_num/(SECTOR_SIZE*8);
  int bit_start_entry = (bit_sector-start)*(SECTOR_SIZE*8);
  int offset = (bit_num-bit_start_entry)/8;
  dprintf("...... the bit is located at %d sector and %d byte\n", bit_sector, offset); 
  // Read the sector
  char buf[SECTOR_SIZE];
  //Disk_Read(bit_sector, buf); 

  for (int i=0; i<SECTOR_SIZE; i++)
  {
     buf[i]=all_ones;
  }
  buf[0]=127;
 
  // The byte and the bit that the bit is located
  unsigned char bit_byte;
  unsigned char byte_128=128;

  bit_byte=(unsigned char)buf[offset];
  dprintf("...... the byte is:\n");
  for (int m=0;m<8;m++)
  {
    dprintf("%d", !!(((unsigned char)bit_byte<< m) & byte_128));
  }
  dprintf("\n"); 

  int k=bit_num-offset*8;
  dprintf("...... the bit is %d\n",!!((bit_byte<<k) & byte_128));
  dprintf("...... the location in a byte is %d\n",k);
  if(!!((bit_byte<<k) & byte_128)==1)
  {
    // The input bit is one
    dprintf("...... the input bit is 1\n");
    return 1;
  }
  else
  {
    // The input bit is zero
    dprintf("...... the input bit is 0\n");
    return 0;
  }

  return -1;
}

int main()
{
   int newsec = 0;
   newsec=is_bitmap_set(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SIZE,4097);
   printf("... formatted sector bitmap (start=%d, size=%d)\n",
	     (int)SECTOR_BITMAP_START_SECTOR, (int)SECTOR_BITMAP_SIZE);
   printf("ibit: %d\n", newsec);   
   return 0;
}

