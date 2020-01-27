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

void rm(char *img, char *path){

	int fd = open(img, O_RDWR);

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
		// Getting inode for file that was asked for
	struct ext2_inode *cur_file = getInode(path, disk );
	if (cur_file == NULL) {
		printf("No such file or directory\n");
		exit(ENOENT);
	}
	else if (cur_file->i_mode & EXT2_S_IFDIR) {
		printf("Cannot remove directory\n");
		exit(ENOENT);
	}

	//path is valid, removing file
	cur_file->i_links_count--;
	//remove parent link
	struct ext2_inode *parentDir = getInode(get_parent_path(path), disk);
	char *file_name = &(path[strlen(path) - (strlen(path) - strlen(get_parent_path(path)))]);
	unsigned int *cur_block = parentDir->i_block;
	unsigned int inodeNum;
	int i;
	for (i = 0; i < 12 && cur_block[i]; i++) {
		unsigned int total_size = EXT2_BLOCK_SIZE;
		struct ext2_dir_entry_2 *de = (struct ext2_dir_entry_2 *)(disk + (cur_block[i] * EXT2_BLOCK_SIZE));
	       	while(total_size > 0) {
				if (strncmp(file_name, de->name, de->name_len) == 0) {
					struct ext2_dir_entry_2 *preventry = (struct ext2_dir_entry_2 *)(disk + (cur_block[i] * EXT2_BLOCK_SIZE));
					while((struct ext2_dir_entry_2 *)((char *)preventry + preventry->rec_len) != de){
						preventry = (struct ext2_dir_entry_2 *)((char *)preventry + preventry->rec_len);
					}
					preventry->rec_len += de->rec_len;
					inodeNum = de->inode;
					memset(de, 0, sizeof(struct ext2_dir_entry_2));
					break;

			}
	     		total_size -= de->rec_len;
	    		de = (struct ext2_dir_entry_2 *)((char *)de + de->rec_len);
		}
		if (total_size >0) {
			break;
		}
	}

	cur_block = cur_file->i_block;
	// if no links exist then set inode = 0, change bitmaps
	if (cur_file->i_links_count == 0) {
		//removing inode bit from inode bitmap
		rem_inode_bitmap(inodeNum, disk);
		//removing inode block bits from block bitmap
		for (i = 0; i < 12 && cur_block[i]; i++) {
			rem_block_bitmap(cur_block[i], disk);
		}

		if (cur_block[i]) {
			int j;
			unsigned int *single_indirect = (unsigned int *) (disk +  cur_block[i] * EXT2_BLOCK_SIZE);
			for (j = 0; single_indirect[j] ; j++) {
				rem_block_bitmap(single_indirect[j], disk);
			}
			rem_block_bitmap(cur_block[i], disk);
		}
		//set inode to 0
		cur_file = 0;
	}

	return ;

}


int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_rm <virtual_disk> <virtual_path>\n");
        exit(1);
    }
    rm(argv[1], argv[2]);
    return 0;
}
