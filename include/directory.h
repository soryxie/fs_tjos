#pragma once
class DirectoryEntry
{
public:
    static const int DIRSIZ = 28;   /* 文件名长度 */

public:
    DirectoryEntry(){};
    DirectoryEntry(int ino, const char *name);
    ~DirectoryEntry(){};

public:
    int m_ino;              /* 文件inode号 */
    char m_name[DIRSIZ];    /* 文件名 */
};
