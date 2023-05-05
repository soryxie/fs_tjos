#include <cstring>

class DirectoryEntry
{
public:
    static const int DIRSIZ = 24;   /* 文件名长度 */

    enum class FileType {
        Unknown,
        RegularFile,
        Directory,
        Link,
        // ...
    };

public:
    DirectoryEntry(){};
    DirectoryEntry(int ino, const char *name, FileType type = FileType::Unknown) {
        m_ino = ino;
        strcpy(m_name, name);
        m_type = type;
    }
    ~DirectoryEntry(){};

public:
    int m_ino;              /* 文件inode号 */
    char m_name[DIRSIZ];    /* 文件名 */
    FileType m_type;        /* 文件类型 */
};
