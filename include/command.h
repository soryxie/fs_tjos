#include <string>
struct Command {
    std::string name;
    void (*handler)();
    std::string help;
};
