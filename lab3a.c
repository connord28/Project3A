

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "ext2_fs.h"

void getTime(unsigned int t, char* buff)
{
   time_t time = (time_t)t;
   struct tm timeStruct;
   timeStruct = *gmtime(&time);
   strftime(buff, 18, "%m/%d/%y %H:%M:%S", &timeStruct);
}

int main(int argc, char** argv)
{
   int fd = 0;
   unsigned int inodeCount= 0, numBlocks= 0, blockSize= 0, blocksPerGroup = 0, inodeSize = 0, fragsPerGroup = 0, fragSize = 0, inodesPerGroup = 0;
   struct ext2_super_block super;

   if(argc != 2)
   {
      fprintf(stderr, "Error: must have only one argument, the file system image to be analyzed, #=%d\n", argc);
      exit(2);
   }

   //char *file = &argv[1];
   if((fd = open(argv[1], O_RDONLY)) == -1)
   {
      fprintf(stderr, "Error opening the specified file\n");
      exit(1);
   }


   //superblock part
   if(pread(fd, &super, sizeof(super), 1024) != 1024)
   {
      fprintf(stderr, "Error when doing preads\n");
      exit(2);
   }   
   blocksPerGroup = super.s_blocks_per_group;
   //printf("#bp=%d\n", blocksPerGroup);
   inodeSize = super.s_inode_size;
   numBlocks =  super.s_blocks_count;
   //printf("#b=%d\n", numBlocks);
   inodeCount = super.s_inodes_count;
   printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", numBlocks, inodeCount, inodeSize, blocksPerGroup, super.s_first_ino);
   fragsPerGroup = super.s_frags_per_group;
   inodesPerGroup = super.s_inodes_per_group;
   blockSize = 1024 << super.s_log_block_size; 
   fragSize = 1024 << super.s_log_frag_size;

   //group part
   //maybe make a function for each part of this like the github's do
   int numGroups = numBlocks / blocksPerGroup;
   unsigned int gBlocks = blocksPerGroup, gInodes = inodesPerGroup;
   //printf("#g=%d\n", numGroups);
   for(int i = 0; i <= numGroups; i++)
   {
      struct ext2_group_desc group;
      
      if(pread(fd, &group, sizeof(group), 1024+blockSize*(i+1)) < 0)
      {
         fprintf(stderr, "Error when doing preads\n");
         exit(2);
      }
      if(i == numGroups) //irregular block with different # of inodes and blocks
      {
         gBlocks = numBlocks - i*blocksPerGroup;
         gInodes = inodeCount - i*inodesPerGroup;
      }
      unsigned int bitBlockNum = group.bg_block_bitmap, inoBitBlock = group.bg_inode_bitmap, inoTable = group.bg_inode_table;
      printf("GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n", i, gBlocks, gInodes, group.bg_free_blocks_count, group.bg_free_inodes_count, bitBlockNum, inoBitBlock, inoTable);

      //free block part
      char *bits = (char*)malloc(blockSize); 
      //fprintf(stderr, "CUM\n");
      if(pread(fd, bits, blockSize, bitBlockNum*blockSize) != blockSize)
      {
         fprintf(stderr, "Error when doing preads\n");
         exit(2);
      }
      //fprintf(stderr, "bits[0]=%d\n", bits[0]);
      for(int j = 1; j <= gBlocks; j++)
      {
         //fprintf(stderr, "ASS=%d\n", blockSize);
         int index = 0, offset = 0;
         index = (j-1)/8;  //which byte within the bitmap stores the info of this block# 
         offset = (j-1)%8;
         //fprintf(stderr, "index=%d, offset=%d, bits=%d\n", index, offset, *bits);
         if(!(*(bits+index) & (1 << offset)))
         {
            printf("BFREE,%d\n", j);
         }
      }

      //inode bitmap part
      if(pread(fd, bits, blockSize, inoBitBlock*blockSize) != blockSize)
      {
         fprintf(stderr, "Error when doing preads\n");
         exit(2);
      }
      for(int j = 1; j <= gInodes; j++)
      {
         int index = 0, offset = 0;
         index = (j-1)/8;  //which byte within the bitmap stores the info of this block# 
         offset = (j-1)%8;
         if(!(*(bits+index) & (1 << offset)))
         {
            printf("IFREE,%d\n", j);
         }
      }
      free(bits);

      //Inode Summary Part
      unsigned int itableSize = gInodes * inodeSize;
      struct ext2_inode *inodes = (struct ext2_inode*)malloc(itableSize);
      if(pread(fd, inodes, itableSize, inoTable*blockSize) != itableSize)
      {
         fprintf(stderr, "Error when doing preads\n");
         exit(2);
      }
      for(int j = 0; j < gInodes; j++)
      {
         struct ext2_inode inode = inodes[j];
         unsigned int mode = inode.i_mode;
         if(mode == 0)
            continue;
         unsigned int linksCount = inode.i_links_count;
         if(linksCount == 0)
            continue;
         //printf("INODE,%d,%c,%o\n"/*,%d,%d,%d,%d,%s,%s,%s\n*/, j+1, fileType, mode&0xFFF);
         char changeTime[18], modTime[18], accessTime[18];
         //int test = inode.i_ctime;
         //printf("INODE,%d,%c,%o\n"/*,%d,%d,%d,%d,%s,%s,%s\n*/, j+1, fileType, mode&0xFFF);
         getTime(inode.i_ctime, changeTime);
         getTime(inode.i_mtime, modTime);
         getTime(inode.i_atime, accessTime);
         int size = inode.i_size;
         char fileType = '?';
         if(S_ISLNK(mode))
         {
            fileType = 's';
            if(size < 60)
            {
               printf("INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d\n", j+1, fileType, mode&0xFFF, inode.i_uid, inode.i_gid, linksCount, changeTime, modTime, accessTime, size, inode.i_blocks);
               continue;
            }
         }
         else if(S_ISREG(mode))
            fileType = 'f';
         else if(S_ISDIR(mode))
         {
            fileType = 'd';
            //probably call directory part
         }
         printf("INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d", j+1, fileType, mode&0xFFF, inode.i_uid, inode.i_gid, linksCount, changeTime, modTime, accessTime, size, inode.i_blocks);
         for(int k = 0; k < 15; k++)
         {
            printf(",%d", inode.i_block[k]);
         }
         printf("\n");
      }

   }


   
   return 0;
}
