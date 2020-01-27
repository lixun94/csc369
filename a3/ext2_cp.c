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

void cp(char* img, char *source, char *dest){

	int fd = open(img, O_RDWR);

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if(disk == MAP_FAILED){
		perror("mmap");
		exit(1);
	}
	//error checking
	//open file to read
	FILE *f = fopen(source, "r");
	// check if file is valid
	if(f == NULL){
		printf("file not exists\n");
		exit(ENOENT);
	}
	//get dest directory
	struct ext2_inode *parent = getInode(get_parent_path(dest), disk);
	//check if dest is valid
	if(parent == NULL){
		printf("Destination not valid\n");
		exit(ENOENT);
	}
	//check if file already exists
	if(getInode(dest, disk) ){
		printf("file already exists\n");
		exit(EEXIST);
	}

	fseek(f, 0L, SEEK_END);
	long int file_size = ftell(f);

	struct ext2_super_block *super_block = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *group_desc = (struct ext2_group_desc *)(disk + (2 * EXT2_BLOCK_SIZE));

	//calculate block needed
	int num_block = file_size / EXT2_BLOCK_SIZE;
	if(file_size % EXT2_BLOCK_SIZE ){
		num_block++;
	}
	if(num_block > 12){
		num_block ++;
	}
	//check if sapce is valid
	if(num_block > super_block -> s_free_blocks_count){
		printf("Not enough space");
		exit(ENOSPC);
	}

	fseek(f, 0L, SEEK_SET);

	struct ext2_inode *inode_table = (struct ext2_inode *)(disk + (group_desc->bg_inode_table * EXT2_BLOCK_SIZE));

	// Assign new inode and mark as used
	unsigned int new_inode_num = first_free_inode(disk);
	add_inode_bitmap(new_inode_num, disk);
	struct ext2_inode *new_inode = &(inode_table[new_inode_num - 1]);

	// Allocate new inode
	new_inode->i_mode = EXT2_S_IFREG;
	new_inode->i_size = file_size;
	new_inode->i_links_count = 1;
	new_inode->i_blocks = 2 * num_block;
	int i;
	for(i = 0; i < 15; i++)
	{
		new_inode->i_block[i] = 0;
	}

	// Allocate all needed blocks
	for(i = 0; i < 13 && i < num_block; i++)
	{
		new_inode->i_block[i] = first_free_block(disk);
		add_block_bitmap(new_inode->i_block[i], disk);
	}
	if(i == 13)
	{
		unsigned int *indirect_block = (unsigned int *)(disk + (new_inode->i_block[12] * EXT2_BLOCK_SIZE));
		int j;
		for(j = 0; i < num_block; i++, j++)
		{
			indirect_block[j] = first_free_block(disk);
			add_block_bitmap(indirect_block[j], disk);
		}
	}

	// Copy info into blocks
	void *block;
	for(i = 0; i < num_block && i < 12; i++)
	{
		block = (void *)(disk + (new_inode->i_block[i] * EXT2_BLOCK_SIZE));
		fread(block, sizeof(char), EXT2_BLOCK_SIZE / sizeof(char), f);
	}
	if(num_block > 12)
	{
		unsigned *indirect_block = (unsigned int *)(disk + (new_inode->i_block[i++] * EXT2_BLOCK_SIZE));
		int j;
		for(j = 0; i < num_block; i++, j++)
		{
			block = (void *)(disk + (indirect_block[j] * EXT2_BLOCK_SIZE));
			fread(block, sizeof(char), EXT2_BLOCK_SIZE / sizeof(char), f);
		}
	}

	// Create a directory entry in parent directory
	unsigned int total_size;
	struct ext2_dir_entry_2 *prev_entry;
	struct ext2_dir_entry_2 *new_dir_entry;
	char *taget_name = &(dest[strlen(get_parent_path(dest))]);
	for(i = 0; i < 12 && parent->i_block[i]; i++)
	{
		prev_entry = (struct ext2_dir_entry_2 *)(disk + (parent->i_block[i] * EXT2_BLOCK_SIZE));
		total_size = EXT2_BLOCK_SIZE;
		while(total_size)
		{
			if(prev_entry->rec_len >= (2 * sizeof(struct ext2_dir_entry_2)) + strlen(taget_name)
					+ (4 - (strlen(taget_name) % 4)) + prev_entry->name_len + (4 - (prev_entry->name_len % 4)))
			{
				break;
			}
			total_size -= prev_entry->rec_len;
			prev_entry = (struct ext2_dir_entry_2 *)((char *)prev_entry + prev_entry->rec_len);
		}
		if(total_size)
		{
			break;
		}
	}

	// If found space
	if(parent->i_block[i])
	{
		unsigned int prev_size = sizeof(struct ext2_dir_entry_2) + prev_entry->name_len + (prev_entry->name_len % 4);
		new_dir_entry = (struct ext2_dir_entry_2 *)((char *)prev_entry + prev_size);
		new_dir_entry->inode = new_inode_num;
		new_dir_entry->rec_len = prev_entry->rec_len - prev_size;
		new_dir_entry->name_len = strlen(taget_name);
		new_dir_entry->file_type = EXT2_FT_REG_FILE;
		strncpy(new_dir_entry->name, taget_name, new_dir_entry->name_len);

		prev_entry->rec_len = prev_size;
	}
	// Add block if no space in any used block
	else
	{
		parent->i_block[i] = first_free_block(disk);
		add_block_bitmap(parent->i_block[i], disk);
		new_dir_entry = (struct ext2_dir_entry_2 *)(disk + (parent->i_block[i] * EXT2_BLOCK_SIZE));
		new_dir_entry->inode = new_inode_num;
		new_dir_entry->rec_len = EXT2_BLOCK_SIZE;
		new_dir_entry->name_len = strlen(taget_name);
		new_dir_entry->file_type = EXT2_FT_REG_FILE;
		strncpy(new_dir_entry->name, taget_name, new_dir_entry->name_len);

		parent->i_size += EXT2_BLOCK_SIZE;
		parent->i_blocks += 2;
	}




}




int main(int argc, char **argv) {

    if(argc != 4) {
        fprintf(stderr, "Usage: ext2_cp <virtual_disk> <source_file_path> <dest_virtual_path>\n");
        exit(1);
    }
    
    cp(argv[1], argv[2], argv[3]);
    return 0;
}
