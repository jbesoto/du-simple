/**
 * @file   du.c
 *
 * @brief  Basic implementation of a disk usage reporting tool similar to 'du'
 *         command. This program only supports the `-a` option to include files
 *         in the usage report, not just directories.
 *
 * @author Juan Diego Becerra (jdb9056@nyu.edu)
 * @date   03-24-2024
 */

#include "du.h"

/**
 * @brief Orchestrates the disk usage calculation process.
 *
 * Parses and validates command line arguments as specified in the usage. Only
 * one path argument is allowed; if not provided, the current directory (".")
 * is used as the default. The function then calls `du` to calculate and report
 * the disk usage starting from the specified path or current directory. Errors
 * during disk usage calculation also result in an exit with failure.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 *
 * @return Returns EXIT_SUCCESS on successful completion of disk usage
 *         calculation, or EXIT_FAILURE on error, such as invalid arguments or
 *         failure in disk usage calculation.
 */
int main(int argc, char* argv[]) {
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

  const char* pathname = (optind < argc) ? argv[optind] : ".";
  if (du(pathname, include_files) < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/**
 * @brief Calculates the disk usage of the given directory or file.
 *
 * Initializes a dynamic array to track seen inodes to avoid counting hard
 * links multiple times. It performs a depth-first search (DFS) to recursively
 * calculate the disk usage of the directory and its contents, optionally
 * including files if specified.
 *
 * @param rootpath      The path to the directory or file whose disk usage is to
 *                      be calculated.
 * @param include_files Flag indicating whether to include files in the usage
 *                      calculation.
 *
 * @return Returns 0 on success or -1 on error.
 */
int du(const char* rootpath, int include_files) {
  int error = 0;
  const size_t kInitSize = 8;

  DynamicArray* seen = InitDynamicArray(kInitSize, sizeof(ino_t));
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

/**
 * @brief Performs a depth-first search to calculate disk usage.
 *
 * Recursively calculates the disk usage of a directory and its contents or
 * a single file. This function is designed to be called by 'du' and updates
 * the 'error' variable if an error occurs during the calculation.
 *
 * @param rootpath      The directory or file path to calculate usage for.
 * @param seen          Dynamic array to keep track of seen inodes to handle
 *                      hard links.
 * @param error         Pointer to an integer to record any errors encountered
 *                      during execution.
 * @param include_files Flag indicating whether to include files in the usage
 *                      calculation.
 *
 * @return Returns the total disk usage in kilobytes of the specified path and
 *         its contents, or 0 if an error is encountered.
 */
blkcnt_t dfs(const char* rootpath, DynamicArray* seen, int* error,
             int include_files) {
  struct stat statbuf;
  blkcnt_t total = 0;

  if (lstat(rootpath, &statbuf) < 0) {
    fprintf(stderr, "Error: Failed to get stat for '%s'.\n", rootpath);
    *error = errno;
    return 0;
  }

  blkcnt_t disk_usage_kb = statbuf.st_blocks / 2;

  if (!S_ISDIR(statbuf.st_mode)) {
    ino_t ino = statbuf.st_ino;

    if (S_ISREG(statbuf.st_mode) && statbuf.st_nlink > 1) {
      if (SearchInode(seen, ino)) {
        return 0;
      }

      if (InsertInode(seen, ino) < 0) {
        fprintf(stderr, "Error: Unable to insert inode '%lu'. Resizing failed.\n",
                statbuf.st_ino);
        *error = errno;
        return 0;
      }
    }

    if (include_files) {
      PrintDiskUsage(disk_usage_kb, rootpath);
    }
    return disk_usage_kb;
  }

  DIR* dirp = opendir(rootpath);
  if (!dirp) {
    fprintf(stderr, "Error: Failed to open directory '%s'.\n", rootpath);
    *error = errno;
    return 0;
  }

  total += disk_usage_kb;

  struct dirent* direntp;
  while ((direntp = readdir(dirp)) && !(*error)) {
    const char* dirname = direntp->d_name;

    // Avoid infinite traversal through file system
    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
      continue;
    }

    char pathname[kPathMax];
    if (snprintf(pathname, kPathMax, "%s/%s", rootpath, dirname) < 0) {
      fprintf(stderr,
              "Error: Failed to concatenate rootpath with directory entry.\n");
      *error = errno;
      break;
    }

    total += dfs(pathname, seen, error, include_files);
  }
  closedir(dirp);

  if (!(*error)) {
    PrintDiskUsage(total, rootpath);
  }

  return total;
}

/**
 * @brief Initializes a dynamic array to store inode numbers.
 *
 * @param size      Initial size of the dynamic array.
 * @param type_size Data type of elements to be stored in the array.
 *
 * @return Returns a pointer to the initialized DynamicArray, or NULL if the
 *         initialization fails.
 */
DynamicArray* InitDynamicArray(size_t size, size_t type_size) {
  DynamicArray* da = malloc(sizeof(DynamicArray));
  if (!da) {
    perror("malloc failed on struct allocation");
    return NULL;
  }

  da->size = size;
  da->len = 0;
  da->data = malloc(size * type_size);
  if (!da->data) {
    free(da);
    return NULL;
  }
  return da;
}

/**
 * @brief Frees the memory allocated for a DynamicArray.
 *
 * @param da Pointer to the DynamicArray to be freed.
 */
void FreeDynamicArray(DynamicArray* da) {
  if (da) {
    if (da->data) {
      free(da->data);
    }
    free(da);
  }
}

/**
 * @brief Searches for an inode in a DynamicArray.
 *
 * @param da  Pointer to the DynamicArray to search.
 * @param ino Inode number to search for.
 *
 * @return Returns a pointer to the inode if found, or NULL if not found or if
 *         the DynamicArray is NULL.
 */
ino_t* SearchInode(DynamicArray* da, ino_t ino) {
  if (!da) {
    return NULL;
  }

  size_t i = 0;
  ino_t* inodes = (ino_t*)da->data;

  while (i < da->len) {
    if (inodes[i] == ino) {
      return &inodes[i];
    }
    i++;
  }
  return NULL;
}

/**
 * @brief Inserts an inode into a DynamicArray, resizing the array if necessary.
 *
 * @param da  Pointer to the DynamicArray where the inode should be inserted.
 * @param ino Inode number to insert.
 *
 * @return Returns 0 on successful insertion, or -1 if the array could not be
 *         resized.
 */
int InsertInode(DynamicArray* da, ino_t ino) {
  if (da->size == da->len) {
    void* dummy = realloc(da->data, da->size * 2);
    if (!dummy) {
      // Cleanup is taken care of by caller
      return -1;
    }

    da->data = dummy;
    da->size *= 2;
  }
  ino_t* inodes = (ino_t*)da->data;
  inodes[da->len] = ino;
  da->len++;

  return 0;
}

/**
 * @brief Prints usage information for the program.
 *
 * @param cmd The name of the command to display in the usage information.
 */
static inline void PrintUsage(const char* cmd) {
  fprintf(stderr, "Usage: %s [-a] [FILE]\n", cmd);
  fprintf(stderr, "Options:\n");
  fprintf(stderr,
          "    -a    write counts for all files, not just directories\n");
}

/**
 * @brief  Prints the disk usage of a file or directory in kilobytes.
 *
 * @param disk_usage Disk usage in kilobytes.
 * @param path       Path of the directory or file.
 */
static inline void PrintDiskUsage(blkcnt_t disk_usage, const char* path) {
  printf("%ld\t%s\n", disk_usage, path);
}
