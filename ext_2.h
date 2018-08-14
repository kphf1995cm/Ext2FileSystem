/*
 * Time: 2016/10/11
 * Author: PengKuang <kphf1995cm@outlook.com>
 */

#ifndef EXT_2_H
#define EXT_2_H
#define blocks 4611
#define blocksiz 512
#define inodesiz 64
#define data_begin_block 515
#define dirsiz 32
#define EXT2_NAME_LEN 15
#define PATH "vdisk.txt"
# define data_blocks 4096
# define block_blockaddr_num 128

typedef struct ext2_group_desc //68byte 80byte
{
	char bg_volume_name[16];
	int bg_block_bitmap;
	int bg_inode_bitmap;
	int bg_indoe_table;
	int bg_free_blocks_count;
	int bg_free_inodes_count;
	int bg_used_dirs_count;//本组目录个数
	char psw[16];
	char bg_pad[24];
}ext2_group_desc;

typedef struct ext2_inode//64byte (int 2) (可能需要改成short)
{
	short i_mode;
	short i_blocks;
	short i_size;
	//time_t i_atime;
	time_t i_ctime;
	time_t i_mtime;
	//time_t i_dtime;
	short i_block[8];
	char i_pad[24];
}ext2_inode;

typedef struct ext2_dir_entry//32byte
{
	int inode;
	int rec_len;
	int name_len;
	int file_type;
	char name[EXT2_NAME_LEN];
	char dir_pad;
}ext2_dir_entry;

#endif //EXT_2_H!