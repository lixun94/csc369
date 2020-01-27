#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "helper.h"
#include "errno.h"
#include "string.h"

unsigned char *disk;


void ln(char *img, char *path1, char *path2){

	int fd = open(img, O_RDWR);

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if(disk == MAP_FAILED) {
		    perror("mmap");
		    exit(1);
		}

	//get source and destination file inode
	struct ext2_inode *source_file = getInode(path1, disk);
	struct ext2_inode *dest_file = getInode(path2, disk);


	//check if valid for ln action
	if(source_file == NULL){
		printf("No such file or directory\n");
		exit(ENOENT);
	}

	else if((dest_file != NULL)&&(dest_file -> i_mode & EXT2_S_IFREG)){
		printf("Destination already exits\n");
		exit(EEXIST);
	}

	else if ((source_file->i_mode & EXT2_S_IFDIR) || (dest_file != NULL && (dest_file->i_mode & EXT2_S_IFDIR))) {
			printf("Cannot link directories\n");
			exit(EISDIR);
		}

	//legal to perform ln action
	struct ext2_inode *parent = getInode(get_parent_path(path1), disk);
	char *source_name = &(path1[strlen(get_parent_path(path1))]);
	unsigned int *source_block = parent -> i_block;
	struct ext2_dir_entry_2 *de;
	struct ext2_dir_entry_2 *source_entry;
	unsigned int total_size;

	//get source file directory entry
	int i;
	for(i = 0; i < 12 && source_block[i]; i++){

		total_size = EXT2_BLOCK_SIZE;
		de = (struct ext2_dir_entry_2 *)(disk + (source_block[i] * EXT2_BLOCK_SIZE));
		while(total_size) {
			if (strncmp(source_name, de->name, de->name_len) == 0) {
				source_entry = de;
				break;
			    }

		    total_size -= de->rec_len;
		    de = (struct ext2_dir_entry_2 *)((char *)de + de->rec_len);
		    }

		if (total_size >0) {
			break;
		    }
	    }

	//get parent inode for destination file
	struct ext2_inode *dest_parent = getInode(get_parent_path(path2), disk);
	char *dest_name = &(path2[strlen(get_parent_path(path2))]);
	unsigned int *dest_block = dest_parent->i_block;


	//find empty directory entry and link

	for(i = 0; i < 12 && dest_block[i]; i++){
		total_size = EXT2_BLOCK_SIZE;
		de = (struct ext2_dir_entry_2 *)(disk + (dest_block[i] * EXT2_BLOCK_SIZE));
		while(total_size){
			int error = 0;
			if(strlen(dest_name) % 4){
				error += 4 - strlen(dest_name)%4;
			}
			if(de -> name_len %4){
				error += 4 - de->name_len %4;
			}
			if(de -> rec_len >= (2 * sizeof(struct ext2_dir_entry_2)) + strlen(dest_name) + de->name_len + error){
						break;
					}
			total_size -= de -> rec_len;
			de = (struct ext2_dir_entry_2 *)((char *)de + de->rec_len);
		}
		if(total_size > 0){
			break;
		}

	}


	if(dest_block[i]){

		unsigned int prev_size = sizeof(struct ext2_dir_entry_2) + de ->name_len + (de->name_len % 4);
		struct ext2_dir_entry_2 *new_de = (struct ext2_dir_entry_2 *)((char *)de + prev_size);
		new_de->inode = source_entry->inode;
		new_de->name_len = strlen(dest_name);
		new_de->rec_len = de->rec_len - prev_size;

		new_de->file_type = EXT2_FT_SYMLINK;
		strncpy(new_de->name, dest_name, new_de->name_len);
		de->rec_len = prev_size;

	}

	else{

		dest_block[i] = first_free_block(disk);
		add_block_bitmap(dest_block[i], disk);
		struct ext2_dir_entry_2 *new_de = (struct ext2_dir_entry_2 *)(disk + (dest_block[i] * EXT2_BLOCK_SIZE));
		new_de->inode = source_entry->inode;
		new_de->rec_len = EXT2_BLOCK_SIZE;
		new_de->name_len = strlen(dest_name);
		new_de->file_type = EXT2_FT_SYMLINK;
		strncpy(new_de->name, dest_name, new_de->name_len);
		dest_parent->i_size += EXT2_BLOCK_SIZE;
		dest_parent->i_blocks += 2;

	}

	source_file -> i_links_count++;
}



int main(int argc, char **argv) {

    if(argc != 4) {
        fprintf(stderr, "Usage: readimg <image file name>\n");
        exit(1);
    }
    ln(argv[1], argv[2], argv[3]);
    
    return 0;
}


