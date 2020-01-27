#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

#define EXT2_S_IFLNK 0xA000 /* symbolic link */

unsigned char *disk;


int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: readimg <image file name>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	   perror("mmap");
	   exit(EXIT_FAILURE);
    }

    /* Super-block struct. Always start at byte offset 1024 from the start of disk.  */
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    
    /* Group description struct:
     * The block group desciptor follows right after the super block. Since each
     * block has size 1024 bytes, the block group descriptors must starts at byte
     * offset 1024 * 2 = 2048*/
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d", gd->bg_used_dirs_count);

    /* Group descriptor tells where the block bit map is. */
    int bit_pos = gd->bg_block_bitmap;

    /* Pointer pointing to the bitmap. */
    unsigned char *bit_map_block = disk + EXT2_BLOCK_SIZE * bit_pos;

    /* Number of blocks == number of bits in the bit map. */
    int num_bit = sb->s_blocks_count;

    printf("\nBlock bitmap: ");
    int i;
    int k;
    // printf("%d\n", num_bit);

    /* Looping through the bit map block byte by byte. */
    for (i = 0; i < num_bit/(sizeof(unsigned char) * 8); i++) {

        /* Looping through each bit a byte. */
        for (k = 0; k < 8; k++) {
            printf("%d", (bit_map_block[i] >> k) & 1);
        }
        printf(" ");
    }


    /* Get the inode bit map position from group descriptor. */
    int inode_bit_pos = gd->bg_inode_bitmap;

    /* Pointer pointing to inode bit map block. */
    unsigned char *inode_bit_map = disk + EXT2_BLOCK_SIZE * inode_bit_pos;

    /* Number of inodes == number of bits in inode_bitmap. */
    int num_inode_bit = sb->s_inodes_count;

    printf("\nInode bitmap: ");
    for (i = 0; i < num_inode_bit/(sizeof(unsigned char) * 8); i++) {

        /* Looping through each bit in a byte and print the least
         * significant bit first. That is reverse the order of the bit. */
        for (k = 0; k < 8; k++) {
            printf("%d", (inode_bit_map[i] >> k) & 1);
        }
        printf(" ");
    }

    printf("\n\n");

    /* Pointer poiting to inode table. */
    struct ext2_inode  *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);

    printf("Inodes:\n");
    /* Loop through each inode in the system. */
    for (i = 0; i < sb->s_inodes_count; i++) {
        /* An inode contains information about a single file. */

        /* The first eleven inodes indexed from 0 to 10 are reserved.
         * We only interest in the root inodes and the inodes that are not reserved.
         * We also don't print any inodes that have no data. */
        if (inode[i].i_size > 0 && (i == 1 || i >= EXT2_GOOD_OLD_FIRST_INO)) {
            char type = '\0';

            /* Check if it is a regular file and must be not a link. */
            if ((inode[i].i_mode & EXT2_S_IFREG) && !((inode[i].i_mode >> 13) & 1)) {
                type = 'f';

            /* Check if it is a directory. */
            } else if (inode[i].i_mode & EXT2_S_IFDIR) {
                type = 'd';

            /* Check if it is a link.*/
            } else if (inode[i].i_mode & EXT2_S_IFLNK) {
                type = 'l';
            }

            /* inode number = index + 1 */
            printf("[%d] type: %c ", i + 1, type);
            printf("size: %u ", inode[i].i_size);
            printf("links: %hu ", inode[i].i_links_count);
            printf("blocks: %u\n", inode[i].i_blocks);
            printf("[%d] Blocks:  ", i + 1);

            int no_white_space;

            /* Identify where not to put the white space. */
            for (k=0; k < 15; k++) {
                if (inode[i].i_block[k]) {
                    no_white_space = k;
                }
            }

            /* Block stores a list of block number that stores the data.
             * The first 12 are direct blocks. */
            for (k=0; k < 15; k++) {
                if (inode[i].i_block[k]) {
                    if (no_white_space == k) {
                        printf("%u", inode[i].i_block[k]);
                    } else {
                        printf("%u ", inode[i].i_block[k]);
                    }

                    if (k == 12) {
                        unsigned int *indirect = (unsigned int *) (disk + EXT2_BLOCK_SIZE * inode[i].i_block[k]);
                        int h;
                        printf("\nSecond level blocks: ");
                        for (h = 0; h < EXT2_BLOCK_SIZE / sizeof(unsigned int); h++) {
                            if (indirect[h]) {
                                printf("%u ", indirect[h]);
                            }
                        }
                    }
                }
            }

            printf("\n");
        }
    }

    /* TE9 */
    printf("\n");
    printf("Directory Blocks:\n");

    /* Table of inodes used to TE9. */
    struct ext2_inode  *inode2 = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);


    /* Loop through each of the inodes. */
    for (i = 0; i < sb->s_inodes_count; i++) {

        /* Only intrested in the inode for the root directiory and the
         * inodes that are not reserved. */
        if (i == 1 || i >= EXT2_GOOD_OLD_FIRST_INO) {


            /* Use the bit map to check if the file for the inode is available.  */
            int bit_map_byte = i / 8;
            int bit_order = i % 8;
            if ((inode_bit_map[bit_map_byte] >> bit_order) & 1) {

                /* If the current inode is a directory. */
                if (inode2[i].i_mode & EXT2_S_IFDIR) {

                    int block_num;

                    /* Only needs to worry about direct block for this ex.
                     * Recalls that there are 12 direct blocks. */
                    for (k = 0; k < 12; k++) {

                        /* If the block exists i.e. not 0. */
                        if (inode[i].i_block[k]) {
                            block_num = inode[i].i_block[0];
                            /* get the block number where the directories are stored. */

                            block_num = inode[i].i_block[k];
                            printf("   DIR BLOCK NUM: %d (for inode %d)\n", block_num, i + 1);


                            /* Go the the block_num on the disk to extract the directory. */
                            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);

                            int curr_pos = 0;

                            /* Total size of the directories in a block cannot exceed a block size */
                            while (curr_pos < EXT2_BLOCK_SIZE) {
                                char type = '\0';
                                if (dir->file_type == EXT2_FT_REG_FILE) {
                                    type = 'f';
                                } else if (dir->file_type == EXT2_FT_DIR) {
                                    type = 'd';
                                } else if (dir->file_type == EXT2_FT_SYMLINK) {
                                    type = 'l';
                                }
                                printf("Inode: %u ", dir->inode);
                                printf("rec_len: %hu ", dir->rec_len);
                                printf("name_len: %u ", dir->name_len);
                                char *print_name = malloc(sizeof(char) * dir->name_len + 1);
                                int u;
                                for (u = 0; u < dir->name_len; u++) {
                                    print_name[u] = dir->name[u];
                                }
                                print_name[dir->name_len] = '\0';
                                printf("type= %c ", type);
                                printf("name=%s\n", print_name);
                                free(print_name);
                                /* Moving to the next directory */
                                curr_pos = curr_pos + dir->rec_len;
                                dir = (void*) dir + dir->rec_len;


                                printf("curr_pos: %d\n", curr_pos);
                                //printf("dir->inode: %d\n", dir->inode);
                                //printf("size of struct: %lu\n", sizeof(struct ext2_dir_entry_2));
                                //printf("size of int: %lu, sizeof short: %lu, size of char: %lu", sizeof(unsigned int), sizeof(unsigned short), sizeof(unsigned char));
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
