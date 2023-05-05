const long FILE_SIZE = 32*1024*2*512;
const long BLOCK_SIZE = 512;
const long INODE_NUM = 100;
const long INODE_SIZE = 64;
const long OFFSET_SUPERBLOCK = 0;
const long OFFSET_INODE = 2*BLOCK_SIZE;
const long OFFSET_DATA = OFFSET_INODE+INODE_NUM*INODE_SIZE;
const long DATA_SIZE = FILE_SIZE-OFFSET_DATA;
const long DATA_NUM = DATA_SIZE/BLOCK_SIZE;
const long ENTRY_SIZE = 32;
const long ENTRYS_PER_BLOCK = BLOCK_SIZE/ENTRY_SIZE;
const long MAX_FILE_SIZE = 6 + 128*2 + 128*128*2;
const long ROOT_INO = 2;  // ¸ùÄ¿Â¼µÄinodeºÅ

typedef unsigned int node_num;
typedef unsigned int block_num;
typedef char buffer;