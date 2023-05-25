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
FileSystem fs("myDisk.img");
int main() {
    User user = login();
    user.set_current_dir(1);
    fs.set_u(&user);

    while (true) {
        // 读取用户输入的命令
        string input;
        cout << "$ "<< user.get_current_dir_name() << ">>";
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
            if(fs.initialize_from_external_directory("test_folder") == false) {
                cout << "Initialize failed!" << endl;
            }
        }
        else if(tokens[0] == "ls"){
            if(tokens.size() > 2)
                fs.ls(tokens[1]);
            else    
                fs.ls("");
        }
        else if(tokens[0] == "cd"){
            fs.changeDir(tokens[1]);
        }
        else if(tokens[0] == "mkdir"){
            fs.createDir(user.current_dir_,tokens[1]);
        }
        else if(tokens[0] == "cat"){
            fs.cat(tokens[1]);
        }
        else if(tokens[0] == "rm"){
            fs.deleteFile(tokens[1]);
        }
        else if(tokens[0] == "cp"){
            fs.copyFile(tokens[1], tokens[2]);
        }
        else if(tokens[0] == "save"){
            fs.saveFile(tokens[1], tokens[2]);
        }
        else if(tokens[0] == "export"){
            fs.exportFile(tokens[1], tokens[2]);
        }       
        else if(tokens[0] == "mv"){
            fs.moveFile(tokens[1], tokens[2]);
        }
        else if(tokens[0] == "exit"){
            break;
        }
 
        /* 
        else if(tokens[0] == "touch"){
            fs.createFile(tokens[1]);
        }
        else if(tokens[0] == "help"){
            fs.help();
        } */
    }
    return 0;
}

