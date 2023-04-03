#include "init_dir.h"
#include "sb.h"
#include "dinode.h"
#include "parameter.h"
#include "directory.h"

extern SuperBlock sb;
extern DiskInode inode[INODE_NUM];
extern char data[DATA_SIZE];

int create_inode(int size)
{
    int ino = sb.s_free[--sb.s_nfree];
    inode[ino].d_nlink = 1;
    inode[ino].d_size = size;
    return ino;
}

int alloc_data_block()
{
    int blkno = sb.s_inode[--sb.s_ninode];
    if (sb.s_ninode < 0)
    {
        cout << "error : super_block empty" << endl;
        return -1;
    }
    return blkno;
}

int create_file(string &fdata)
{
    int datap = 0, addr_n = 0;

    int ino = create_inode(fdata.length());
    cout<<"filelen:"<<inode[ino].d_size<<endl;;

    while (datap < inode[ino].d_size)
    {
        int blkno = alloc_data_block();
        int copy_len = BLOCK_SIZE;
        if (inode[ino].d_size - datap < BLOCK_SIZE)
            copy_len = inode[ino].d_size - datap;
        memcpy(data + blkno * BLOCK_SIZE, fdata.c_str() + datap, copy_len);
        cout << "offset:" << hex << OFFSET_DATA + blkno * BLOCK_SIZE << dec << endl;
        inode[ino].d_addr[addr_n++] = blkno;
        datap += copy_len;
    }

    return ino;
}

int scan_path(string path)
{
    int dir_file_ino = 0;
    DIR *pDIR = opendir((path + '/').c_str());
    dirent *pdirent = NULL;
    if (pDIR != NULL)
    {
        ostringstream dir_data;

        cout << "在目录" << path << "下：" << endl;
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
                    cout << "目录文件：" << Name << endl;
                    ino = scan_path(path + '/' + Name);
                }
                else if (S_ISREG(buf.st_mode))
                {
                    ifstream tmp;
                    ostringstream tmpdata;
                    tmp.open(path + '/' + Name);
                    tmpdata << tmp.rdbuf(); // 数据流导入进file_data
                    string tmp_data_string = tmpdata.str();
                    ino = create_file(tmp_data_string);
                    tmp.close();
                }
                cout << "name:" << Name << " ino:" << ino << "加入到目录数据段中" << endl;
                dir_data << ino << setfill('\0') << setw(DirectoryEntry::DIRSIZ) << Name << endl;
            }
        }

        cout << "构造目录文件:" << path << endl;
        string dir_data_string = dir_data.str();
        dir_file_ino = create_file(dir_data_string);
        cout << "构造目录inode成功inode:" << dir_file_ino << endl;
    }
    closedir(pDIR);
    return dir_file_ino;
}
