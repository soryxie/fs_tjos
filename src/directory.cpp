#include "../include/directory.h"
#include <cstring>

DirectoryEntry::DirectoryEntry(int ino, const char *name, FileType type) {
    m_ino = ino;
    strcpy(m_name, name);
    m_type = type;
}