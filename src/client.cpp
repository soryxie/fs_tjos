#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include "../include/fs.h"

using namespace std;

extern User login();
/*
template<typename... Args>
struct CommandInfo {
    using HandlerFn = void(*)(Args...);
    HandlerFn Handler;
    int num_args;
    std::string help;

    // 构造函数，用于初始化 CommandInfo
    CommandInfo(HandlerFn handler, int num_args, const std::string& help_info)
        : Handler(handler), num_args(num_args), help(help) {}
};

map<std::string, CommandInfo <string> > cmd;

void init_command() {
    cmd["ls"] = {FileSystem::listDir, 0, "ls"};
    cmd["mkdir"] = {FileSystem::createDir, 1, "mkdir <dirname>"};
    cmd["cd"] = {FileSystem::changeDir, 1, "cd <dirname>"};
    cmd["touch"] = {FileSystem::createFile, 1, "touch <filename>"};
    cmd["rm"] = {FileSystem::deleteFile, 1, "rm <filename>"};
    cmd["mv"] = {FileSystem::moveFile, 2, "mv <src> <dst>"};
    cmd["cp"] = {FileSystem::copyFile, 2, "cp <src> <dst>"};
    cmd["cat"] = {FileSystem::readFile, 1, "cat <filename>"};
    cmd["write"] = {FileSystem::writeFile, 1, "write <filename>"};
    cmd["rename"] = {FileSystem::renameFile, 2, "rename <filename> <newname>"};
    cmd["rm"] = {FileSystem::deleteFile, 1, "rm <filename>"};
    cmd["exit"] = {FileSystem::exit, 0, "exit"};
    cmd["help"] = {FileSystem::help, 0, "help"};
    cmd["init"] = CommandInfo <string> (FileSystem::initialize_from_external_directory, 1, "init <external_root_path>");
}
*/
FileSystem fs("/root/fs/myDisk.img");
int main() {
    User user = login();
    user.set_current_dir(1);
    fs.set_u(&user);
    


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
        }
        if (tokens.empty()) {
            continue;
        }

        if(tokens[0] == "init"){
            //if(fs.initialize_from_external_directory(tokens[1]) == false) {
            if(fs.initialize_from_external_directory("/root/fs/test_folder") == false) {
                cout << "Initialize failed!" << endl;
            }
        }
        else if(tokens[0] == "ls"){
            if(tokens.size() > 2)
                fs.ls(tokens[1]);
            else    
                fs.ls("");
        }
        /* else if(tokens[0] == "mkdir"){
            fs.createDir(tokens[1]);
        }
        else if(tokens[0] == "cd"){
            fs.changeDir(tokens[1]);
        }
        else if(tokens[0] == "touch"){
            fs.createFile(tokens[1]);
        }
        else if(tokens[0] == "rm"){
            fs.deleteFile(tokens[1]);
        }
        else if(tokens[0] == "mv"){
            fs.moveFile(tokens[1], tokens[2]);
        }
        else if(tokens[0] == "cp"){
            fs.copyFile(tokens[1], tokens[2]);
        }
        else if(tokens[0] == "cat"){
            fs.readFile(tokens[1]);
        }
        else if(tokens[0] == "write"){
            fs.writeFile(tokens[1]);
        }
        else if(tokens[0] == "rename"){
            fs.renameFile(tokens[1], tokens[2]);
        }
        else if(tokens[0] == "rm"){
            fs.deleteFile(tokens[1]);
        }
        else if(tokens[0] == "exit"){
            fs.exit();
            break;
        }
        else if(tokens[0] == "help"){
            fs.help();
        }*/


        
        // 在命令列表中查找对应的命令
        /*const auto& command_info = cmd.find(tokens[0]);
        if (command_info != cmd.end()) {
            const auto& command = command_info->second;
            // 检查命令参数数量是否正确
            if (tokens.size() - 1 != command.num_args) {
                std::cout << "Invalid number of arguments. Usage: " << command.help_info << std::endl;
                continue;
            }
            // 执行命令
            if (command.num_args == 0) {
                (fs.*command.handler)();
            } else if (command.num_args == 1) {
                (fs.*command.handler)(tokens[1]);
            } else if (command.num_args == 2) {
                (fs.*command.handler)(tokens[1], tokens[2]);
            } else if (command.num_args == 3) {
                (fs.*command.handler)(tokens[1], tokens[2], tokens[3]);
            }
        } else {
            std::cout << "Invalid command." << std::endl;
        }*/
    }

    fs.~FileSystem();
    return 0;
}

