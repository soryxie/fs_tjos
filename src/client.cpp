#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include "../include/user.h"
#include "../include/command.h"
#include "../include/fs.h"

using namespace std;

extern User login();

Command commands[] = {
    //{"ls", &listFiles},
    //{"mkdir", &makeDirectory},
    //{"cd", &changeDirectory},
    // 更多命令可以在这里添加
};

int main() {
    User user = login();

    FileSystem fs("myDisk.img");
    cout<<"inum="<<fs.sb.s_ninode<<endl;




    while (true) {
        // 读取用户输入的命令
        string input;
        cout << ">> ";
        std::getline(std::cin, input);

        // 解析命令
        std::istringstream iss(input);
        string s;
        vector<std::string> tokens;
        while(iss){
            iss >> s;
            tokens.emplace_back(s);
            cout<<s;
        }
        if (tokens.empty()) {
            continue;
        }

        // 在命令列表中查找对应的命令
        bool found = false;
        for (const auto& command : commands) {
            if (command.name == tokens[0]) {
                // 执行命令
                command.handler();
                found = true;
                break;
            }
        }

        if (!found) {
            std::cout << "Invalid command." << std::endl;
        }
    }


    fs.~FileSystem();
    return 0;
}
