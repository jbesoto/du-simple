// du.c
//
// Author: Juan Diego Becerra (jdb9056@nyu.edu)
// Brief:  Basic implementation of a disk usage reporting tool similar to 'du' command.
//         Supports the `-a` option to include files in the usage report, not
//         just directories.

#include <stdio.h>    // fprintf, printf, snprintf
#include <dirent.h>   // opendir, readdir, closedir, dirent
#include <sys/stat.h> // lstat, stat, S_IFMT, S_IFDIR, S_IFREG
#include <stdlib.h>   // EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>   // strerror, strcmp
#include <unistd.h>   // getopt, optind
#include <limits.h>   // PATH_MAX
#include <errno.h>    // errno
#include <stdint.h>   // uint64_t

extern int optind;

static const int kMaxArgs = 3;
static int include_files = 0;

int du(const char *rootpath);
uint64_t dfs(const char *rootpath, int *error);
static inline void PrintUsage(const char *cmd);
static inline void PrintDiskUsage(uint64_t disk_usage, const char *path);

// Parses command-line arguments and calls the `du` function accordingly.
int main(int argc, char *argv[]) {
    if (argc > kMaxArgs) {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

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
    if (du(pathname) < 0) {
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

// Calculates the disk usage of a directory or file.
//
// This function computes the disk usage of the directory or file specified
// by `rootpath`. It logs an error message to stderr if it encounters an error
// during the calculation.
//
// Args:
//   rootpath: A pointer to a char array that contains the path of the
//             directory or file whose disk usage is to be calculated.
//
// Returns:
//   An integer indicating the success or failure of the operation:
//   0 on success, -1 on failure.
int du(const char *rootpath) {
    int error = 0;
    dfs(rootpath, &error);

    if (error) {
        fprintf(stderr, "%s\n", strerror(error));
        return -1;
    }

    return 0;
}

// Performs a depth-first search to calculate disk usage.
//
// Recursively calculates the disk usage of a directory and its contents or
// a single file. This function is designed to be called by `du` and updates
// the `error` variable if an error occurs during the calculation.
//
// Args:
//   rootpath: A pointer to a char array that contains the path of the
//             directory or file to calculate disk usage for.
//   error: A pointer to an int that will be updated with the error code
//          (errno) if an error occurs during disk usage calculation.
//
// Returns:
//   The total disk usage in kilobytes (KB) of the directory or file
//   specified by `rootpath`. Returns 0 and sets `error` if an error occurs.
uint64_t dfs(const char *rootpath, int *error) {
    struct stat statbuf;    
    uint64_t disk_usage_kb;
    uint64_t total = 0;

    if (lstat(rootpath, &statbuf) < 0) {
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
        *error = errno;
        return 0;
    }

    struct dirent *direntp;
    while ((direntp = readdir(dirp))) {
        // Avoid infinite loop during traversal
        if (!strcmp(direntp->d_name, ".") || !strcmp(direntp->d_name,"..")) {
            continue;
        }

        char pathname[PATH_MAX];
        if (snprintf(pathname, PATH_MAX, "%s/%s", rootpath, direntp->d_name) < 0) {
            *error = errno;
            break;
        }

        if (lstat(pathname, &statbuf) < 0) {
            *error = errno;
            break;
        }

        disk_usage_kb = statbuf.st_blocks / 2;
        if (S_ISDIR(statbuf.st_mode)) {
            total += dfs(pathname, error);
            if (*error) {
                break;
            }
        } else if (S_ISLNK(statbuf.st_mode) || S_ISREG(statbuf.st_mode)) {
            total += disk_usage_kb;
            if (include_files) {
                PrintDiskUsage(disk_usage_kb, pathname);
            }
        }
    }
    closedir(dirp);
    
    PrintDiskUsage(total, rootpath);
    return total;
}

// Prints the usage message for the program.
//
// Args:
//   cmd: A pointer to a C-string containing the name of the command or
//        program being executed.
static inline void PrintUsage(const char *cmd) {
    fprintf(stderr, "Usage: %s [-a] [FILE]\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -a    write counts for all files, not just directories\n");
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
static inline void PrintDiskUsage(uint64_t disk_usage, const char *path) {
    printf("%ld\t%s\n", disk_usage, path);
}