/*
 * CS3600, Fall 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This program is intended to format your disk file, and should be executed
 * BEFORE any attempt is made to mount your file system.  It will not, however
 * be called before every mount (you will call it manually when you format 
 * your disk file).
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "3600fs.h"
    
void myformat(int size) {
    // Do not touch or move this function
    dcreate_connect();

    // set vcb data and write to disk
    char * n = DISKFILE;
    vcb * v = vcb_create(1337, n);
    v->root = blocknum_create(1,1);
    v->free = blocknum_create(3, 1);

    // Write to disk
    if (dwrite(0, (char *) v) < 0)
        perror("Error writing block 0 to disk.");
    vcb_free(v);

    // create first dnode block
    dnode * d = dnode_create(2, getuid(), getgid(), 0777);
    d->direct[0] = blocknum_create(2, 1);
    d->single_indirect = blocknum_create(0, 0);
    d->double_indirect = blocknum_create(0, 0);
    clock_gettime(CLOCK_REALTIME, &(d->create_time));
    clock_gettime(CLOCK_REALTIME, &(d->access_time));
    clock_gettime(CLOCK_REALTIME, &(d->modify_time));

    if (dwrite(1, (char *)d) < 0)
        perror("Error writing block 1 to disk.");
    dnode_free(d);

    // create fist dirent block
    // types: (not sure if these are decided by us or by inodes spec?)
    // 0 = directory
    // 1 = file
    dirent * de = dirent_create();
    de->entries[0] = direntry_create(".", 0, blocknum_create(1, 1));
    de->entries[1] = direntry_create("..", 0, blocknum_create(1, 1));

    if (dwrite(2, (char *)de) < 0)
        perror("Error writing block 2 to disk.");
    dirent_free(de);

    // mark rest of blocks as free
    for (int i = 3; i < size; i++) {
        freeblock * f;
        if (i + 1 != size) { // if the current free block is not the last one
            f = freeblock_create(blocknum_create(i+1, 1)); // point to next block
        }
        else { // it is the last one
            f = freeblock_create(blocknum_create(0, 0)); // pointer to next block is NULL
        }

        if (dwrite(i, (char *) f) < 0)
            perror("Error writing free block to disk.");
        freeblock_free(f);
    }

    // Do not touch or move this function
    dunconnect();
}

int main(int argc, char** argv) {
    // Do not touch this function
    if (argc != 2) {
            printf("Invalid number of arguments \n");
            printf("usage: %s diskSizeInBlockSize\n", argv[0]);
            return 1;
    }

    unsigned long size = atoi(argv[1]);
    printf("Formatting the disk with size %lu \n", size);

    myformat(size);
}
