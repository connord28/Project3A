// NAME: Connor Daly, Ryan Daly
// EMAIL: connord2838@gmail.com, ryand3031@ucla.edu
// ID: 305416912, 505416119
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "ext2_fs.h"
unsigned int blockSize= 0;
int fd = 0;

void print_Dir_Entries( unsigned int inode_num, unsigned int start_pos){
   struct ext2_dir_entry* entry = (struct ext2_dir_entry*) malloc(sizeof(struct ext2_dir_entry));
   unsigned int offset = 0;
   while(offset<blockSize){
      if(pread(fd, entry, sizeof(struct ext2_dir_entry), start_pos + offset) <0)
      {
         fprintf(stderr, "Error when doing preads\n");
         exit(2);
      }
      char name[256];
      for(unsigned int i = 0; i< entry->name_len; i++){
         name[i] = entry->name[i];
      }
      name[entry->name_len] = '\0';

      unsigned int entry_len = entry->rec_len;
      if(entry->inode !=0){
         fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,\'%s\'\n", inode_num, offset, entry->inode, entry_len, entry->name_len, name );
      }
      offset += entry_len;
   }
   free(entry);
}

void getTime(unsigned int t, char* buff)
{
   time_t time = (time_t)t;
   struct tm timeStruct;
   timeStruct = *gmtime(&time);
   strftime(buff, 18, "%m/%d/%y %H:%M:%S", &timeStruct);
}

int main(int argc, char** argv)
{
   unsigned int inodeCount= 0, numBlocks= 0, blocksPerGroup = 0, inodeSize = 0, inodesPerGroup = 0;
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
   inodesPerGroup = super.s_inodes_per_group;
   blockSize = 1024 << super.s_log_block_size;
   printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", numBlocks, inodeCount, blockSize, inodeSize, blocksPerGroup, inodesPerGroup, super.s_first_ino);
   //fragsPerGroup = super.s_frags_per_group; 
   //fragSize = 1024 << super.s_log_frag_size;

   //group part
   //maybe make a function for each part of this like the github's do
   unsigned int numGroups = numBlocks / blocksPerGroup;
   unsigned int gBlocks = blocksPerGroup, gInodes = inodesPerGroup;
   //printf("#g=%d\n", numGroups);
   for(unsigned int i = 0; i <= numGroups; i++)
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
      for(unsigned int j = 1; j <= gBlocks; j++)
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
      for(unsigned int j = 1; j <= gInodes; j++)
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
      for(unsigned int j = 0; j < gInodes; j++)
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


         //print directory entries
         //struct ext2_dir_entry* entry = (struct ext2_dir_entry*) malloc(sizeof(struct ext2_dir_entry));
         if(fileType=='d'){
            for(unsigned int k = 0; k<12 && inode.i_block[k]!=0; k++){
               print_Dir_Entries(j+1, inode.i_block[k] *blockSize);
               /*
               unsigned int offset = 0;
               while(offset<blockSize){
                  if(pread(fd, entry, sizeof(struct ext2_dir_entry), inode.i_block[k] *blockSize + offset) <0)
                  {
                     fprintf(stderr, "Error when doing preads\n");
                     exit(2);
                  }
                  unsigned int entry_len = entry->rec_len;
                  if(entry->inode !=0){
                     fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,\'%s\'\n", j+1, offset, entry->inode, entry_len, entry->name_len, entry->name );
                  }
                  offset += entry_len;
               }*/
            }
   
         }
         //single indirect
         unsigned int* block_nums = (unsigned int *)malloc(blockSize);
         unsigned int* block_nums2 = (unsigned int *)malloc(blockSize);
         unsigned int* block_nums3 = (unsigned int *)malloc(blockSize);
         if(inode.i_block[12]!=0){
            if(pread(fd, block_nums, blockSize, inode.i_block[12] *blockSize) <0)
               {
                  fprintf(stderr, "Error when doing preads\n");
                  exit(2);
               }
            for(unsigned int k = 0; k<blockSize/4; k++){
               if(block_nums[k]==0)
                  continue;
               if(fileType=='d'){
                  print_Dir_Entries(j+1, blockSize*block_nums[k]);
               }
               fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", j+1, 1, 12 + k, inode.i_block[12], block_nums[k] );
            }
         }
         //unsigned int* block_nums = (unsigned int *)malloc(blockSize);

         //double indirect
          if(inode.i_block[13]!=0){
            if(pread(fd, block_nums, blockSize, inode.i_block[13] *blockSize) <0)
               {
                  fprintf(stderr, "Error when doing preads\n");
                  exit(2);
               }
            for(unsigned int k = 0; k<blockSize/4; k++){
               if(block_nums[k]==0)
                  continue;
               fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", j+1, 2, 12 + 256 + k*256, inode.i_block[13], block_nums[k] );
               //unsigned int* block_nums2 = (unsigned int *)malloc(blockSize);
               if(pread(fd, block_nums2, blockSize, block_nums[k] *blockSize) <0)
               {
                  fprintf(stderr, "Error when doing preads\n");
                  exit(2);
               }
               for(unsigned int l = 0; l<blockSize/4; l++){
                  if(block_nums2[l]==0)
                     continue;
                  if(fileType=='d'){
                     print_Dir_Entries(j+1, blockSize*block_nums2[l]);
                  }
                  fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", j+1, 1, 12 + 256+(k*256)+l, block_nums[k], block_nums2[l] );
               }
            }
         }

         //tripple indirect
         if(inode.i_block[14]!=0){
            if(pread(fd, block_nums, blockSize, inode.i_block[14] *blockSize) <0)
               {
                  fprintf(stderr, "Error when doing preads\n");
                  exit(2);
               }
            for(unsigned int k = 0; k<blockSize/4; k++){
               if(block_nums[k]==0)
                  continue;
               fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", j+1, 3, 12 + 256 + 256*256 + k*256*256, inode.i_block[14], block_nums[k] );
               //unsigned int* block_nums2 = (unsigned int *)malloc(blockSize);
               if(pread(fd, block_nums2, blockSize, block_nums[k] *blockSize) <0)
               {
                  fprintf(stderr, "Error when doing preads\n");
                  exit(2);
               } 
               for(unsigned int l = 0; l<blockSize/4; l++){
                  if(block_nums2[l]==0)
                  continue;
                  fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", j+1, 2, 12 + 256 + 256*256 + k*256*256 + l*256, block_nums[k], block_nums2[l] );
                  if(pread(fd, block_nums3, blockSize, block_nums2[l] *blockSize) <0)
                  {
                     fprintf(stderr, "Error when doing preads\n");
                     exit(2);
                  }
                  for(unsigned int m = 0; m<blockSize/4; m++){
                     if(block_nums3[m]==0)
                        continue;
                     if(fileType=='d'){
                        print_Dir_Entries(j+1, blockSize*block_nums3[m]);
                     }
                     fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n", j+1, 1, 12 + 256 + 256*256 + k*256*256 + l*256 + m, block_nums2[l], block_nums3[m] );
                  }
               }
            }
         }
            free(block_nums);
            free(block_nums2);
            free(block_nums3);
      }
      free(inodes);
   }


   
   return 0;
}

