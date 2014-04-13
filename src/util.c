#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include "sb.h"
#include "inode.h"
#include "userfs.h"
#include "blocks.h"
#include "bitmap.h"
#include "dir.h"


/*
 * Formats the virtual disk. Saves the superblock
 * bit map and the single level directory.
 */
int u_format(int diskSizeBytes, char* file_name)
{
	int i;
	int minimumBlocks;
	inode in;

	/* create the virtual disk */
	if ((virtual_disk = open(file_name, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR)) < 0)
	{
		fprintf(stderr, "Unable to create virtual disk file: %s\n", file_name);
		return 0;
	}


	fprintf(stderr, "Formatting userfs of size %d bytes with %d block size in file %s\n",
		diskSizeBytes, BLOCK_SIZE_BYTES, file_name);

	minimumBlocks = 3+ NUM_INODE_BLOCKS+1;
	if (diskSizeBytes/BLOCK_SIZE_BYTES < minimumBlocks){
		fprintf(stderr, "Minimum size virtual disk is %d bytes %d blocks\n",
			BLOCK_SIZE_BYTES*minimumBlocks, minimumBlocks);
		fprintf(stderr, "Requested virtual disk size %d bytes results in %d bytes %d blocks of usable space\n",
			diskSizeBytes, BLOCK_SIZE_BYTES*minimumBlocks, minimumBlocks);
		return 0;
	}


	/*************************  BIT MAP **************************/

	assert(sizeof(BIT_FIELD)* BIT_MAP_SIZE <= BLOCK_SIZE_BYTES);
	fprintf(stderr, "%d blocks %d bytes reserved for bitmap (%lu bytes required)\n", 
		1, BLOCK_SIZE_BYTES, sizeof(BIT_FIELD)* BIT_MAP_SIZE );
	fprintf(stderr, "\tImplies Max size of disk is %lu blocks or %lu bytes\n",
		BIT_MAP_SIZE*BITS_PER_FIELD, BIT_MAP_SIZE*BLOCK_SIZE_BYTES*BITS_PER_FIELD);
  
	if (diskSizeBytes >= BIT_MAP_SIZE* BLOCK_SIZE_BYTES){
		fprintf(stderr, "Unable to format a userfs of size %d bytes\n",
			diskSizeBytes);
		return 0;
	}

	init_bit_map();
  
	/* first three blocks will be taken with the 
	   superblock, bitmap and directory */
	allocate_block(BIT_MAP_BLOCK);
	allocate_block(SUPERBLOCK_BLOCK);
	allocate_block(DIRECTORY_BLOCK);
	/* next NUM_INODE_BLOCKS will contain inodes */
	for (i=3; i< 3+NUM_INODE_BLOCKS; i++){
		allocate_block(i);
	}
  
	write_bitmap();
	
	/***********************  DIRECTORY  ***********************/
	assert(sizeof(dir_struct) <= BLOCK_SIZE_BYTES);

	fprintf(stderr, "%d blocks %d bytes reserved for the userfs directory (%lu bytes required)\n", 
		1, BLOCK_SIZE_BYTES, sizeof(dir_struct));
	fprintf(stderr, "\tMax files per directory: %d\n",
		MAX_FILES_PER_DIRECTORY);
	fprintf(stderr,"Directory entries limit filesize to %d characters\n",
		MAX_FILE_NAME_SIZE);

	init_dir();
	write_dir();

	/***********************  INODES ***********************/
	fprintf(stderr, "userfs will contain %lu inodes (directory limited to %d)\n",
		MAX_INODES, MAX_FILES_PER_DIRECTORY);
	fprintf(stderr,"Inodes limit filesize to %d blocks or %d bytes\n",
		MAX_BLOCKS_PER_FILE, 
		MAX_BLOCKS_PER_FILE* BLOCK_SIZE_BYTES);

	in.free = 1;
	for (i=0; i< MAX_INODES; i++){
		write_inode(i, &in);
	}

	/***********************  SUPERBLOCK ***********************/
	assert(sizeof(superblock) <= BLOCK_SIZE_BYTES);
	fprintf(stderr, "%d blocks %d bytes reserved for superblock (%lu bytes required)\n", 
		1, BLOCK_SIZE_BYTES, sizeof(superblock));
	init_superblock(diskSizeBytes);
	fprintf(stderr, "userfs will contain %d total blocks: %d free for data\n",
		sb.disk_size_blocks, sb.num_free_blocks);
	fprintf(stderr, "userfs contains %lu free inodes\n", MAX_INODES);
	
	write_block(SUPERBLOCK_BLOCK, &sb, sizeof(superblock));
	sync();


	/* when format complete there better be at 
	   least one free data block */
	assert( u_quota() >= 1);
	fprintf(stderr,"Format complete!\n");
	
	close(virtual_disk);
	return 1;
}


int u_fsck() {
	
    int file_count = 0;
    int free_block_count = 0;
    bool inode_flag = 0;
    bool bitmap_flag = 0;

    inode in; 

    int i,j,k;

    //count the number of user files and check if equal to dir.no_files
    for(i=0;i<MAX_FILES_PER_DIRECTORY; ++i){
	if(!root_dir.u_file[i].free){ //find files that are NOT free
	    ++file_count;
	}
    }

    if(file_count != root_dir.no_files){
	printf("\n ***WARNING*** \n The number of files counter is not equal to number of files in this directory\n");
	return 0;   
    }


       /* loop through all the inodes to check two things;
        *
        *  1. Check that if an inode is marked not free, that it belongs to some
        *  file
        *  2. Ensure that the number of free blocks is equal to the superblock
        *  count.
        *
        */

	for(j=0; j< MAX_INODES; j++){
		read_inode(j, &in);
		if(in.free){ /* if the inode is used, it must belong ot some file */
			for (i=0; i< MAX_FILES_PER_DIRECTORY ; i++){
				/* If data integrity is maintained, there must be SOME file in the directory that has this inode number */
				if(root_dir.u_file[i].inode_number == j){
					inode_flag = 1;		  
				}
        		}

			if(!inode_flag){
				printf("warning, used inode found that does not correspond to any existing file, freeing inode\n");
				in.free = 1;
				return 0;
			}
		}

		/* loop through all of the blocks belong to this inode */
		for(k = 0; k<MAX_BLOCKS_PER_FILE; k++){
			if(bit_map[in.blocks[k]] == 1){
				free_block_count++;
			}
		}
	}

	if(sb.num_free_blocks != free_block_count){
		printf("warning, the number of free blocks found in all of the inodes is not equivalent to the superblock\n");
		return 0;
	}
	
	/* Ensure that any used blocks in the bitmap are used by some file */

	for (i=NUM_INODE_BLOCKS+MAX_FILES_PER_DIRECTORY; i< BIT_MAP_SIZE; i++){
	   	if(bit_map[i] == 1){ //If a block is allocated make sure that it is pointed to by some file 
			for (k=0; k< MAX_FILES_PER_DIRECTORY ; k++) {	
				if(!root_dir.u_file[k].free){ // find the files that are NOT free 
					read_inode(root_dir.u_file[k].inode_number, &in);
					for(j=0; j < in.no_blocks; j++) {
						if(in.blocks[j] == i){ 
							bitmap_flag = 1;
						}
					}
				}
			}
		}
	}
	
	if(!bitmap_flag){
		printf("warning, there is a used block that is not pointed to by any existing file\n");
		return 0;
	}
	
	/*
	 - return 1 for success, 0 for failure
	 - any inodes maked taken not pointed to by the directory?
	 - are there any things marked taken in bit map not pointed to by a file?
	*/


	printf("File System integrity maintained!\n");
	return 1;
}

/*
 * Attempts to recover a file system given the virtual disk name
 */
int recover_file_system(char *file_name)
{

	if ((virtual_disk = open(file_name, O_RDWR)) < 0)
	{
		printf("virtual disk open error\n");
		return 0;
	}

	read_block(SUPERBLOCK_BLOCK, &sb, sizeof(superblock));
	read_block(BIT_MAP_BLOCK, bit_map, sizeof(BIT_FIELD)*BIT_MAP_SIZE);
	read_block(DIRECTORY_BLOCK, &root_dir, sizeof(dir_struct));

	if (!superblockMatchesCode()){
		fprintf(stderr,"Unable to recover: userfs appears to have been formatted with another code version\n");
		return 0;
	}
	if (!sb.clean_shutdown)
	{
		/* Try to recover your file system */
		fprintf(stderr, "u_fsck in progress......");
		if (u_fsck()){
			fprintf(stderr, "Recovery complete\n");
			return 1;
		}else {
			fprintf(stderr, "Recovery failed\n");
			return 0;
		}
	}
	else{
		fprintf(stderr, "Clean shutdown detected\n");
		return 1;
	}
}

int u_clean_shutdown()
{
	/* write code for cleanly shutting down the file system
	   return 1 for success, 0 for failure */
  
	sb.num_free_blocks = u_quota();
	sb.clean_shutdown = 1;

	lseek(virtual_disk, BLOCK_SIZE_BYTES* SUPERBLOCK_BLOCK, SEEK_SET);
	crash_write(virtual_disk, &sb, sizeof(superblock));
	sync();

	close(virtual_disk);
	/* is this all that needs to be done on clean shutdown? */
	return !sb.clean_shutdown;
}
