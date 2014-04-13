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
	}//TODO: add to root_dir the number of files
    }
}

bool is_dir_full() {
    
    if (root_dir.no_files == MAX_FILES_PER_DIRECTORY){
	return true;
    }else{
	return false;
    }
}

/* TODO
   Finds the file specified by name
   sets the file parameter to the file that was found
*/
bool find_file(const char * name, file_struct * file) {
	return false;
}

/* TODO
   Free file's blocks
   Free file's inode
   Free file
*/
void dir_remove_file(file_struct file) {
}

/* TODO */
void dir_rename_file(const char * old, const char * new) {
}
