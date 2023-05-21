# 第一次报告



### 设计思路：

-> 创建img文件 

-> 调整文件大小 

-> 初始化superblock(free[100], inode[100]，外部的free列表) 

-> 沿test_folder出发，递归扫描内部目录、文件 

-> 创建他们的inode并存放进datablock 

-> 批量使用mmap，将superblock、inode[100]、data[DATASIZE]拷贝进img文件。

流程结束。





### 组件说明

<img src=".\first_report.assets\image-20230403155044218.png" alt="image-20230403155044218" style="zoom:50%;" />

**数据结构**

> sb.h 		-> superblock 数据结构
>
> inode.h 	-> diskinode 数据结构
>
> directory.h  -> directory entry 数据结构

**init**

> init.cpp 	-> 核心逻辑 main函数
>
> parameter.h  -> 文件系统参数配置，各段的大小

**init_dir**

> init_dir.cpp / -.h
>
> 根据给定的目录"./test_folder"，以它作为根目录，将内部文件和目录全部转移到img中



### 进入img的数据组成

分为三个数据结构（init.cpp）

```c++
SuperBlock sb;					// superblock
DiskInode inode[INODE_NUM];		// inode 共100个
char data[DATA_SIZE];    		// 数据区
```



### 核心代码

```c++
const char *file_path = "myDisk.img";
int fd;
/* open("*.img") */

/* 调整文件大小 */
ftruncate(fd, FILE_SIZE) < 0)

/* 初始化superblock */
init_superblock();

/* 开始扫描文件,创建inode,填充数据块 */
scan_path("./test_folder");

int size = 0;
/* 初次拷贝，superblock, inode ,部分data */
size += map_img(fd, 0, 2 * PAGE_SIZE, copy_first);

/* 从剩余data部分开始 */
int data_off = 2 * PAGE_SIZE - OFFSET_DATA;
for (; data_off + PAGE_SIZE < DATA_SIZE; data_off += PAGE_SIZE)
    size += map_img(fd, size, PAGE_SIZE, copy_subseq);
if (DATA_SIZE - data_off > 0)
    size += map_img(fd, size, DATA_SIZE - data_off, copy_subseq);

// 关闭文件
close(fd);
```



### 实验结果

**运行成功**

<img src=".\first_report.assets\image-20230403155838193.png" alt="image-20230403155838193" style="zoom:50%;" />



**在myDisk.img中检验：**

<img src=".\first_report.assets\image-20230403153101808.png" alt="image-20230403155838193" style="zoom:50%;" />

<img src=".\first_report.assets\image-20230403152859599.png" alt="image-20230403155838193" style="zoom:50%;" />

<img src=".\first_report.assets\image-20230403153050452.png" alt="image-20230403155838193" style="zoom:50%;" />

综上，数据完全一致，文件目录树也已建立。