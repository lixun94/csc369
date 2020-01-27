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



void ls(char *img, char *dir){

	int fd = open(img, O_RDWR);

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (disk == MAP_FAILED){
		perror("mmap");
		exit(1);
	}

	// get the current directory inode
	struct ext2_inode *currDir = getInode(dir, disk);

	if (currDir == NULL){
		printf("No such file or directory");
		exit(ENOENT);
	}
	else if(currDir -> i_mode & EXT2_S_IFREG){
		printf("%s\n", &(dir[strlen(get_parent_path(dir))]));
	}

	unsigned int *currBlock = currDir -> i_block;
	int i = 0;

	while (i < 12 && currBlock[i]){
		unsigned int total_size = EXT2_BLOCK_SIZE;

		struct ext2_dir_entry_2 *dir2 = (struct ext2_dir_entry_2 *)(disk + (currBlock[i] * EXT2_BLOCK_SIZE));

		while (total_size > 0) {
 			char dirName[256];
 			strncpy(dirName, dir2->name, (int)dir2->name_len);
 			dirName[(int)dir2->name_len] = '\0';
            printf("%s\n", dirName);
            dir2 = (struct ext2_dir_entry_2 *)((char *)dir2 + dir2->rec_len);
            total_size -= dir2->rec_len;
            }

		i++;
	}

}


int main(int argc, char *argv[]) {

    if(argc != 3) {
        fprintf(stderr, "ext2_ls <image name> <absolute path on the disk>\n");
        exit(1);
    }

    ls(argv[1], argv[2]);

    return 0;

}
