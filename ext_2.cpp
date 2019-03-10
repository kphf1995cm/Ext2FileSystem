/*
 *
 * Desc: a 2MB ext2 file system
 * Time: 2016/10/11
 * Author: PengKuang <kphf1995cm@outlook.com>
 *
 */

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>
#include<string>
#include<iostream>
#include"ext_2.h"
using namespace std;

ext2_group_desc group_desc;
ext2_inode inode;
ext2_dir_entry dir;
FILE *f;
unsigned int last_allco_inode = 0;
unsigned int last_allco_block = 0;

int format()
{
	FILE*fp = NULL;
	int i;
	unsigned int zero[blocksiz / 4];
	time_t now;
	time(&now);
	while (fp == NULL)
		fp = fopen(PATH, "w");
	for (i = 0; i < blocksiz / 4; i++)
		zero[i] = 0;
	for (i = 0; i < blocks; i++)
	{
		fseek(fp, i*blocksiz, SEEK_SET);
		fwrite(&zero, blocksiz, 1, fp);
	}
	//init group_desc
	strcpy(group_desc.bg_volume_name, "a disk");
	group_desc.bg_block_bitmap = 1;
	group_desc.bg_inode_bitmap = 2;
	group_desc.bg_indoe_table = 3;
	group_desc.bg_free_blocks_count = data_blocks - 1;
	group_desc.bg_free_inodes_count = data_blocks - 1;
	group_desc.bg_used_dirs_count = 1;
	strcpy(group_desc.psw, "000");
	fseek(fp, 0, SEEK_SET);
	fwrite(&group_desc, sizeof(ext2_group_desc), 1, fp);

	//init data_bitmap inode_bitmap
	zero[0] = 0x80000000;
	fseek(fp, blocksiz, SEEK_SET);
	fwrite(&zero, blocksiz, 1, fp);
	fseek(fp, 2 * blocksiz, SEEK_SET);
	fwrite(&zero, blocksiz, 1, fp);

	inode.i_mode = 2;
	inode.i_blocks = 1;
	inode.i_size = 64;
	//inode.i_atime = now;
	inode.i_ctime = now;
	inode.i_mtime = now;
	//inode.i_dtime = now;
	fseek(fp, 3 * blocksiz, SEEK_SET);
	fwrite(&inode, sizeof(ext2_inode), 1, fp);

	dir.inode = 0;
	dir.rec_len = 32;
	dir.name_len = 1;
	dir.file_type = 2;
	strcpy(dir.name, ".");
	fseek(fp, data_begin_block*blocksiz, SEEK_SET);
	fwrite(&dir, sizeof(ext2_dir_entry), 1, fp);

	dir.inode = 0;
	dir.rec_len = 32;
	dir.name_len = 2;
	dir.file_type = 2;
	strcpy(dir.name, "..");
	fseek(fp, data_begin_block*blocksiz + dirsiz, SEEK_SET);
	fwrite(&dir, sizeof(ext2_dir_entry), 1, fp);

	fclose(fp);
	return 0;
}

int dir_entry_position(int dir_entry_begin, short i_block[8])
{
	int dir_blocks = dir_entry_begin / 512;
	int block_offset = dir_entry_begin % 512;
	int a;
	FILE*fp = NULL;
	if (dir_blocks <= 5)
		return data_begin_block*blocksiz + i_block[dir_blocks] * blocksiz + block_offset;
	else//间接索引
	{
		while (fp == NULL)
			fp = fopen(PATH, "r+");
		dir_blocks -= 6;
		if (dir_blocks < block_blockaddr_num)//一级索引 4*128
		{
			int a;
			fseek(fp, data_begin_block*blocksiz + i_block[6] * blocksiz + dir_blocks * 4, SEEK_SET);
			fread(&a, sizeof(int), 1, fp);
			fclose(fp);
			return data_begin_block*blocksiz + a*blocksiz + block_offset;
		}
		else
		{
			dir_blocks = dir_blocks - block_blockaddr_num;
			fseek(fp, data_begin_block*blocksiz + i_block[7] * blocksiz + dir_blocks / block_blockaddr_num * 4, SEEK_SET);
			int addr1;
			fread(&addr1, sizeof(int), 1, fp);
			fseek(fp, data_begin_block*blocksiz + addr1 * blocksiz + dir_blocks % block_blockaddr_num * 4, SEEK_SET);
			int addr2;
			fread(&addr2, sizeof(int), 1, fp);
			fclose(fp);
			return data_begin_block*blocksiz + addr2*blocksiz + block_offset;
		}
	}
}

int Open(ext2_inode*current, char*name)//在当前目录下打开一个目录
{
	FILE*fp = NULL;
	int i;
	while (fp == NULL)
		fp = fopen(PATH, "r+");
	for (i = 0; i < (current->i_size / 32); i++)
	{
		fseek(fp, dir_entry_position(i * 32, current->i_block), SEEK_SET);
		fread(&dir, sizeof(ext2_dir_entry), 1, fp);
		if (!strcmp(dir.name, name))
		{
			if (dir.file_type == 2)//directory
			{
				fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
				fread(current, sizeof(ext2_inode), 1, fp);
				fclose(fp);
				return 0;
			}
		}
	}
	fclose(fp);
	return 1;
}

int Close(ext2_inode*current)//关闭时修改最后访问时间，打上以目录作为当前目录
{
	time_t now;
	ext2_dir_entry bentry;
	FILE* fout;
	fout = fopen(PATH, "r+");
	time(&now);
	//current->i_atime = now;
	fseek(fout, data_begin_block*blocksiz + (current->i_block[0]) * blocksiz, SEEK_SET);
	fread(&bentry, sizeof(ext2_dir_entry), 1, fout);
	fseek(fout, 3 * blocksiz + bentry.inode * sizeof(ext2_inode), SEEK_SET);
	fwrite(current, sizeof(ext2_inode), 1, fout);
	fclose(fout);
	return Open(current, "..");
}

int Read(ext2_inode*current, char*name)// read file from current dir
{
	FILE*fp = NULL;
	while (fp == NULL)
		fp = fopen(PATH, "r+");
	int i;
	for (i = 0; i < current->i_size / 32; i++)
	{
		fseek(fp, dir_entry_position(i * 32, current->i_block), SEEK_SET);
		fread(&dir, sizeof(ext2_dir_entry), 1, fp);
		if (strcmp(dir.name, name) == 0)
		{
			if (dir.file_type == 1)//file
			{
				time_t now;
				ext2_inode node;
				char content_char;
				fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
				fread(&node, sizeof(ext2_inode), 1, fp);
				//i = 0;
				for (i = 0; i < node.i_size; i++)
				{
					fseek(fp, dir_entry_position(i, node.i_block), SEEK_SET);
					fread(&content_char, sizeof(char), 1, fp);
					if (content_char == 0xD)
						printf("\n");
					else
						printf("%c", content_char);
				}
				printf("\n");
				time(&now);
				//node.i_atime = now;
				fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
				fwrite(&node, sizeof(ext2_inode), 1, fp);
				fclose(fp);
				return 0;
			}
		}
	}
	fclose(fp);
	return 1;
}

//find free inode
int FindInode()
{
	FILE*fp = NULL;
	unsigned int zero[blocksiz / 4];
	int i;
	while (fp == NULL)
		fp = fopen(PATH, "r+");
	fseek(fp, 2 * blocksiz, SEEK_SET);
	fread(zero, blocksiz, 1, fp);//read inode bitmap
	for (i = last_allco_inode; i < (last_allco_inode + blocksiz / 4); i++)
	{
		if (zero[i % (blocksiz / 4)] != 0xffffffff)
		{
			unsigned int j = 0x80000000, k = zero[i % (blocksiz / 4)], l = i;;
			for (i = 0; i < 32; i++)
			{
				if (!(k&j))
				{
					zero[l % (blocksiz / 4)] = zero[l % (blocksiz / 4)] | j;
					group_desc.bg_free_inodes_count -= 1;
					fseek(fp, 0, SEEK_SET);
					fwrite(&group_desc, sizeof(ext2_group_desc), 1, fp);

					fseek(fp, 2 * blocksiz, SEEK_SET);
					fwrite(zero, blocksiz, 1, fp);
					last_allco_inode = l % (blocksiz / 4);
					fclose(fp);
					return l % (blocksiz / 4) * 32 + i;
				}
				else
					j = j / 2;
			}


		}
	}
	fclose(fp);
	return -1;
}


//find free block
int FindBlock()
{
	FILE*fp = NULL;
	unsigned int zero[blocksiz / 4];
	int i;
	while (fp == NULL)
		fp = fopen(PATH, "r+");
	fseek(fp, 1 * blocksiz, SEEK_SET);
	fread(zero, blocksiz, 1, fp);//read inode bitmap
	for (i = last_allco_block; i < (last_allco_block + blocksiz / 4); i++)
	{
		if (zero[i % (blocksiz / 4)] != 0xffffffff)
		{
			unsigned int j = 0x80000000, k = zero[i % (blocksiz / 4)], l = i;;
			for (i = 0; i < 32; i++)
			{
				if (!(k&j))
				{
					zero[l % (blocksiz / 4)] = zero[l % (blocksiz / 4)] | j;
					group_desc.bg_free_blocks_count -= 1;
					fseek(fp, 0, SEEK_SET);
					fwrite(&group_desc, sizeof(ext2_group_desc), 1, fp);

					fseek(fp, 1 * blocksiz, SEEK_SET);
					fwrite(zero, blocksiz, 1, fp);
					last_allco_block = l % (blocksiz / 4);
					fclose(fp);
					return l % (blocksiz / 4) * 32 + i;
				}
				else
					j = j / 2;
			}


		}
	}
	fclose(fp);
	return -1;
}

//remove inode,set inode bitmap 0
void DelInode(int len)//len is the index of inode
{
	unsigned int zero[blocksiz / 4], i;
	int j;
	f = fopen(PATH, "r+");
	fseek(f, 2 * blocksiz, SEEK_SET);
	fread(zero, blocksiz, 1, f);
	i = 0x80000000;
	for (j = 0; j < len % 32; j++)
		i = i / 2;
	zero[len / 32] = zero[len / 32] ^ i;
	fseek(f, 2 * blocksiz, SEEK_SET);
	fwrite(zero, blocksiz, 1, f);
	fclose(f);
}

//remove data_lock,set block bitmap 0
void DelBlock(int len)//len is the index of block
{
	unsigned int zero[blocksiz / 4], i;
	int j;
	f = fopen(PATH, "r+");
	fseek(f, 1 * blocksiz, SEEK_SET);
	fread(zero, blocksiz, 1, f);
	i = 0x80000000;
	for (j = 0; j < len % 32; j++)
		i = i / 2;
	zero[len / 32] = zero[len / 32] ^ i;
	fseek(f, 1 * blocksiz, SEEK_SET);
	fwrite(zero, blocksiz, 1, f);
	fclose(f);
}


void add_block(ext2_inode*current, int i, int j)//有问题，二级索引里面，if（i%128==0）前面应增加else
{ // i is _block the pos of i_block, j is the index of data_block
	FILE*fp = NULL;
	while (fp == NULL)
		fp = fopen(PATH, "r+");
	if (i < 6)//direct index
	{
		current->i_block[i] = j;
	}
	else
	{
		i = i - 6;
		if (i == 0)
		{
			current->i_block[6] = FindBlock();
			fseek(fp, data_begin_block*blocksiz + current->i_block[6] * blocksiz, SEEK_SET);
			fwrite(&j, sizeof(int), 1, fp);
		}
		else
		{
			if (i < 128)// one index
			{
				fseek(fp, data_begin_block*blocksiz + current->i_block[6] * blocksiz + i * 4, SEEK_SET);
				fwrite(&j, sizeof(int), 1, fp);
			}
			else//two index
			{
				i = i - 128;
				if (i == 0)
				{
					current->i_block[7] = FindBlock();
					fseek(fp, data_begin_block*blocksiz + current->i_block[7] * blocksiz, SEEK_SET);
					i = FindBlock();
					fwrite(&i, sizeof(int), 1, fp);
					fseek(fp, data_begin_block*blocksiz + i*blocksiz, SEEK_SET);
					fwrite(&j, sizeof(int), 1, fp);
				}
				else
				{
					if (i % 128 == 0)
					{
						fseek(fp, data_begin_block*blocksiz + current->i_block[7] * blocksiz + i / 128 * 4, SEEK_SET);
						i = FindBlock();
						fwrite(&i, sizeof(int), 1, fp);
						fseek(fp, data_begin_block*blocksiz + i*blocksiz, SEEK_SET);
						fwrite(&j, sizeof(int), 1, fp);
					}
					else
					{
						fseek(fp, data_begin_block*blocksiz + current->i_block[7] * blocksiz + i / 128 * 4, SEEK_SET);
						fread(&i, sizeof(int), 1, fp);
						fseek(fp, data_begin_block*blocksiz + i * blocksiz + i % 128 * 4, SEEK_SET);
						fwrite(&j, sizeof(int), 1, fp);
					}
				}

			}
		}

	}
}

//find free entry
int FindEntry(ext2_inode*current)//modify some para
{
	FILE*fout = NULL;
	int location;
	int block_location;
	int temp;
	int remain_block;
	location = data_begin_block*blocksiz;
	temp = blocksiz / sizeof(int);
	fout = fopen(PATH, "r+");
	if (current->i_size%blocksiz == 0)
	{
		add_block(current, current->i_blocks, FindBlock());
		current->i_blocks++;
	}
	if (current->i_blocks <= 6)//direct index
	{
		location += current->i_block[current->i_blocks - 1] * blocksiz;
		location += current->i_size%blocksiz;
	}
	else
	{
		if (current->i_blocks <= temp + 6)//one index
		{
			block_location = current->i_block[6];
			fseek(fout, (data_begin_block + block_location)*blocksiz + (current->i_blocks - 6 - 1) * sizeof(int), SEEK_SET);
			fread(&block_location, sizeof(int), 1, fout);
			location += block_location*blocksiz;
			location += current->i_size%blocksiz;
		}
		else
		{
			block_location = current->i_block[7];
			remain_block = current->i_blocks - 6 - temp;
			fseek(fout, (data_begin_block + block_location)*blocksiz + (int)((remain_block - 1) / (temp) * sizeof(int)), SEEK_SET);
			fread(&block_location, sizeof(int), 1, fout);
			remain_block = (remain_block - 1) % temp;
			fseek(fout, (data_begin_block + block_location)*blocksiz + remain_block * sizeof(int), SEEK_SET);
			fread(&block_location, sizeof(int), 1, fout);
			location += block_location*blocksiz;
			location += current->i_size%blocksiz;
		}
	}
	current->i_size += dirsiz;
	fclose(fout);
	return location;
}

//type=1,create file,type=2,create dir
int Create(int type, ext2_inode*current, char *name)
{
	FILE* fout = NULL;
	int i;
	int block_location;
	int node_location;
	int dir_entry_location;
	time_t now;
	ext2_inode ainode;
	ext2_dir_entry aentry, bentry;
	time(&now);
	fout = fopen(PATH, "r+");
	node_location = FindInode();
	//判断该文件或目录是否已存在
	for (i = 0; i < current->i_size / dirsiz; i++)
	{
		fseek(fout, dir_entry_position(i * sizeof(ext2_dir_entry), current->i_block), SEEK_SET);
		fread(&aentry, sizeof(ext2_dir_entry), 1, fout);
		if (aentry.file_type == type&&strcmp(aentry.name, name) == 0)
			return 1;//current dir already exsit one
	}
	fseek(fout, (data_begin_block + current->i_block[0])*blocksiz, SEEK_SET);
	fread(&bentry, sizeof(ext2_dir_entry), 1, fout);//save current dir
													//创建该文件或目录的inode节点
	if (type == 1)//file
	{
		ainode.i_mode = 1;
		ainode.i_blocks = 0;
		ainode.i_size = 0;
		//ainode.i_atime = now;
		ainode.i_ctime = now;
		ainode.i_mtime = now;
		//ainode.i_dtime = 0;
		for (i = 0; i < 8; i++)
			ainode.i_block[i] = 0;
		for (i = 0; i < 24; i++)
			ainode.i_pad[i] = (char)(0xff);
	}
	else//dir
	{
		ainode.i_mode = 2;
		ainode.i_blocks = 1;
		ainode.i_size = 64;//. .. 32*2
						   //ainode.i_atime = now;
		ainode.i_ctime = now;
		ainode.i_mtime = now;
		//ainode.i_dtime = 0;
		block_location = FindBlock();
		//创建当前目录以及父目录
		ainode.i_block[0] = block_location;
		for (i = 1; i < 8; i++)
			ainode.i_block[i] = 0;
		for (i = 0; i < 24; i++)
			ainode.i_pad[i] = (char)(0xff);
		aentry.inode = node_location;
		aentry.rec_len = sizeof(ext2_dir_entry);
		aentry.name_len = 1;
		aentry.file_type = 2;
		strcpy(aentry.name, ".");
		aentry.dir_pad = 0;
		fseek(fout, (data_begin_block + block_location)*blocksiz, SEEK_SET);
		fwrite(&aentry, sizeof(ext2_dir_entry), 1, fout);
		aentry.inode = bentry.inode;
		aentry.rec_len = sizeof(ext2_dir_entry);
		aentry.name_len = 2;
		aentry.file_type = 2;
		strcpy(aentry.name, "..");
		aentry.dir_pad = 0;
		fwrite(&aentry, sizeof(ext2_dir_entry), 1, fout);
		aentry.inode = 0;
		aentry.rec_len = sizeof(ext2_dir_entry);
		aentry.name_len = 0;
		aentry.file_type = 0;
		aentry.name[EXT2_NAME_LEN] = 0;
		aentry.dir_pad = 0;
		fwrite(&aentry, sizeof(ext2_dir_entry), 14, fout);
	}

	fseek(fout, 3 * blocksiz + (node_location) * sizeof(ext2_inode), SEEK_SET);
	fwrite(&ainode, sizeof(ext2_inode), 1, fout);
	// 创建一个目录指向新建的inode节点
	aentry.inode = node_location;
	aentry.rec_len = dirsiz;
	aentry.name_len = strlen(name);
	if (type == 1)
		aentry.file_type = 1;
	else
		aentry.file_type = 2;
	strcpy(aentry.name, name);
	aentry.dir_pad = 0;
	dir_entry_location = FindEntry(current);
	fseek(fout, dir_entry_location, SEEK_SET);
	fwrite(&aentry, sizeof(ext2_dir_entry), 1, fout);
	for (i = 0; i < current->i_size / dirsiz; i++)
	{
		fseek(fout, dir_entry_position(i * sizeof(ext2_dir_entry), current->i_block), SEEK_SET);
		fread(&aentry, sizeof(ext2_dir_entry), 1, fout);
		//if (aentry.file_type == type&&strcmp(aentry.name, name) == 0)
		//	return 1;//current dir already exsit one
	}
	fseek(fout, 3 * blocksiz + bentry.inode * sizeof(ext2_inode), SEEK_SET);
	fwrite(current, sizeof(ext2_inode), 1, fout);
	for (i = 0; i < current->i_size / dirsiz; i++)
	{
		fseek(fout, dir_entry_position(i * sizeof(ext2_dir_entry), current->i_block), SEEK_SET);
		fread(&aentry, sizeof(ext2_dir_entry), 1, fout);
		//if (aentry.file_type == type&&strcmp(aentry.name, name) == 0)
		//	return 1;//current dir already exsit one
	}
	fclose(fout);
	return 0;
}

//write data to file name,if not exist,then create one
int Write(ext2_inode *current, char *name)
{
	FILE*fp = NULL;
	ext2_dir_entry dir;
	ext2_inode node;
	time_t now;
	//char str;
	int i;
	while (fp == NULL)
		fp = fopen(PATH, "r+");
	while (1)
	{
		for (i = 0; i < (current->i_size / 32); i++)
		{
			fseek(fp, dir_entry_position(i * 32, current->i_block), SEEK_SET);
			fread(&dir, sizeof(ext2_dir_entry), 1, fp);
			if (!strcmp(dir.name, name))
			{
				if (dir.file_type == 1)
				{
					fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
					fread(&node, sizeof(ext2_inode), 1, fp);
					break;
				}
			}
		}
		if (i < current->i_size / 32)
			break;
		//Create(1,current,name);
		printf("There isn't this file,please create it first\n");
		return 0;
	}
	string data;
	cin >> data;
	for (int i = 0; i<data.size(); i++)
	{
		if (!(node.i_size % 512))
		{
			add_block(&node, node.i_size / 512, FindBlock());
			node.i_blocks += 1;
		}
		fseek(fp, dir_entry_position(node.i_size, node.i_block), SEEK_SET);
		fwrite(&(data[i]), sizeof(char), 1, fp);
		node.i_size += sizeof(char);
	}
	/*scanf("%c", &str);
	while (str != 27)
	{
	if (!(node.i_size % 512))
	{
	add_block(&node, node.i_size / 512, FindBlock());
	node.i_blocks += 1;
	}
	fseek(fp, dir_entry_position(node.i_size, node.i_block), SEEK_SET);
	fwrite(&str, sizeof(char), 1, fp);
	node.i_size += sizeof(char);
	scanf("%c", &str);
	}*/
	time(&now);
	node.i_mtime = now;
	//node.i_atime = now;
	fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
	fwrite(&node, sizeof(ext2_inode), 1, fp);
	fclose(fp);
	printf("\n");
	return 0;
}

void Ls(ext2_inode *current)
{
	ext2_dir_entry dir;
	int i, j;
	char timestr[150];
	ext2_inode node;
	f = fopen(PATH, "r+");
	//printf("Type\tFileName\tCreateTime LastAccessTime ModifyTime\n");
	for (i = 0; i < current->i_size / 32; i++)
	{
		fseek(f, dir_entry_position(i * 32, current->i_block), SEEK_SET);
		fread(&dir, sizeof(ext2_dir_entry), 1, f);
		fseek(f, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
		fread(&node, sizeof(ext2_inode), 1, f);
		strcpy(timestr, "");
		strcat(timestr, asctime(localtime(&node.i_ctime)));
		//strcat(timestr, asctime(localtime(&node.i_atime)));
		strcat(timestr, asctime(localtime(&node.i_mtime)));
		for (j = 0; j < strlen(timestr) - 1; j++)
			if (timestr[j] == '\n')
				timestr[j] = ' ';
		if (dir.file_type == 1)
			printf("F %s %s", dir.name, timestr);
		else
			printf("D %s %s", dir.name, timestr);
	}
	fclose(f);
}

int initialize(ext2_inode *cu)
{
	f = fopen(PATH, "r+");
	fseek(f, 3 * blocksiz, 0);
	fread(cu, sizeof(ext2_inode), 1, f);
	fclose(f);
	return 0;
}

int Password()
{
	char psw[16], ch[10];
	printf("Pleease input the old password\n");
	scanf("%s", psw);
	if (strcmp(psw, group_desc.psw) != 0)
	{
		printf("Password error!\n");
		return 1;
	}
	while (1)
	{
		printf("Modify the password?[Y?N]");
		scanf("%s", ch);
		if (ch[0] == 'N' || ch[0] == 'n')
		{
			printf("You canceled the modify of your password\n");
			return 1;
		}
		else
		{
			if (ch[0] == 'Y' || ch[0] == 'y')
			{
				printf("input the new password\n");
				scanf("%s", psw);
				strcpy(group_desc.psw, psw);
				f = fopen(PATH, "r+");
				fseek(f, 0, SEEK_SET);
				fwrite(&group_desc, sizeof(ext2_group_desc), 1, f);
				fclose(f);
				return 0;
			}
			else
				printf("Meaningless command\n");
		}
	}

}

int login()
{
	char psw[16];
	printf("please input the password(init:000):");
	scanf("%s", psw);
	return strcmp(group_desc.psw, psw);
}

void exitdisplay()
{
	printf("Thank you for using the system!\n");
}

int initfs(ext2_inode *cu)
{
	f = fopen(PATH, "r+");
	if (f == NULL)
	{
		char ch;
		int i;
		printf("File system couldn't be found. Do you want to create one?\n[Y?N]");
		i = 1;
		while (i)
		{
			scanf("%c", &ch);
			switch (ch)
			{
			case 'Y':
			case 'y':
			{
				if (format() != 0)
					return 1;
				f = fopen(PATH, "r");
				i = 0;
				break;
			}
			case 'N':
			case 'n':
			{
				exitdisplay();
				return 1;
				break;
			}
			default:printf("Sorry,meaningless command\n");
				break;
			}
		}
	}
	//format();
	fseek(f, 0, SEEK_SET);
	fread(&group_desc, sizeof(ext2_group_desc), 1, f);
	fseek(f, 3 * blocksiz, SEEK_SET);
	fread(&inode, sizeof(ext2_inode), 1, f);
	fclose(f);
	initialize(cu);
	return 0;
}

void getstring(char *cs, ext2_inode node)//获取当前目录的目录名
{
	ext2_inode current = node;
	int i, j;
	ext2_dir_entry dir;
	f = fopen(PATH, "r+");
	//Open(&current, "..");
	for (i = 0; i < current.i_size / 32; i++)
	{
		fseek(f, dir_entry_position(i * 32, node.i_block), SEEK_SET);
		fread(&dir, sizeof(ext2_dir_entry), 1, f);
		if (!strcmp(dir.name, "."))
		{
			j = dir.inode;
			break;
		}
	}
	Open(&current, "..");
	for (i = 0; i < current.i_size / 32; i++)
	{
		fseek(f, dir_entry_position(i * 32, current.i_block), SEEK_SET);
		fread(&dir, sizeof(ext2_dir_entry), 1, f);
		if (dir.inode == j)
		{
			strcpy(cs, dir.name);
			break;
		}
	}
}

//在当前目录删除目录或文件

int Delet(int type, ext2_inode*current, char*name)
{
	FILE*fout = NULL;
	int i, j, t, k, flag;
	int Blocation2, Blocation3;
	int node_location, dir_entry_location, block_location;
	int block_location2, block_location3;
	int cur_dir_inode;
	ext2_inode cinode;
	ext2_dir_entry bentry, centry, dentry;
	dentry.inode = 0;
	dentry.rec_len = sizeof(ext2_dir_entry);
	dentry.name_len = 0;
	dentry.file_type = 0;
	strcpy(dentry.name, "");
	dentry.dir_pad = 0;
	fout = fopen(PATH, "r+");
	t = (int)(current->i_size / dirsiz);
	flag = 0;
	for (i = 0; i < t; i++)
	{
		dir_entry_location = dir_entry_position(i*dirsiz, current->i_block);
		fseek(fout, dir_entry_location, SEEK_SET);
		fread(&centry, sizeof(ext2_dir_entry), 1, fout);
		if (strcmp(centry.name, ".") == 0)
			cur_dir_inode = centry.inode;
		if ((strcmp(centry.name, name) == 0) && (centry.file_type == type))
		{
			flag = 1;
			j = i;
			break;
		}
	}
	if (flag)
	{
		node_location = centry.inode;
		fseek(fout, 3 * blocksiz + node_location * sizeof(ext2_inode), SEEK_SET);
		fread(&cinode, sizeof(ext2_inode), 1, fout);
		block_location = cinode.i_block[0];
		if (type == 2)//remove dir
		{
			if (cinode.i_size > 2 * dirsiz)
				printf("The folder is not empty!\n");
			else
			{
				DelBlock(block_location);
				DelInode(node_location);
				dir_entry_location = dir_entry_position(current->i_size - sizeof(ext2_dir_entry), current->i_block);
				fseek(fout, dir_entry_location, SEEK_SET);
				fread(&centry, dirsiz, 1, fout);//将最后一条条目存入centry
				fseek(fout, dir_entry_location, SEEK_SET);
				fwrite(&dentry, dirsiz, 1, fout);//清空最后一条条目
				dir_entry_location -= data_begin_block*blocksiz;
				//如果这个位置刚好是一个块的起始位置，则删掉这个块
				if (dir_entry_location%blocksiz == 0)
				{
					DelBlock((int)(dir_entry_location / blocksiz));
					current->i_blocks--;
					//如果剩下6块
					if (current->i_blocks == 6)
						DelBlock(current->i_block[6]);
					else
					{
						//如果刚好剩下6+128块
						if (current->i_blocks == (blocksiz / sizeof(int) + 6))
						{
							int a;
							fseek(fout, data_begin_block*blocksiz + current->i_block[7] * blocksiz, SEEK_SET);
							fread(&a, sizeof(int), 1, fout);
							DelBlock(a);
							DelBlock(current->i_block[7]);
						}
						else
						{
							if ((current->i_blocks - 6 - (blocksiz / sizeof(int))) % ((blocksiz) / sizeof(int)) == 0)
							{
								int a;//更改
								fseek(fout, data_begin_block*blocksiz + current->i_block[7] * blocksiz + ((current->i_blocks - 6 - block_blockaddr_num) / block_blockaddr_num) * sizeof(int), SEEK_SET);
								fread(&a, sizeof(int), 1, fout);
								DelBlock(a);
							}
						}

					}
				}
			}
			current->i_size -= dirsiz;
			if (j*dirsiz < current->i_size)
			{
				dir_entry_location = dir_entry_position(j*dirsiz, current->i_block);
				fseek(fout, dir_entry_location, SEEK_SET);
				fwrite(&centry, dirsiz, 1, fout);
			}
			printf("The %s is deleted!", name);
		}
		else
		{
			//remove direct index file
			for (i = 0; i < 6; i++)
			{
				if (cinode.i_blocks == 0)
					break;
				block_location = cinode.i_block[i];
				DelBlock(block_location);
				cinode.i_blocks--;
			}
			if (cinode.i_blocks > 0)//one index file
			{
				block_location = cinode.i_block[6];
				fseek(fout, (data_begin_block + block_location)*blocksiz, SEEK_SET);
				for (i = 0; i < blocksiz / sizeof(int); i++)
				{
					if (cinode.i_blocks == 0)
						break;
					fread(&Blocation2, sizeof(int), 1, fout);
					DelBlock(block_location2);
					cinode.i_blocks--;
				}
				DelBlock(block_location);
			}

			if (cinode.i_blocks > 0)//two index exist
			{
				block_location = cinode.i_block[7];
				for (i = 0; i < blocksiz / sizeof(int); i++)
				{
					fseek(fout, (data_begin_block + block_location)*blocksiz + i * sizeof(int), SEEK_SET);
					fread(&Blocation2, sizeof(int), 1, fout);
					fseek(fout, (data_begin_block + Blocation2)*blocksiz, SEEK_SET);
					for (k = 0; k < blocksiz / sizeof(int); k++)
					{
						if (cinode.i_blocks == 0)
							break;
						fread(&Blocation3, sizeof(int), 1, fout);
						DelBlock(Blocation3);
						cinode.i_blocks--;

					}
					DelBlock(Blocation2);
					if (cinode.i_blocks == 0)
						break;
				}
				DelBlock(block_location);
			}
			DelInode(node_location);//删除文件的inode
			dir_entry_location = dir_entry_position(current->i_size - sizeof(ext2_dir_entry), current->i_block);
			fseek(fout, dir_entry_location, SEEK_SET);
			fread(&centry, dirsiz, 1, fout);//将最后一条条目存入centry
			fseek(fout, dir_entry_location, SEEK_SET);
			fwrite(&dentry, dirsiz, 1, fout);//清空最后一条条目
			dir_entry_location -= data_begin_block*blocksiz;
			if (dir_entry_location%blocksiz == 0)
			{
				DelBlock((int)(dir_entry_location / blocksiz));
				current->i_blocks--;
				if (current->i_blocks == 6)
					DelBlock(current->i_block[6]);
				else
				{
					if (current->i_blocks == (blocksiz / sizeof(int) + 6))
					{
						int a;
						fseek(fout, data_begin_block*blocksiz + current->i_block[7] * blocksiz, SEEK_SET);
						fread(&a, sizeof(int), 1, fout);
						DelBlock(a);
						DelBlock(current->i_block[7]);
					}
					else
					{
						if (((current->i_blocks - 6 - blocksiz / sizeof(int)) % (blocksiz / sizeof(int))) == 0)
						{
							int a;
							fseek(fout, data_begin_block*blocksiz + current->i_block[7] * blocksiz + ((current->i_blocks - 6 - blocksiz / sizeof(int)) / (blocksiz / sizeof(int))) * sizeof(int), SEEK_SET);
							fread(&a, sizeof(int), 1, fout);
							DelBlock(a);
						}
					}
				}
			}
			current->i_size -= dirsiz;
			if (j*dirsiz < current->i_size)
			{
				dir_entry_location = dir_entry_position(j*dirsiz, current->i_block);
				fseek(fout, dir_entry_location, SEEK_SET);
				fwrite(&centry, dirsiz, 1, fout);
			}
		}
		//fseek(fout, (data_begin_block + current->i_block[0])*blocksiz, SEEK_SET);
		//fread(&bentry, sizeof(ext2_dir_entry), 1, fout);
		fseek(fout, 3 * blocksiz + (cur_dir_inode) * sizeof(ext2_inode), SEEK_SET);
		fwrite(current, sizeof(ext2_inode), 1, fout);
		fseek(fout, 3 * blocksiz + (cur_dir_inode) * sizeof(ext2_inode), SEEK_SET);
		fread(current, sizeof(ext2_inode), 1, fout);
	}
	else
	{
		fclose(fout);
		return 1;
	}
	fclose(fout);
	return 0;
}

void shellloop(ext2_inode currentdir)
{
	char command[10], var1[10], var2[128], path[10];
	ext2_inode temp;
	int i, j;
	char currentstring[20];
	bool running = true;
	char ctable[13][10] = { "create","delete","cd","close","read","write","password","format","exit","login","logout","ls","help" };
	while (running)
	{
		getstring(currentstring, currentdir);
		printf("\n%s=>#", currentstring);
		scanf("%s", command);
		for (i = 0; i < 13; i++)
			if (!strcmp(command, ctable[i]))
				break;
		switch (i)
		{
		case 0://create file or directory
		case 1: {
			scanf("%s", var1);
			scanf("%s", var2);
			if (var1[0] == 'f')
				j = 1;
			else
			{
				if (var1[0] == 'd')
					j = 2;
				else
				{
					printf("the first variant must be [f/d]\n");
					continue;
				}
			}
			if (i == 0)
			{
				if (Create(j, &currentdir, var2) == 1)
					printf("Failed!%s can't be created\n", var2);
				else
				{
					printf("Congratulations! %s is created\n", var2);
				}
			}
			else
			{
				if (Delet(j, &currentdir, var2) == 1)
					printf("Failed! %s can't be deleted!\n", var2);
				else
				{
					printf("Congratulations! %s is deleted\n", var2);
				}
			}
			break;
		}
		case 2: {
			scanf("%s", var2);
			i = 0; j = 0;
			temp = currentdir;
			while (1)
			{
				path[i] = var2[j];
				if (path[i] == '/')
				{
					if (j == 0)
						initialize(&currentdir);
					else
					{
						if (i == 0)
						{
							printf("path input error!\n");
							break;
						}
						else
						{
							path[i] = '\0';
							if (Open(&currentdir, path) == 1)
							{
								printf("path input error!\n");
								currentdir = temp;
							}
						}
						i = 0;
					}
				}
				else
				{
					if (path[i] == '\0')
					{
						if (i == 0)
							break;
						if (Open(&currentdir, path) == 1)
						{
							printf("path input error!\n");
							currentdir = temp;
						}
						break;

					}
					else i++;
					j++;
				}
			}
			break;
		}//cd
		case 3: {
			scanf("%d", &i);
			for (j = 0; j<i; j++)
				if (Close(&currentdir) == 1)
				{
					printf("Warining! the number %d is too large\n", i);
					break;
				}
			break;
		}
		case 4: {
			scanf("%s", var2);
			if (Read(&currentdir, var2) == 1)
				printf("Failed! The file can't be read\n");
			break;
		}
		case 5: {
			scanf("%s", var2);
			if (Write(&currentdir, var2) == 1)
				printf("Failed! The file can't be written\n");
			break;
		}
		case 6: {
			Password();
			break;
		}
		case 7: {
			format();
			break;
		}
		case 8:running = false; break;
		case 9: {if (login() == 0)
			break;
				else
				{
					printf("wrong password\n");
					//exitdisplay();
					running = false;
					break;
				}
		}
		case 10: {initialize(&currentdir); break; }
		case 11:Ls(&currentdir); break;
		case 12: {
			int i;
			printf("commands:");
			for (i = 0; i < 13; i++)
			{
				printf("%s ", ctable[i]);
			}
			break;
		}
		default:printf("Failed!Command not available\n");
		}
	}
}

int main()
{
	ext2_inode cu;
	printf("Welcome to Ext2_like file system!\n");
	if (initfs(&cu) == 1)
		return 0;
	if (login() != 0)
	{
		printf("Wrong password!\n");
		exitdisplay();
		return 0;
	}
	shellloop(cu);
	exitdisplay();
	return 0;
}