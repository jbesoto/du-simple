// du.c
//
// Author: Juan Diego Becerra (jdb9056@nyu.edu)
// Brief:  Basic implementation of a disk usage reporting tool similar to 'du'
//         command. Supports the `-a` option to include files in the usage
//         report, not just directories.

#include "du.h"

// Parses command-line arguments and calls the `du` function accordingly.
int main(int argc, char *argv[]) {
  if (argc > kMaxArgs) {
    PrintUsage(argv[0]);
    return EXIT_FAILURE;
  }

  int include_files = 0;
  int opt;
  while ((opt = getopt(argc, argv, "a")) != -1) {
    switch (opt) {
      case 'a': {
        include_files = 1;
        break;
      }
      default: {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
      }
    }
  }

  // More than one file provided
  if (argc - optind > 1) {
    PrintUsage(argv[0]);
    return EXIT_FAILURE;
  }

  const char *pathname = (optind < argc) ? argv[optind] : ".";
  if (du(pathname, include_files) < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// Calculates the disk usage of a directory or file.
//
// Initiates the calculation of disk usage starting from the specified root
// path. It uses a depth-first search algorithm, managed by the 'dfs' function,
// to traverse directories and files. A dynamic array is utilized to keep track
// of seen inodes, preventing the double counting of hard links.
//
// Args:
//   rootpath: A pointer to a char array containing the path of the directory
//             or file from which to start the disk usage calculation.
//   include_files: Integer flag used to determine whether to print all file
//                  types.
//
// Returns:
//   0 on successful calculation and -1 if an error occurs, with the specific
//   error reported to stderr via perror.
int du(const char *rootpath, int include_files) {
  int error = 0;
  const size_t kInitSize = 8;

  DynamicArray *seen = InitDynamicArray(kInitSize, sizeof(ino_t));
  if (!seen) {
    perror("failed to initialize dynamic array");
    return -1;
  }

  dfs(rootpath, seen, &error, include_files);
  FreeDynamicArray(seen);

  if (error) {
    return -1;
  }

  return 0;
}

// Performs a depth-first search to calculate disk usage.
//
// Recursively calculates the disk usage of a directory and its contents or
// a single file. This function is designed to be called by 'du' and updates
// the 'error' variable if an error occurs during the calculation.
//
// Args:
//   rootpath: A pointer to a char array that contains the path of the
//             directory or file to calculate disk usage for.
//   seen: A pointer to the dynamic array used for keeping track of seen
//         i-nodes with multiple links.
//   error: A pointer to an int that will be updated with the error code
//          (errno) if an error occurs during disk usage calculation.
//   include_files: Integer flag used to determine whether to print all file
//                  types.
//
// Returns:
//   The total disk usage in kilobytes (KB) of the directory or file specified
//   by `rootpath`. Returns 0 and sets `error` if an error occurs.
blkcnt_t dfs(const char *rootpath, DynamicArray *seen, int *error,
             int include_files) {
  struct stat statbuf;
  blkcnt_t disk_usage_kb;
  blkcnt_t total = 0;

  if (lstat(rootpath, &statbuf) < 0) {
    fprintf(stderr, "failed to get stat for '%s'\n", rootpath);
    *error = errno;
    return 0;
  }

  disk_usage_kb = statbuf.st_blocks / 2;

  // Handle case where `rootpath` is a regular file
  if (!S_ISDIR(statbuf.st_mode)) {
    PrintDiskUsage(disk_usage_kb, rootpath);
    return disk_usage_kb;
  } else {
    total += disk_usage_kb;
  }

  DIR *dirp = opendir(rootpath);
  if (!dirp) {
    fprintf(stderr, "failed to open directory '%s'\n", rootpath);
    *error = errno;
    return 0;
  }

  struct dirent *direntp;
  while ((direntp = readdir(dirp))) {
    // Avoid infinite loop during traversal
    if (!strcmp(direntp->d_name, ".") || !strcmp(direntp->d_name, "..")) {
      continue;
    }

    char pathname[kPathMax];
    if (snprintf(pathname, kPathMax, "%s/%s", rootpath, direntp->d_name) < 0) {
      perror("failed to concatenate root path to filename");
      *error = errno;
      break;
    }

    if (lstat(pathname, &statbuf) < 0) {
      fprintf(stderr, "failed to retrieve stat on '%s'\n", strerror(errno));
      *error = errno;
      break;
    }

    disk_usage_kb = statbuf.st_blocks / 2;
    if (S_ISDIR(statbuf.st_mode)) {
      total += dfs(pathname, seen, error, include_files);
      if (*error) {
        break;
      }
    } else if (S_ISREG(statbuf.st_mode)) {
      if (statbuf.st_nlink > 1) {
        if (SearchInode(seen, statbuf.st_ino)) {
          // I-node already accounted for, so ignore hardlink
          continue;
        }

        if (InsertInode(seen, statbuf.st_ino) < 0) {
          fprintf(stderr, "failed to insert inode '%lu'\n", statbuf.st_ino);
          *error = errno;
          break;
        }
      }
      total += disk_usage_kb;
    }

    if (include_files && !S_ISDIR(statbuf.st_mode)) {
      PrintDiskUsage(disk_usage_kb, pathname);
    }
  }
  closedir(dirp);

  if (!(*error)) {
    PrintDiskUsage(total, rootpath);
  }
  return total;
}

// Initializes a dynamic array.
//
// Allocates memory for a DynamicArray structure and its data array, based
// on the specified initial size and the size of each element. If memory
// allocation fails at any step, the function cleans up any allocated memory
// and returns NULL.
//
// Args:
//   size: The initial number of elements in the dynamic array.
//   type_size: The size (in bytes) of each element in the dynamic array.
//
// Returns:
//   A pointer to the initialized DynamicArray structure, or NULL if memory
//   allocation fails.
DynamicArray *InitDynamicArray(size_t size, size_t type_size) {
  DynamicArray *da = malloc(sizeof(DynamicArray));
  if (!da) {
    perror("malloc failed on struct allocation");
    return NULL;
  }

  da->size = size;
  da->len = 0;
  da->data = malloc(size * type_size);
  if (!da->data) {
    free(da);
    perror("malloc failed on array allocation");
    return NULL;
  }
  return da;
}

// Frees a dynamic array.
//
// Deallocates the memory allocated for the dynamic array's data and the
// DynamicArray structure itself. It's safe to call this function with a NULL
// pointer.
//
// Args:
//   da: A pointer to the DynamicArray to be freed.
void FreeDynamicArray(DynamicArray *da) {
  if (da) {
    if (da->data) {
      free(da->data);
    }
    free(da);
  }
}

// Searches for an inode in a dynamic array.
//
// Iterates over the elements of the dynamic array to find the first instance
// of the specified inode. If found, returns a pointer to the inode within
// the array.
//
// Args:
//   da: A pointer to the DynamicArray to search.
//   ino: The inode number to search for.
//
// Returns:
//   A pointer to the found inode within the dynamic array, or NULL if the
//   inode is not found or if the dynamic array pointer is NULL.
ino_t *SearchInode(DynamicArray *da, ino_t ino) {
  if (!da) {
    return NULL;
  }

  size_t i = 0;
  ino_t *inodes = (ino_t *)da->data;

  while (i < da->len) {
    if (inodes[i] == ino) {
      return &inodes[i];
    }
    i++;
  }
  return NULL;
}

// Inserts an inode into a dynamic array.
//
// Adds a new inode to the dynamic array. If the array is full, it attempts
// to double its size using realloc. If realloc fails, the function returns
// an error code, and cleanup is expected to be handled by the caller.
//
// Args:
//   da: A pointer to the DynamicArray where the inode is to be inserted.
//   ino: The inode number to insert.
//
// Returns:
//   0 on successful insertion, or -1 if realloc fails to allocate additional
//   memory.
int InsertInode(DynamicArray *da, ino_t ino) {
  if (da->size == da->len) {
    void *dummy = realloc(da->data, da->size * 2);
    if (!dummy) {
      // Cleanup is taken care of by caller
      return -1;
    }

    da->data = dummy;
    da->size *= 2;
  }
  ino_t *inodes = (ino_t *)da->data;
  inodes[da->len] = ino;
  da->len++;

  return 0;
}

// Prints the usage message for the program.
//
// Args:
//   cmd: A pointer to a C-string containing the name of the command or
//        program being executed.
static inline void PrintUsage(const char *cmd) {
  fprintf(stderr, "Usage: %s [-a] [FILE]\n", cmd);
  fprintf(stderr, "Options:\n");
  fprintf(stderr,
          "    -a    write counts for all files, not just directories\n");
}

// Prints the disk usage of a file or directory.
//
// Outputs the disk usage in kilobytes (KB) and the path of the file or
// directory to stdout.
//
// Args:
//   disk_usage: The disk usage in kilobytes (KB) of the file or directory.
//   path: A pointer to a C-string containing the path of the file or
//         directory whose disk usage is being reported.
static inline void PrintDiskUsage(blkcnt_t disk_usage, const char *path) {
  printf("%ld\t%s\n", disk_usage, path);
}
