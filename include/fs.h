#include "sb.h"
#include "inode.h"
#include "user.h"
#include "BlockCache.h"
#include <string>
#include <fstream>

typedef unsigned int uint;

class FileSystem {
public:
    friend class Inode; 
    friend class BlockCache; 

private:
    User *user_;
    std::fstream disk_;     // Disk file stream
    std::string diskfile_;  // Disk file name
    SuperBlock sb;          // Super block
    Inode inodes[INODE_NUM];  // Inode table

    BlockCacheMgr block_cache_mgr_;

public:

    std::vector<std::string> split_path(std::string path);

    /*
    * --------- 物理层 ------------
    */

    /* 获取一个空闲inode，并初步初始化 */
    int alloc_inode();

    /* 获取一个空闲物理块 */
    int alloc_block();

    /* 放回一个inode */
    int dealloc_inode(int ino);

    /* 放回一个物理块 */
    int dealloc_block(int blkno);

    /* 读取一个物理块 */
    bool read_block(int blkno, buffer* buf);
    
    /* 写入一个物理块 */
    bool write_block(int blkno, buffer* buf);

    /*
    * --------- 目录层 ------------
    */
   
    /* 针对文件路径的查找：从给定路径查找对应文件的inode编号 否则返回-1 */
    int find_from_path(const std::string& path);

public:
    FileSystem() {};
    FileSystem(const std::string& diskfile);
    ~FileSystem();

    void set_u(User *u) {user_ = u;};
    
    /* 将一个外部文件系统目录作为内部文件系统的根目录并初始化文件系统的目录和文件 */
    int initialize_from_external_directory(const std::string& external_root_path, const int root_no = 1);

    int createDir(const int current_dir, const std::string& dirname);
    //在当前目录下创建一个新目录,返回值是该目录的ino

    int exportFile(const std::string& src, const std::string& outsideFile);
    //从文件系统中的文件导出到本地

    int saveFile(const std::string& outsideFile, const std::string& dst);
    //将本地文件复制到文件系统中

    int deleteDir(const std::string& dirname);
    //删除当前目录下的指定目录

    int deleteFile(const std::string& filename);
    //删除当前目录下的指定文件

    int openFile(const std::string& filename);
    //在用户打开文件表中添加一个文件，准备进行读写操作

    int closeFile(const std::string& filename);
    //从用户打开文件表中移除指定文件

    int readFile(const std::string& filename, char* buffer, int size, int offset);
    //从指定文件中读取数据到缓冲区中

    int writeFile(const std::string& filename, const char* buffer, int size, int offset);
    //将缓冲区中的数据写入指定文件中

    int moveFile(const std::string& src, const std::string& dst);
    //将一个文件从一个目录移动到另一个目录

    int copyFile(const std::string& src, const std::string& dst);
    //将一个文件从一个目录复制到另一个目录

    int renameFile(const std::string& filename, const std::string& newname);
    //修改文件名

    int changeDir(std::string& dirname);
    //更改当前目录到指定目录

    std::string ls(const std::string& path);
    //获取当前目录下的所有文件和目录

    bool getFileInfo(const std::string& filename, Inode& ino);
    //获取指定文件的详细信息，如文件大小，创建时间等

    void set_current_dir_name(std::string& path); 
    //修改User的当前目录字符串

    std::string cat(const std::string& filename);
    // 输出指定文件的内容
};
