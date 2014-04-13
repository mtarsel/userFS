#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "userfs.h"
#include "blocks.h"
#include "file.h"
#include "dir.h"
#include "inode.h"

void init_dir() {
	int i;
	root_dir.no_files = 0;
	for (i=0; i< MAX_FILES_PER_DIRECTORY; i++) {
		root_dir.u_file[i].free = 5;
	}
}

void write_dir() {
	write_block(DIRECTORY_BLOCK, &root_dir, sizeof(dir_struct));
}

void dir_allocate_file(int inode, const char * name) {

    int i;

    for (i=0; i< MAX_FILES_PER_DIRECTORY;++i){
	if( root_dir.u_file[i].free ){
	    root_dir.u_file[i].inode_number = inode;    
	    root_dir.u_file[i].free = false;
	    strcpy(root_dir.u_file[i].file_name, name);
	    break;
	}// add to root_dir the number of files???
    }
}

bool is_dir_full() {
    
    if (root_dir.no_files == MAX_FILES_PER_DIRECTORY){
	return true;
    }else{
	return false;
    }
}

/*
   Finds the file specified by name
   sets the file parameter to the file that was found
*/
bool find_file(const char * name, file_struct * file) {
    
    int i;

    for(i=0;i<MAX_FILES_PER_DIRECTORY; ++i){
	if(!root_dir.u_file[i].free){
	    if(!strcmp(root_dir.u_file[i].file_name, name)){
		*file = root_dir.u_file[i];
		return true;
	    }

	}	
    }
    
    return false;
}

/*
   Free file's blocks
   Free file's inode
   Free file
*/
void dir_remove_file(file_struct file) {

    int i;
    inode in;

    read_inode(file.inode_number, &in);

    for(i=0;i<in.no_blocks;++i){
	free_block(in.blocks[i]);
    }

    in.free=true;
    write_inode(file.inode_number, &in);

    for(i=0;i<MAX_FILES_PER_DIRECTORY;++i){
	if(root_dir.u_file[i].inode_number == file.inode_number){
	    root_dir.u_file[i].free = true;
	}
    }
}

void dir_rename_file(const char * old, const char * new) {

    int i;
    
    for(i=0; i<MAX_FILES_PER_DIRECTORY; ++i){
	if(strcmp(root_dir.u_file[i].file_name, old) == 0){
	    strcpy(new, root_dir.u_file[i].file_name);
	    //warning: new is a const and strcpy takes char *
	}
    }
}
