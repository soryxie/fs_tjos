#include "../include/fs.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <cstring>
#include <vector>
#include <sstream>
#include <iterator>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>

buffer inner_buf[BLOCK_SIZE];

using namespace std;

time_t get_cur_time() {
    return chrono::system_clockto_time_t(chrono::system_clock::now());
}

FileSystem::FileSystem(const std::string& diskfile) {
    // r+w 打开磁盘文件
    std::fstream disk(diskfile, std::ios::in | std::ios::out | std::ios::binary);

    if (!disk) {
        std::cerr << "Failed to open disk file " << diskfile << std::endl;
        exit(EXIT_FAILURE);
    }

    // 读取superBlock
    disk.seekg(0, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // 读取Inode 
    disk.seekg(OFFSET_INODE, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&inodes[0]), sizeof(DiskInode)*INODE_NUM);

    // 保存
    diskfile_ = diskfile;
    disk_ = std::move(disk);
}

FileSystem::~FileSystem() {
    // 保存数据
    disk_.seekp(0, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    disk_.seekp(OFFSET_INODE, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&inodes[0]), sizeof(DiskInode)*INODE_NUM);

    // 关闭文件
    disk_.close();
}

node_num FileSystem::alloc_inode() {
    if(sb.s_ninode <= 1)
        return 0;
    int ino = sb.s_inode[--sb.s_ninode];
    inodes[ino].d_uid = user_->uid;
    inodes[ino].d_gid = user_->group;
    inodes[ino].d_nlink = 1;

    // 获取当前时间的Unix时间戳
    inodes[ino].d_atime = inodes[ino].d_mtime = get_cur_time();
    
    
    // 将Int型的时间变为可读
    char* str_time = ctime((const time_t *)&inodes[ino].d_atime);
    cout << "创建新的INode "<< ino <<", uid="<< inodes[ino].d_uid <<", gid="<< inodes[ino].d_gid <<", 创建时间"<< str_time << endl;

    return ino;
}

block_num FileSystem::alloc_block() {
    int blkno;
    if(sb.s_nfree <= 1) // free list 不足
    {
        // 换一张新表
        buffer buf = read_block(sb.s_free[0]);
        int *table = reinterpret_cast<int *>(buf);
        sb.s_nfree = table[0];
        for(int i=0;i<sb.s_nfree;i++)
            sb.s_free[i] = table[i+1];
    }
    // 取一个块
    blkno = sb.s_free[--sb.s_nfree];
    if (blkno == 0) {
        cout << "error : super_block empty" << endl;
        return -1;
    }
    return blkno;
}

buffer* FileSystem::read_block(block_num blkno) {
    // 暂未实现缓存
    disk_.seekg(blkno*BLOCK_SIZE, std::ios::beg);
    disk_.read(inner_buf, sizeof(BLOCK_SIZE));
    return inner_buf;
}

bool FileSystem::write_block(block_num blkno, buffer* buf) {
    disk_.seekg(blkno*BLOCK_SIZE, std::ios::beg);
    disk_.write(buf, sizeof(BLOCK_SIZE));
}

block_num FileSystem::translate_block(const DiskInode& inode, uint no) {
    // 如果 block_num 大于文件大小或者超出文件系统范围，返回 -1
    if (no >= inode.d_size/BLOCK_SIZE || no >= (6 + 2*128 + 2*128*128)) {
        return -1;
    }

    /* 直接索引 */
    if (no < 6) {           //直接返回对应位置的blkno
        return inode.d_addr[no];
    }

    /* 一级索引 */
    no -= 6;        
    if (no < 128 * 2) {     // d_add[6 or 7], 512/sizeof(uint)=128
        return ((block_num *)read_block(inode.d_addr[6 + (no/128)]))[no%128];
    }

    /* 二级索引 */
    no -= 128 * 2;
    block_num first_level_no = (no % (128*128)) / 128;
    block_num second_block_no = ((block_num *)read_block(inode.d_addr[8 + (no/128*128)]))[first_level_no];
    block_num second_level_no = no % 128;
    return ((block_num *)read_block(second_block_no))[second_level_no];
}

block_num FileSystem::file_add_block(DiskInode& inode) {
    int block_idx = inode.d_size / BLOCK_SIZE; // 下一个物理块应该在哪个位置
    int second_blocks = 128 * 128 * 2;

    /* 直接索引 */
    if (block_idx < 6) {
        block_num blkno = alloc_block();
        inode.d_addr[block_idx] = blkno;
        return blkno;
    }

    /* 一级索引 */
    block_idx -= 6;
    if (block_idx < 128 * 2) {
        // 是否需要先分配一级索引表的物理块
        if (block_idx % 128 == 0) {
            block_num blkno = alloc_block();
            inode.d_addr[6 + (block_idx / 128)] = blkno;
        }

        // 更新一级索引表
        block_num *first_level_table = (block_num *)read_block(inode.d_addr[6 + (block_idx / 128)]);
        int offset = block_idx % 128;
        block_num blkno = alloc_block();
        first_level_table[offset] = blkno;
        write_block(inode.d_addr[6 + (block_idx / 128)], (buffer *)first_level_table);
        return blkno;
    }

    /* 二级索引 */
    block_idx -= 128 * 2;
    if (block_idx < 128 * 128 * 2) {
        // 分配二级索引表的物理块
        if (block_idx % (128 * 128) == 0) {
            block_num blkno = alloc_block();
            inode.d_addr[8 + (block_idx / (128 * 128))] = blkno;
        }

        // 更新二级索引表
        block_num first_level_no = (block_idx % (128*128)) / 128;
        block_num *first_level_table = (block_num *)read_block(inode.d_addr[8 + (block_idx / (128 * 128))]);
        block_num second_block = first_level_table[first_level_no];
        if (second_block == 0) {
            // 分配一级索引表中指向二级索引表的物理块
            block_num blkno = alloc_block();
            first_level_table[first_level_no] = blkno;
            write_block(inode.d_addr[8 + (block_idx / (128 * 128))], first_level_table );
            second_block = blkno;
        }
        block_num *second_level_table = (block_num *)read_block(second_block);
        int offset = block_idx % 128;
        block_num blkno = alloc_block();
        second_level_table[offset] = blkno;
        write_block( second_block, (buffer *)second_level_table);
        return blkno;
    }

    // 超出范围，返回-1
    return -1;
}

uint FileSystem::read(const DiskInode& inode, buffer* buf, uint size, uint offset) {
    if (offset >= inode.d_size) {
        return 0;
    }

    if (offset + size > inode.d_size) {
        size = inode.d_size - offset;
    }

    uint read_size = 0;

    for (uint pos = offset; pos < offset + size;) {
        uint no = pos / BLOCK_SIZE;             //计算inode中的块编号
        uint block_offset = pos % BLOCK_SIZE;   //块内偏移
        block_num blkno = translate_block(inode, no);

        if (blkno < 0)
            break;

        /* 读块 */
        buffer *block = read_block(blkno);
        /* 读取可能的最大部分 */
        uint block_read_size = std::min<uint>(BLOCK_SIZE - block_offset, size - read_size);
        memcpy(buf + read_size, inner_buf + block_offset, block_read_size);
        read_size += block_read_size;
        pos += block_read_size;
    }

    return read_size;
}

uint FileSystem::write(const DiskInode& inode, const buffer* buf, uint size, uint offset) {
    if (offset + size > MAX_FILE_SIZE) {
        return 0;
    }

    uint written_size = 0;

    for (uint pos = offset; pos < offset + size;) {
        uint no = pos / BLOCK_SIZE;             //计算inode中的块编号
        uint block_offset = pos % BLOCK_SIZE;   //块内偏移
        block_num blkno = translate_block(inode, no);

        if (blkno < 0) {
            // 如果块不存在，需要为文件添加一个新块
            blkno = file_add_block(inode);
            if (blkno < 0) {
                // 添加新块失败，返回已经写入的字节数
                return written_size;
            }
        }

        /* 写块 */
        buffer *block = read_block(blkno);
        /* 写入可能的最大部分 */
        uint block_write_size = std::min<uint>(BLOCK_SIZE - block_offset, size - written_size);
        memcpy(inner_buf + block_offset, buf + written_size, block_write_size);
        write_block(blkno, block);
        written_size += block_write_size;
        pos += block_write_size;
    }

    // 更新inode的文件大小和最后修改时间
    if (offset + written_size > inode.d_size) {
        inode.d_size = offset + written_size;
    }
    inode.d_mtime = time(NULL);

    return written_size;
}

node_num FileSystem::createFile(const node_num dir, const std::string& filename, DirectoryEntry::FileType type=DirectoryEntry::FileType::RegularFile) {
    // 开辟空间，并读取所有目录项，这里预读了最后一格物理块的所有空间
    uint blocks = (inodes[dir].d_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    // 转化成一个完整的vector
    DirectoryEntry *entry_table = (DirectoryEntry *)malloc(blocks * BLOCK_SIZE);

    if(entry_table == nullptr || read(inodes[dir], (buffer *)entry_table, inodes[dir].d_size, 0) == false) {
        std::cerr << "createFile: read dirextory entries failed." << std::endl;
        free(entry_table);
        return -1;
    }

    // 遍历所有目录项
    int entry_no, entry_num_max = blocks * ENTRYS_PER_BLOCK;
    for (entry_no = 0; entry_no < entry_num_max; entry_no++) {
        // 查同名
        if (entry_table[entry_no].m_ino && strcmp(entry_table[entry_no].m_name, filename.c_str()) == 0) {
            std::cerr << "createFile: File already exists." << std::endl;
            free(entry_table);
            return -1;
        }

        // 如果到结尾，跳出循环
        if (entry_table[entry_no].m_ino == 0) 
            break;
    }

    // 目录文件已满，需要分配新的数据块
    // 原因，这里的entry_num_max指的是当前所有已申请的物理块能放下的最多entry数
    if (entry_no == entry_num_max) {
        blocks++;

        block_num newblkno = file_add_block(inodes[dir]);
        entry_no = 0;
        entry_table = reinterpret_cast<DirectoryEntry*>(read_block(newblkno));
    }

    // 找到空闲的inode和数据块
    node_num ino = alloc_inode();
    if (ino == 0) {
        std::cerr << "createFile: No free inode" << std::endl;
        return -1;
    }


    // 填入新entry 位置永远是table[no]
    entry_table[entry_no].m_ino = ino;
    strncpy(entry_table[entry_no].m_name, filename.c_str(), DirectoryEntry::DIRSIZ);
    entry_table[entry_no].m_type = type;
    // 写回只用写最后一块
    if(entry_no == 0)
        write_block(translate_block(inodes[dir], blocks), (buffer *)entry_table); // 写回目录文件内容
    else 
        write_block(translate_block(inodes[dir], blocks), (buffer *)entry_table + (blocks-1)*BLOCK_SIZE); // 写回目录文件内容

    // 更新目录文件inode
    inodes[dir].d_size += ENTRY_SIZE;
    inodes[dir].d_mtime = time(nullptr);

    free(entry_table);
    return ino;
}

bool FileSystem::createDir(const node_num dir, const std::string& dirname) {
    // 先创建一个文件， 类型设置为目录
    node_num ino = createFile(dir, dirname, DirectoryEntry::FileType::Directory);
    if(ino == -1) {
        std::cerr << "createdir : dir file creating failed" << std::endl;
        return false;
    }

    // 目录文件创建需要自带 . 和 ..
    DirectoryEntry dotEntry(ino, ".", DirectoryEntry::FileType::Directory);
    DirectoryEntry dotDotEntry(dir, "..", DirectoryEntry::FileType::Directory);
    
    block_num blkno = alloc_block();
    buffer* blockBuf = read_block(blkno);
    if (blockBuf == nullptr) {
        std::cerr << "Error: Failed to read block." << std::endl;
        return false;
    }
    memcpy(blockBuf, &dotEntry, ENTRY_SIZE);
    memcpy(blockBuf + ENTRY_SIZE, &dotDotEntry, ENTRY_SIZE);
    if (!write_block(blkno, blockBuf)) {
        std::cerr << "Error: Failed to write block." << std::endl;
        return false;
    }

    // 更新inode和superblock
    inodes[ino].d_mtime = get_cur_time();
    sb.s_time = get_cur_time();

    return true;
}

node_num FileSystem::find_from_rootpath(const string& path) {
    // 重新解析Path
    std::vector<std::string> tokens;
    std::istringstream iss(path);
    std::string token;
    while (getline(iss, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    // 从根目录开始查找
    int ino = ROOT_INO;

    // 依次查找每一级目录或文件
    for (const auto& token : tokens) {
        DiskInode inode = inodes[ino];

        DirectoryEntry *entry_table = (DirectoryEntry *)malloc(inode.d_size);

        if(entry_table == nullptr || read(inode, (buffer *)entry_table, inode.d_size, 0) == false) {
            std::cerr << "createFile: read dirextory entries failed." << std::endl;
            return -1;
        }

        // 遍历所有目录项
        bool found = false;
        int entry_no, entry_num = inode.d_size/ENTRY_SIZE;
        for (entry_no = 0; entry_no < entry_num; entry_no++) {
            // 查同名
            if (entry_table[entry_no].m_ino && strcmp(entry_table[entry_no].m_name, token.c_str()) == 0) {
                ino = entry_table[entry_no].m_ino;
                found = true;
                break;
            }
        }

        free(entry_table);
        if (!found) 
            return -1;
    }
    return ino;
}

bool FileSystem::saveFile(const std::string& src, const std::string& filename) {
    std::ifstream infile(src, std::ios::binary | std::ios::in);
    if (!infile) {
        std::cerr << "Failed to open file: " << src << std::endl;
        return false;
    }

    // 找到目标目录的inode编号
    node_num dir = find_from_rootpath(filename.substr(0, filename.rfind('/')));
    if (dir == -1) {
        std::cerr << "Failed to find directory: " << filename.substr(0, filename.rfind('/')) << std::endl;
        return false;
    }

    // 在目标目录下创建新文件
    node_num ino = createFile(dir, filename.substr(filename.rfind('/') + 1));
    if (ino == -1) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    // 获取inode
    DiskInode& inode = inodes[ino];

    // 从文件读取并写入inode
    uint offset = 0;
    char buf[BLOCK_SIZE];
    while (infile) {
        infile.read(buf, BLOCK_SIZE);
        uint count = infile.gcount();

        while (count > 0) {
            // 找到当前偏移量所对应的物理块号
            uint blockno = offset / BLOCK_SIZE;
            block_num blkno = translate_block(inode, blockno);

            // 如果物理块不存在，则分配一个
            if (blkno == 0) {
                blkno = alloc_block();
                if (blkno == 0) {
                    std::cerr << "Failed to allocate block for file: " << filename << std::endl;
                    return false;
                }
                inode.d_addr[blockno] = blkno;
            }

            // 读取物理块内容
            char* block_buf = read_block(blkno);

            // 写入数据
            uint pos = offset % BLOCK_SIZE;
            uint n = std::min(BLOCK_SIZE - pos, (long)count);
            memcpy(block_buf + pos, buf + (infile.gcount() - count), n);
            count -= n;
            offset += n;

            // 写入物理块
            if (!write_block(blkno, block_buf)) {
                std::cerr << "Failed to write block for file: " << filename << std::endl;
                return false;
            }
        }
    }

    // 更新inode信息
    inode.d_size = offset;
    inode.d_mtime = get_cur_time();
    inode.d_atime = get_cur_time();
    inode.d_nlink = 1;

    return true;
}


// 定义内部文件系统根目录的路径和外部文件系统目录的路径
const std::string internal_root_path = "/root";

bool FileSystem::initialize_from_external_directory(string external_root_path)
{
    // 首先检查外部文件系统目录是否存在
    struct stat st;
    if (stat(external_root_path.c_str(), &st) != 0)
    {
        return false;
    }

    // 然后遍历外部文件系统目录中的所有文件和子目录，并在内部文件系统中创建相应的文件和目录
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(external_root_path.c_str())) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            // 忽略特殊目录（"."和".."）
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            {
                continue;
            }

            // 构造外部文件系统中文件或目录的完整路径
            std::string external_file_path = external_root_path + "/" + ent->d_name;

            // 检查文件或目录是否存在
            struct stat st;
            if (stat(external_file_path.c_str(), &st) != 0)
            {
                continue;
            }

            // 如果是一个目录，则在内部文件系统中创建相应的目录
            if (S_ISDIR(st.st_mode))
            {
                std::string internal_dir_path = internal_root_path + "/" + ent->d_name;
                create_directory(internal_dir_path);
            }
            // 如果是一个普通文件，则在内部文件系统中创建相应的文件，并将外部文件中的内容复制到内部文件中
            else if (S_ISREG(st.st_mode))
            {
                std::string internal_file_path = internal_root_path + "/" + ent->d_name;
                std::ifstream external_file(external_file_path);
                std::ofstream internal_file(internal_file_path);
                internal_file << external_file.rdbuf();
            }
        }
        closedir(dir);
    }
    return true;
}
