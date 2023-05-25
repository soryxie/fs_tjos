#include "../include/directory.h"
#include <cstring>

DirectoryEntry::DirectoryEntry(int ino, const char *name) {
    m_ino = ino;
    strcpy(m_name, name);
}