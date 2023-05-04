#include "sb.h"
#include "dinode.h"
#include "parameter.h"
#include <string>
#include <fstream>

class FileSystem {
public:
    FileSystem() {};
    FileSystem(const std::string& diskfile);
    ~FileSystem();

//private:
    std::fstream disk_;     // Disk file stream
    std::string diskfile_;  // Disk file name
    SuperBlock sb;          // Super block
    DiskInode inodes[INODE_NUM];  // Inode table
};
