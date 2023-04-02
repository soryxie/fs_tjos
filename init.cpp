#include "sb.h"
#include "dinode.h"
#include <string>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

using namespace std;

const long FILE_SIZE = 256*2*512;
const long BLOCK_SIZE = 512;
const long INODE_NUM = 100;
const long INODE_SIZE = 64;
const long OFFSET_SUPERBLOCK = 0;
const long OFFSET_INODE = 2*BLOCK_SIZE;
const long OFFSET_DATA = OFFSET_INODE+INODE_NUM*INODE_SIZE;
const long DATA_SIZE = FILE_SIZE-OFFSET_DATA;
const long DATA_NUM = DATA_SIZE/BLOCK_SIZE;


SuperBlock sb;
DiskInode inode[INODE_NUM];
char data[DATA_SIZE];

void init_superblock()
{
    sb.s_isize = 12;
    sb.s_fsize = FILE_SIZE/BLOCK_SIZE;
    /* 初始化s_inode */
    for(int i=0; i<100&&i<INODE_NUM; i++)
        sb.s_inode[i] = i;
    /* 初始化data表 */
    int data_i = 0;
    /* 先填写superblock表的空闲表 */
    for(int i=0; i<100&&data_i<DATA_NUM; i++)
        sb.s_inode[i] = data_i++;         //
    
    /* 填写后续的空闲表 */
    int blkno = sb.s_inode[99];  //获取到接下来要填写的物理块号
    while(data_i < DATA_NUM) {
        cout << "in block:" << blkno << " "<< data_i <<endl;
        char *p = data + blkno*BLOCK_SIZE;
        int *table = (int *)p;      // table就是新的空闲表的区域
        if(DATA_NUM - data_i > 100)
            table[0] = 100;
        else 
            table[0] = DATA_NUM - data_i;
        
        for(int i=1; i<=table[0]; i++){
            table[i] = data_i++;
        }
        blkno = table[table[0]];
    }
}

void copy_data(char *addr, int size) {
    static int data_p = 0;
    if(size >= DATA_SIZE - data_p) 
        size = DATA_SIZE - data_p;
    memcpy(addr, data+data_p, sizeof(char)*size);
    data_p += size;
}

const int PAGE_SIZE = 4*1024; // mmap 限定4KB
void copy_first(char *addr) {
    memcpy(addr, &sb, sizeof(sb));
    memcpy(addr+OFFSET_INODE, &inode[0], sizeof(inode));
    copy_data(addr+OFFSET_DATA, 2*PAGE_SIZE - OFFSET_DATA);
}

void copy_subseq(char *addr) {
    copy_data(addr, PAGE_SIZE);
}


int map(int fd, int offset, int size, void(*fun)(char *)) {
    // 映射文件到内存中
    void *addr = mmap(NULL, size , PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    if (addr == MAP_FAILED) {
        std::cerr << "Failed to mmap superblock " << std::endl;
        close(fd);
        return -1;
    }

    fun((char *)addr);

    // 解除内存映射
    if (munmap(addr, size) < 0) {
        std::cerr << "Failed to munmap superblock " << std::endl;
        close(fd);
        return -1;
    }
    return size;
}


int main(int argc, char *argv[])
{
    const char* file_path = "myDisk.img";
    int fd;
    if(argc > 1) {
        fd = open(argv[1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    }
    else {
        fd = open(file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    }

    if (fd < 0) {
        cerr << "Failed to open file " << file_path << endl;
        return 1;
    }else {
        cout << "start map "<< file_path << endl;
    }



    // 调整文件大小
    if (ftruncate(fd, FILE_SIZE) < 0) {
        cerr << "Failed to truncate file " << file_path << endl;
        close(fd);
        return 1;
    }
    
    
    //初始化superblock
    init_superblock();

    
    
    int size = 0;
    /* 初次拷贝，superblock, inode ,部分data */
    size += map(fd, 0, 2*PAGE_SIZE, copy_first);

    /* 从剩余data部分开始 */
    int data_off = 2*PAGE_SIZE - OFFSET_DATA;
    for(; data_off+PAGE_SIZE < DATA_SIZE; data_off+=PAGE_SIZE)
        size += map(fd, size, PAGE_SIZE, copy_subseq);
    if(DATA_SIZE - data_off > 0)
        size += map(fd, size, DATA_SIZE - data_off, copy_subseq);



    // 关闭文件
    close(fd);

    cout << "map disk done~~" << endl;
    return 0;
}