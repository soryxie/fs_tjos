# filesystem

先看看需要什么数据结构 ： 

+ SuperBlock ： 文件系统存储资源管理块，位于磁盘最前端

  ```c++
  int		s_isize;		/* 外存Inode区占用的盘块数 */
  int		s_fsize;		/* 盘块总数 */
  
  int		s_nfree;		/* 直接管理的空闲盘块数量 */
  int		s_free[100];	/* 直接管理的空闲盘块索引表 */
  
  int		s_ninode;		/* 直接管理的空闲外存Inode数量 */
  int		s_inode[100];	/* 直接管理的空闲外存Inode索引表 */
  
  int		s_flock;		/* 封锁空闲盘块索引表标志 */
  int		s_ilock;		/* 封锁空闲Inode表标志 */
  
  int		s_fmod;			/* 内存中super block副本被修改标志，意味着需要更新外存对应的Super Block */
  int		s_ronly;		/* 本文件系统只能读出 */
  int		s_time;			/* 最近一次更新时间 */
  int		padding[47];	/* 填充使SuperBlock块大小等于1024字节，占据2个扇区 */
  ```

+ Mount文件系统装配块 : 装配块用于实现子文件系统与根文件系统的连接

  ```c++
  short 		m_dev;		/* 文件系统设备号 */
  SuperBlock* m_spb;		/* 指向文件系统的Super Block对象在内存中的副本 */
  Inode*		m_inodep;	/* 指向挂载子文件系统的内存INode */
  ```

+ FileSystem : 文件系统类 -> 管理文件存储设备中的各类存储资源，磁盘块、外存INode的分配、释放

  ```c++
  //只有些常量，写死了, 列一些重点的
  
  /* 定义SuperBlock位于磁盘上的扇区号，占据200，201两个扇区。 */
  static const int SUPER_BLOCK_SECTOR_NUMBER = 200;	
  
  /* 文件系统根目录外存Inode编号 */
  static const int ROOTINO = 1;	
  
  /* 每个磁盘块可以存放512/64 = 8个外存Inode */
  static const int INODE_NUMBER_PER_SECTOR = 8;	
  /* 外存Inode区位于磁盘上的起始扇区号，前两个是superblock */
  static const int INODE_ZONE_START_SECTOR = 202;		
  static const int INODE_ZONE_SIZE = 1024 - 202;		
  
  /* 数据区 */
  static const int DATA_ZONE_START_SECTOR = 1024;	
  static const int DATA_ZONE_END_SECTOR = 18000 - 1;	
  static const int DATA_ZONE_SIZE = 18000 - DATA_ZONE_START_SECTOR;	
  ```

  









接下来看`filesystem`有什么重点的函数：

### 先讲一下short dev这个参数，在后续函数做参数经常出现

实际上就是多个存储设备挂载在系统上，需要有个编号，用来区分他们每个人不同的文件系统。

	/* 获取到superblock内存上的对象的方式 */
	SuperBlock* sb = this->GetFS(dev);



### Initalize 没意义，初始化锁和缓存



### LoadSuperBlock 系统初始化时从硬盘读入SuperBlock

```c++
void FileSystem::LoadSuperBlock()
{
	/* superblock 占两个数据块，因此要循环两边，最终读到了g_spb中 */
    /* gspb(global superblock) 就是系统在内存中专门给superblock留的区域 */
	for (int i = 0; i < 2; i++)
	{
		int* p = (int *)&g_spb + i * 128; 	//标记内存地址
		pBuf = bufMgr.Bread(DeviceManager::ROOTDEV, FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);
		Utility::DWordCopy((int *)pBuf->b_addr, p, 128);  //进缓存，复制
		bufMgr.Brelse(pBuf);				//释放缓存
	}

    /* 初始化一些内存中的信息，锁、时间等 */
	g_spb.s_flock = 0;
	g_spb.s_ilock = 0;
	g_spb.s_ronly = 0;
	g_spb.s_time = Time::time;
}
```



### GetFS 当有多个文件系统设备挂载，可以用它找到某一个superblock

```c++
SuperBlock* FileSystem::GetFS(short dev)
{
	SuperBlock* sb;
	...
	return sb;
}
```



### Update 将内存中的superblock 保存回硬盘中

```c++
void FileSystem::Update()
{
	/* 上锁了， 另一进程正在进行同步，则直接返回 */
	if(this->updlock) return;

	/* 拿到锁，上锁之后再执行逻辑 */
	this->updlock++;

	/* 同步SuperBlock到磁盘，假设只有一个文件系统设备 */
	
    SuperBlock* sb = this->m_Mount[i].m_spb;

    /* 如果该SuperBlock内存副本没有被修改，返回*/
	if(sb->s_fmod == 0) return;
    
    /* 清SuperBlock修改标志 */
    sb->s_fmod = 0;
    /* 写入SuperBlock最后存访时间 */
    sb->s_time = Time::time;

    /* 写回同样也是申请两个数据块的缓存，因而循环两次 */
    for(int j = 0; j < 2; j++)
    {
        int* p = (int *)sb + j * 128;
        /* 拿到磁盘中对应superblock位置的数据块 -> 对应的缓存 */
        pBuf = this->m_BufferManager->GetBlk(this->m_Mount[i].m_dev, FileSystem::SUPER_BLOCK_SECTOR_NUMBER + j);
        Utility::DWordCopy(p, (int *)pBuf->b_addr, 128);
        this->m_BufferManager->Bwrite(pBuf);   //标记写回
    }

	/* 清除Update()函数锁 */
	this->updlock = 0;

	/* 将刚才标记的superblock立刻同步到磁盘上 */
	this->m_BufferManager->Bflush(DeviceManager::NODEV);
}
```



### IAlloc 磁盘上分配一个空闲外存INode，一般用于创建新的文件。
```c++
Inode* FileSystem::IAlloc(short dev)
{
	SuperBlock* sb;
	Buf* pBuf;
	Inode* pNode;

	/* 获取到superblock内存上的对象的方式 */
	sb = this->GetFS(dev);

	/* 这个if表达的是：SuperBlock直接管理的空闲Inode索引表已空，必须到磁盘上搜索空闲Inode */
	if(sb->s_ninode <= 0)
	{
        int ino = -1;	/* 分配到的空闲外存Inode编号 */
		/* 依次读入磁盘中Inode区的磁盘块，搜索空闲Inode，记入空闲Inode索引表 */
		for(int i = 0; i < sb->s_isize; i++)
		{
			pBuf = this->m_BufferManager->Bread(dev, FileSystem::INODE_ZONE_START_SECTOR + i);

			/* 获取刚读到的缓冲区首地址 */
			int* p = (int *)pBuf->b_addr;

			/* 检查该缓冲区中每个Inode */
            /* i_mode != 0，表示已经被占用 */
			for(int j = 0; j < FileSystem::INODE_NUMBER_PER_SECTOR; j++)
			{
				ino++;
				int mode = *( p + j * sizeof(DiskInode)/sizeof(int) );
				if(mode != 0) continue;

                /* 该外存Inode没有对应的内存拷贝，将其记入空闲Inode索引表 */
                sb->s_inode[sb->s_ninode++] = ino;

                /* 直到空闲索引表装满，不继续搜索 */
                if(sb->s_ninode >= 100)
                    break;
			}

			/* 读完当前磁盘块，释放相应的缓存 */
			this->m_BufferManager->Brelse(pBuf);

			/* 如果空闲索引表已经装满，则不继续搜索 */
			if(sb->s_ninode >= 100)
				break;
		}
        
		/* 如果在磁盘上没有搜索到任何可用外存Inode，返回NULL */
		if(sb->s_ninode <= 0)
		{
			Diagnose::Write("No Space On %d !\n", dev);
			u.u_error = User::ENOSPC;
			return NULL;
		}
	}

	/* 现在，空闲Inode索引表中已经有可用的Inode了 */
	while(true)
	{
		/* 从索引表“栈顶”获取空闲外存Inode编号 */
		ino = sb->s_inode[--sb->s_ninode];

		/* 将空闲Inode读入内存 */
		pNode = g_InodeTable.IGet(dev, ino);

		/* 如果该Inode空闲, 表示已经找到了可用indoe，清空数据 */
		if(0 == pNode->i_mode)
		{
			pNode->Clean();
			/* 设置SuperBlock被修改标志 */
			sb->s_fmod = 1;
			return pNode;
		}
		else	/* 如果该Inode已被占用 */
		{
			g_InodeTable.IPut(pNode);
			continue;	/* while循环，直到拿到可用inode */
		}
	}
	return NULL;
}
```



### IFree 释放编号为number的外存INode，一般用于删除文件。
```c++
/* 仅完成登记到空闲表的逻辑 */
void FileSystem::IFree(short dev, int number)
{
	SuperBlock* sb;
    /* 获取到superblock内存上的对象的方式 */
	sb = this->GetFS(dev);	
	
	/* 锁逻辑，上锁就不登记到空闲表了 */

	/* 如果超级块直接管理的空闲外存Inode超过100个，同样不登记到空闲表了 */
	if(sb->s_ninode >= 100)  return;

	sb->s_inode[sb->s_ninode++] = number;

	/* 设置SuperBlock被修改标志 */
	sb->s_fmod = 1;
}
```



### Alloc 在磁盘上分配空闲磁盘块

```c++
/* 这里使用的是另一个空闲表，仅容纳空闲数据块，跟之前的inode空闲表不是一个 */
Buf* FileSystem::Alloc(short dev)
{
	int blkno;	/* 分配到的空闲磁盘块编号 */

	/* 获取到superblock内存上的对象的方式 */
	SuperBlock* sb = this->GetFS(dev);

	/* 锁逻辑 */

	/* 从索引表“栈顶”获取空闲磁盘块编号 */
	blkno = sb->s_free[--sb->s_nfree];

	/* 若获取磁盘块编号为零，则表示已分配尽所有的空闲磁盘块，直接失败 */
	if(0 == blkno)
	{
		sb->s_nfree = 0;
		Diagnose::Write("No Space On %d !\n", dev);
		u.u_error = User::ENOSPC;
		return NULL;
	}
    /* 若检查是badblock，直接失败(下一个函数将怎么判断) */
	if( this->BadBlock(sb, dev, blkno) )
		return NULL;

	/* 如果等于0，而且blkno是有效的，说明这张空闲表耗尽了，blkno记录了下一张空闲表的编号,*/
	if(sb->s_nfree <= 0)
	{
		/* 读入该空闲磁盘块 */
		pBuf = this->m_BufferManager->Bread(dev, blkno);

		/* 从该磁盘块的0字节开始记录 */
		int* p = (int *)pBuf->b_addr;

		/* 第0号元素记录了这张表的项数 s_nfree */
		sb->s_nfree = *p++;

		/* 读取缓存中的空闲表，放入s_free中 */
		Utility::DWordCopy(p, sb->s_free, 100);

		/* 缓存使用完毕，释放以便被其它进程使用 */
		this->m_BufferManager->Brelse(pBuf);
	}

	/* 此时blkno就是可用的数据块 */
	pBuf = this->m_BufferManager->GetBlk(dev, blkno);	/* 为该磁盘块申请缓存 */
	this->m_BufferManager->ClrBuf(pBuf);				/* 清空缓存中的数据 */
	sb->s_fmod = 1;										/* 设置SuperBlock被修改标志 */

	return pBuf;
}
```



### Free 释放硬盘上编号为blkno的磁盘块

```c++
void FileSystem::Free(short dev, int blkno)
{
	/* 获取到superblock内存上的对象的方式 */
	SuperBlock* sb = this->GetFS(dev);

	/* 设置SuperBlock被修改标志，保证原子的删除操作*/
	sb->s_fmod = 1;

	/* 检查释放磁盘块的合法性，下一个函数讲badblock */
	if(this->BadBlock(sb, dev, blkno))
		return;

	/* 空闲表是空的，登记为当前第一个空闲盘块 */
	if(sb->s_nfree <= 0)
	{
		sb->s_nfree = 1;
		sb->s_free[0] = 0;	/* 使用0标记空闲盘块链结束标志 */
	}
    /* 正常情况下，直接压栈 */
    else if(sb->s_nfree < 100)
    {
        sb->s_free[sb->s_nfree++] = blkno;	/* SuperBlock中记录下当前释放盘块号 */
        sb->s_fmod = 1;
    }
    /* 当前空闲表已满100，准备开新的 */
    else
    {
		/* 使用当前正要释放的磁盘块blkno: 存放前一组100个空闲表 */
		pBuf = this->m_BufferManager->GetBlk(dev, blkno);

		/* 拿到blkno在磁盘的缓存首地址 */
		int* p = (int *)pBuf->b_addr;
		
		/* 首先写入空闲盘块数 */
		*p++ = sb->s_nfree;
		/* 将s_free[100]写入缓存中 */
		Utility::DWordCopy(sb->s_free, p, 100);

		sb->s_nfree = 0;
		/* 将存这个缓存中的空闲表写入磁盘 */
		this->m_BufferManager->Bwrite(pBuf);
	}
}
```



### BadBlock 检查blkno的磁盘块是否属于数据盘块区，啥也没干



