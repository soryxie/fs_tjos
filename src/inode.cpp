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
const int DIRECT_COUNT=6;
const int FIRST_LEVEL_COUNT=128;
const int SECOND_LEVEL_COUNT=128*128;

int Inode::get_block_id(int inner_id) {
    // 如果 int 大于文件大小，返回 -1
    if(inner_id > d_size/BLOCK_SIZE) {
        return FAIL;
    }

    /* 直接索引 */
    if (inner_id < DIRECT_COUNT) {
        return d_addr[inner_id];
    }
    else if(inner_id < FIRST_LEVEL_COUNT) {
        cout<<"Unimplement Error!"<<endl;
        exit(-1);
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
                      .get_block_cache(blkno)
                      ->data();
        //fs.read_block(blkno, inner_buf);
        /* 读取可能的最大部分 */
        int block_read_size = std::min<int>(BLOCK_SIZE - block_offset, size - read_size);
        memcpy(buf + read_size, inner_buf + block_offset, block_read_size);
        read_size += block_read_size;
        pos += block_read_size;
    }

    return read_size;
}

int Inode::resize(int size) {
    if (size <= d_size) { //TODO 收缩
        return 0;
    }

    int append_block_num = (size + BLOCK_SIZE - 1) / BLOCK_SIZE - (d_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    while(append_block_num--) {
        if (push_back_block() == FAIL) {
            return FAIL;
        }
    }

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
        
        /* 是否读原本的内容(有cache不考虑了，都需要进缓存才行) */
        //if(block_offset == 0 && block_write_size < BLOCK_SIZE) // 要用到原本的内容
            //fs.read_block(blkno, inner_buf);

        char *inner_buf = fs.block_cache_mgr_
                .get_block_cache(blkno)
                ->data();
        fs.block_cache_mgr_.get_block_cache(blkno)->modified_ = true;

        memcpy(inner_buf + block_offset, buf + written_size, block_write_size);
        //fs.write_block(blkno, inner_buf);
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
    if(blkno == FAIL) return FAIL;
    d_addr[d_size / BLOCK_SIZE] = blkno;
    return blkno;

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

int Inode::init_as_dir(int ino, int fa_ino) {
    int sub_dir_blk = push_back_block();
    if (sub_dir_blk == 0) {
        std::cerr << "createFile: No free block" << std::endl;
        return -1;
    }
    // 初始化目录文件
    auto cache_blk = fs.block_cache_mgr_.get_block_cache(sub_dir_blk);
    auto sub_entrys = (DirectoryEntry *)cache_blk->data();
    cache_blk->modified_ = true;
    //auto sub_entrys = (DirectoryEntry *)inner_buf;
    //fs.read_block(sub_dir_blk, inner_buf);
    //memset(sub_entrys, 0, BLOCK_SIZE);
    DirectoryEntry dot_entry(ino, 
                        ".", 
                        DirectoryEntry::FileType::Directory);
    DirectoryEntry dotdot_entry(fa_ino, 
                                "..", 
                                DirectoryEntry::FileType::Directory);
    sub_entrys[0] = dot_entry;
    sub_entrys[1] = dotdot_entry;
    //fs.write_block(sub_dir_blk, inner_buf);
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
    if (is_dir) {
        fs.inodes[ino].init_as_dir(ino, i_ino); //TODO 换成INode类
    }
    
    int blknum = entrynum / ENTRYS_PER_BLOCK;
    //DirectoryEntry entry_block[ENTRYS_PER_BLOCK];


    auto cache_blk = fs.block_cache_mgr_.get_block_cache(get_block_id(blknum));
    auto entry_block = (DirectoryEntry *)cache_blk->data();
    cache_blk->modified_ = true;



    //fs.read_block(get_block_id(blknum), (buffer *)entry_block); // 读取目录文件内容
    DirectoryEntry new_entry(ino, 
                            filename.c_str(), 
                            is_dir? DirectoryEntry::FileType::Directory : DirectoryEntry::FileType::RegularFile);
    entry_block[entrynum % ENTRYS_PER_BLOCK] = new_entry;
    //fs.write_block(get_block_id(blknum), (buffer *)entry_block); // 写回目录文件内容

    // 更新目录文件inode
    d_size += ENTRY_SIZE;
    //inodes[dir].d_mtime = get_cur_time();
    
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
