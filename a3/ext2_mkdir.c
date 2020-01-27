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

void makedir(char* img, char *dest){

	int fd = open(img, O_RDWR);

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(disk == MAP_FAILED) {
	    perror("mmap");
		exit(1);
		}

	struct ext2_inode *parent = getInode(get_parent_path(dest), disk);

	//error checking
	if(!parent){
		printf("Path not exist\n");
		exit(ENOENT);
	}
	if(getInode(dest, disk)){
		printf("Directory already exist\n");
		exit(EEXIST);
	}

	unsigned int inode_num = first_free_inode(disk);
	unsigned int block_num = first_free_block(disk);

	add_inode_bitmap(inode_num, disk);
	add_block_bitmap(block_num, disk);

	struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + (2 * EXT2_BLOCK_SIZE));
	struct ext2_inode *inode_table = (struct ext2_inode *)(disk + (gd->bg_inode_table * EXT2_BLOCK_SIZE));
	struct ext2_inode *new_inode = &(inode_table[inode_num - 1]);

	new_inode -> i_mode = EXT2_S_IFDIR;
	new_inode -> i_links_count = 2;
	new_inode -> i_size = EXT2_BLOCK_SIZE;
	new_inode -> i_blocks = 2;
	new_inode -> i_block[0] = block_num;

	int i = 1;
	while(i < 15){
		i++;
		new_inode -> i_block[i] = 0;
	}

	//set first dir_entry to itself
	struct ext2_dir_entry_2 *new_de = (struct ext2_dir_entry_2 *)(disk + (block_num * EXT2_BLOCK_SIZE));
	new_de->inode = inode_num;
	new_de->rec_len = sizeof(struct ext2_dir_entry_2) + 4;
	new_de->name_len = 1;
	new_de->file_type = EXT2_FT_DIR;
	strncpy(new_de->name, ".", 1);

	//set another dir_entry to = parent
	new_de = (struct ext2_dir_entry_2 *)((char *)new_de + new_de->rec_len);
	new_de->inode = ((struct ext2_dir_entry_2 *)(disk + (parent->i_block[0] * EXT2_BLOCK_SIZE)))->inode;
	new_de->rec_len = EXT2_BLOCK_SIZE - (sizeof(struct ext2_dir_entry_2) + 4);
	new_de->name_len = 2;
	new_de->file_type = EXT2_FT_DIR;
	strncpy(new_de->name, "..", 2);
	parent->i_links_count++;

	//find empty dir_entry in parent and set
	unsigned int total_size;
	struct ext2_dir_entry_2 *prev_entry;
	char *source_name = &(dest[strlen(get_parent_path(dest))]);
	for(i = 0; i < 12 && parent->i_block[i]; i++)	{

		prev_entry = (struct ext2_dir_entry_2 *)(disk + (parent->i_block[i] * EXT2_BLOCK_SIZE));
		total_size = EXT2_BLOCK_SIZE;
		while(total_size){
			int rounding_error = 0;
			if(strlen(source_name) % 4){
				rounding_error += 4 - (strlen(source_name) % 4);
				}
			if(prev_entry->name_len % 4){
				rounding_error += 4 - (prev_entry->name_len % 4);
				}

			if(prev_entry->rec_len >= (2 * sizeof(struct ext2_dir_entry_2)) + strlen(source_name) + prev_entry->name_len + rounding_error)
				{
					break;
				}
			total_size -= prev_entry->rec_len;
			prev_entry = (struct ext2_dir_entry_2 *)((char *)prev_entry + prev_entry->rec_len);
		}
		if(total_size){
				break;
			}
	}

	if(parent->i_block[i]){
		unsigned int prev_size = sizeof(struct ext2_dir_entry_2) + prev_entry->name_len;
		if(prev_entry->name_len % 4){
			prev_size += 4 - (prev_entry->name_len % 4);
		}

		new_de = (struct ext2_dir_entry_2 *)((char *)prev_entry + prev_size);
		new_de->inode = inode_num;
		new_de->rec_len = prev_entry->rec_len - prev_size;
		new_de->name_len = strlen(source_name);
		new_de->file_type = EXT2_FT_DIR;
		strncpy(new_de->name, source_name, new_de->name_len);
		prev_entry->rec_len = prev_size;
		}
	else{
		parent->i_block[i] = first_free_block(disk);
		add_block_bitmap(parent->i_block[i], disk);
		new_de = (struct ext2_dir_entry_2 *)(disk + (parent->i_block[i] * EXT2_BLOCK_SIZE));
		new_de->inode = inode_num;
		new_de->rec_len = EXT2_BLOCK_SIZE;
		new_de->name_len = strlen(source_name);
		new_de->file_type = EXT2_FT_DIR;
		strncpy(new_de->name, source_name, new_de->name_len);

		parent->i_size += EXT2_BLOCK_SIZE;
		parent->i_blocks += 2;
	}

		


}


int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_mkdir <virtual_disk> <virtual_path>\n");
        exit(1);
    }
    makedir(argv[1], argv[2]);
    
    return 0;
}
