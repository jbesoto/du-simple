# `du` Command (Simplified)

## Overview

This utility offers a basic but effective implementation of the `du` command, familiar to users of Unix-like operating systems. It's designed to report the disk usage of directories and files, with an added option to include files in the usage reports. Unlike the traditional `du` command, this utility utilizes a dynamic array to track seen i-nodes, ensuring accurate reporting and avoiding the double counting of hard links.

## Synopsis

```sh
./du [OPTIONS]... [FILE]...
```

**Options:**
- `-a` Include all files in the usage report, not just directories.


## Design and Implementation

The utility is structured around key functionalities that mirror the behavior of the Unix `du` command, with specific enhancements for improved performance and accuracy:

- **Dynamic Array for I-node Tracking**: To avoid double counting files with hard links, seen i-nodes are tracked using a dynamic array. This choice was made for simplicity, though it may impact performance with a larger number of i-nodes.
  
- **Optimized Function Calls**: Functions critical to performance, such as `PrintUsage` and `PrintDiskUsage`, have been optimized using `static inline` to reduce function call overhead and ensure internal linkage.

- **Enhanced Error Handling**: Error handling has been significantly refactored to improve the utility's robustness. Errors during disk usage calculation now result in immediate and clear feedback, ensuring reliability across various file system structures.

## Next Steps

While the current implementation offers a solid foundation, there are several areas identified for future development:

- **Performance Optimization**: Transitioning from a dynamic array to a more efficient data structure, such as a set or an AVL tree, for i-node tracking could significantly improve lookup performance.
  
- **Feature Expansion**: Adding more options for detailed reporting and filtering can provide users with functionality closer to the full `du` command, enhancing the utility's usefulness.