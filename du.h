#ifndef DU_H_
#define DU_H_

#include <dirent.h>     // opendir, readdir, closedir, dirent
#include <errno.h>      // errno
#include <stdio.h>      // fprintf, printf, snprintf
#include <stdlib.h>     // EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>     // strerror, strcmp
#include <sys/stat.h>   // lstat, stat, S_IFMT, S_IFDIR, S_IFREG
#include <sys/types.h>  // ino_t
#include <unistd.h>     // getopt, optind

typedef struct DynamicArray {
  size_t size;
  size_t len;
  void *data;
} DynamicArray;

extern int optind;

const size_t kPathMax = 512;  // bytes
const int kMaxArgs = 3;

// Program-Specific Functions
int du(const char *rootpath, int include_files);
blkcnt_t dfs(const char *rootpath, DynamicArray *seen, int *error,
             int include_files);

// DynamicArray-Specific Functions
DynamicArray *InitDynamicArray(size_t size, size_t type_size);
void FreeDynamicArray(DynamicArray *da);
ino_t *SearchInode(DynamicArray *da, ino_t ino);
int InsertInode(DynamicArray *da, ino_t ino);

// Utility Functions
static inline void PrintUsage(const char *cmd);
static inline void PrintDiskUsage(blkcnt_t disk_usage, const char *path);

#endif  // DU_H_