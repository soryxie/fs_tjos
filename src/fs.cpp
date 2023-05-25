#include "../include/fs.h"
#include <iostream>
#include <iomanip>
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
    cout << "alloc inode : " << ino << endl;
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

    cout << "alloc block : " << blkno << endl;
    return blkno;
}

int FileSystem::dealloc_inode(int ino) {
    cout << "dealloc inode : " << ino << endl;
    sb.s_inode[sb.s_ninode++] = ino;
    return 0;
}

int FileSystem::dealloc_block(int blkno) {
    cout << "dealloc block : " << blkno << endl;
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

void FileSystem::set_current_dir_name(string& token) {
    vector<string> paths = split_path(token);
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

int FileSystem::exportFile(const string& src, const std::string& outsideFile) {
    int ino = find_from_path(src);
    if (ino == FAIL) {
        cerr << "Failed to find file: " << src << endl;
        return false;
    }

    Inode &inode = inodes[ino];
    
    // 判断是否为文件
    if(!(inode.d_mode & Inode::FileType::RegularFile)) {
        cerr << "Failed to export " << src << ". Is not a Regular File!" << endl;
        return FAIL;
    }

    // 打开外部文件
    ofstream fout(outsideFile, ios::binary);
    if (!fout) {
        cerr << "Failed to open file: " << outsideFile << endl;
        return FAIL;
    }

    // 读取文件内容
    int size = inode.d_size;
    string buf;
    buf.resize(size);
    inode.read_at(0, (char *)buf.data(), size);

    // 写入外部文件
    fout.write(buf.data(), size);
    fout.close();

    return 0;
}    

int FileSystem::saveFile(const string& outsideFile, const string& dst) {
    // 找到目标文件所在目录的inode编号
    int dir;
    if(dst.rfind('/') == -1)
        dir = user_->current_dir_;
    else {
        dir = find_from_path(dst.substr(0, dst.rfind('/')));
        if (dir == FAIL) {
            cerr << "Failed to find directory: " << dst.substr(0, dst.rfind('/')) << endl;
            return false;
        }
    }

    // 在目标目录下创建新文件
    string name = dst.substr(dst.rfind('/') + 1);
    int ino = inodes[dir].create_file(name, false);
    if (ino == FAIL) {
        cerr << "Failed to create file: " << dst << endl;
        return false;
    }

    // 获取inode
    Inode& inode = inodes[ino];

    // 从文件读取并写入inode
    ifstream infile(outsideFile, ios::binary | ios::in);
    if (!infile) {
        cerr << "Failed to open file: " << outsideFile << endl;
        return false;
    }

    // 获取文件大小
    infile.seekg(0, ios::end);
    size_t size = infile.tellg();
    infile.seekg(0, ios::beg);

    // 一次性读取文件数据
    vector<char> data(size);
    infile.read(data.data(), size);

    // 写入inode
    if (!inode.write_at(0, data.data(), size)) {
        cerr << "Failed to write data to inode" << endl;
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

    if (path_no == FAIL) {
        std::cerr << "ls: cannot access '" << path << "': No such file or directory" << std::endl;
        return false;
    }

    Inode &inode = inodes[path_no];
    // 检查是否是目录
    if(!(inode.d_mode & Inode::FileType::Directory)) {
        cerr << "'" << path << "' is not a directory" << endl;
        return FAIL;
    }

    auto entries = inode.get_entry();
    for (auto& entry : entries) {
        if (entry.m_ino) {
            Inode child_inode = inodes[entry.m_ino];
            string name(entry.m_name);

            //输出用户            
            cout.width(7); 
            cout << user_->username << " ";
            
            //输出文件大小
            float size = child_inode.d_size;
            if(size < 1024) {
                cout.width(7);
                cout << child_inode.d_size  << "B ";
            }
            else {
                size = size / 1024.0;
                if(size < 1024) {
                    cout.width(7);
                    cout << std::fixed << std::setprecision(2) << size << "K "; 
                }
                else {
                    size = size / 1024.0;
                    cout.width(7);
                    cout << std::fixed << std::setprecision(2) << size << "M "; 
                }
            }

            //输出最后修改时间
            std::time_t t_time = child_inode.d_mtime;
            std::tm* local_time = std::localtime(&t_time);  // int时间转换为当地时间
            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);  // 格式化输出
            std::cout << buffer << " ";

            //输出文件名
            cout << name;
            if (child_inode.d_mode & Inode::FileType::Directory) {
                cout << "/";
            }
            cout << endl;
        }
    }

    return true;
}

int FileSystem::changeDir(string& dirname) {
    // 找到目标文件所在目录的inode编号
    int dir = find_from_path(dirname);
    if (dir == FAIL) {
        cerr << "Failed to find directory: " << dirname << endl;
        return FAIL;
    }

    //检查进入的是否是一个目录，如果进入的是文件则拒绝cd
    auto dir_inode = inodes[dir];
    if((dir_inode.d_mode & Inode::FileType::Directory) == 0) {
        cerr << "'" << dirname << "'is not a directory" << endl;
        return FAIL;
    }
    else {  
        // 检查通过，设置cur_dir的 INode 和 路径字符串 
        user_->set_current_dir(dir); 
        set_current_dir_name(dirname);
        return 0;
    }
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

    Inode &inode = inodes[path_no];
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
    // 找到目标文件所在目录的inode编号
    int dir, ino;
    if(filename.rfind('/') == -1)
        dir = user_->current_dir_;
    else {
        dir = find_from_path(filename.substr(0, filename.rfind('/')));
    }

    // 找到目标文件
    ino = find_from_path(filename);
    if(dir == FAIL || ino == FAIL) {
        cerr << "rm: '" << filename << "' No such file or directory" << endl;
        return FAIL;
    }

    // 确定文件类型
    if(inodes[ino].d_mode & Inode::FileType::Directory) {
        cerr << "rm: cannot remove '" << filename << "': Is a directory" << endl;
        return FAIL;
    }

    // 从父目录中删除entry项
    ino = inodes[dir].delete_file_entry(filename.substr(filename.rfind('/')+1));
    if (ino == FAIL) {
        return FAIL;
    }

    // 删除文件 释放inode
    inodes[ino].clear();
    return 0;
}

int FileSystem::copyFile(const string& src, const string& dst) {
    int src_ino = find_from_path(src);
    if(src_ino == FAIL) {
        cerr << "cp: cannot stat '" << src << "': No such file or directory" << endl;
        return FAIL;
    }

    int dst_dir;
    if(dst.rfind('/') == -1)
        dst_dir = user_->current_dir_;
    else {
        dst_dir = find_from_path(dst.substr(0, dst.rfind('/')));
    }

    int dst_ino = find_from_path(dst);
    if(dst_ino != FAIL) {
        cerr << "cp: cannot stat '" << dst << "': File exists" << endl;
        return FAIL;
    }

    // 复制inode和数据
    int new_ino = inodes[dst_dir].create_file(dst.substr(dst.rfind('/')+1), false);
    inodes[new_ino].copy_from(inodes[src_ino]);

    return 0;
}

 int FileSystem::moveFile(const std::string& src, const std::string& dst) {
    
    //src是否存在
        //src不存在
            //报错
        //src存在
            //src存在dst不存在
                //dst父目录不存在   报错
                //dst父目录存在     可以移动
            //dst存在
                //dst是文件 报错
                //dst是目录 可以移动
                

    int src_ino = find_from_path(src);
    if(src_ino == FAIL) {
        cerr << "mv: cannot move '" << src << "': No such file or directory" << endl;
        return FAIL;
    }
    int src_dir;
    if(src.rfind('/') == -1)
        src_dir = user_->current_dir_;
    else {
        src_dir = find_from_path(src.substr(0, src.rfind('/')));
    }

    int dst_dir = -1;
    if(dst.empty())
    {
        cerr << "mv: insufficent args" << endl;
        return FAIL;
    }
    int dst_ino = find_from_path(dst);
    std::string dst_filename = "";
    if(dst_ino == FAIL) {
        
        if(dst.rfind('/') == -1)
            dst_dir = user_->current_dir_;
        else 
        dst_dir = find_from_path(dst.substr(0, dst.rfind('/')));

        if(dst_dir == FAIL)
        {
            cerr << "mv: cannot move to '" << dst.substr(0, dst.rfind('/')) << "': No such directory" << endl;
            return FAIL;
        }

        dst_filename =  dst.substr(dst.rfind('/')+1);

    }
    else
    {
        if(src_ino == dst_ino)
        {
            cout << "mv: you are wasting time finding bugs" << endl;
            return 0;
        }
        bool dst_is_file = inodes[dst_ino].d_mode & Inode::FileType::RegularFile;
        if(dst_is_file)
        {
            std::cerr << "mv: '"<< dst <<"is a file, can not move dir to file" << std::endl;
            return FAIL;
        }
        dst_filename =  src.substr(src.rfind('/')+1);
        dst_dir = dst_ino;
    }
    //此时已经确定当前情形一定可以移动
    //src_dir是src的父目录，src_ino是src的ino，dst_filename是移过去之后的文件名，dst_dir是移动的目标路径（src置于dst_dir下）

    //删除src
    inodes[src_dir].delete_file_entry(src.substr(src.rfind('/')+1));
    //在dst_dir下新建一个文件
    bool src_is_dir = inodes[src_ino].d_mode & Inode::FileType::Directory;
    //cout << dst_filename << endl;
    dst_ino = inodes[dst_dir].create_file(dst_filename,src_is_dir);
    //把src的信息复制过来
    inodes[dst_ino].copy_from(inodes[src_ino]);

    return 0;
}


/*
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //判断src是否存在/为文件
    int src_ino = find_from_path(src);
    int dst_ino = find_from_path(dst);

    int src_path = -1;
    std::string src_file_name = "";
    int dst_path = -1;
    std::string dst_filename = "";


    if (src_ino == -1) {
        std::cerr << "mv: cannot access '" << src << "': No such file or directory" << std::endl;
        return FAIL;
    }

    bool src_is_dir = (inodes[src_ino].d_mode & Inode::FileType::Directory);
    //判断dst是否以/结尾，如果以/结尾就一定是目录，反之一定是文件
    bool dst_is_dir = (dst[dst.length()-1] == '/');
    

    //处理dst
    //如果dst是目录，判断其是否存在
    if(dst_is_dir)
    {  
        if (dst_ino == -1) 
        {
            dst_ino = createDir(user_->current_dir_ ,dst);
        }
        dst_path = dst_ino;
    }   
    else 
    {
        //如果dst是文件,src是目录，报错
        if(src_is_dir)
        {
            std::cerr << "mv: can not move dir to file" << std::endl;
            return FAIL;
        }
        //没有/的情况
        if(dst.rfind('/') == -1)
        {
            dst_path = user_->current_dir_;
            dst_filename = dst;
        }
        else
        {
            dst_path = find_from_path(dst.substr(0, dst.rfind('/')));
            dst_filename =  dst.substr(dst.rfind('/')+1);
            if (dst_path == FAIL) 
            {
                std::cerr << "cd: Failed to find directory: " << dst.substr(0, dst.rfind('/')) << std::endl;
                return FAIL;
            }
        }

    }
    
    
    //处理src
    if(src.rfind('/') == -1)
    {
        src_file_name = src;
        src_path = user_->current_dir_;
    }
    else
    {
        src_file_name =  src.substr(src.rfind('/')+1);
        src_path =  find_from_path(src.substr(0, src.rfind('/')));
    }
    
    src_ino = inodes[src_path].delete_file_entry(src);

    if(dst_filename == "")
    {
        dst_filename = src_file_name;
    }


    dst_ino = inodes[dst_path].create_file(dst_filename,src_is_dir);
    inodes[dst_ino].copy_from(inodes[src_ino]);
    
    return 0;

}*/
