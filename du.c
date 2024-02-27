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

static int include_files = 0;

uint64_t du(const char *rootpath);
void PrintUsage(const char *cmd);
void PrintDiskUsage(uint64_t disk_usage, const char *path);

int main(int argc, char *argv[]) {
    // TODO: Sloppy parsing, if two filenames are passed, program still accepts it
    if (argc > 3) {
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

    const char *pathname = (optind < argc) ? argv[optind] : ".";
    if (du(pathname) == (uint64_t)-1) {
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

uint64_t du(const char *rootpath) {
    // TODO: Program should be able to take regular file paths as input
    struct stat statbuf;

    uint64_t total = 0;
    DIR *dirp = opendir(rootpath);
    if (!dirp) {
        fprintf(stderr, "Could not open directory '%s': %s", rootpath, strerror(errno));
        return (uint64_t)-1;
    }

    struct dirent *direntp;
    while ((direntp = readdir(dirp))) {
        // Avoid infinite loop during traversal
        if (!strcmp(direntp->d_name, ".") || !strcmp(direntp->d_name,"..")) {
            continue;
        }

        // Append file name to root path 
        char pathname[PATH_MAX];
        if (snprintf(pathname, PATH_MAX, "%s/%s", rootpath, direntp->d_name) < 0) {
            fprintf(stderr, "Failed to generate pathname string\n");
            closedir(dirp);
            return (uint64_t)-1;
        }

        if (lstat(pathname, &statbuf) < 0) {
            fprintf(stderr, "Failed to get stat on '%s': %s\n", pathname, strerror(errno));
            closedir(dirp);
            return (uint64_t)-1;
        }

        uint64_t disk_usage_kb = statbuf.st_blocks / 2;
        switch (statbuf.st_mode & S_IFMT) {
            case S_IFDIR: {
                total += du(pathname);
                break;
            }
            case S_IFREG: {
                total += disk_usage_kb;
                if (include_files) {
                    PrintDiskUsage(disk_usage_kb, pathname);
                }
                continue;
            }
            default:  // S_IFLNK
                continue;
        }
    }

    if (lstat(rootpath, &statbuf) < 0) {
        fprintf(stderr, "Failed to get stat on '%s': %s\n", rootpath, strerror(errno));
        closedir(dirp);
        return (uint64_t)-1;
    }

    total += statbuf.st_blocks / 2;
    PrintDiskUsage(total, rootpath);
    closedir(dirp);

    return total;
}

void PrintUsage(const char *cmd) {
    fprintf(stderr, "Usage: %s [-a] [FILE]\n", cmd);
    printf("Options:\n");
    printf("    -a    write counts for all files, not just directories\n");
}

void PrintDiskUsage(uint64_t disk_usage, const char *path) {
    printf("%ld\t%s\n", disk_usage, path);
}