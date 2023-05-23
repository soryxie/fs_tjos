
// 用户信息
#include <string>
#include <vector>
struct OpenFile {
    int fd;           // 文件描述符
    int inum;         // inode号
    int mode;         // 打开方式
    int offset;       // 当前读写位置
};

struct User {
public:
    void set_current_dir(int inum) { current_dir_ = inum; }
    int get_current_dir() const { return current_dir_; }
    std::string get_current_dir_name() const { return current_dir_name; }

    /*void set_current_dir_name(std::string& path) 
    {
        // 重新解析Path
        std::vector<std::string> tokens;
        std::istringstream iss(path);
        std::string token;
        while (getline(iss, token, '/')) 
        {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }

        // 依次查找每一级目录或文件
        for (const auto& token : tokens)
        {  
            if (token == "..") 
            {
                if(current_dir_name !="/")
                {
                    size_t pos = current_dir_name.rfind('/');
                    if (pos != std::string::npos) 
                    {
                        current_dir_name = current_dir_name.substr(0, pos);
                    } 
                    else 
                    {
                        current_dir_name = "/";
                    }
                    if(current_dir_name == "")
                        current_dir_name = "/";
                }
                return;
            } 
            else if (token == ".") {}
            else
            {
                if (current_dir_name.back() != '/') 
                {
                    current_dir_name += '/';
                }
                current_dir_name += path;
            }
        }
    }*/



    void add_to_file_table(int fd, int inum, int mode) {
        //OpenFile file = { fd, inum, mode, 0 };
        //file_table_.push_back(file);
    }

    void remove_from_file_table(int fd) {
        /*for (auto it = file_table_.begin(); it != file_table_.end(); ++it) {
            if (it->fd == fd) {
                file_table_.erase(it);
                break;
            }
        }*/
    }

    int uid;
    std::string username;
    std::string password;
    int group;
//private:
    int current_dir_;
    std::string current_dir_name = "/";
    //std::vector<OpenFile> file_table_;
};