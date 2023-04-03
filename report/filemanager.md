# filemanager

### 数据结构

+ FileManager

  ```c++
  /* 根目录内存Inode */
  Inode* rootDirInode;
  
  /* 对全局对象g_FileSystem的引用，该对象负责管理文件系统存储资源 */
  FileSystem* m_FileSystem;
  
  /* 对全局对象g_InodeTable的引用，该对象负责内存Inode表的管理 */
  InodeTable* m_InodeTable;
  
  /* 对全局对象g_OpenFileTable的引用，该对象负责打开文件表项的管理 */
  OpenFileTable* m_OpenFileTable;
  ```

+ DirectoryEntry

  ```c++
  static const int DIRSIZ = 28;	/* 目录项中路径部分的最大字符串长度 */
  int m_ino;						/* 目录项中Inode编号部分 */
  char m_name[DIRSIZ];			/* 目录项中路径名部分 */
  ```



### DirectoryEntry 目录对象初始化

```c++
this->m_ino = 0;
this->m_name[0] = '\0';
```



### Initialize 初始化对全局对象的引用, 没什么意思



## 一开始主要讲一些跟初始化有关的----目录相关的函数



### NameI 目录搜索，将路径转化为相应的Inode，返回上锁后的Inode

```c++
/* 第一个参数func实际上就是路径名，只不过他包装成了函数：每次只返回一个字符（接着上次返回的位置） */
/* 这个逻辑便于按路劲递进地访问目录结构（每次只读取一段路径名） */
Inode* FileManager::NameI( char (*func)(), enum DirectorySearchMode mode )
{
	
	Buf* pBuf;
	char curchar;
	char* pChar;
	int freeEntryOffset;	/* 以创建文件模式搜索目录时，记录空闲目录项的偏移量 */

    
    Inode* pInode; /* 这是最核心的元素，指向当前搜索的inode号（无论是目录还是文件） */
    
	/* 如果该路径是'/'开头的，从根目录开始搜索，否则从进程当前工作目录开始搜索。 */
	pInode = u.u_cdir;
	if ( '/' == (curchar = (*func)()) )
		pInode = this->rootDirInode;

	/* 允许出现////a//b 这种路径 这种路径等价于/a/b */
	while ( '/' == curchar )
		curchar = (*func)();	//每次都向后读一个字符
    
	/* 如果试图更改和删除'/'目录，则出错 */
	if ( '\0' == curchar && mode != FileManager::OPEN )
	{
		u.u_error = User::ENOENT;
		goto out;
	}

	/* 每次处理pathname中一段路径 */
	while (true)
	{
		/* 整个路径搜索完毕，返回相应Inode指针。目录搜索成功返回。 */
		if ( '\0' == curchar ) return pInode;

		/* 如果要进行搜索的不是目录文件，出错，释放相关Inode资源则退出 */
		if ( (pInode->i_mode & Inode::IFMT) != Inode::IFDIR )
		{
			u.u_error = User::ENOTDIR;
			break;	
		}

		/* 进行目录搜索权限检查,IEXEC在目录文件中表示搜索权限 */
		if ( this->Access(pInode, Inode::IEXEC) )
		{
			u.u_error = User::EACCES;
			break;	/* 不具备目录搜索权限 */
		}

        /* u.u_dbuf[]存放当前将要匹配的文件名字（目录、文件） */
        /* 取两个'/'之间的内容 */
		pChar = &(u.u_dbuf[0]);  //指针从头开始覆盖
		while ( '/' != curchar && '\0' != curchar && u.u_error == User::NOERROR )
		{
			if ( pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]) )
			{
				*pChar = curchar;
				pChar++;
			}
			curchar = (*func)();
		}
		/* 将u_dbuf剩余的部分填充为'\0' */
		while ( pChar < &(u.u_dbuf[DirectoryEntry::DIRSIZ]) )
		{
			*pChar = '\0';
			pChar++;
		}

		/* 允许出现////a//b 这种路径 读取多余的'/ */
		while ( '/' == curchar )
			curchar = (*func)();

		/* 设置参数，准备把目录文件读进来 */
		u.u_IOParam.m_Offset = 0;
		u.u_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4); 
		freeEntryOffset = 0;
		pBuf = NULL;

        /* 对u.u_dbuf[]中存储的路径名分量，逐个搜寻匹配的目录项 */
		while (true)
		{
			/* 如果对目录项已经搜索完毕 */
			if ( 0 == u.u_IOParam.m_Count )
			{
                /* 释放缓存 */
				if ( NULL != pBuf )
					bufMgr.Brelse(pBuf);
                
				/* 如果是创建新文件 */
				if ( FileManager::CREATE == mode && curchar == '\0' )
				{
					/* 判断该目录是否可写 */

					/* 将父目录Inode指针保存起来，以后写目录项WriteDir()函数会用到 */
					u.u_pdir = pInode;

					if ( freeEntryOffset )	/* 此变量存放了空闲目录项位于目录文件中的偏移量 */
					{
						/* 将空闲目录项偏移量存入u区中，写目录项WriteDir()会用到 */
						u.u_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);
					}
					else  /*问题：为何if分支没有置IUPD标志？  这是因为文件的长度没有变呀*/
					{
						pInode->i_flag |= Inode::IUPD;
					}
					/* 找到可以写入的空闲目录项位置，NameI()函数返回 */
					return NULL;
				}
				
				/* 目录项搜索完毕而没有找到匹配项，释放相关Inode资源，并推出 */
				u.u_error = User::ENOENT;
				goto out;
			}

			/* 已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
			if ( 0 == u.u_IOParam.m_Offset % Inode::BLOCK_SIZE )
			{
				if ( NULL != pBuf )
				{
					bufMgr.Brelse(pBuf);
				}
				/* 计算要读的物理盘块号 */
				int phyBlkno = pInode->Bmap(u.u_IOParam.m_Offset / Inode::BLOCK_SIZE );
				pBuf = bufMgr.Bread(pInode->i_dev, phyBlkno );
			}

			/* 没有读完当前目录项盘块，则读取下一目录项至u.u_dent */
			int* src = (int *)(pBuf->b_addr + (u.u_IOParam.m_Offset % Inode::BLOCK_SIZE));
			Utility::DWordCopy( src, (int *)&u.u_dent, sizeof(DirectoryEntry)/sizeof(int) );

			u.u_IOParam.m_Offset += (DirectoryEntry::DIRSIZ + 4);
			u.u_IOParam.m_Count--;

			/* 如果是空闲目录项，记录该项位于目录文件中偏移量 */
			if ( 0 == u.u_dent.m_ino )
			{
				if ( 0 == freeEntryOffset )
				{
					freeEntryOffset = u.u_IOParam.m_Offset;
				}
				/* 跳过空闲目录项，继续比较下一目录项 */
				continue;
			}

			int i;
			for ( i = 0; i < DirectoryEntry::DIRSIZ; i++ )
			{
				if ( u.u_dbuf[i] != u.u_dent.m_name[i] )
				{
					break;	/* 匹配至某一字符不符，跳出for循环 */
				}
			}

			if( i < DirectoryEntry::DIRSIZ )
			{
				/* 不是要搜索的目录项，继续匹配下一目录项 */
				continue;
			}
			else
			{
				/* 目录项匹配成功，回到外层While(true)循环 */
				break;
			}
		}

		/* 
		 * 从内层目录项匹配循环跳至此处，说明pathname中
		 * 当前路径分量匹配成功了，还需匹配pathname中下一路径
		 * 分量，直至遇到'\0'结束。
		 */
		if ( NULL != pBuf )
		{
			bufMgr.Brelse(pBuf);
		}

		/* 如果是删除操作，则返回父目录Inode，而要删除文件的Inode号在u.u_dent.m_ino中 */
		if ( FileManager::DELETE == mode && '\0' == curchar )
		{
			/* 如果对父目录没有写的权限 */
			if ( this->Access(pInode, Inode::IWRITE) )
			{
				u.u_error = User::EACCES;
				break;	/* goto out; */
			}
			return pInode;
		}

		/* 
		 * 匹配目录项成功，则释放当前目录Inode，根据匹配成功的
		 * 目录项m_ino字段获取相应下一级目录或文件的Inode。
		 */
		short dev = pInode->i_dev;
		this->m_InodeTable->IPut(pInode);
		pInode = this->m_InodeTable->IGet(dev, u.u_dent.m_ino);
		/* 回到外层While(true)循环，继续匹配Pathname中下一路径分量 */

		if ( NULL == pInode )	/* 获取失败 */
		{
			return NULL;
		}
	}
out:
	this->m_InodeTable->IPut(pInode);
	return NULL;
}
```

