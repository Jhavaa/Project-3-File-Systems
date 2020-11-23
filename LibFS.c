#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "LibDisk.h"
#include "LibFS.h"

// set to 1 to have detailed debug print-outs and 0 to have none
#define FSDEBUG 1

#if FSDEBUG
#define dprintf printf
#else
#define dprintf noprintf
void noprintf(char* str, ...) {}
#endif

// the file system partitions the disk into five parts:

// 1. the superblock (one sector), which contains a magic number at
// its first four bytes (integer)
#define SUPERBLOCK_START_SECTOR 0

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

// size of each entry in dir
// Each entry is of size 20 bytes and contains 16-byte names of the files (or directories) 
// within the directory named by path, followed by the 4-byte integer inode number.
#define FILE_OR_DIR_ENTRY_SIZE 20

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

/* the following functions are internal helper functions */

// check magic number in the superblock; return 1 if OK, and 0 if not
static int check_magic()
{
  char buf[SECTOR_SIZE];
  if(Disk_Read(SUPERBLOCK_START_SECTOR, buf) < 0)
    return 0;
  if(*(int*)buf == OS_MAGIC) return 1;
  else return 0;
}

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

// initialize a bitmap with 'num' sectors starting from 'start'
// sector; all bits should be set to zero except that the first
// 'nbits' number of bits are set to one
static void bitmap_init(int start, int num, int nbits)
{
  /* YOUR CODE */

  dprintf("... bitmap_init\n");
  dprintf("...... start=%d, num=%d, nbits=%d\n", start, num, nbits);

  char buf[SECTOR_SIZE];
  unsigned char all_ones=255;
  //unsigned char all_zeros=0;
  int start_sector;

  // Find the number of sectors and bytes with all the bit values of 1
  int num_full_sectors=(nbits/8)/SECTOR_SIZE;
  dprintf("...... full sector number: %d\n",num_full_sectors);
  memset(buf, all_ones, SECTOR_SIZE);
  for (int j=0; j<num_full_sectors; j++)
  {
     if(Disk_Write(start+j, buf) < 0) 
       dprintf("Error in writing initialized bitmap to disk");
  }

  // For the sector with partially full bytes
  // Bytes with all ones
  start_sector=start+num_full_sectors;
  int num_full_bytes=(nbits-num_full_sectors*SECTOR_SIZE*8)/8;
  dprintf("...... number of full bytes: %d\n",num_full_bytes);
  for (int i=0; i<num_full_bytes;i++)
  {
      buf[i]=all_ones;
  }
  // The byte that is between zero and all ones
  int remaining_bits=nbits-num_full_sectors*SECTOR_SIZE*8-num_full_bytes*8;
  dprintf("...... remaining bits:%d\n",remaining_bits); 
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
  // Bytes with all zeors
  for (int j=num_full_bytes+1;j<SECTOR_SIZE;j++)
  {
    //buf[j]=all_zeros;
    buf[j]=0;
  }
  if(Disk_Write(start_sector, buf) < 0) 
       dprintf("Error in writing initialized bitmap to disk");

  // Sectors with all zeros
  start_sector=start_sector+1;
  //memset(buf, all_zeros, SECTOR_SIZE);
  memset(buf, 0, SECTOR_SIZE);
  for (int j=0; j<num-num_full_sectors-1; j++)
  {
     if(Disk_Write(start_sector+j, buf) < 0)        
       dprintf("Error in writing initialized bitmap to disk");
  }
  dprintf("...... update bitmap sector is done\n");
}

// set the first unused bit from a bitmap of 'nbits' bits (flip the
// first zero appeared in the bitmap to one) and return its location;
// return -1 if the bitmap is already full (no more zeros)
static int bitmap_first_unused(int start, int num, int nbits)
{
  /* YOUR CODE */

  dprintf("... bitmap_first_unused\n");
  dprintf("...... bitmap_first_unused: start=%d, num=%d, nbits=%d\n",start, num, nbits);

  char buf[SECTOR_SIZE];
  unsigned char all_ones=255;
  unsigned char byte_128=128;

  // ending_byte is used to determine the ending location of bitmap
  // a bitmap may not occupy the whole sector
  int ending_byte=SECTOR_SIZE;
  int flag_flip=0;
  int ibit=-1;

  // Loop through all the bitmap sectors
  for (int i=0; i<num; i++)
  {
    Disk_Read(start+i, buf);
    // Find the ending location of bitmap in each sector
    if (i<num-1)
    {
      ending_byte=SECTOR_SIZE;
    }
    else
    {
      ending_byte=nbits-i*SECTOR_SIZE;
      dprintf("...... ending byte is %d\n",ending_byte);
    }
    for (int j=0; j<ending_byte; j++)
    {
       unsigned char unfull_byte=(unsigned char)buf[j];
       if(unfull_byte!=all_ones)
       {
         // Not full byte
         
         // Byte before flip
         dprintf("...... byte before flip:");
         for (int m=0;m<8;m++)
         {
             dprintf("%d", !!((unfull_byte<< m) & byte_128));
         }
         dprintf("\n");

         int bits[8];
         for (int k=0; k<8; k++)
         {
            bits[k]=!!((unfull_byte<< k) & byte_128);  
            if(bits[k]==0 && flag_flip==0)
            {
               // Flip the bit
               bits[k]=1; 
               flag_flip=1; 
               // Bit location
               ibit=i*SECTOR_SIZE*8+j*8+k;
               dprintf("...... first unused bit: %d\n", ibit);       
            }
         }
         if(flag_flip==1)
         {
           buf[j]=bits_to_byte(bits);
           dprintf("...... byte after flip:");
           for (int m=0;m<8;m++)
           {
             dprintf("%d", !!((((unsigned char)buf[j])<< m) & byte_128));
           }
           dprintf("\n");           
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
       Disk_Write(start+i, buf);
       return ibit;
    }
  }
  return -1;
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
  for (int i = start; i <= num; i++)
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

	dprintf("\nbits array: ");

        // save bits in array
        for(int k = 0; k < 8; k++)
        {
          bits[k] = !!((jth_byte << k) & 0x80);
        }

        // Reset bit in 'pos' position
        bits[pos] = 0;

	unsigned char byte_128 = 128;
	dprintf("\n... Before reset:");
        for (int m=0;m<8;m++)
        {
            dprintf("%d", !!((((unsigned char)buf[j])<< m) & byte_128));
        }

        // Write back to disk
        buf[j]=bits_to_byte(bits);

	dprintf("\n... After reset: ");
        for (int m=0;m<8;m++)
        {
            dprintf("%d", !!((((unsigned char)buf[j])<< m) & byte_128));
        }
        
	Disk_Write(i, buf);

        // Successful reset
        return 0;
      }
    }
    bit_count = 0;
  }
  // Unsuccessful reset
  return -1;
}

// Check if a sepcific bit (either inode or sector number) in a bitmap is used or not
int is_bitmap_set(int start, int nbits, int bit_num)
{
  dprintf("... is_bitmap_set: start %d, nbits %d, bit number %d\n", start, nbits, bit_num);
  
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
  Disk_Read(bit_sector, buf); 

  // The byte and the bit that an input bit is located
  unsigned char bit_byte;
  unsigned char byte_128=128;

  bit_byte=(unsigned char)buf[offset];
  dprintf("...... the byte is:");
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

// Set the n bits of bitmap to be one based on input locations
static int bitmap_set_nbits(int start, int max_nbit, int nbits, int* bit_address)
{
  /* YOUR CODE */
  dprintf("... bitmap_set_nbits: start %d, total number of sector bits %d, total number of bits %d\n", start, max_nbit, nbits);
  
  // Initialize parameters
  int remaining_bit=nbits;
  int ibit;
  int isector=start;
  int ibyte;

  char buf[SECTOR_SIZE];
  int bits[8];
  int flag_flip=0;
  int flag_sector_write=0;
  int ibyte_end_bit=8;
  unsigned char byte_128=128;

  // Check if the input bits are between 0 and maximum bits
  for (int i=0; i<nbits; i++)
  {
    if(bit_address[i]>(max_nbit*8) || bit_address[i]<0)
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
    Disk_Read(isector, buf); 
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

      if(bit_address[nbits-remaining_bit]<=(ibyte+1)*8+(isector-start)*SECTOR_SIZE*8 && bit_address[nbits-1]>=ibyte*8+(isector-start)*SECTOR_SIZE*8)
      {
        // Do the following only when there are some bits in this byte
        for(int k=0;k<8;k++)
        {
          bits[k]=!!(((unsigned char)buf[ibyte]<< k) & byte_128);  
          //dprintf("...... %d bit of %d byte is %d\n", k, ibyte, bits[k]);

          for (int l=0; l<ibyte_end_bit; l++)
          {
            //dprintf("....... location of bit %d is %d\n", l, bit_address[nbits-remaining_bit+l]);
            if(bit_address[nbits-remaining_bit+l]==k+ibyte*8+(isector-start)*SECTOR_SIZE*8)
            {
              // Set up the bit
              bits[k]=1;
              flag_flip++; 
              //dprintf("...... %d bit of %d byte is changed\n", k, ibyte);
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
    if(flag_sector_write==1)
    {
      // Write the flipped sector back to disk
      Disk_Write(isector, buf);      
    }
  }
  return 0;
}

// Find n first unused bits and return their addresses 
// Try to assign bits that are as continuous as possible
int bitmap_available_address(int start, int num_sectors, int bitmap_size, int nbits, int* nbits_address)
{
  dprintf("... bitmap_available_address: need to find %d unused bits\n",nbits);
  dprintf("...... start sector %d, number of sectors %d, bitmap size %d\n", start, num_sectors, bitmap_size);

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
  unsigned char byte_128=128;

  // Loop through bitmap to find the first unused segment of bits that satisfy the criteria 
  // If not found, list all unused bits
  while (flag_found==0 && isector<num_sectors)
  {
    //dprintf("...... sector number: %d\n", isector);

    // Read one sector of bitmap
    Disk_Read(start+isector, buf); 
    
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
               dprintf("...... length of holes: %d, %d, %d\n", unused_bit_seg, unused_bit_len, nbits_addr[unused_bit_seg-1][1]);
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

  // For the last segment with available bits
  if(flag_seg_start==1)
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

// return 1 if the file name is illegal; otherwise, return 0; legal
// characters for a file name include letters case sensitive),
// numbers, dots, dashes, and underscores; and a legal file name
// should not be more than MAX_NAME-1 in length
static int illegal_filename(char* name)
{
  /* YOUR CODE */
 
  /* 1. Identify all legal characters for a file name.
      - An array of legal characters can be used to check each character in a file name.
      - Possibly regex can be used.
     2. Make sure file name is less than MAX_NAME - 1 in length.
      - This should be the first check to avoid any wasted time.*/
 
  char* legal = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-_";
 
  // Check if the length of name is less than MAX_NAME - 1
  //If yes, enter the if statement. Otherwise, return 1.
  if(strlen(name) <= MAX_NAME - 1 && strlen(name) > 0){
    // Begin checking if characters are legal
    int i;
    for(i = 0; i < strlen(name); i++){
      // Check if name[i] is in the legal char* and return the position.
      //if the character is not in legal, pos == NULL and returns 1.
      if(strchr(legal, name[i]) == NULL){
        break;
      }
    }
    if(i == strlen(name)){
      return 0;
    }
  }
  return 1; 
}


// return the child inode of the given file name 'fname' from the
// parent inode; the parent inode is currently stored in the segment
// of inode table in the cache (we cache only one disk sector for
// this); once found, both cached_inode_sector and cached_inode_buffer
// may be updated to point to the segment of inode table containing
// the child inode; the function returns -1 if no such file is found;
// it returns -2 is something else is wrong (such as parent is not
// directory, or there's read error, etc.)
static int find_child_inode(int parent_inode, char* fname,
			    int *cached_inode_sector, char* cached_inode_buffer)
{
  int cached_start_entry = ((*cached_inode_sector)-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  int offset = parent_inode-cached_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* parent = (inode_t*)(cached_inode_buffer+offset*sizeof(inode_t));
  dprintf("... load parent inode: %d (size=%d, type=%d)\n",
	 parent_inode, parent->size, parent->type);
  if(parent->type != 1) {
    dprintf("... parent not a directory\n");
    return -2;
  }

  int nentries = parent->size; // remaining number of directory entries 
  int idx = 0;
  while(nentries > 0) {
    char buf[SECTOR_SIZE]; // cached content of directory entries
    if(Disk_Read(parent->data[idx], buf) < 0) return -2;
    for(int i=0; i<DIRENTS_PER_SECTOR; i++) {
      if(i>nentries) break;
      if(!strcmp(((dirent_t*)buf)[i].fname, fname)) {
	// found the file/directory; update inode cache
	int child_inode = ((dirent_t*)buf)[i].inode;
	dprintf("... found child_inode=%d\n", child_inode);
	int sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
	if(sector != (*cached_inode_sector)) {
	  *cached_inode_sector = sector;
	  if(Disk_Read(sector, cached_inode_buffer) < 0) return -2;
	  dprintf("... load inode table for child\n");
	}
	return child_inode;
      }
    }
    idx++; nentries -= DIRENTS_PER_SECTOR;
  }
  dprintf("... could not find child inode\n");
  return -1; // not found
}

// follow the absolute path; if successful, return the inode of the
// parent directory immediately before the last file/directory in the
// path; for example, for '/a/b/c/d.txt', the parent is '/a/b/c' and
// the child is 'd.txt'; the child's inode is returned through the
// parameter 'last_inode' and its file name is returned through the
// parameter 'last_fname' (both are references); it's possible that
// the last file/directory is not in its parent directory, in which
// case, 'last_inode' points to -1; if the function returns -1, it
// means that we cannot follow the path
static int follow_path(char* path, int* last_inode, char* last_fname)
{
  if(!path) {
    dprintf("... invalid path\n");
    return -1;
  }
  if(path[0] != '/') {
    dprintf("... '%s' not absolute path\n", path);
    return -1;
  }
  
  // make a copy of the path (skip leading '/'); this is necessary
  // since the path is going to be modified by strsep()
  char pathstore[MAX_PATH]; 
  strncpy(pathstore, path+1, MAX_PATH-1);
  pathstore[MAX_PATH-1] = '\0'; // for safety
  char* lpath = pathstore;
  
  int parent_inode = -1, child_inode = 0; // start from root
  // cache the disk sector containing the root inode
  int cached_sector = INODE_TABLE_START_SECTOR;
  char cached_buffer[SECTOR_SIZE];
  if(Disk_Read(cached_sector, cached_buffer) < 0) return -1;
  dprintf("... load inode table for root from disk sector %d\n", cached_sector);
  
  // for each file/directory name separated by '/'
  char* token;
  while((token = strsep(&lpath, "/")) != NULL) {
    dprintf("... process token: '%s'\n", token);
    if(*token == '\0') continue; // multiple '/' ignored
    if(illegal_filename(token)) {
      dprintf("... illegal file name: '%s'\n", token);
      return -1; 
    }
    if(child_inode < 0) {
      // regardless whether child_inode was not found previously, or
      // there was issues related to the parent (say, not a
      // directory), or there was a read error, we abort
      dprintf("... parent inode can't be established\n");
      return -1;
    }
    parent_inode = child_inode;
    child_inode = find_child_inode(parent_inode, token,
				   &cached_sector, cached_buffer);
    if(last_fname) strcpy(last_fname, token);
  }
  if(child_inode < -1) return -1; // if there was error, abort
  else {
    // there was no error, several possibilities:
    // 1) '/': parent = -1, child = 0
    // 2) '/valid-dirs.../last-valid-dir/not-found': parent=last-valid-dir, child=-1
    // 3) '/valid-dirs.../last-valid-dir/found: parent=last-valid-dir, child=found
    // in the first case, we set parent=child=0 as special case
    if(parent_inode==-1 && child_inode==0) parent_inode = 0;
    dprintf("... found parent_inode=%d, child_inode=%d\n", parent_inode, child_inode);
    *last_inode = child_inode;
    return parent_inode;
  }
}

// add a new file or directory (determined by 'type') of given name
// 'file' under parent directory represented by 'parent_inode'
int add_inode(int type, int parent_inode, char* file)
{
  // get a new inode for child
  int child_inode = bitmap_first_unused(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, INODE_BITMAP_SIZE);
  if(child_inode < 0) {
    dprintf("... error: inode table is full\n");
    return -1; 
  }
  dprintf("... new child inode %d\n", child_inode);

  // load the disk sector containing the child inode
  int inode_sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
  char inode_buffer[SECTOR_SIZE];
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for child inode from disk sector %d\n", inode_sector);

  // get the child inode
  int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  int offset = child_inode-inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* child = (inode_t*)(inode_buffer+offset*sizeof(inode_t));

  // update the new child inode and write to disk
  memset(child, 0, sizeof(inode_t));
  child->type = type;
  if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... update child inode %d (size=%d, type=%d), update disk sector %d\n",
	 child_inode, child->size, child->type, inode_sector);

  // get the disk sector containing the parent inode
  inode_sector = INODE_TABLE_START_SECTOR+parent_inode/INODES_PER_SECTOR;
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for parent inode %d from disk sector %d\n",
	 parent_inode, inode_sector);

  // get the parent inode
  inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  offset = parent_inode-inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* parent = (inode_t*)(inode_buffer+offset*sizeof(inode_t));
  dprintf("... get parent inode %d (size=%d, type=%d)\n",
	 parent_inode, parent->size, parent->type);

  // get the dirent sector
  if(parent->type != 1) {
    dprintf("... error: parent inode is not directory\n");
    return -2; // parent not directory
  }
  int group = parent->size/DIRENTS_PER_SECTOR;
  char dirent_buffer[SECTOR_SIZE];
  if(group*DIRENTS_PER_SECTOR == parent->size) {
    // new disk sector is needed
    int newsec = bitmap_first_unused(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE);
    if(newsec < 0) {
      dprintf("... error: disk is full\n");
      return -1;
    }
    parent->data[group] = newsec;
    memset(dirent_buffer, 0, SECTOR_SIZE);
    dprintf("... new disk sector %d for dirent group %d\n", newsec, group);
  } else {
    if(Disk_Read(parent->data[group], dirent_buffer) < 0)
      return -1;
    dprintf("... load disk sector %d for dirent group %d\n", parent->data[group], group);
  }

  // add the dirent and write to disk
  int start_entry = group*DIRENTS_PER_SECTOR;
  offset = parent->size-start_entry;
  dirent_t* dirent = (dirent_t*)(dirent_buffer+offset*sizeof(dirent_t));
  strncpy(dirent->fname, file, MAX_NAME);
  dirent->inode = child_inode;
  if(Disk_Write(parent->data[group], dirent_buffer) < 0) return -1;
  dprintf("... append dirent %d (name='%s', inode=%d) to group %d, update disk sector %d\n",
	  parent->size, dirent->fname, dirent->inode, group, parent->data[group]);

  // update parent inode and write to disk
  parent->size++;
  if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... update parent inode on disk sector %d\n", inode_sector);
  
  return 0;
}

// used by both File_Create() and Dir_Create(); type=0 is file, type=1
// is directory
int create_file_or_directory(int type, char* pathname)
{
  int child_inode;
  char last_fname[MAX_NAME];
  int parent_inode = follow_path(pathname, &child_inode, last_fname);
  dprintf("... parent inode: %d; child_node inode: %d\n", parent_inode, child_inode);
  if(parent_inode >= 0) {
    if(child_inode >= 0) {
      dprintf("... file/directory '%s' already exists, failed to create\n", pathname);
      osErrno = E_CREATE;
      return -1;
    } else {
      if(add_inode(type, parent_inode, last_fname) >= 0) {
	dprintf("... successfully created file/directory: '%s'\n", pathname);
	return 0;
      } else {
	dprintf("... error: something wrong with adding child inode\n");
	osErrno = E_CREATE;
	return -1;
      }
    }
  } else {
    dprintf("... error: something wrong with the file/path: '%s'\n", pathname);
    osErrno = E_CREATE;
    return -1;
  }
}

// remove the child from parent; the function is called by both
// File_Unlink() and Dir_Unlink(); the function returns 0 if success,
// -1 if general error, -2 if directory not empty, -3 if wrong type
int remove_inode(int type, int parent_inode, int child_inode)
{
  /* YOUR CODE */
  
  /*
     1. Load and find the child inode
     2. Reclaim file/directory/data
     3. Modify parent inode
     4. Update.
  */

  //// Load and find the child inode

  // load the disk sector containing the child inode



  // Think of it as pages in a book: The book starts after the table of contents
  //                                 Each page holds a certain number of lines
  //                                 We are looking for a specific line in the book
  // EX1: child_inode = 1, INODE_TABLE_START_SECTOR = 2, INODES_PER_SECTOR = 10
  // inode_sector = 2 + (1/10) == 2 + 0 == 2
  // the child node is in sector 2 and is the 2nd (2 % 10) inode there.
  //
  // EX2: child_inode = 24, INODE_TABLE_START_SECTOR = 2, INODES_PER_SECTOR = 10
  // inode_sector = 2 + (24/10) == 2 + 2 == 4
  // the child node is in sector 4 and is the 4th (24 % 10) inode there.
  int inode_sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;

  // a char object array that can hold all the vaules from inode_sector
  // ** The number of bytes (8 bits) in a sector is defined in SECTOR_SIZE
  char inode_buffer[SECTOR_SIZE];

  // copy the data from the inode_sector sector (inode_sector is the number of the sector) into inode_buffer
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for child inode from disk sector %d\n", inode_sector);

  // get the child inode



  // inode_start_entry is the position where the inode we are looking for is located.
  // EX1: child_inode = 1, INODE_TABLE_START_SECTOR = 2, INODES_PER_SECTOR = 10
  // inode_start_entry = (2 - 2) * 10 == 0 * 10 == 0
  // The inode we are looking for is in a sector with starting inode 0
  //
  // EX2: child_inode = 24, INODE_TABLE_START_SECTOR = 2, INODES_PER_SECTOR = 10
  // inode_start_entry = (4 - 2) * 10 == 2 * 10 == 20
  // The inode we are looking for is in a sector with starting inode 20
  int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;

  // offset is the offset value from the start of the sector.
  // EX1: child_inode = 1, INODE_TABLE_START_SECTOR = 2, INODES_PER_SECTOR = 10
  // offset = 1 - 0 == 1
  // the child_inode is 1 position away from the start of the sector

  // EX2: child_inode = 24, INODE_TABLE_START_SECTOR = 2, INODES_PER_SECTOR = 10
  // offset = 24 - 20 == 4
  // the child_inode is 4 positions away from 
  int offset = child_inode-inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);

  // child is an inode_t struct that points to the position of the child_inode in inode_buffer.
  // The value that is saved in inode_buffer is the value child holds.
  // We multiply offset by sizeof(inode_t), because sizeof(inode_t) is allocated for each inode entry in the sector.
  // EX: offset = 4
  // position = offset * sizeof(inode_t) == 4 * 88 == 352
  // there are 3 inodes prior to the one we are looking for, and they take up 352 bytes.
  // the inode we are looking for is located on the 352nd byte.
  inode_t* child = (inode_t*)(inode_buffer+offset*sizeof(inode_t));

  // if child inode is a directory
  if(child->type == 1 && type == 1)
  {
    // check if child->data is empty.
    int i, n;
    for(i = 0, n = 0; i < sizeof(child->data)/sizeof(child->data[0]) && n == 0; ++i)
    {
      if(child->data[i] != 0)
      {
        dprintf("... error: child inode is not empty\n");
        return -2;
      }
    }
    //// Reclaim directory
    // set child_inode to 0
    bitmap_reset(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, child_inode);
    //// Reclaim data
    memset(child, 0, sizeof(inode_t));
  }
  else if(child->type == 0 && type == 0)
  {
    //// Reclaim file
    bitmap_reset(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, child_inode);
    //// Reclaim data
    memset(child, 0, sizeof(inode_t));
  }
  else
  {
    dprintf("... error: child inode (type %d) is not type %d\n", child->type, type);
    return -3;
  }

  //// Update
  if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... update child inode on disk sector %d\n", inode_sector);
  

  // get the disk sector containing the parent inode
  inode_sector = INODE_TABLE_START_SECTOR+parent_inode/INODES_PER_SECTOR;
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for parent inode %d from disk sector %d\n",
	 parent_inode, inode_sector);

  // get the parent inode
  inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  offset = parent_inode-inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);

  // parent is an inode_t struct that points to the position of the parent_inode in inode_buffer.
  // The value that is saved in inode_buffer is the value parent holds.
  inode_t* parent = (inode_t*)(inode_buffer+offset*sizeof(inode_t));
  dprintf("... get parent inode %d (size=%d, type=%d)\n",
	 parent_inode, parent->size, parent->type);

  // get the dirent sector
  if(parent->type != 1) {
    dprintf("... error: parent inode is not directory\n");
    return -2; // parent not directory
  }

  // group is the sector that holds the last byte in parent
  // EX1: parent->size = 3, DIRENTS_PER_SECTOR = 20
  // group = parent->size/DIRENTS_PER_SECTOR == 3 / 20 == 0
  // parent is held in the first sector.
  // EX2: parent->size = 30, DIRENTS_PER_SECTOR = 20
  // group = parent->size/DIRENTS_PER_SECTOR == 30 / 20 == 1
  // parent is held in the first and second sector.
  // int group = parent->size/DIRENTS_PER_SECTOR;
  char dirent_buffer[SECTOR_SIZE];

  // Read parent sector (parent->data[group]) and write to dirent_buffer
  // if(Disk_Read(parent->data[group], dirent_buffer) < 0)
  //     return -1;

  // Find the dirent refering to the child_inode
  // start_entry is the starting sector for a new dirent.
  // int start_entry = group*DIRENTS_PER_SECTOR;
  // offset = parent->size-start_entry;
  // dirent_t* dirent = (dirent_t*)(dirent_buffer+offset*sizeof(dirent_t));

  // because inodes are unique, we can search for the file referred to
  ///by the child_inode by dirent inode.
  int group;
  dirent_t* dirent;
  offset = 0;
  // Loop through all the sectors that could contain the child_inode dirent data.
  for(group = 0; group <= parent->size/DIRENTS_PER_SECTOR; group++)
  {
    // read data blocks in parent directory to dirent_buffer
    if(Disk_Read(parent->data[group], dirent_buffer) < 0)
      return -1;
    // check each dirent in the sector
    for(offset = 0; offset < parent->size; offset++)
    {
      dirent = (dirent_t*)(dirent_buffer+offset*sizeof(dirent_t));
      // if dirent->inode == child_inode, then clear (reclaim) the data.
      if(dirent->inode == child_inode)
      {
        // memcpy(dirent, dirent + sizeof(dirent_t), (parent->size - offset) * sizeof(dirent_t));

        // This for loop is extremely important!!
        // The purpose of the for loop here is to reclaim the data by shifting all
        // all the values to the left by sizeof(dirent_t). We shift by sizeof(dirent_t)
        // because that is how much data we are going to need to fill in the previous
        // address.
        // parent->size - (offset + 1): this is the limit.
        //                              - parent->size is the total number of dirents in the parent directory.
        //                              - (offset + 1) is the position of the file to be removed. We add 1 to
        //                                the offset because the offset refers to how far the target is from
        //                                the starting address. If the offset is 4, then the actual position is 5
        //                                because the starting address holds the 0 position dirent which is counted
        //                                as 1 in parent->size.
        for(int j = 0; j < parent->size - (offset + 1); j++)
        {

          // memcpy is used here to copy data, from the right of the current dirent
          // address, to the current until the end of buffer is reached.
          // memcpy(destination, source, number of chars)
          //  destination = dirent + (j * sizeof(dirent_t))
          //    - dirent is the starting address where the child dirent data is stored
          //    - j is used to shift the current address we are replacing.
          //    - sizeof(dirent_t) is the number of bytes we are shifting by.
          //  source = dirent + (j + 1) * sizeof(dirent_t)
          //    - (j + 1) * sizeof(dirent_t) is the next dirent (source) after the one we want to replace (destination).
          //  number of chars = sizeof(dirent_t)
          //    - sizeof(dirent_t) is the number of bytes that we want to copy from source to destination.
          //      * this is also the number of bytes that makes up one dirent!!

          // *We could also just copy ((parent->size - (offset + 1)) * sizeof(dirent_t)) bytes, but doing so has shown to not work in special cases

          memcpy(dirent + (j * sizeof(dirent_t)), dirent + (j + 1) * sizeof(dirent_t), sizeof(dirent_t));
        }
    
      }
    }
    
    // memset(dirent, 0, sizeof(dirent_t));
    
    // if(Disk_Read(parent->data[group], dirent_buffer) < 0)
    //   return -1;
    
  }

  //// Modify parent inode
  parent->size--;

  //// Update
  if(Disk_Write(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... update parent inode on disk sector %d\n", inode_sector);


  return 0;
}

// representing an open file
typedef struct _open_file {
  int inode; // pointing to the inode of the file (0 means entry not used)
  int size;  // file size cached here for convenience
  int pos;   // read/write position
} open_file_t;
static open_file_t open_files[MAX_OPEN_FILES];

// return true if the file pointed to by inode has already been open
int is_file_open(int inode)
{
  for(int i=0; i<MAX_OPEN_FILES; i++) {
    if(open_files[i].inode == inode)
      return 1;
  }
  return 0;
}

// return a new file descriptor not used; -1 if full
int new_file_fd()
{
  for(int i=0; i<MAX_OPEN_FILES; i++) {
    if(open_files[i].inode <= 0)
      return i;
  }
  return -1;
}

/* end of internal helper functions, start of API functions */

int FS_Boot(char* backstore_fname)
{
  dprintf("FS_Boot('%s'):\n", backstore_fname);
  // initialize a new disk (this is a simulated disk)
  if(Disk_Init() < 0) {
    dprintf("... disk init failed\n");
    osErrno = E_GENERAL;
    return -1;
  }
  dprintf("... disk initialized\n");
  
  // we should copy the filename down; if not, the user may change the
  // content pointed to by 'backstore_fname' after calling this function
  strncpy(bs_filename, backstore_fname, 1024);
  bs_filename[1023] = '\0'; // for safety
  
  // we first try to load disk from this file
  if(Disk_Load(bs_filename) < 0) {
    dprintf("... load disk from file '%s' failed\n", bs_filename);

    // if we can't open the file; it means the file does not exist, we
    // need to create a new file system on disk
    if(diskErrno == E_OPENING_FILE) {
      dprintf("... couldn't open file, create new file system\n");

      // format superblock
      char buf[SECTOR_SIZE];
      memset(buf, 0, SECTOR_SIZE);
      *(int*)buf = OS_MAGIC;
      if(Disk_Write(SUPERBLOCK_START_SECTOR, buf) < 0) {
	dprintf("... failed to format superblock\n");
	osErrno = E_GENERAL;
	return -1;
      }
      dprintf("... formatted superblock (sector %d)\n", SUPERBLOCK_START_SECTOR);

      // format inode bitmap (reserve the first inode to root)
      bitmap_init(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SECTORS, 1);
      dprintf("... formatted inode bitmap (start=%d, num=%d)\n",
	     (int)INODE_BITMAP_START_SECTOR, (int)INODE_BITMAP_SECTORS);
      
      // format sector bitmap (reserve the first few sectors to
      // superblock, inode bitmap, sector bitmap, and inode table)
      bitmap_init(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS,
		  DATABLOCK_START_SECTOR);
      dprintf("... formatted sector bitmap (start=%d, num=%d)\n",
	     (int)SECTOR_BITMAP_START_SECTOR, (int)SECTOR_BITMAP_SECTORS);
      
      // format inode tables
      for(int i=0; i<INODE_TABLE_SECTORS; i++) {
	memset(buf, 0, SECTOR_SIZE);
	if(i==0) {
	  // the first inode table entry is the root directory
	  ((inode_t*)buf)->size = 0;
	  ((inode_t*)buf)->type = 1;
	}
	if(Disk_Write(INODE_TABLE_START_SECTOR+i, buf) < 0) {
	  dprintf("... failed to format inode table\n");
	  osErrno = E_GENERAL;
	  return -1;
	}
      }
      dprintf("... formatted inode table (start=%d, num=%d)\n",
	     (int)INODE_TABLE_START_SECTOR, (int)INODE_TABLE_SECTORS);
      
      // we need to synchronize the disk to the backstore file (so
      // that we don't lose the formatted disk)
      if(Disk_Save(bs_filename) < 0) {
	// if can't write to file, something's wrong with the backstore
	dprintf("... failed to save disk to file '%s'\n", bs_filename);
	osErrno = E_GENERAL;
	return -1;
      } else {
	// everything's good now, boot is successful
	dprintf("... successfully formatted disk, boot successful\n");
	memset(open_files, 0, MAX_OPEN_FILES*sizeof(open_file_t));
	return 0;
      }
    } else {
      // something wrong loading the file: invalid param or error reading
      dprintf("... couldn't read file '%s', boot failed\n", bs_filename);
      osErrno = E_GENERAL; 
      return -1;
    }
  } else {
    dprintf("... load disk from file '%s' successful\n", bs_filename);
    
    // we successfully loaded the disk, we need to do two more checks,
    // first the file size must be exactly the size as expected (thiis
    // supposedly should be folded in Disk_Load(); and it's not)
    int sz = 0;
    FILE* f = fopen(bs_filename, "r");
    if(f) {
      fseek(f, 0, SEEK_END);
      sz = ftell(f);
      fclose(f);
    }
    if(sz != SECTOR_SIZE*TOTAL_SECTORS) {
      dprintf("... check size of file '%s' failed\n", bs_filename);
      osErrno = E_GENERAL;
      return -1;
    }
    dprintf("... check size of file '%s' successful\n", bs_filename);
    
    // check magic
    if(check_magic()) {
      // everything's good by now, boot is successful
      dprintf("... check magic successful\n");
      memset(open_files, 0, MAX_OPEN_FILES*sizeof(open_file_t));
      return 0;
    } else {      
      // mismatched magic number
      dprintf("... check magic failed, boot failed\n");
      osErrno = E_GENERAL;
      return -1;
    }
  }
}

int FS_Sync()
{
  if(Disk_Save(bs_filename) < 0) {
    // if can't write to file, something's wrong with the backstore
    dprintf("FS_Sync():\n... failed to save disk to file '%s'\n", bs_filename);
    osErrno = E_GENERAL;
    return -1;
  } else {
    // everything's good now, sync is successful
    dprintf("FS_Sync():\n... successfully saved disk to file '%s'\n", bs_filename);
    return 0;
  }
}

int File_Create(char* file)
{
  dprintf("File_Create('%s'):\n", file);
  return create_file_or_directory(0, file);
}

/*This function is the opposite of File_Create(). This function should delete the file
  referenced by file, including removing its name from the directory it is in, and
  freeing up any data blocks and inodes that the file has been using. If the file does
  not currently exist, return -1 and set osErrno to E_NO_SUCH_FILE. If the file is
  currently open, return -1 and set osErrno to E_FILE_IN_USE (and do NOT delete the
  file). Upon success, return 0.*/
int File_Unlink(char* file)
{
  /* YOUR CODE */
  dprintf("File_Unlink('%s'):\n", file);

  //// Find the inode for the file
  // child_inode will be the inode of the file specified in the parameter of this function.
  int parent_inode, child_inode;

  // follow_path will return the parent inode of the file specified by the parameter of this function.
  // It will also store the child inode in the variable passed throught he second parameter.

  // file is the path of the file we want to unlink. Here we want the inode of the file and its parent.
  // EX. file = /path/to/file.c
  //     child path == /path/to/file.c
  //     parent path == /path/to
  parent_inode = follow_path(file, &child_inode, NULL);

  //// Check for errors:

  // "If the file does not currently exist, return -1 and set osErrno to E_NO_SUCH_FILE."
  if(child_inode == -1)
  {
    dprintf("The file does not exist.\n");
    osErrno = E_NO_SUCH_FILE;
    return -1;
  }

  // "If the file is currently open, return -1 and set osErrno to E_FILE_IN_USE (and do NOT delete the file)."
  if(is_file_open(child_inode))
  {
    dprintf("The file is currently open.\n");
    osErrno = E_FILE_IN_USE;
    return -1;
  }

  // Checks if follow_path function failed
  if(parent_inode == -1)
  {
    dprintf("Could not follow path (follow_path(%s, child_inode, NULL) failed)\n", file);
    return -1;
  }

  //// Update inode-related information

  // This function works under the assumption that the file being unlinked is 100% a file. dir_unlink assumes the opposite.
  //                        (This is checked in remove_inode, so that's totally fine)

  if(remove_inode(0, parent_inode, child_inode) == 0) return 0;

  return -1;
}

int File_Open(char* file)
{
  dprintf("File_Open('%s'):\n", file);
  int fd = new_file_fd();
  if(fd < 0) {
    dprintf("... max open files reached\n");
    osErrno = E_TOO_MANY_OPEN_FILES;
    return -1;
  }

  int child_inode;
  follow_path(file, &child_inode, NULL);
  if(child_inode >= 0) { // child is the one
    // load the disk sector containing the inode
    int inode_sector = INODE_TABLE_START_SECTOR+child_inode/INODES_PER_SECTOR;
    char inode_buffer[SECTOR_SIZE];
    if(Disk_Read(inode_sector, inode_buffer) < 0) { osErrno = E_GENERAL; return -1; }
    dprintf("... load inode table for inode from disk sector %d\n", inode_sector);

    // get the inode
    int inode_start_entry = (inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
    int offset = child_inode-inode_start_entry;
    assert(0 <= offset && offset < INODES_PER_SECTOR);
    inode_t* child = (inode_t*)(inode_buffer+offset*sizeof(inode_t));
    dprintf("... inode %d (size=%d, type=%d)\n",
	    child_inode, child->size, child->type);

    if(child->type != 0) {
      dprintf("... error: '%s' is not a file\n", file);
      osErrno = E_GENERAL;
      return -1;
    }

    // initialize open file entry and return its index
    open_files[fd].inode = child_inode;
    open_files[fd].size = child->size;
    open_files[fd].pos = 0;
    return fd;
  } else {
    dprintf("... file '%s' is not found\n", file);
    osErrno = E_NO_SUCH_FILE;
    return -1;
  }  
}

int File_Read(int fd, void* buffer, int size)
{
  /* YOUR CODE */
  dprintf("Read file with file id of %d and size of %d\n", fd, size);

  int actual_read_size=0;
  
  // Check if the file is able to open or not
  if(fd < 0) {
    dprintf("... cannot open file: input file id is negative\n");
    osErrno = E_BAD_FD;
    return -1;
  }
  else if (fd>=MAX_OPEN_FILES)
  {
    dprintf("... cannot read file: file id is greater than allowed maximum open files\n");
    osErrno = E_BAD_FD;
    return -1;
  }
  else if (open_files[fd].inode<=0)
  {
    dprintf("... cannot read file: input file with a negative or zero inode\n");
    osErrno = E_BAD_FD;
    return -1;    
  }
  else if (open_files[fd].inode>=MAX_FILES)
  {
    dprintf("... cannot read file: exceed maximum files\n");
    osErrno = E_BAD_FD;
    return -1;    
  }

  // Find the file inode
  int inode=open_files[fd].inode;
  int file_size=open_files[fd].size;
  int curr_position=open_files[fd].pos;

  // Check if the corresponding inode bitmap is set or not
  if(is_bitmap_set(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SIZE, inode)<1)
  {
    dprintf("... cannot read file: the corresponding inode is not set\n");
    osErrno = E_BAD_FD;
    return -1; 
  }

  // Check if the file pointer is at the end of the file, if yes, return 0 
  if (curr_position==file_size)
  {
   // End of the file
   dprintf("... the file pointer is at the end of the file\n");
   actual_read_size=0;
  }
  else
  {
    if((file_size-curr_position)>=size)
    {
       actual_read_size=size;
    }
    else 
    {
       actual_read_size=file_size-curr_position;
    }
    dprintf("... file size=%d, current position=%d, actual read size=%d\n", file_size, curr_position, actual_read_size);

    // Find the inode information from inode table
    // The sector where the inode is located 
    int inode_sector = INODE_TABLE_START_SECTOR+inode/INODES_PER_SECTOR;
    dprintf("... inode is located at disk sector %d\n", inode_sector);

    char inode_buf[SECTOR_SIZE];
    if(Disk_Read(inode_sector, inode_buf) < 0) 
    { 
        dprintf("... cannot load the sector where inode is located\n");
        osErrno = E_BAD_FD;; 
        return -1; 
    }

    // inode entry
    int offset = inode-(inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
    inode_t* inode_entry = (inode_t*)(inode_buf+offset*sizeof(inode_t));
    dprintf("... inode entry: inode=%d; size=%d; type=%d \n", inode, inode_entry->size, inode_entry->type);

    // Find the data block that is correpsonding to the current file pointer
    int idatablock=curr_position/SECTOR_SIZE;
    int datablock_offset=curr_position-(idatablock*SECTOR_SIZE);
    char iblock_buf[SECTOR_SIZE];
    if (Disk_Read(inode_entry->data[idatablock], iblock_buf) < 0)
    {
       dprintf("... cannot load the data block: %d\n", idatablock);
       osErrno = E_BAD_FD;
       return -1;
    }      
    dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock], idatablock);
    
    // Export the read bytes to read_buffer
    if (SECTOR_SIZE-datablock_offset>=actual_read_size)
    {
        // Export size is less than the remaining bytes in a sector
        memcpy(buffer, &iblock_buf[datablock_offset], actual_read_size);

        // Test copied data
        dprintf("... offset %d\n", datablock_offset);
        for(int itest=0; itest<actual_read_size; itest++)
        {
          printf("%c", iblock_buf[itest+datablock_offset]);
        }
        printf("\n");  

        dprintf("... load disk sector %d for data block %d and size %d\n", inode_entry->data[idatablock], idatablock, actual_read_size);
    }
    else 
    {
        memcpy(buffer, &iblock_buf[datablock_offset], SECTOR_SIZE-datablock_offset);  

        // Remaining full sectors with data
        int remaining_full_sectors=(actual_read_size-SECTOR_SIZE+datablock_offset)/SECTOR_SIZE;
        dprintf("... remaining full sector number is %d\n",remaining_full_sectors);

        // Full sectors with data
        for (int i=0; i<remaining_full_sectors; i++)
        {
            if (Disk_Read(inode_entry->data[idatablock+1+i], iblock_buf) < 0)
            {
               dprintf("... cannot load the data block: %d\n", idatablock+1+i);
               osErrno = E_BAD_FD;
               return -1;
            }      
            dprintf("... load disk sector %d for data block %d and size %d\n", inode_entry->data[idatablock+1+i], idatablock+1+i, SECTOR_SIZE);
            memcpy(buffer+(SECTOR_SIZE-datablock_offset+i*SECTOR_SIZE), &iblock_buf[0], SECTOR_SIZE);  
        
            // Test copied data
            dprintf("... offset %d\n", datablock_offset);
            for(int itest=0; itest<SECTOR_SIZE; itest++)
            {
              dprintf("%c", iblock_buf[itest]);
            }
             dprintf("\n");
        }

        // Final partial sector        
        int remaining_bytes=actual_read_size-remaining_full_sectors*SECTOR_SIZE-(SECTOR_SIZE-datablock_offset);
        dprintf("... remaining partial sector bytes are %d\n",remaining_bytes);        
        if (Disk_Read(inode_entry->data[idatablock+1+remaining_full_sectors], iblock_buf) < 0)
            {
               dprintf("... cannot load the data block: %d\n",idatablock+1+remaining_full_sectors);
               osErrno = E_BAD_FD;
               return -1;
            }      
            dprintf("... load disk sector %d for data block %d and size %d\n", inode_entry->data[idatablock+1+remaining_full_sectors], idatablock+1+remaining_full_sectors, remaining_bytes);            
            memcpy(buffer+SECTOR_SIZE-datablock_offset+remaining_full_sectors*SECTOR_SIZE, &iblock_buf[0], remaining_bytes); 

            // Test copied data
            for(int itest=0; itest<remaining_bytes; itest++)
            {
              dprintf("%c", iblock_buf[itest]);
            }
             dprintf("\n");
    }
    dprintf("... data read is complete\n");

    // Update file pointer current position
    open_files[fd].pos=File_Seek(fd, curr_position+actual_read_size);
    dprintf("... new file pointer location is %d\n", open_files[fd].pos);
    dprintf("... file pointer location is updated \n"); 
  }

  dprintf("... actual read size is %d\n", actual_read_size);
  return actual_read_size; 
}

int File_Write(int fd, void* buffer, int size)
{
  /* YOUR CODE */
  dprintf("Write file with file id of %d and size of %d\n", fd, size);

  // Check if the file is able to open or not
  if(fd < 0) {
    dprintf("... cannot open file: input file id is negative\n");
    osErrno = E_BAD_FD;
    return -1;
  }
  else if (fd>=MAX_OPEN_FILES)
  {
    dprintf("... cannot wrie file: file id is greater than allowed maximum open files\n");
    osErrno = E_BAD_FD;
    return -1;
  }
  else if (open_files[fd].inode<=0)
  {
    dprintf("... cannot write file: input file with a negative or a zero inode\n");
    osErrno = E_BAD_FD;
    return -1;    
  }
  else if (open_files[fd].inode>=MAX_FILES)
  {
    dprintf("... cannot write file: exceed maximum files\n");
    osErrno = E_BAD_FD;
    return -1;    
  }
  else if (size==0)
  {
    dprintf("... file write size is zero\n");
    return -1;  
  }

  // Find the file inode
  int inode=open_files[fd].inode;
  int file_size=open_files[fd].size;
  int curr_position=open_files[fd].pos;
  dprintf("... inode is %d, file_size is %d, and curr_position is %d\n", inode, file_size, curr_position);

  // Check if the corresponding inode bitmap is set or not
  if(is_bitmap_set(INODE_BITMAP_START_SECTOR, INODE_BITMAP_SIZE, inode)<1)
  {
    dprintf("... cannot write file: the corresponding inode is not set for the file\n");
    osErrno = E_BAD_FD;
    return -1; 
  }

  // Check if the file size will exceed maximum file size or not
  if ((curr_position+size)>(MAX_SECTORS_PER_FILE*SECTOR_SIZE))
  {
    dprintf("... write size is too large, exceeding maximum file size\n");
    dprintf("... required size %d, maximum allowed file size %d\n", (curr_position+size), MAX_SECTORS_PER_FILE*SECTOR_SIZE);
    osErrno = E_FILE_TOO_BIG;
    return -1; 
  }

  // Find the inode information from inode table
  // The sector where the inode is located 
  int inode_sector = INODE_TABLE_START_SECTOR+inode/INODES_PER_SECTOR;
  dprintf("... inode is located at disk sector %d\n", inode_sector);

  char inode_buf[SECTOR_SIZE];
  if(Disk_Read(inode_sector, inode_buf) < 0) 
  { 
      dprintf("... cannot load the sector where inode is located\n");
      osErrno = E_BAD_FD;; 
      return -1; 
  }

  // inode entry
  int offset = inode-(inode_sector-INODE_TABLE_START_SECTOR)*INODES_PER_SECTOR;
  inode_t* inode_entry = (inode_t*)(inode_buf+offset*sizeof(inode_t));
  dprintf("... inode entry: inode=%d; size=%d; type=%d \n", inode, inode_entry->size, inode_entry->type);
  int inodefile_size=inode_entry->size;

  // Find the data block that is correpsonding to the current file pointer
  int idatablock=curr_position/SECTOR_SIZE;
  int datablock_offset=curr_position-(idatablock*SECTOR_SIZE);
  char iblock_buf[SECTOR_SIZE];

  dprintf("... first data block is located at %d sector and offset is %d\n", idatablock, datablock_offset);

  int remaining_free_bytes;
  int required_datablock=0; 
  int datablock_address[MAX_SECTORS_PER_FILE];

  // Check if file size is zero or the existing data blocks are already full
  if(inode_entry->size==0 || inode_entry->data[idatablock]==0)
  {
    // It is an empty file and it needs new sectors to write data
    if(inode_entry->size==0)
    {
      dprintf("... empty file\n");
    }
    else
    {
      dprintf("... non-empty file with data blocks that are already full\n");
    }

    // remaining_free_bytes: remaining free bytes in the current sector
    remaining_free_bytes=0;
    required_datablock=(size-remaining_free_bytes+SECTOR_SIZE-1)/SECTOR_SIZE;
    dprintf("... requires %d new data block and remaining free bytes in the current sector %d\n", required_datablock, remaining_free_bytes);

    // Search available sectors in sector bitmap and obtain the data block address for each extended data block    
    // bitmap_available_address searches available data block spaces. 
    // If available, flip them to be 1 for those selected data space
    // Else, return -1 as error message for not enough data block space
    if(bitmap_available_address(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE,required_datablock, datablock_address)<0)
    {
      // There is no enough free data block space for the write operation
      dprintf("... error in finding enough data block space for write\n");
      osErrno=E_NO_SPACE;
      return -1;
    }
    
    // With enough data blocks
    // Set the corresponding bits in sector bitmap to be one
    if(bitmap_set_nbits(SECTOR_BITMAP_START_SECTOR,SECTOR_BITMAP_SIZE, required_datablock, datablock_address)>=0)
    {
       dprintf("...... find the %d unused bits and set them to be one\n",required_datablock);
    }
    else
    {
       dprintf("...... error in set up the required datablocks\n");
       return -1;
    }   

    // Write the data
    // First write data to full data blocks
    for (int i=0; i<required_datablock-1; i++)
    {
      // New data sector
      memset(iblock_buf, 0, SECTOR_SIZE);
      dprintf("... load disk sector %d for data block %d\n", datablock_address[i], idatablock+i);

      memcpy(&iblock_buf[0], buffer+i*SECTOR_SIZE, SECTOR_SIZE);

      // Test copied data
      for(int itest=0; itest<SECTOR_SIZE; itest++)
      {
        dprintf("%c", iblock_buf[itest]);
      }
      dprintf("\n");   

      // Write to disk for this data block 
      if (Disk_Write(datablock_address[i], iblock_buf) < 0)
      {
        dprintf("... cannot write the data block: %d\n", idatablock);
        return -1;
      }      
      dprintf("... write size %d bytes to disk sector %d for data block %d\n", SECTOR_SIZE,datablock_address[i], idatablock+i);     

      // Update the data block address in inode 
      inode_entry->data[idatablock+i]=datablock_address[i];       
    }

    // For the last data block
    // New data sector
    //memset(iblock_buf, 0, SECTOR_SIZE);
    memset(iblock_buf, 0, (size-((required_datablock-1)*SECTOR_SIZE)));
    dprintf("... load disk sector %d for data block %d\n", datablock_address[required_datablock-1], idatablock+required_datablock-1);

    memcpy(&iblock_buf[0], buffer+((required_datablock-1)*SECTOR_SIZE), (size-((required_datablock-1)*SECTOR_SIZE)));
        
    // Test copied data
    for(int itest=0; itest<(size-((required_datablock-1)*SECTOR_SIZE)); itest++)
    {
      dprintf("%c", iblock_buf[itest]);
    }
    dprintf("\n");   

    // Write to disk for this data block 
    if (Disk_Write(datablock_address[required_datablock-1], iblock_buf) < 0)
    {
       dprintf("... cannot write the data block: %d\n", idatablock+required_datablock-1);
       return -1;
    }      
    dprintf("... write size %d bytes to disk sector %d for data block %d\n", (size-((required_datablock-1)*SECTOR_SIZE)),datablock_address[required_datablock-1], idatablock+required_datablock-1);     

    // Update the data block address in inode 
    inode_entry->data[idatablock+required_datablock-1]=datablock_address[required_datablock-1];       
    inode_entry->size=inodefile_size+size;

    // Update inode information
    if(Disk_Write(inode_sector, inode_buf) < 0) 
    { 
      dprintf("... cannot update inode information for write operation\n");
      return -1; 
    }

    // Update file pointer current location
    open_files[fd].size=inodefile_size+size;
    open_files[fd].pos=File_Seek(fd, curr_position+size);
    dprintf("... new file pointer location is %d\n", open_files[fd].pos);
    dprintf("... file pointer location is updated \n"); 
 
    // return file write size
    return size; 
  }
  else 
  {
    // File already has some data
    dprintf("... non-empty file\n");

    // Give a warning for overwriting the old data
    if(curr_position<inode_entry->size)
    {
      dprintf("... warning: some old data will be overwritten\n");
    }

    // Check if the current data block has enough space for writing the data
    remaining_free_bytes=SECTOR_SIZE-datablock_offset;
    dprintf("... non-empty file, remaining free bytes %d in the current data block\n", remaining_free_bytes);

    // Case when the remaining avaialbe byte is enough for writing new data
    // These available bytes may already have data. In this case, the data will be overwritten
    // The available bytes may also be empty
    if (remaining_free_bytes>=size)
    {
      // Current data block has enough space
      dprintf("... non-empty file and current data block has enough space\n");

      // Current data block
      if (Disk_Read(inode_entry->data[idatablock], iblock_buf) < 0)
      {
         dprintf("... cannot load the data block: %d\n", idatablock);
         osErrno = E_BAD_FD;
         return -1;
      }      
      dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock], idatablock);

      memcpy(&iblock_buf[datablock_offset], buffer, size); 
    
      dprintf("... copied %d bytes\n", size);
      for (int itest=0; itest<5; itest++)
      {
        dprintf("... copied byte %c\n", iblock_buf[datablock_offset+itest]);
      }
    
      // Write current data block back to the disk
      if (Disk_Write(inode_entry->data[idatablock], iblock_buf) < 0)
      {
        dprintf("... cannot write the data block: %d\n", idatablock);
        return -1;
      }      
      dprintf("... write size %d bytes to disk sector %d for data block %d\n", size,inode_entry->data[idatablock], idatablock);
  
      // Update inode information
      if (curr_position+size>inodefile_size)
      {
        inode_entry->size=curr_position+size;
        if(Disk_Write(inode_sector, inode_buf) < 0) 
        { 
          dprintf("... cannot update inode information for write operation\n");
          return -1; 
        }

        // Update file size
        open_files[fd].size=curr_position+size;
      }
      // else: nothing needs to be changed

      // Update file pointer current location
      open_files[fd].pos=File_Seek(fd, curr_position+size);
      dprintf("... new file pointer location is %d\n", open_files[fd].pos);
      dprintf("... new file size is %d\n", open_files[fd].size);
      dprintf("... file pointer location is updated \n"); 
 
      // return file write size
      return size;
    } 
    else
    {
      // Current data block is not enough for the data
      dprintf("... non-empty file and current data block does not have enough space\n");

      // Check if the remaining data blocks are enough for the data 
      int remaining_file_dblock=(inode_entry->size-curr_position-remaining_free_bytes+SECTOR_SIZE-1)/SECTOR_SIZE;
      dprintf("... file has %d more data blocks for writing the data\n", remaining_file_dblock);      

      if(remaining_free_bytes+remaining_file_dblock*SECTOR_SIZE>=size)
      {
        // Remaining data blocks are enough for the input data and no need for requesting new sectors
        dprintf("... non-empty file and remaining data blocks have enough space\n");

        // Write to the current data block
        dprintf("... write to current data block\n");

        // Current data block
        if (Disk_Read(inode_entry->data[idatablock], iblock_buf) < 0)
        {
           dprintf("... cannot load the data block: %d\n", idatablock);
           return -1;
        }      
        dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock], idatablock);

        memcpy(&iblock_buf[datablock_offset], buffer, remaining_free_bytes);

        // Test copied data
        dprintf("... offset %d\n", datablock_offset);
        for(int itest=datablock_offset; itest<SECTOR_SIZE; itest++)
        {
          dprintf("%c", iblock_buf[itest]);
        }
        dprintf("\n");         

        // Write current data block back to the disk
        if (Disk_Write(inode_entry->data[idatablock], iblock_buf) < 0)
        {
          dprintf("... cannot write the data block: %d\n", idatablock);
          return -1;
        }      
        dprintf("... write size %d bytes to disk sector %d for data block %d\n", remaining_free_bytes,inode_entry->data[idatablock], idatablock);
        
        // Full data blocks
        int nfull_datablocks=(size-remaining_free_bytes)/SECTOR_SIZE;
        dprintf("... remaining %d full data blocks that will be written to\n", nfull_datablocks); 
    
        for (int i=0; i<nfull_datablocks; i++)
        {
          memset(iblock_buf, 0, SECTOR_SIZE);
          dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock+1+i], idatablock+i+1);
          
          // Copy buffer data to datablock
          memcpy(&iblock_buf[0], buffer+remaining_free_bytes+i*SECTOR_SIZE, SECTOR_SIZE);

          // Write to disk for this data block 
          if (Disk_Write(inode_entry->data[idatablock+1+i], iblock_buf) < 0)
          {
            dprintf("... cannot write the data block: %d\n", idatablock+1+i);
            return -1;
          }      
          dprintf("... write size %d bytes to disk sector %d for data block %d\n", SECTOR_SIZE,inode_entry->data[idatablock+1+i], idatablock+i+1);        
        }

        // For the last data block 
        if(remaining_free_bytes+nfull_datablocks*SECTOR_SIZE<size)
        {
          // If the existing data blocks can exactly accomandate the data
          // Then there is no addditonal data blcok
          // Otherwise, need the last data block       
          if (Disk_Read(inode_entry->data[idatablock+1+nfull_datablocks], iblock_buf) < 0)
          {
             dprintf("... cannot load the data block: %d\n", idatablock);
             return -1;
          }   
          dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock+1+nfull_datablocks], idatablock+1+nfull_datablocks);

          //dprintf("... existing data block data:\n");
          //for(int itest=0; itest<SECTOR_SIZE; itest++)
          //{
          //  dprintf("%c", iblock_buf[itest]);
          //}
          //dprintf("\n"); 

          //memset(iblock_buf, 0, SECTOR_SIZE);  
          memset(iblock_buf, 0, size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE);   
          dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock+1+nfull_datablocks], idatablock+1+nfull_datablocks);
        
          //char temp_buf[SECTOR_SIZE];
          //memcpy(temp_buf, iblock_buf,SECTOR_SIZE);

          memcpy(&iblock_buf[0], buffer+remaining_free_bytes+nfull_datablocks*SECTOR_SIZE, size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE);

          //memcpy(&iblock_buf[size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE], temp_buf+size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE, SECTOR_SIZE-(size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE));
        
          // Test copied data
          dprintf("... replaced data:\n");
          for(int itest=0; itest<size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE; itest++)
          {
            dprintf("%c", iblock_buf[itest]);
          }
          dprintf("\n"); 

          dprintf("... remaining data:\n");
          for(int itest=size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE; itest<SECTOR_SIZE; itest++)
          {
            dprintf("%c", iblock_buf[itest]);
          }
          dprintf("\n"); 

          // Write to disk for this data block 
          if(Disk_Write(inode_entry->data[idatablock+1+nfull_datablocks], iblock_buf) < 0)
          {
             dprintf("... cannot write the data block: %d\n", idatablock+1+nfull_datablocks);
             return -1;
          }      
          dprintf("... write size %d bytes to sector %d for data block %d\n", (size-remaining_free_bytes-nfull_datablocks*SECTOR_SIZE),inode_entry->data[idatablock+1+nfull_datablocks], idatablock+1+nfull_datablocks);     
        }
        
        // Update inode information
        if (curr_position+size>inodefile_size)
        {
          inode_entry->size=curr_position+size;
          if(Disk_Write(inode_sector, inode_buf) < 0) 
          { 
            dprintf("... cannot update inode information for write operation\n");
            return -1; 
          }

          // Update file size
          open_files[fd].size=curr_position+size;
        }
        // else: nothing needs to be changed

        // Update file pointer current location
        open_files[fd].pos=File_Seek(fd, curr_position+size);
        dprintf("... new file pointer location is %d\n", open_files[fd].pos);
        dprintf("... new file size is %d\n", open_files[fd].size);
        dprintf("... file pointer location is updated \n"); 

        // return file write size
        return size;
      }
      else 
      {
        // Existing data blocks are not enough for writing the data        
        dprintf("... non-empty file: existing data blocks are not enough for writing the data\n");

        required_datablock=(size-remaining_free_bytes-remaining_file_dblock*SECTOR_SIZE+SECTOR_SIZE-1)/SECTOR_SIZE;
        dprintf("... file has %d more data blocks for writing the data but still require %d new data block\n", remaining_file_dblock, required_datablock);

        // Search available sectors in sector bitmap and obtain the data block address for each extended data block    
        // bitmap_available_address searches available data block spaces. 
        // Else, return -1 as error message for not enough data block space
        if(bitmap_available_address(SECTOR_BITMAP_START_SECTOR, SECTOR_BITMAP_SECTORS, SECTOR_BITMAP_SIZE,required_datablock, datablock_address)<0)
        {
          // There is no enough free data block space for the write operation
          dprintf("... error in finding enough data block space for write\n");
          osErrno=E_NO_SPACE;
          return -1;
        }  

        // Set the bits to be one for the new sectors
        if(bitmap_set_nbits(SECTOR_BITMAP_START_SECTOR,SECTOR_BITMAP_SIZE, required_datablock,datablock_address)>=0)
        {
          dprintf("...... find the %d unused bits and set them to be one\n",required_datablock);
        }
        else
        {
          dprintf("...... error in set up the required datablocks\n");
          return -1;
        }

        // Write to the current data block
        dprintf("... write to current data block\n");

        // Current data block
        if (Disk_Read(inode_entry->data[idatablock], iblock_buf) < 0)
        {
           dprintf("... cannot load the data block: %d\n", idatablock);
           return -1;
        }      
        dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock], idatablock);

        memcpy(&iblock_buf[datablock_offset], buffer, remaining_free_bytes);

        // Write current data block back to the disk
        if (Disk_Write(inode_entry->data[idatablock], iblock_buf) < 0)
        {
          dprintf("... cannot write the data block: %d\n", idatablock);
          return -1;
        }      
        dprintf("... write size %d bytes to disk sector %d for data block %d\n", remaining_free_bytes,inode_entry->data[idatablock], idatablock);
        
        // Existing full data blocks
        dprintf("... remaining %d full data blocks that will be written to\n", remaining_file_dblock); 
    
        for (int i=0; i<remaining_file_dblock; i++)
        {
          memset(iblock_buf, 0, SECTOR_SIZE);
          dprintf("... load disk sector %d for data block %d\n", inode_entry->data[idatablock+1+i], idatablock+i+1);
          
          // Copy buffer data to datablock
          memcpy(&iblock_buf[0], buffer+remaining_free_bytes+i*SECTOR_SIZE, SECTOR_SIZE);

          // Write to disk for this data block 
          if (Disk_Write(inode_entry->data[idatablock+1+i], iblock_buf) < 0)
          {
            dprintf("... cannot write the data block: %d\n", idatablock+1+i);
            return -1;
          }      
          dprintf("... write size %d bytes to disk sector %d for data block %d\n", SECTOR_SIZE,inode_entry->data[idatablock+1+i], idatablock+i+1);        
        }

        // New full data blocks
        for (int i=0; i<required_datablock-1; i++)
        {
          memset(iblock_buf, 0, SECTOR_SIZE);   
          dprintf("... load disk sector %d for data block %d\n", datablock_address[i], idatablock+i+1+remaining_file_dblock);
         
          memcpy(&iblock_buf[0], buffer+remaining_free_bytes+(remaining_file_dblock+i)*SECTOR_SIZE, SECTOR_SIZE);

          // Write to disk for this data block 
          if (Disk_Write(datablock_address[i], iblock_buf) < 0)
          {
            dprintf("... cannot write the data block: %d\n", idatablock);
            return -1;
          }      
          dprintf("... write size %d bytes to disk sector %d for data block %d\n", SECTOR_SIZE,datablock_address[i], idatablock+i+1+remaining_file_dblock);     

          // Update the data block address in inode 
          inode_entry->data[idatablock+i+1+remaining_file_dblock]=datablock_address[i];       
        }  

        // For the last new data block
        //memset(iblock_buf, 0, SECTOR_SIZE);
        memset(iblock_buf, 0, (size-remaining_free_bytes-((remaining_file_dblock+required_datablock-1)*SECTOR_SIZE)));
        dprintf("... load disk sector %d for data block %d\n", datablock_address[required_datablock-1], idatablock+remaining_file_dblock+required_datablock);

        memcpy(&iblock_buf[0], buffer+remaining_free_bytes+((remaining_file_dblock+required_datablock-1)*SECTOR_SIZE), (size-remaining_free_bytes-((remaining_file_dblock+required_datablock-1)*SECTOR_SIZE)));
        
        // Write to disk for the last data block 
        if (Disk_Write(datablock_address[required_datablock-1], iblock_buf) < 0)
        {
           dprintf("... cannot write the data block: %d\n", idatablock+remaining_file_dblock+required_datablock);
           return -1;
        }      
        dprintf("... write size %d bytes to disk sector %d for data block %d\n", (size-remaining_free_bytes-((remaining_file_dblock+required_datablock-1)*SECTOR_SIZE)),datablock_address[required_datablock-1], idatablock+remaining_file_dblock+required_datablock);     

        // Update the data block address in inode 
        inode_entry->data[idatablock+remaining_file_dblock+required_datablock]=datablock_address[required_datablock-1];   

        // Update inode size
        if (curr_position+size>inodefile_size)
        {
          inode_entry->size=curr_position+size;

          // Update file size
          open_files[fd].size=curr_position+size;
        }
        // else: no need to change inode size

        if(Disk_Write(inode_sector, inode_buf) < 0) 
        { 
            dprintf("... cannot update inode information for write operation\n");
            return -1; 
        }

        // Update file pointer current location
        open_files[fd].pos=File_Seek(fd, curr_position+size);
        dprintf("... new file pointer location is %d\n", open_files[fd].pos);
        dprintf("... new file size is %d\n", open_files[fd].size);
        dprintf("... file pointer location is updated \n"); 

        // return file write size
        return size;
      }
    }
  }
  return -1;
}

  
//File_Seek() should update the current location of the file pointer. 
//The location is given as an offset from the beginning of the file.
//If offset is larger than the size of the file or negative, return -1 and
//set osErrno to E_SEEK_OUT_OF_BOUNDS.
//If the file is not currently open, return -1 and set osErrno to E_BAD_FD. 
//Upon success, return the new location of the file pointer.

int File_Seek(int fd, int offset)
{

  dprintf("File seek for %d and offset of %d\n", fd, offset);

  
  // Check if the file is able to open or not
  if(fd < 0) {
    dprintf("... cannot open file: input file id is negative\n");
    osErrno = E_BAD_FD;
    return -1;
  }
  else if (fd>=MAX_OPEN_FILES)
  {
    dprintf("... cannot read file: file id is greater than allowed maximum open files\n");
    osErrno = E_BAD_FD;
    return -1;
  }
  else if (open_files[fd].inode<0)
  {
    dprintf("... cannot read file: input file with a negative inode\n");
    osErrno = E_BAD_FD;
    return -1;    
  }
  else if (open_files[fd].inode>=MAX_FILES)
  {
    dprintf("... cannot read file: exceed maximum files\n");
    osErrno = E_BAD_FD;
    return -1;    
  }

  // Reset the current file pointer location
  int file_size=open_files[fd].size;
  if (offset<0)
  {
     dprintf("... offset %d is negative\n", offset);     
     osErrno=E_SEEK_OUT_OF_BOUNDS;
     return -1;
  }
  else if (offset>file_size)
  {
     dprintf("... offset %d is greater than file size %d\n", offset, file_size);     
     osErrno=E_SEEK_OUT_OF_BOUNDS;
     return -1;    
  }

  open_files[fd].pos=offset;
  dprintf("... current file pointer is located at %d; file size is %d\n", open_files[fd].pos,open_files[fd].size);
  return offset;
}

int File_Close(int fd)
{
  dprintf("File_Close(%d):\n", fd);
  if(0 > fd || fd > MAX_OPEN_FILES) {
    dprintf("... fd=%d out of bound\n", fd);
    osErrno = E_BAD_FD;
    return -1;
  }
  if(open_files[fd].inode <= 0) {
    dprintf("... fd=%d not an open file\n", fd);
    osErrno = E_BAD_FD;
    return -1;
  }

  dprintf("... file closed successfully\n");
  open_files[fd].inode = 0;
  return 0;
}

int Dir_Create(char* path)
{
  dprintf("Dir_Create('%s'):\n", path);
  return create_file_or_directory(1, path);
}

// Dir_Unlink() removes a directory referred to by path, freeing up its inode and data blocks, and removing its entry from the parent directory. 
// Upon success, return 0. If the directory does not currently exist, return -1 and set osErrno to E_NO_SUCH_DIR. 
// Dir_Unlink() should only be successful if there are no files within the directory. 
// If there are still files within the directory, return -1 and set osErrno to E_DIR_NOT_EMPTY. 
// It’s not allowed to remove the root directory ("/"), in which case the function should return -1 and set osErrno to E_ROOT_DIR.
int Dir_Unlink(char* path)
{
  if (path == NULL) {
      return -1; // invalid input
  }

  int child_inode;
  char last_fname[MAX_NAME];
  int parent_inode = follow_path(path, &child_inode, last_fname);
  printf("parent_inode: %d\n", parent_inode);
  printf("child_inode: %d\n", child_inode);

  if (parent_inode < 0)
  {
    // unknown error, somehow cannot follow the path
    return -1;
  }

  if (parent_inode == 0 && child_inode == 0) 
  {
    // root path
    osErrno = E_ROOT_DIR;
    return -1;
  }

  if (child_inode == -1) 
  {
    // non-exist path
    osErrno = E_NO_SUCH_DIR;
    return -1;
  }

  int remove_inode_re = remove_inode(1, parent_inode, child_inode);
  dprintf("[INFO] remove inode result = %d\n", remove_inode_re);
  if (remove_inode_re == -2)
  {
      // dir is not empty
      osErrno = E_DIR_NOT_EMPTY;
      return -1;
  } else if (remove_inode_re == -1 || remove_inode_re == -3) {
      // unexpected error
      dprintf("[WARN] unexpected error\n");
      return -1;
  }
  return 0;
}
/**
 * Dir_Size() returns the number of bytes in the directory referred to by path. 
 * This function should be used to find the size of the directory before calling Dir_Read() (described below) 
 * to find the contents of the directory.
 *
 * The size actually is a product of the number of file/dir existed under the given path. 
 * To be specific, size = 20 bytes * count_of_entry.
 *
 * if path is not found, -1 is returned
 */
int Dir_Size(char* path)
{
  // Find the child inode 
  if (path == NULL) 
  {
      // invalid path
      printf("Invalid empty path\n");
      return -1;
  }

  // child inode is the leaf node within the path
  // For example, path "/a/b/c/" will have dir c as the child inode
  int child_inode;
  char last_fname[MAX_NAME];
  if (follow_path(path, &child_inode, last_fname) == -1 || child_inode == -1) 
  {
      // path not found
      return -1; 
  }

  // get the disk sector containing the child inode
  char inode_buffer[SECTOR_SIZE];
  int inode_sector = INODE_TABLE_START_SECTOR + child_inode/INODES_PER_SECTOR;
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for child inode %d from disk sector %d\n",
                 child_inode, inode_sector);

  // get child inode
  int inode_start_entry = (inode_sector - INODE_TABLE_START_SECTOR) * INODES_PER_SECTOR;
  int offset = child_inode - inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* child = (inode_t*)(inode_buffer + offset * sizeof(inode_t));
  dprintf("... get child inode %d (size=%d, type=%d)\n", child_inode, child->size, child->type);

  return child->size * FILE_OR_DIR_ENTRY_SIZE;
}

/**
 * This method reads the dir specified by the path. It returns the count of the entires (e.g. file or dir). 
 * Each entry will occupy 20 bytes in the buffer array. The first 16 bytes consist of the name of the entry,
 * and last 4 bytes are the inode number. 
 */
int Dir_Read(char* path, void* buffer, int size)
{
  if (path == NULL) {
    // invalid path
    return -1;
  }

  dprintf("... read dir %s with buffer size %d", path, size);

  int dir_size = Dir_Size(path);

  if (dir_size <= 0) {
    // path not found or empty dir
    printf("path not found or empty dir: %s", path);
    return dir_size;
  }

  if (size < dir_size) {
    // buffer isn't big enough to hold the content
    osErrno = E_BUFFER_TOO_SMALL;
    return -1;
  }

  int child_inode;
  char last_fname[MAX_NAME];
  if (follow_path(path, &child_inode, last_fname) == -1 || child_inode == -1) 
  {
      return -1; // path not found but it should not happen since we already called Dir_Size();
  }

  // get the disk sector containing the child inode
  char inode_buffer[SECTOR_SIZE];
  int inode_sector = INODE_TABLE_START_SECTOR + child_inode/INODES_PER_SECTOR;
  if(Disk_Read(inode_sector, inode_buffer) < 0) return -1;
  dprintf("... load inode table for child inode %d from disk sector %d\n",
                 child_inode, inode_sector);

  // get child inode
  int inode_start_entry = (inode_sector - INODE_TABLE_START_SECTOR) * INODES_PER_SECTOR;
  int offset = child_inode - inode_start_entry;
  assert(0 <= offset && offset < INODES_PER_SECTOR);
  inode_t* child = (inode_t*)(inode_buffer + offset * sizeof(inode_t));
  dprintf("... get child inode %d (size=%d, type=%d)\n", child_inode, child->size, child->type);

  //int groups = dir_size / FILE_OR_DIR_ENTRY_SIZE / DIRENTS_PER_SECTOR;
  int total_entry_size = dir_size / FILE_OR_DIR_ENTRY_SIZE;
  char dirent_buffer[SECTOR_SIZE];
  
  // traverse each dirent group
  // read dirent from sector
  if(Disk_Read(child->data[0], dirent_buffer) < 0) return -1;
  int dirent_offset = 0;
  int sec_idx = 0;
  while (sec_idx * DIRENTS_PER_SECTOR + dirent_offset < total_entry_size) {
      dirent_t* dirent = (dirent_t*)(dirent_buffer + dirent_offset * sizeof(dirent_t));
      dprintf("found file name: %-15s with inode %-d\n", dirent, dirent->inode);

      strncpy(buffer, dirent, MAX_NAME);
      memcpy(buffer + MAX_NAME, &(dirent->inode), sizeof(int));

      dirent_offset++;        
      buffer += FILE_OR_DIR_ENTRY_SIZE;

      if (dirent_offset == DIRENTS_PER_SECTOR) {
          dirent_offset = 0;
          sec_idx++;

          memset(dirent_buffer, 0, SECTOR_SIZE);
          if(Disk_Read(child->data[sec_idx], dirent_buffer) < 0) return -1;

        dprintf("...... move to next dirent group %d\n", sec_idx);
      }
  }

  dprintf("... total entry size in dir '%s' is: %d\n", path, total_entry_size);
  return total_entry_size;
}

