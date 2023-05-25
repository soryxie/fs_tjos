#include "../include/fs.h"
#include <cstring>
#include <iostream>
using namespace std;

extern FileSystem fs; 

/*
* 针对内部的块号 inner_id 
* 定位到对应的物理块号
*/
const int DIRECT_COUNT       = 6;
const int FIRST_LEVEL_SIZE   = 128;
const int FIRST_LEVEL_COUNT  = DIRECT_COUNT + FIRST_LEVEL_SIZE*2;
const int SECOND_LEVEL_SIZE  = 128 * 128;
const int SECOND_LEVEL_COUNT = FIRST_LEVEL_COUNT + SECOND_LEVEL_SIZE*2;

int Inode::get_block_id(int inner_id) {
    // 如果 int 大于文件大小，返回 -1
    if(inner_id > d_size/BLOCK_SIZE) {
        return FAIL;
    }

    /* 直接索引 */
    if (inner_id < DIRECT_COUNT) {
        return d_addr[inner_id];
    }
    /* 一级索引 */
    else if(inner_id < FIRST_LEVEL_COUNT) {
        int first_level_id = inner_id - DIRECT_COUNT;
        int first_level_block = d_addr[DIRECT_COUNT + first_level_id / FIRST_LEVEL_SIZE];
        
        if(first_level_block == 0) return FAIL;
        int *first_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .read_only_cache(first_level_block)
                                                        .data());
        return first_table[first_level_id % FIRST_LEVEL_SIZE];
    }
    /* 二级索引 */
    else if(inner_id < SECOND_LEVEL_COUNT) {
        int second_level_id = inner_id - FIRST_LEVEL_COUNT;
        int second_level_block = d_addr[DIRECT_COUNT + FIRST_LEVEL_SIZE + second_level_id / SECOND_LEVEL_SIZE];
        
        if(second_level_block == 0) return FAIL;
        int *second_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .read_only_cache(second_level_block)
                                                        .data());
        int first_level_id = second_level_id % SECOND_LEVEL_SIZE;
        int first_level_block = second_table[first_level_id / FIRST_LEVEL_SIZE];
        
        if(first_level_block == 0) return FAIL;
        int *first_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                  .read_only_cache(first_level_block)
                                  .data());
        return first_table[first_level_id % FIRST_LEVEL_SIZE];
    }
    return FAIL;
}

int Inode::read_at(int offset, char* buf, int size) {
    if (offset >= d_size) {
        return 0;
    }

    if (offset + size > d_size) {
        size = d_size - offset;
    }

    int read_size = 0;

    for (int pos = offset; pos < offset + size;) {
        int no = pos / BLOCK_SIZE;             //计算inode中的块编号
        int block_offset = pos % BLOCK_SIZE;   //块内偏移
        int blkno = get_block_id(no);

        if (blkno < 0)
            break;

        /* 读块 */
        char *inner_buf = fs.block_cache_mgr_
                      .read_only_cache(blkno)
                      .data();

        /* 读取可能的最大部分 */
        int block_read_size = std::min<int>(BLOCK_SIZE - block_offset, size - read_size);
        memcpy(buf + read_size, inner_buf + block_offset, block_read_size);
        read_size += block_read_size;
        pos += block_read_size;
    }

    return read_size;
}

int Inode::resize(int size) {
    // 计算总共变动的的块数
    d_size = (d_size + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
    int block_num_delta = (size + BLOCK_SIZE - 1) / BLOCK_SIZE - d_size / BLOCK_SIZE;

    if(block_num_delta == 0) return 0;
    else if(block_num_delta > 0) {
        // 逐个push_back
        while(block_num_delta--) {
            if (push_back_block() == FAIL)
                return FAIL;
            d_size += BLOCK_SIZE;
        }
    }
    else{
        // 逐个pop_back
        while(block_num_delta++) {
            if (pop_back_block() == FAIL)
                return FAIL;
            d_size -= BLOCK_SIZE;
        }
    }

    // 重置为规定大小
    d_size = size;
    return 0;
}

int Inode::write_at(int offset, const char* buf, int size) {
    if (offset + size > d_size) {
        resize(offset + size);
    }

    int written_size = 0;

    for (int pos = offset; pos < offset + size;) {
        int no = pos / BLOCK_SIZE;             //计算inode中的块编号
        int block_offset = pos % BLOCK_SIZE;   //块内偏移
        int blkno = get_block_id(no);

        if (blkno < 0) {
            // 添加新块失败，返回已经写入的字节数
            return written_size;
        }

        /* 写入可能的最大部分 */
        int block_write_size = std::min<int>(BLOCK_SIZE - block_offset, size - written_size);

        char *inner_buf = fs.block_cache_mgr_
                .writable_cache(blkno)
                .data();

        memcpy(inner_buf + block_offset, buf + written_size, block_write_size);
        written_size += block_write_size;
        pos += block_write_size;
    }

    // 更新inode的文件大小和最后修改时间
    if (offset + written_size > d_size) {
        d_size = offset + written_size;
    }
    //d_mtime = get_cur_time();

    return written_size;
}

int Inode::push_back_block() {
    int blkno = fs.alloc_block();
    if(blkno == 0) return FAIL;

    int blk_id = d_size / BLOCK_SIZE;
    if(blk_id < DIRECT_COUNT) {
        d_addr[blk_id] = blkno;
    }
    else if(blk_id < FIRST_LEVEL_COUNT) {
        int first_level_id = blk_id - DIRECT_COUNT;
        int first_level_block = d_addr[DIRECT_COUNT + first_level_id / FIRST_LEVEL_SIZE];
        
        if(first_level_block == 0) {
            first_level_block = fs.alloc_block();
            if(first_level_block == 0) return FAIL;
            d_addr[DIRECT_COUNT + first_level_id / FIRST_LEVEL_SIZE] = first_level_block;
        }
        int *first_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .writable_cache(first_level_block)
                                                        .data());
        first_table[first_level_id % FIRST_LEVEL_SIZE] = blkno;
    }
    else if(blk_id < SECOND_LEVEL_COUNT) {
        int second_level_id = blk_id - FIRST_LEVEL_COUNT;
        int second_level_block = d_addr[DIRECT_COUNT + FIRST_LEVEL_COUNT + second_level_id / SECOND_LEVEL_SIZE];
        
        if(second_level_block == 0) {
            second_level_block = fs.alloc_block();
            if(second_level_block == 0) return FAIL;
            d_addr[DIRECT_COUNT + FIRST_LEVEL_COUNT + second_level_id / SECOND_LEVEL_SIZE] = second_level_block;
        }
        int *second_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .writable_cache(second_level_block)
                                                        .data());
        int first_level_id = second_level_id % SECOND_LEVEL_SIZE;
        int first_level_block = second_table[first_level_id / FIRST_LEVEL_SIZE];
        
        if(first_level_block == 0) {
            first_level_block = fs.alloc_block();
            if(first_level_block == 0) return FAIL;
            second_table[first_level_id / FIRST_LEVEL_SIZE] = first_level_block;
        }
        int *first_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .writable_cache(first_level_block)
                                                        .data());
        first_table[first_level_id % FIRST_LEVEL_SIZE] = blkno;
    }
    else {
        cerr << "push_back_block: too large file." << endl;
        return FAIL;
    }
    return blkno;
}

int Inode::pop_back_block() {
    int blk_id = d_size / BLOCK_SIZE;
    if(blk_id < DIRECT_COUNT) {
        fs.dealloc_block(d_addr[blk_id]);
        d_addr[blk_id] = 0;
    }
    else if(blk_id < FIRST_LEVEL_COUNT) {
        int first_level_id = blk_id - DIRECT_COUNT;
        int first_level_block = d_addr[DIRECT_COUNT + first_level_id / FIRST_LEVEL_SIZE];
        
        int *first_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .writable_cache(first_level_block)
                                                        .data());
        first_table[first_level_id % FIRST_LEVEL_SIZE] = 0;
        fs.dealloc_block(d_addr[blk_id]);
        d_addr[blk_id] = 0;
    }
    else if(blk_id < SECOND_LEVEL_COUNT) {
        int second_level_id = blk_id - FIRST_LEVEL_COUNT;
        int second_level_block = d_addr[DIRECT_COUNT + FIRST_LEVEL_COUNT + second_level_id / SECOND_LEVEL_SIZE];
        
        int *second_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .writable_cache(second_level_block)
                                                        .data());
        int first_level_id = second_level_id % SECOND_LEVEL_SIZE;
        int first_level_block = second_table[first_level_id / FIRST_LEVEL_SIZE];
        
        int *first_table = reinterpret_cast<int *>(fs.block_cache_mgr_
                                                        .writable_cache(first_level_block)
                                                        .data());
        first_table[first_level_id % FIRST_LEVEL_SIZE] = 0;
        fs.dealloc_block(d_addr[blk_id]);
        d_addr[blk_id] = 0;
    }
    else {
        cerr << "pop_back_block: too large file." << endl;
        return FAIL;
    }
    return 0;
}

vector<DirectoryEntry> Inode::get_entry() {
    vector<DirectoryEntry> entrys;
    entrys.resize(d_size / ENTRY_SIZE);

    if(entrys.size() == 0) return entrys;
    if(!read_at(0, (char *)entrys.data(), d_size)) {
        cerr << "getEntry: read directory entries failed." << endl;
        return entrys;
    }
    return entrys;
}

int Inode::set_entry(vector<DirectoryEntry>& entrys) {
    if(!write_at(0, (char *)entrys.data(), d_size)) {
        cerr << "setEntry: write directory entries failed." << endl;
        return FAIL;
    }
    return 0;
}

int Inode::init_as_dir(int ino, int fa_ino) {
    d_mode |= FileType::Directory;

    int sub_dir_blk = push_back_block();
    if (sub_dir_blk == 0) {
        std::cerr << "createFile: No free block" << std::endl;
        return -1;
    }
    // 初始化目录文件
    auto sub_entrys = (DirectoryEntry *)fs.block_cache_mgr_
                                          .writable_cache(sub_dir_blk)
                                          .data();
    
    DirectoryEntry dot_entry(ino, ".");
    DirectoryEntry dotdot_entry(fa_ino, "..");
    sub_entrys[0] = dot_entry;
    sub_entrys[1] = dotdot_entry;
    d_size += ENTRY_SIZE*2;
    return 0;
}

int Inode::create_file(const string& filename, bool is_dir) {
    auto entrys = get_entry();
    int entrynum = entrys.size();
    for (auto& entry : entrys) {
        if (entry.m_ino && strcmp(entry.m_name, filename.c_str()) == 0) {
            std::cerr << "createFile: File already exists." << std::endl;
            return -1;
        }
    }
 
    if (entrynum % ENTRYS_PER_BLOCK == 0) { // 需要分配新block
        push_back_block();
    }

    // 找到空闲的inode和数据块
    int ino = fs.alloc_inode();
    if (ino == 0) {
        std::cerr << "createFile: No free inode" << std::endl;
        return -1;
    }

    // 更改文件类型
    if (is_dir) 
        fs.inodes[ino].init_as_dir(ino, i_ino);
    else 
        fs.inodes[ino].d_mode |= FileType::RegularFile;
    
    // 更新目录文件内容
    int blknum = entrynum / ENTRYS_PER_BLOCK;
    auto entry_block = (DirectoryEntry *)fs.block_cache_mgr_
                                           .writable_cache(get_block_id(blknum))
                                           .data();

    // 创建新目录项
    DirectoryEntry new_entry(ino, filename.c_str());
    entry_block[entrynum % ENTRYS_PER_BLOCK] = new_entry;

    // 更新目录文件inode
    d_size += ENTRY_SIZE;
    
    return ino;
}

int Inode::delete_file_entry(const string& filename) {
    auto entrys = get_entry();
    int ino;

    bool found = false;
    for (int i=0; i<entrys.size(); i++) {
        if (found) {                            // 删除节点之后，后续的目录项前移
            entrys[i-1] = entrys[i];
        }
        if (entrys[i].m_ino && strcmp(entrys[i].m_name, filename.c_str()) == 0) {
            ino = entrys[i].m_ino;
            found = true;
        }
    }
    if(!found) {
        std::cerr << "deleteFile: File not found." << std::endl;
        return FAIL;
    }

    // 更新目录文件内容
    if(set_entry(entrys) == FAIL)
        return FAIL;

    d_size -= ENTRY_SIZE;                       // 目录文件收缩

    if (d_size % BLOCK_SIZE == 0) {             // 需要弹出最后一块物理块
        push_back_block();
    }

    return ino;
}

int Inode::find_file(const string &name) {
    auto entrys = get_entry();
    int entrynum = entrys.size();
    for (auto& entry : entrys) {
        if (entry.m_ino && strcmp(entry.m_name, name.c_str()) == 0) {
            return entry.m_ino;
        }
    }
    return FAIL;
}

int Inode::clear() {
    resize(0);
    fs.dealloc_inode(i_ino);
    return 0;
}

int Inode::copy_from(Inode &src) {
    /* 复制物理块内容（逐块复制） */
    int blknum = (src.d_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for(int i=0, d_size=0; i<blknum; i++, d_size+=BLOCK_SIZE) {
        int srcno = src.get_block_id(i);
        int new_blkno = push_back_block();
        if(new_blkno == 0) {
            cerr << "copyFrom: No free block" << endl;
            return FAIL;
        }

        char *src_buf = fs.block_cache_mgr_
                          .read_only_cache(srcno)
                          .data();
        char *new_buf = fs.block_cache_mgr_
                          .writable_cache(new_blkno)
                          .data();
        memcpy(new_buf, src_buf, BLOCK_SIZE);
    }

    d_mode = src.d_mode;
    d_size = src.d_size;
    /* TODO change time
    d_mtime = src.d_mtime;
    d_atime = src.d_atime;
    d_ctime = src.d_ctime;
    */
    return 0;
}