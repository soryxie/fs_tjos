#include "../include/fs.h"
#include <cstring>
#include <iostream>
using namespace std;

extern FileSystem fs; 

/*
* 针对内部的块号 inner_id 
* 定位到对应的物理块号
* 如果 create 为 true，那么如果对应的物理块不存在，就创建一个
*/
int DiskInode::get_block_id(int inner_id, bool create) {
    // 如果 int 大于文件大小或者超出文件系统范围，返回 -1
    if (create == false && (inner_id > d_size/BLOCK_SIZE || inner_id >= (6 + 2*128 + 2*128*128))) {
        return FAIL;
    }

    /* 直接索引 */
    if (inner_id < 6) {
        if(create){  // 此处需要新增
            if(d_addr[inner_id]) {  // 已有块，失败
                cout << "block in " << inner_id << "already exist! is:" << d_addr[inner_id] << endl;
                //return FAIL;
            }
            else {                         // 分配
                int blkno = fs.alloc_block();
                if(blkno == FAIL) return FAIL;
                d_addr[inner_id] = blkno;
                //cout << "file:" << (&inode - inodes)/sizeof(DiskInode) << " alloc " << blkno << endl;
            }
        }
        return d_addr[inner_id];
    }

    /* 一级索引 */
    inner_id -= 6;
    if (inner_id < 128 * 2) {
        // 是否需要先分配一级索引表的物理块
        if (d_addr[6 + (inner_id / 128)] == 0) {
            if(create) {
                int blkno = fs.alloc_block();
                if(blkno == FAIL) return FAIL;
                d_addr[6 + (inner_id / 128)] = blkno;
            }
            else
                return 0; // 试图索引不存在的块，失败
        }

        // 访问一级索引
        buffer buf_1[BLOCK_SIZE] = "";
        memset(buf_1, 1, BLOCK_SIZE);
        int *first_level_table = reinterpret_cast<int *>(buf_1);
        fs.read_block(d_addr[6 + (inner_id / 128)], buf_1);

        int idx_in_ftable = inner_id % 128;
        if( create ) {
            if(first_level_table[idx_in_ftable] == 0) {
                int blkno = fs.alloc_block();
                if(blkno == FAIL) return FAIL;
                first_level_table[idx_in_ftable] = blkno;
                fs.write_block(d_addr[6 + (inner_id / 128)], buf_1);
            }
            else {
                cout << "block in " << inner_id << "already exist! is:" << first_level_table[idx_in_ftable] << endl;
            }
        }
        return first_level_table[idx_in_ftable];
    }

    /* 二级索引 */
    inner_id -= 128 * 2;
    if (inner_id < 128 * 128 * 2) {
        // 是否需要先分配一级索引表的物理块
        if (d_addr[8 + (inner_id / (128 * 128))] == 0) {
            if(create) {
                int blkno = fs.alloc_block();
                if(blkno == FAIL) return FAIL;
                d_addr[8 + (inner_id / (128 * 128))] = blkno;
            }
            else
                return 0; // 试图索引不存在的块，失败
        }

        // 访问一级索引表
        int first_level_no = (inner_id % (128*128)) / 128;

        buffer buf_1[BLOCK_SIZE] = "";
        int *first_level_table = reinterpret_cast<int *>(buf_1);
        fs.read_block(d_addr[8 + (inner_id / (128 * 128))], buf_1);


        int second_block = first_level_table[first_level_no];
        // 一级索引表中的对应项为空
        if (second_block == 0) {
            if( create ) {
                // 分配一级索引表中指向二级索引表的物理块
                int blkno = fs.alloc_block();
                if(blkno == FAIL) return FAIL;
                first_level_table[first_level_no] = blkno;
                fs.write_block(d_addr[8 + (inner_id / (128 * 128))], buf_1);
                second_block = blkno;
            }
            else 
                return 0;
        }

        // 访问二级索引表
        
        buffer buf_2[BLOCK_SIZE] = "";
        int *second_level_table = reinterpret_cast<int *>(buf_2);
        fs.read_block(second_block, buf_2);

        int idx_in_stable = inner_id % 128;
        if( create ) {
            if(second_level_table[idx_in_stable] == 0) {
                int blkno = fs.alloc_block();
                if( blkno == FAIL) return FAIL;
                second_level_table[idx_in_stable] = blkno;
                fs.write_block( second_block, (buffer *)second_level_table);
            }
            else {
                cout << "block in " << inner_id << "already exist! is:" << second_level_table[idx_in_stable] << endl;
                //return FAIL;
            }
        }
        return second_level_table[idx_in_stable];
    }

    // 超出范围，返回FAIL
    return FAIL;
}