# Inode

### 补充 ： User::IOParameter 是inode::readi/writei的参数

```c++
/*
 * 文件I/O的参数类
 * 对文件读、写时需用到的读、写偏移量、
 * 字节数以及目标区域首地址参数。
 */
class IOParameter
{
	unsigned char* m_Base;	/* 当前读、写用户目标区域的首地址 */
	int m_Offset;	/* 当前读、写文件的字节偏移量 */
	int m_Count;	/* 当前还剩余的读、写字节数量 */
};
```



### readI() 与 user的参数配合，读取文件到指定区域

```c++
/* 涉及到预读，相关元素为 ：Inode::rablock -> 预读块号 inode::i_lastr -> 上次读的块号*/

void Inode::ReadI()
{
	int lbn;	/* 文件逻辑块号 */
	int bn;		/* lbn对应的物理盘块号 */
	int offset;	/* 当前字符块内起始偏移量 */
	int nbytes;	/* 用户目标区字节数量 */
	short dev;

	/* 需要读字节数为零，则返回 */
    
	/* 标记Flag为”更新访问时间“ */

	/* 如果是字符设备文件（不是文件系统中正常的二进制文件） ，调用外设读函数*/

	/* 如果是二进制文件（数据块），一次一个块地读入所需全部数据，直至遇到文件尾 */
	while( User::NOERROR == u.u_error && u.u_IOParam.m_Count != 0)
	{
        /* 根据偏移量寻找起始的块号和块内偏移 */
		lbn = bn = u.u_IOParam.m_Offset / Inode::BLOCK_SIZE; 
		offset = u.u_IOParam.m_Offset % Inode::BLOCK_SIZE;
        
		/* 计算本块内需要复制的字节数量（是否要读到块结尾） */
		nbytes = min(BLOCK_SIZE - offset , u.u_IOParam.m_Count);

		/* 如果是普通块设备文件 */
		if( (this->i_mode & Inode::IFMT) != Inode::IFBLK )
		{	
            /* 更新nbytes : 是否已读到文件结尾，上一个nbytes是检查是否读到块结尾 */
            /* i_size 是inode对应的整个文件大小 */
			int remain = this->i_size - u.u_IOParam.m_Offset;
			if( remain <= 0) return;
			nbytes = Utility::Min(nbytes, remain);
            
			/* 根据逻辑块号lbn找到物理盘块号bn，细节见下一个函数 */
			if( (bn = this->Bmap(lbn)) == 0 ) return;
			dev = this->i_dev;
		}

		/* 预读，相关逻辑细节都在外设dev中，这里理解行为就好 */        
		if( this->i_lastr + 1 == lbn )	/* 如果是顺序读，则进行预读 */
			/* 读当前块，并预读下一块 */
			pBuf = bufMgr.Breada(dev, bn, Inode::rablock);
		else
            /* 正常载入 */
			pBuf = bufMgr.Bread(dev, bn);
        /* 返回的是一个缓存块pBuf，服务于接下来的缓存存移到用户指定位置 */
        
		/* 记录为上次读块号 */
		this->i_lastr = lbn;

		/* 缓存中数据起始读位置 */
		unsigned char* start = pBuf->b_addr + offset;
		
		/* 读操作: 从缓冲区拷贝到用户目标区 */
		Utility::IOMove(start, u.u_IOParam.m_Base, nbytes);

		/* 用传送字节数nbytes更新读写位置 */
		u.u_IOParam.m_Base += nbytes;
		u.u_IOParam.m_Offset += nbytes;
		u.u_IOParam.m_Count -= nbytes;

        /* 使用完缓存，释放该资源 */
		bufMgr.Brelse(pBuf);	
	}
}
```



### bmap 服务于readi/writei, 逻辑块号->物理块号

每个inode中存放的数据块分布如下：

```c++
/* 
* Unix V6++的文件索引结构：(小型、大型和巨型文件)
* (1) i_addr[0] - i_addr[5]为直接索引表，文件长度范围是0 - 6个盘块；
* 
* (2) i_addr[6] - i_addr[7]存放一次间接索引表所在磁盘块号，每磁盘块
* 上存放128个文件数据盘块号，此类文件长度范围是7 - (128 * 2 + 6)个盘块；
*
* (3) i_addr[8] - i_addr[9]存放二次间接索引表所在磁盘块号，每个二次间接
* 索引表记录128个一次间接索引表所在磁盘块号，此类文件长度范围是
* (128 * 2 + 6 ) < size <= (128 * 128 * 2 + 128 * 2 + 6)
*/
```

所谓逻辑块号实际上就是一个索引，告诉你他是该文件inode中第几个数据块，

从iaddr出发最终找到的都是物理块号，也就是能在磁盘中直接按地址访问到的区域

**代码：**

```c++
int Inode::Bmap(int lbn)
{
	Buf* pFirstBuf;
	Buf* pSecondBuf;
	int phyBlkno;	/* 转换后的物理盘块号 */
	int* iTable;	/* 用于访问索引盘块中一次间接、两次间接索引表 */
	int index;
	
    /* 给的逻辑块号超过了inode能存放的数据块上限，报错 */
	if(lbn >= Inode::HUGE_FILE_BLOCK) ...

/* 如果是小型文件，从基本索引表i_addr[0-5]中获得物理盘块号即可 */
	if(lbn < 6)		
	{
		phyBlkno = this->i_addr[lbn];

		/* 如果该逻辑块号（索引）下没有物理块，则分配一个 */
		if( phyBlkno == 0 && (pFirstBuf = fileSys.Alloc(this->i_dev)) != NULL )
		{
			/* 不急于立刻写进到磁盘；而是将缓存标记为延迟写方式，这样可以减少系统的I/O操作 */
			bufMgr.Bdwrite(pFirstBuf);
			phyBlkno = pFirstBuf->b_blkno;
            
			/* 申请到之后，登记 */
			this->i_addr[lbn] = phyBlkno;
			this->i_flag |= Inode::IUPD;
		}
        
		/* 找到预读块对应的物理盘块号 */
		Inode::rablock = 0;
		if(lbn <= 4)
		{
			/* 将预读限制在了iaddr[0-5]区间，因为之后的预读需要先读间接索引表（新数据块）*/
			Inode::rablock = this->i_addr[lbn + 1];
		}

		return phyBlkno;
	}

/* 系统中最常见的小文件的逻辑已经结束了，下面就是略显复杂的大文件的物理块寻找过程 */
	else	
	{
/* 计算过程无非就是计算他在 次级/三级索引表上的位置 */
/* 大型文件: 长度介于7 - (128 * 2 + 6)个盘块之间 */
		if(lbn < Inode::LARGE_FILE_BLOCK)	
			index = (lbn - Inode::SMALL_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK + 6;
/* 巨型文件: 长度介于263 - (128 * 128 * 2 + 128 * 2 + 6)个盘块之间 */
		else	
			index = (lbn - Inode::LARGE_FILE_BLOCK) / (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK) + 8;

        
       	/* 拿到间接索引表数据块的物理页号 */
		phyBlkno = this->i_addr[index];
        
		/* 若该项为零，则表示不存在相应的间接索引表块 */
		if( 0 == phyBlkno )
		{
			this->i_flag |= Inode::IUPD;
			fileSys.Alloc(this->i_dev)  /* 分配一空闲盘块存放间接索引表 */
			this->i_addr[index] = pFirstBuf->b_blkno; /* 登记物理盘块号 */
		}
        /* 已存在。读出存储间接索引表的字符块 */
		else
			pFirstBuf = bufMgr.Bread(this->i_dev, phyBlkno);
        
		/* 获取缓冲区首址 */
		iTable = (int *)pFirstBuf->b_addr;

		if(index >= 8)	/* ASSERT: 8 <= index <= 9 */
		{
			/*  对于巨型文件的情况，重复一次类似次级索引表的处理逻辑，此处省略 */
			...
			iTable = (int *)pSecondBuf->b_addr;
		}
        
		/* 计算最终在这张索引表上的索引 */
		
        /* 是否需要开辟新的物理块 */
        
		/* 释放间接索引表占用缓存 */
        
		/* 找到预读块对应的物理盘块号，如果获取预读块号需要额外的一次for间接索引块的IO，不合算，放弃 */
		Inode::rablock = 0;
		if( index + 1 < Inode::ADDRESS_PER_INDEX_BLOCK)
		{
			Inode::rablock = iTable[index + 1];
		}
        
		return phyBlkno;
	}
}
```



### OpenI 和 CloseI 属于特殊设备处理逻辑，此处略过，为了掌握fs的整体行为



### IUpdate 更新inode数据，最后访问，修改时间

```c++
void Inode::IUpdate(int time)
{
	Buf* pBuf;
	DiskInode dInode;

	/* 标志位检查 */
	if( (this->i_flag & (Inode::IUPD | Inode::IACC))!= 0 )
	{
        /* 如果该文件系统只读 */
		if( filesys.GetFS(this->i_dev)->s_ronly != 0 )return;

		/* Inode在系统初始化的时候都被读进了缓存块中，直接修改这部分的数据，之后会写回磁盘 */
        /* 实现了修改磁盘中inode的数据 */
		pBuf = bufMgr.Bread(this->i_dev, FileSystem::INODE_ZONE_START_SECTOR + this->i_number / FileSystem::INODE_NUMBER_PER_SECTOR);

		/* 把系统中修改好的数据保存进一个临时的inode对象 */
		/* 包括一切信息，和最新的时间信息*/

		/* p指向缓存区中磁盘inode对应的缓存区地址 */
		unsigned char* p = pBuf->b_addr + (this->i_number % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
		DiskInode* pNode = &dInode;

		/* 用dInode中的新数据覆盖缓存中的旧外存Inode */
		Utility::DWordCopy( (int *)pNode, (int *)p, sizeof(DiskInode)/sizeof(int) );

		/* 将缓存写回至磁盘，达到更新旧外存Inode的目的 */
		bufMgr.Bwrite(pBuf);
	}
}
```



### ITeunc 释放inode对应文件占用的磁盘块

```c++
/* 采用FILO方式释放，以尽量使得SuperBlock中记录的空闲盘块号连续。
   翻译，逆向+递归删除*/
```

**代码**

```c++
void Inode::ITrunc()
{
    /* 从i_addr[9]到i_addr[0],逆向释放，使得登记在 */
	for(int i = 9; i >= 0; i--)		
	{
		/* 如果i_addr[]中第i项存在索引 */

        /* 如果是i_addr[]中的一次间接、两次间接索引项 */
        if( i >= 6 && i <= 9 )
        {
            /* 将间接索引表读入缓存 */
            Buf* pFirstBuf = bm.Bread(this->i_dev, this->i_addr[i]);
            /* 获取缓冲区首址 */
            int* pFirst = (int *)pFirstBuf->b_addr;

            /* 每张间接索引表记录 512/sizeof(int) = 128个磁盘块号，遍历这全部128个磁盘块 */
            for(int j = 128 - 1; j >= 0; j--)
            {
                if( pFirst[j] != 0)	/* 如果该项存在索引 */
                {
                    /* 如果是两次间接索引表，i_addr[8]或i_addr[9]项 */
					/* 还得再缓存一层索引表，清空内容 */
                    if( i >= 8 && i <= 9){}
                    filesys.Free(this->i_dev, pFirst[j]);
                }
            }
            bm.Brelse(pFirstBuf);
        }
        /* 释放索引表本身占用的磁盘块 */
        filesys.Free(this->i_dev, this->i_addr[i]);
        /* 0表示该项不包含索引 */
        this->i_addr[i] = 0;
	}
	
	/* 盘块释放完毕，文件大小清零 */
	this->i_size = 0;
	/* 增设IUPD标志位，表示此内存Inode需要同步到相应外存Inode */
	this->i_flag |= Inode::IUPD;
	/* 清大文件标志 和原来的RWXRWXRWX比特*/
	this->i_mode &= ~(Inode::ILARG & Inode::IRWXU & Inode::IRWXG & Inode::IRWXO);
	this->i_nlink = 1;
}
```



### Prele,Plock,NFrele,NFlock 都是锁逻辑，暂时不看



### Clean

```c++
/* 用于IAlloc()中清空新分配 DiskInode的原有数据， */
void Inode::Clean()
{
	this->i_mode = 0;
	this->i_nlink = 0;
	this->i_uid = -1;
	this->i_gid = -1;
	this->i_size = 0;
	this->i_lastr = -1;
	for(int i = 0; i < 10; i++)
		this->i_addr[i] = 0;
}
```



### ICopy

```c++
/* 将外存Inode信息 拷贝给自己(Inode) */
/* 给定缓存块和编号 */
void Inode::ICopy(Buf *bp, int inumber)
{
	DiskInode dInode;
	DiskInode* pNode = &dInode;

	/* 将p指向缓存区中编号为inumber外存Inode的偏移位置 */
	unsigned char* p = bp->b_addr + (inumber % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
	/* 将缓存中外存Inode数据拷贝到临时变量dInode中，按4字节拷贝 */
	Utility::DWordCopy( (int *)p, (int *)pNode, sizeof(DiskInode)/sizeof(int) );

	/* 将外存Inode变量dInode中信息复制到内存Inode中 */
	this->i_mode = dInode.d_mode;
	this->i_nlink = dInode.d_nlink;
	this->i_uid = dInode.d_uid;
	this->i_gid = dInode.d_gid;
	this->i_size = dInode.d_size;
	for(int i = 0; i < 10; i++)
		this->i_addr[i] = dInode.d_addr[i];
}
```

