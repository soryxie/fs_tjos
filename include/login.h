#ifndef LOGIN_H
#define LOGIN_H

// 用户信息
#include <string>
struct User {
    std::string username;
    std::string password;
    int group;
};

// 验证用户名和密码是否正确
User login();
#endif // LOGIN_H
