#include "sb.h"
#include "inode.h"
#include "user.h"
#include <string>
#include <fstream>

typedef unsigned int uint;

class FileSystem {
public:
    friend class Inode; 

private:
    User *user_;
    std::fstream disk_;     // Disk file stream
    std::string diskfile_;  // Disk file name
    SuperBlock sb;          // Super block
    Inode inodes[INODE_NUM];  // Inode table

    

private:
    /*
    * --------- 物理层 ------------
    */

    /* 获取一个空闲inode，并初步初始化 */
    int alloc_inode();

    /* 获取一个空闲物理块 */
    int alloc_block();

    /* 获取一个物理块的所有内容，返回指向这片缓存的buffer(char *)类型 */
    bool read_block(int blkno, buffer* buf);
    
    /* 写入一个物理块(全覆盖) */
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
    bool initialize_from_external_directory(const std::string& external_root_path, const int root_no = 1);
    
    int createFile(const int dir, const std::string& filename, DirectoryEntry::FileType type);
    //在dir目录下创建一个新文件

    int createDir(const int dir, const std::string& dirname);
    //在dir目录下创建一个新目录

    bool createRootDir();
    // 创建根目录

    bool loadFile(const std::string& filename, const std::string& dst);
    //从文件系统中加载一个文件，将其复制到本地目录中

    bool saveFile(const std::string& src, const std::string& filename);
    //将本地文件复制到文件系统中

    bool deleteDir(const std::string& dirname);
    //删除当前目录下的指定目录

    bool deleteFile(const std::string& filename);
    //删除当前目录下的指定文件

    bool openFile(const std::string& filename);
    //在用户打开文件表中添加一个文件，准备进行读写操作

    bool closeFile(const std::string& filename);
    //从用户打开文件表中移除指定文件

    bool readFile(const std::string& filename, char* buffer, int size, int offset);
    //从指定文件中读取数据到缓冲区中

    bool writeFile(const std::string& filename, const char* buffer, int size, int offset);
    //将缓冲区中的数据写入指定文件中

    bool moveFile(const std::string& src, const std::string& dst);
    //将一个文件从一个目录移动到另一个目录

    bool renameFile(const std::string& filename, const std::string& newname);
    //修改文件名

    bool changeDir(const std::string& dirname);
    //更改当前目录到指定目录

    bool ls(const std::string& path);
    //获取当前目录下的所有文件和目录

    bool getFileInfo(const std::string& filename, Inode& ino);
    //获取指定文件的详细信息，如文件大小，创建时间等



};
