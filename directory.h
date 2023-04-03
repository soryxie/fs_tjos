class DirectoryEntry
{
public:
    static const int DIRSIZ = 28;   /* 文件名长度 */

public:
    DirectoryEntry(){};
    DirectoryEntry(int ino, const char *name) {
        m_ino = ino;
        strcpy(m_name, name);
    }
    ~DirectoryEntry(){};

public:
    int m_ino;              /* 文件inode号 */
    char m_name[DIRSIZ];    /* 文件名 */
};