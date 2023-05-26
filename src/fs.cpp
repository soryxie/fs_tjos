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

    // 读取用户信息
    // 打开存储用户名和密码的文件
    ifstream file("users.txt");
    if (!file.is_open()) {
        cerr << "Failed to open users.txt" << endl;
        exit(0); 
    }

    // 读取文件中的用户信息
    while (!file.eof()) {
        User user;
        file >> user.uid >> user.username >> user.password >> user.group;
        user_.push_back(user);
    }
    file.close();
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
    inodes[ino].d_uid = user_[uid_].uid;
    inodes[ino].d_gid = user_[uid_].group;
    
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
            ino = user_[uid_].current_dir_;
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
            if(user_[uid_].current_dir_name !="/") {
                size_t pos = user_[uid_].current_dir_name.rfind('/');
                user_[uid_].current_dir_name = user_[uid_].current_dir_name.substr(0, pos);
                if(user_[uid_].current_dir_name == "")
                    user_[uid_].current_dir_name = "/";
            }
        } 
        else if (path == ".") {}
        else {
            if (user_[uid_].current_dir_name.back() != '/') {
                user_[uid_].current_dir_name += '/';
            }
            user_[uid_].current_dir_name += path;
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
        dir = user_[uid_].current_dir_;
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
                    user_[uid_].current_dir_ = ino;
                    if(initialize_from_external_directory(path + '/' + Name, ino) == false){
                        return false;
                    }
                    user_[uid_].current_dir_ = root_no;

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

string FileSystem::ls(const string& path) {
    ostringstream oss;
    int path_no;
    if(path.empty())
        path_no = user_[uid_].current_dir_;
    else
        path_no = find_from_path(path);

    if (path_no == FAIL) {
        oss << "ls: cannot access '" << path << "': No such file or directory" << std::endl;
        return oss.str();
    }

    Inode &inode = inodes[path_no];
    // 检查是否是目录
    if(!(inode.d_mode & Inode::FileType::Directory)) {
        oss << "'" << path << "' is not a directory" << endl;
        return oss.str();
    }

    auto entries = inode.get_entry();
    for (auto& entry : entries) {
        if (entry.m_ino) {
            Inode child_inode = inodes[entry.m_ino];
            string name(entry.m_name);

            //输出用户            
            oss.width(7); 
            oss << user_[uid_].username << " ";
            
            //输出文件大小
            float size = child_inode.d_size;
            if(size < 1024) {
                oss.width(7);
                oss << child_inode.d_size  << "B ";
            }
            else {
                size = size / 1024.0;
                if(size < 1024) {
                    oss.width(7);
                    oss << std::fixed << std::setprecision(2) << size << "K "; 
                }
                else {
                    size = size / 1024.0;
                    oss.width(7);
                    oss << std::fixed << std::setprecision(2) << size << "M "; 
                }
            }

            //输出最后修改时间
            std::time_t t_time = child_inode.d_mtime;
            std::tm* local_time = std::localtime(&t_time);  // int时间转换为当地时间
            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);  // 格式化输出
            oss << buffer << " ";

            //输出文件名
            oss << name;
            if (child_inode.d_mode & Inode::FileType::Directory) {
                oss << "/";
            }
            oss << endl;
        }
    }

    return oss.str();
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
        user_[uid_].set_current_dir(dir); 
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

string FileSystem::cat(const string& path) {
    ostringstream oss;
    int path_no = find_from_path(path);
    if (path_no == -1) {
        oss << "cat: cannot access '" << path << "': No such file or directory" << std::endl;
        return oss.str();
    }

    Inode &inode = inodes[path_no];
    /*if (inode.d_mode & S_IFDIR) {
        std::cerr << "cat: " << path << ": Is a directory" << std::endl;
        return false;
    }*/
    string str;
    str.resize(inode.d_size+1);
    inode.read_at(0, str.data(), inode.d_size);
    
    oss << str << endl;
    return oss.str();
}

int FileSystem::deleteFile(const string& filename) {
    // 找到目标文件所在目录的inode编号
    int dir, ino;
    if(filename.rfind('/') == -1)
        dir = user_[uid_].current_dir_;
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
        dst_dir = user_[uid_].current_dir_;
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
    int src_ino = find_from_path(src);
    if(src_ino == FAIL) {
        cerr << "mv: cannot move '" << src << "': No such file or directory" << endl;
        return FAIL;
    }
    int src_dir;
    if(src.rfind('/') == -1)
        src_dir = user_[uid_].current_dir_;
    else {
        src_dir = find_from_path(src.substr(0, src.rfind('/')));
    }

    int dst_dir = -1;
    if(dst.empty()) {
        cerr << "mv: insufficent args" << endl;
        return FAIL;
    }
    int dst_ino = find_from_path(dst);
    std::string dst_filename = "";
    if(dst_ino == FAIL) {
        
        if(dst.rfind('/') == -1)
            dst_dir = user_[uid_].current_dir_;
        else 
        dst_dir = find_from_path(dst.substr(0, dst.rfind('/')));

        if(dst_dir == FAIL) {
            cerr << "mv: cannot move to '" << dst.substr(0, dst.rfind('/')) << "': No such directory" << endl;
            return FAIL;
        }

        dst_filename =  dst.substr(dst.rfind('/')+1);

    }
    else {
        if(src_ino == dst_ino) {
            cout << "mv: you are wasting time finding bugs" << endl;
            return 0;
        }
        bool dst_is_file = inodes[dst_ino].d_mode & Inode::FileType::RegularFile;
        if(dst_is_file) {
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

string FileSystem::pCommand(int uid, string& command) {
    set_u(uid);

    // 解析命令
    istringstream iss(command);
    string s;
    vector<string> tokens;
    while(iss) {
        iss >> s;
        tokens.emplace_back(s);
    }

    // 结果统一存入result
    string result;
    if(tokens[0] == "init"){
        //if(initialize_from_external_directory(tokens[1]) == false) {
        if(initialize_from_external_directory("test_folder") == false)
            result = "Initialize failed!\n";
        else
            result = "Initialize success!\n";
    }
    else if(tokens[0] == "ls"){
        if(tokens.size() > 2)
            result = ls(tokens[1]);
        else
            result = ls("");
    }
    else if(tokens[0] == "cd"){
        if(changeDir(tokens[1]) != FAIL)
            result = "cd : success!\n";
    }
    else if(tokens[0] == "mkdir"){
        if(createDir(user_[uid_].current_dir_,tokens[1]) != FAIL)
            result = "mkdir : success!\n";
    }
    else if(tokens[0] == "cat"){
        result = cat(tokens[1]);
    }
    else if(tokens[0] == "rm"){
        if(deleteFile(tokens[1]) != FAIL)
            result = "rm : success!\n";
    }
    else if(tokens[0] == "cp"){
        if(copyFile(tokens[1], tokens[2]) != FAIL)
            result = "cp : success!\n";
    }
    else if(tokens[0] == "save"){
        if(saveFile(tokens[1], tokens[2]) != FAIL)
            result = "save : success!\n";
    }
    else if(tokens[0] == "export"){
        if(exportFile(tokens[1], tokens[2]) != FAIL)
            result = "export : success!\n";
    }
    else if(tokens[0] == "mv"){
         if(moveFile(tokens[1], tokens[2]) != FAIL)
            result = "move : success!\n";
    }
    else {
        result = "Invalid command!\n";
    }
    return result;
}
