#include "parameter.h"
#include "directory.h"
#include <string>
#include <vector>

class DiskInode
{
public:
	enum INodeFlag
	{
		ILOCK = 0x1,		/* 索引节点上锁 */
		IUPD  = 0x2,		/* 内存inode被修改过，需要更新相应外存inode */
		IACC  = 0x4,		/* 内存inode被访问过，需要修改最近一次访问时间 */
		IMOUNT = 0x8,		/* 内存inode用于挂载子文件系统 */
		IWANT = 0x10,		/* 有进程正在等待该内存inode被解锁，清ILOCK标志时，要唤醒这种进程 */
		ITEXT = 0x20		/* 内存inode对应进程图像的正文段 */
	};
	
	DiskInode() {;};
	~DiskInode() {;};

	int get_block_id(int inner_id, bool create);
	int increase_size();
	int decrease_size();
	int push_back_block();
	int pop_back_block();
	
	//char *read_block(int blk_id);
	//bool write_block(int blk_id, char *buffer);
	
	int read_at(int offset, char *buf, int size);
	int write_at(int offset, const char *buf, int size);
	
	/* dir inode only */
	int init_as_dir(int ino, int fa_ino);
	std::vector<DirectoryEntry> get_entry();
	int find_file(const std::string &name);
	int create_file(const std::string &name, bool is_dir);
	int delete_file(const std::string &name);

public:
	unsigned int d_mode;	/* 状态的标志位，定义见enum INodeFlag */
	int		d_nlink;		/* 文件联结计数，即该文件在目录树中不同路径名的数量 ？？？*/
	
	short	d_uid;			/* 文件所有者的用户标识数 */
	short	d_gid;			/* 文件所有者的组标识数 */
	
	int		d_size;			/* 文件大小，字节为单位 */
	int	d_addr[10];		/* 用于文件逻辑块号和物理块号转换的基本索引表 ？？？*/
	
	int		d_atime;		/* 最后访问时间 */
	int		d_mtime;		/* 最后修改时间 */
};