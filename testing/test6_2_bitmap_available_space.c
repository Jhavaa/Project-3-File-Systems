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

// Find n first unused bits and return their addresses 
// Try to assign bits that are as continuous as possible
int bitmap_available_address(int start, int num_sectors, int bitmap_size, int nbits, int* nbits_address)
{
  dprintf("... bitmap_available_address: find %d unused bits: start sector %d, number of sectors %d, bitmap size %d\n", nbits, start, num_sectors, bitmap_size);

  // Initialize parameters
  // nbits_addr array to store unused bit segments: starting bit and length of unused bits
  int remaining_bits=nbits;
  int nbits_addr[bitmap_size][2];
  int unused_bit_seg=0;
  int unused_bit_len=0;
  int flag_seg_start=0;

  int flag_found=0;
  int ibyte=0;
  int isector=0;
  int ibit=0;

  char buf[SECTOR_SIZE];
  unsigned char all_ones=255;
  unsigned char all_zeros=0;
  unsigned char byte_128=128;

  // Loop through bitmap to find the first unused segment of bits that satisfy the criteria 
  // If not found, list all unused bits
  while (flag_found==0 && isector<num_sectors)
  {
    // Read one sector of bitmap
    //Disk_Read(start+isector, buf); 

  for (int i=0; i<SECTOR_SIZE; i++)
  {
     if(i==2 && isector==1)
     {
       buf[i]=(unsigned char)7; //all_zeros;
          for (int m=0;m<8;m++)
          {
            dprintf("%d", !!((((unsigned char)buf[i])<< m) & byte_128));
          }
          dprintf("\n"); 
     }
     else if(i==2)
     {
       buf[i]=(unsigned char)31;
     }
     else
     {
       buf[i]=all_ones;
     }
  }

    
    // Scan the sector for unused bits
    ibyte=0;
    while (flag_found==0 && ibyte<SECTOR_SIZE)
    {
       if((unsigned char)buf[ibyte]!=all_ones && isector*SECTOR_SIZE+ibyte<bitmap_size)
       {
         ibit=0;
         while (ibit<8)
         {
           if(!!(((unsigned char)buf[ibyte]<< ibit) & byte_128)==0)
           {
             // With unused bits
             if (flag_seg_start==0)
             {
               // New unused bit segment
               flag_seg_start=1;
               unused_bit_seg++;
               nbits_addr[unused_bit_seg-1][0]=isector*SECTOR_SIZE*8+ibyte*8+ibit;
               unused_bit_len++;
             }
             else
             {
               // Continue with unused bit segment
               unused_bit_len++;
             }
           }
           else 
           {
             if (flag_seg_start==1)
             {
               // End of the unused segment
               flag_seg_start=0;
               nbits_addr[unused_bit_seg-1][1]=unused_bit_len;

               // Check if the length satisfies the required number of bits
               if (unused_bit_len==nbits)
               {
                 // Find the required unused bits
                 flag_found=1;
                 for (int i=0; i<nbits; i++)
                 {
                   nbits_address[i]=nbits_addr[unused_bit_seg-1][0]+i;
                 }
                 dprintf("...... find continous %d unused bits at starting location %d\n",nbits, nbits_addr[unused_bit_seg-1][0]);
               }
               unused_bit_len=0;
             }
           }
           ibit++;
         }  
       }
       else if ((unsigned char)buf[ibyte]==all_ones && isector*SECTOR_SIZE+ibyte<bitmap_size)
       {
         if (flag_seg_start==1)
         {
           // End of the unused segment
           flag_seg_start=0;
           nbits_addr[unused_bit_seg-1][1]=unused_bit_len;  

           // Check if the length satisfies the required number of bits
           if (unused_bit_len==nbits)
           {
             // Find the required unused bits             
             flag_found=1;
             for (int i=0; i<nbits; i++)
             {
               nbits_address[i]=nbits_addr[unused_bit_seg-1][0]+i;
             }
             dprintf("...... find continous %d unused bits at starting location %d\n",nbits, nbits_addr[unused_bit_seg-1][0]);
           }   
           unused_bit_len=0;        
         }
       }
       ibyte++;
    }
    isector++;
  } 

  for(int ii=0; ii<unused_bit_seg; ii++)
  {
    dprintf("...... %d %d %d\n", ii, nbits_addr[ii][0], nbits_addr[ii][1]);
  }

  int iseg=0;
  //int ilen=0;
  if(flag_found==1)
  {
    // Find exactly matched nbits
    dprintf("...... find the exact %d unused bit\n",nbits);
    return 1;
  }
  else 
  {
    // Not find the exacted matched nbits

    // Check if the total number of unused bits is enough or not
    int sum_unused_bits=0;
    for (int i=0; i<unused_bit_seg; i++)
    {
       sum_unused_bits+=nbits_addr[i][1];
    }
   
    if (sum_unused_bits<nbits)
    {
      // Not enough nbits
      dprintf("...... not enough bits: requested %d and have %d unused bits\n", nbits, sum_unused_bits);
      return -1;
    }
    else
    {
      // Enough nbits

      // No exact match and check if any segment with enough bits
      // Assign the first segment with enough bits
      iseg=0;
      while (iseg<unused_bit_seg && flag_found==0)
      {
        if(nbits_addr[iseg][1]>nbits)
        {
          // Find the required unused bits
          flag_found=1;
          for (int i=0; i<nbits; i++)
          {
            nbits_address[i]=nbits_addr[iseg][0]+i;
          }
          dprintf("...... find continous %d unused bits at starting location %d\n",nbits, nbits_addr[iseg][0]);
          return 1;
        }
        iseg++;
      }

      if (flag_found==0)
      {
         // No segments with enough bits

         // Loop through the free bitmap segment array to assign the first unused bits
         iseg=0;
         while(flag_found==0 && iseg<unused_bit_seg)
         {
           if(nbits_addr[iseg][1]<remaining_bits)
           {
             for(int i=0; i<nbits_addr[iseg][1]; i++)
             {
               nbits_address[nbits-remaining_bits]=nbits_addr[iseg][0]+i;
               remaining_bits--;
             }
           }
           else
           {
             for(int i=0; i<remaining_bits; i++)
             {
               nbits_address[nbits-remaining_bits]=nbits_addr[iseg][0]+i;
               remaining_bits--;
             }   
             flag_found=1;
           }
           iseg++;           
         }
     
         dprintf("...... find the %d uncontinuous unused bits\n",nbits);
         return 1;
      }      
    }
  }
  return -1;
}

int main()
{
   int newsec = 0;
   int n=5;
   int bit_addr[n]; 
 
   newsec=bitmap_available_address(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS,SECTOR_BITMAP_SIZE,n,bit_addr);
   printf("... formatted sector bitmap (start=%d, max_byte=%d, size=%d)\n",
	     (int)SECTOR_BITMAP_START_SECTOR, (int)SECTOR_BITMAP_SIZE, (int)n);
   printf("ibit: %d\n", newsec);  
   for(int i=0; i<n; i++)
   {
     dprintf("... %d: %d\n", i, bit_addr[i]);
   }
   return 0;
}

