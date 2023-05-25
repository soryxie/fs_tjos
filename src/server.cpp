#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <map>
#include <iterator>
#include "../include/fs.h"

using namespace std;

extern User login();

FileSystem fs("myDisk.img");

void handleClient(int clientSocket) {
    User user = login();
    user.set_current_dir(1);
    fs.set_u(&user);

    char buffer[1024];

    // 读取客户端发送的命令行字符串
    while(true){
        
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            // 解析命令
            string command(buffer);
            istringstream iss(command);
            string s;
            vector<string> tokens;
            while(iss){
                iss >> s;
                tokens.emplace_back(s);
            }

            //啥也没输入，跳过
            if (tokens.empty())        
                continue;

            string result;
            if(tokens[0] == "init"){
                //if(fs.initialize_from_external_directory(tokens[1]) == false) {
                if(fs.initialize_from_external_directory("test_folder") == false) {
                    result = "Initialize failed!\n";
                }
                else
                    result = "Initialize success!\n";
            }
            else if(tokens[0] == "ls"){
                if(tokens.size() > 2)
                    result = fs.ls(tokens[1]);
                else    
                    result = fs.ls("");
            }
            else if(tokens[0] == "cd"){
                if(fs.changeDir(tokens[1]) != FAIL)
                    result = "cd : success!\n";
            }
            else if(tokens[0] == "mkdir"){
                if(fs.createDir(user.current_dir_,tokens[1]) != FAIL)
                    result = "mkdir : success!\n";
            }
            else if(tokens[0] == "cat"){
                result = fs.cat(tokens[1]);
            }
            else if(tokens[0] == "rm"){
                if(fs.deleteFile(tokens[1]) != FAIL)
                    result = "rm : success!\n";
            }
            else if(tokens[0] == "cp"){
                if(fs.copyFile(tokens[1], tokens[2]) != FAIL)
                    result = "cp : success!\n";
            }
            else if(tokens[0] == "save"){
                if(fs.saveFile(tokens[1], tokens[2]) != FAIL)
                    result = "save : success!\n";
            }
            else if(tokens[0] == "export"){
                if(fs.exportFile(tokens[1], tokens[2]) != FAIL)
                    result = "export : success!\n";
            }
            else if(tokens[0] == "exit"){
                result = "exit!";
                write(clientSocket, result.c_str(), result.length());
                break;
            }
            // 发送结果给客户端
            write(clientSocket, result.c_str(), result.length());


            /* 
            else if(tokens[0] == "touch"){
                fs.createFile(tokens[1]);
            }
            else if(tokens[0] == "mv"){
                fs.moveFile(tokens[1], tokens[2]);
            }
            else if(tokens[0] == "rename"){
                fs.renameFile(tokens[1], tokens[2]);
            }
            else if(tokens[0] == "help"){
                fs.help();
            } */            
        }
    }
    close(clientSocket);  // 关闭客户端套接字
}

int main() {
    int serverSocket, newSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen;

    // 创建套接字
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Failed to create socket" << endl;
        return 1;
    }

    // 设置服务器地址
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8888);  // 指定服务器端口号

    // 绑定套接字到指定端口
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Failed to bind socket" << endl;
        return 1;
    }

    // 监听传入的连接
    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen on socket" << endl;
        return 1;
    }

    cout << "Server started. Listening on port 8888" << endl;

    while (true) {
        // 接受新的连接
        clientAddrLen = sizeof(clientAddr);
        newSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (newSocket < 0) {
            cerr << "Failed to accept connection" << endl;
            continue;
        }

        // 创建新线程处理客户端连接
        thread clientThread(handleClient, newSocket);
        clientThread.detach();  // 在后台运行线程，不阻塞主循环
    }

    close(serverSocket);  // 关闭服务器端的套接字

    return 0;
}

