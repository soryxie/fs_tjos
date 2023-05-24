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

using namespace std;

time_t get_cur_time() {
    return chrono::system_clock::to_time_t(chrono::system_clock::now());
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
    DiskInode dinodes[INODE_NUM];
    disk.seekg(OFFSET_INODE, std::ios::beg);
    disk.read(reinterpret_cast<char*>(&dinodes[0]), sizeof(DiskInode)*INODE_NUM);
    for (int i=0; i<INODE_NUM;i++) {
        inodes[i].i_ino = i;
        inodes[i].d_mode = dinodes[i].d_mode;
        inodes[i].d_nlink = dinodes[i].d_nlink;
        inodes[i].d_uid = dinodes[i].d_uid;
        inodes[i].d_gid = dinodes[i].d_gid;
        inodes[i].d_size = dinodes[i].d_size;
        inodes[i].d_atime = dinodes[i].d_atime;
        inodes[i].d_mtime = dinodes[i].d_mtime;
        for (int j = 0; j < 10; j++)
            inodes[i].d_addr[j] = dinodes[i].d_addr[j];
    }

    // 保存
    diskfile_ = diskfile;
    disk_ = std::move(disk);
}

FileSystem::~FileSystem() {
    // 保存数据
    disk_.seekp(0, std::ios::beg);
    disk_.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    disk_.seekp(OFFSET_INODE, std::ios::beg);
    DiskInode dinodes[INODE_NUM];
    for (int i=0; i<INODE_NUM;i++) {
        dinodes[i].d_mode = inodes[i].d_mode;
        dinodes[i].d_nlink = inodes[i].d_nlink;
        dinodes[i].d_uid = inodes[i].d_uid;
        dinodes[i].d_gid = inodes[i].d_gid;
        dinodes[i].d_size = inodes[i].d_size;
        dinodes[i].d_atime = inodes[i].d_atime;
        dinodes[i].d_mtime = inodes[i].d_mtime;
        for (int j = 0; j < 10; j++)
            dinodes[i].d_addr[j] = inodes[i].d_addr[j];
    }
    disk_.write(reinterpret_cast<char*>(&dinodes[0]), sizeof(DiskInode)*INODE_NUM);

    // 关闭文件
    //cout<<"close"<<endl;
    //disk_.close();
}

int FileSystem::alloc_inode() {
    if(sb.s_ninode <= 1)
        return 0;
    int ino = sb.s_inode[--sb.s_ninode];
    inodes[ino].d_nlink = 1;
    inodes[ino].d_uid = user_->uid;
    inodes[ino].d_gid = user_->group;
    

    // 获取当前时间的Unix时间戳
    inodes[ino].d_atime = inodes[ino].d_mtime = get_cur_time();

    // 将Int型的时间变为可读
    //char* str_time = ctime((const time_t *)&d_atime);
    //cout << "allocate new inode "<< ino <<", uid="<< d_uid <<", gid="<< d_gid <<", time="/*<< str_time*/ << endl;

    return ino;
}

int FileSystem::alloc_block() {
    int blkno;
    if(sb.s_nfree <= 1) // free list 不足
    {
        // 换一张新表
        buffer buf[BLOCK_SIZE] = "";
        read_block(sb.s_free[0], buf);
        int *table = reinterpret_cast<int *>(buf);
        sb.s_nfree = table[0];
        for(int i=0;i<sb.s_nfree;i++)
            sb.s_free[i] = table[i+1];
    }
    // 取一个块
    blkno = sb.s_free[--sb.s_nfree];
    if (blkno == 0) {
        cout << "error : block list empty" << endl;
        return FAIL;
    }
    return blkno;
}

int FileSystem::dealloc_inode(int ino) {
    sb.s_inode[sb.s_ninode++] = ino;
    return 0;
}

int FileSystem::dealloc_block(int blkno) {
    if(sb.s_nfree >= 100) // free list 满了
    {
        // 换一张新表
        buffer buf[BLOCK_SIZE] = "";
        int *table = reinterpret_cast<int *>(buf);
        table[0] = sb.s_nfree;
        for(int i=0;i<sb.s_nfree;i++)
            table[i+1] = sb.s_free[i];
        write_block(sb.s_free[0], buf);
        sb.s_nfree = 1;
    }
    // 放回free list
    sb.s_free[sb.s_nfree++] = blkno;
    return 0;
}

bool FileSystem::read_block(int blkno, buffer* buf) {
    // 暂未实现缓存
    //cout << "read " << blkno << endl;
    disk_.seekg(OFFSET_DATA + blkno*BLOCK_SIZE, std::ios::beg);
    disk_.read(buf, BLOCK_SIZE);
    return true;
}

bool FileSystem::write_block(int blkno, buffer* buf) {
    //cout << "wrire " << blkno << endl;
    disk_.seekg(OFFSET_DATA + blkno*BLOCK_SIZE, std::ios::beg);
    disk_.write(buf, BLOCK_SIZE);
    return true;
}

vector<string> FileSystem::split_path(string path) {
    vector<string> tokens;
    istringstream iss(path);
    string token;
    while (getline(iss, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

int FileSystem::find_from_path(const string& path) {
    int ino;    // 起始查询的目录INode号
    if(path.empty()){
        cerr << "error path!" << endl;
        return FAIL;
    }
    else {      // 判断是相对路径还是绝对路径
        if(path[0] == '/')
            ino = ROOT_INO;         
        else
            ino = user_->current_dir_;
    }

    // 重新解析Path
    auto tokens = split_path(path);

    // 依次查找每一级目录或文件
    for (const auto& token : tokens) {
        ino = inodes[ino].find_file(token);
        if (ino == FAIL) return FAIL;
    }

    return ino;
}

void FileSystem::set_current_dir_name(std::string& token) {
    std::vector<std::string> paths = split_path(token);
    for (auto& path : paths) {
        if (path == "..")  {
            if(user_->current_dir_name !="/") {
                size_t pos = user_->current_dir_name.rfind('/');
                user_->current_dir_name = user_->current_dir_name.substr(0, pos);
                if(user_->current_dir_name == "")
                    user_->current_dir_name = "/";
            }
            
        } 
        else if (path == ".") {}
        else {
            if (user_->current_dir_name.back() != '/') {
                user_->current_dir_name += '/';
            }
            user_->current_dir_name += path;
        }
    }
}
    
/**
* 从外部文件读取
* 写入TJ_FS
*/
int FileSystem::saveFile(const std::string& src, const std::string& filename) {
    // 找到目标文件所在目录的inode编号
    int dir;
    if(filename.rfind('/') == -1)
        dir = user_->current_dir_;
    else {
        dir = find_from_path(filename.substr(0, filename.rfind('/')));
        if (dir == FAIL) {
            std::cerr << "Failed to find directory: " << filename.substr(0, filename.rfind('/')) << std::endl;
            return false;
        }
    }

    // 在目标目录下创建新文件
    string name = filename.substr(filename.rfind('/') + 1);
    int ino = inodes[dir].create_file(name, false);
    if (ino == FAIL) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    // 获取inode
    Inode& inode = inodes[ino];

    // 从文件读取并写入inode
    std::ifstream infile(src, std::ios::binary | std::ios::in);
    if (!infile) {
        std::cerr << "Failed to open file: " << src << std::endl;
        return false;
    }

    // 获取文件大小
    infile.seekg(0, std::ios::end);
    size_t size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    // 一次性读取文件数据
    std::vector<char> data(size);
    infile.read(data.data(), size);

    // 写入inode
    if (!inode.write_at(0, data.data(), size)) {
        std::cerr << "Failed to write data to inode" << std::endl;
        return false;
    }

    // 更新inode信息
    inode.d_size = size;
    inode.d_mtime = get_cur_time();
    inode.d_atime = get_cur_time();
    inode.d_nlink = 1;

    infile.close();

    return true;
}

int FileSystem::initialize_from_external_directory(const string& path, const int root_no) {
    if(inodes[ROOT_INO].d_size == 0)
        inodes[ROOT_INO].init_as_dir(ROOT_INO, ROOT_INO);
    DIR *pDIR = opendir((path + '/').c_str());
    dirent *pdirent = NULL;
    if (pDIR != NULL)
    {
        cout << "under " << path << ":" << endl;
        while ((pdirent = readdir(pDIR)) != NULL)
        {
            string Name = pdirent->d_name;
            if (Name == "." || Name == "..")
                continue;


            struct stat buf;
            if (!lstat((path + '/' + Name).c_str(), &buf))
            {
                int ino; // 获取文件的inode号
                if (S_ISDIR(buf.st_mode))
                {
                    ino = inodes[root_no].create_file(Name, true);
                    cout << "make folder: " << Name << " success! inode:" << ino << endl;

                    /* 递归进入，需要递进用户目录 */
                    user_->current_dir_ = ino;
                    if(initialize_from_external_directory(path + '/' + Name, ino) == false){
                        return false;
                    }
                    user_->current_dir_ = root_no;

                }
                else if (S_ISREG(buf.st_mode))
                {
                    cout << "normal file:" << Name << endl;
                    if(saveFile(path + '/' + Name, Name) == false)
                        return false;
                }
                else
                {
                    cout << "other file" << Name << endl;
                }
            }
        }
    }
    closedir(pDIR);
    return true;
}

int FileSystem::ls(const string& path) {
    int path_no;
    if(path.empty())
        path_no = user_->current_dir_;
    else
        path_no = find_from_path(path);

    if (path_no == -1) {
        std::cerr << "ls: cannot access '" << path << "': No such file or directory" << std::endl;
        return false;
    }

    Inode inode = inodes[path_no];

    auto entries = inode.get_entry();
    for (auto& entry : entries) {
        if (entry.m_ino) {
            Inode child_inode = inodes[entry.m_ino];
            string name(entry.m_name);

            //输出用户            
            cout.width(7); 
            cout << user_->username;
            
            //输出文件大小
            cout.width(5);
            cout << child_inode.d_size  << "B ";

            //输出最后修改时间
            std::time_t t_time = child_inode.d_mtime;
            std::tm* local_time = std::localtime(&t_time);  // int时间转换为当地时间
            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);  // 格式化输出
            std::cout << buffer << " ";

            //输出文件名
            cout << name;
            if (entry.m_type == DirectoryEntry::FileType::Directory) {
                cout << "/";
            }
            cout << endl;
        }
    }

    return true;
}

int FileSystem::changeDir(string& dirname) {
    // 找到目标文件所在目录的inode编号
    int dir;
    if(dirname.rfind('/') == -1)
        dir = user_->current_dir_;
    else {
        dir = find_from_path(dirname.substr(0, dirname.rfind('/')));
        if (dir == FAIL) {
            std::cerr << "Failed to find directory: " << dirname.substr(0, dirname.rfind('/')) << std::endl;
            return FAIL;
        }
    }
    int path_no = find_from_path(dirname);
    //检查进入的是否是一个目录，如果进入的是文件则拒绝cd
    auto entries = inodes[dir].get_entry();
    for (auto& entry : entries) {
        if(entry.m_ino == path_no) {
            if(entry.m_type != DirectoryEntry::FileType::Directory) {
                std::cerr << "'" << dirname << "'is not a directory" << std::endl;
                return FAIL;
            }
            else {
                user_->set_current_dir(path_no);
                set_current_dir_name(dirname);
                return 0;
            }
        }
    } 
    std::cerr << "Failed to find directory: " << dirname << std::endl;
    return FAIL;
}

int FileSystem::createDir(const int current_dir, const string& dirname) {
    int path_no = inodes[current_dir].create_file(dirname, true);
    cout << "make folder: " << dirname << " success! inode:" << path_no << endl;
    return path_no;
}

int FileSystem::cat(const string& path) {
    int path_no = find_from_path(path);
    if (path_no == -1) {
        std::cerr << "cat: cannot access '" << path << "': No such file or directory" << std::endl;
        return false;
    }

    Inode inode = inodes[path_no];
    /*if (inode.d_mode & S_IFDIR) {
        std::cerr << "cat: " << path << ": Is a directory" << std::endl;
        return false;
    }*/
    string str;
    str.resize(inode.d_size+1);
    inode.read_at(0, str.data(), inode.d_size);
    
    cout << str << endl;
    return true;
}

int FileSystem::deleteFile(const string& filename) {
    return 0;
}