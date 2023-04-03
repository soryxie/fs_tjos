#include "init_dir.h"
#include "sb.h"
#include "dinode.h"
#include "parameter.h"
#include "directory.h"

extern SuperBlock sb;
extern DiskInode inode[INODE_NUM];
extern char data[DATA_SIZE];


void create_file()
{
	;
}

void scan_path(string path) {
    DIR* pDIR = opendir((path + '/').c_str());
	dirent* pdirent = NULL;
	if (pDIR != NULL) {
		map<string, ostringstream> file_data; //文件名与数据的map
		while ((pdirent = readdir(pDIR)) != NULL) {
			string Name = pdirent->d_name;
			if (Name == "." || Name == "..")
				continue;
			struct stat buf;
			if (!lstat((path + '/' + Name).c_str(), &buf)) {
				if (S_ISDIR(buf.st_mode)) {
					scan_path(path + '/' + Name);
				}
				else if (S_ISREG(buf.st_mode)) {
					ifstream tmp;
					tmp.open(path + '/' + Name);
					file_data[Name] << tmp.rdbuf(); //数据流导入进file_data
					tmp.close();
				}
			}
		}
		cout << "在目录" << path << "下：" << endl;
		for(auto &f : file_data){
			cout << "导入文件:" << f.first << endl;
			cout<<"文件长度"<<f.second.str().length()<<endl;

		}
	}
	closedir(pDIR);
}

