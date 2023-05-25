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

FileSystem fs("myDisk.img");
User login(int clientSocket);

void handleClient(int clientSocket) {
    User user = login(clientSocket);
    user.set_current_dir(1);
    fs.set_u(&user);

    char buffer[1024];

    // 读取客户端发送的命令行字符串
    while(true){
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            string command = buffer;
            if(command == "exit"){
                write(clientSocket, "exit!", 6);
                break;
            }

            // fs 处理
            string result = fs.pCommand(user, command);

            // 发送结果给客户端
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, result.c_str());
            strcpy(buffer+result.length() + 1, user.get_current_dir_name().c_str());
            int len = result.length() + 1 + user.get_current_dir_name().length() + 1;
            write(clientSocket, buffer, len); 
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

// 验证用户名和密码是否正确
User verifyLogin(const string& username, const string& password) {
    vector<User> users;

    // 打开存储用户名和密码的文件
    ifstream file("users.txt");
    if (!file.is_open()) {
        return User(); // 返回一个空的 User 对象
    }

    // 读取文件中的用户信息
    while (!file.eof()) {
        User user;
        file >> user.uid >> user.username >> user.password >> user.group;
        users.push_back(user);
    }
    file.close();

    // 遍历用户列表，查找匹配的用户
    for (int i = 0; i < users.size(); i++) {
        if (users[i].username == username) {
            // 验证密码是否正确
            if (users[i].password == password) {
                return users[i]; // 返回包含用户信息的 User 对象
            }
            else {
                return User(); // 返回一个空的 User 对象表示密码错误
            }
        }
    }
    return User(); // 返回一个空的 User 对象表示用户名不存在
}

User login(int clientSocket){
    return verifyLogin("alice","123456");
    string username, password, reply;
    char buffer[1024];
    int stage = 0;

    while(true){
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            if(stage == 0) {
                username = buffer;
                reply = "Please enter your password: ";
                stage = 1;
            }
            else if(stage == 1) {
                password = buffer;
                // 验证登录信息
                User user = verifyLogin(username, password);
                if (user.username != "") {
                    reply = "Login successful";
                    write(clientSocket, reply.c_str(), reply.length());
                    return user;
                }
                else {
                    reply = "Incorrect username or password";
                    stage = 0;
                }
            }
            write(clientSocket, reply.c_str(), reply.length());
        }
    }
}