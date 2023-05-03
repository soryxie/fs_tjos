#include "../include/login.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
using namespace std;

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
        file >> user.username >> user.password >> user.group;
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

User login(){
    string username, password;

    while(true){
        // 提示用户输入用户名和密码
        cout << "Username: ";
        getline(cin, username);
        cout << "Password: ";
        getline(cin, password);

        // 验证登录信息
        User user = verifyLogin(username, password);
        if (user.username != "") {
            cout << "Login successful" << endl;
            return user;
        }
        else {
            cout << "Incorrect username or password" << endl;
        }
    }
}
