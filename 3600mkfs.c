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

    if (dwrite(1, (char *)d)) < 0)
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

// Constructors
blocknum *blocknum_create(int num, unsigned int valid){
    blocknum *s;
    s = (blocknum *)calloc(1, sizeof(blocknum));
    assert(s != NULL);

    s->block = num;
    s->valid = valid;

    return s;
}

vcb *vcb_create(int magic, char *name) {
    vcb *s;
    s = (vcb *)calloc(1, sizeof(vcb));
    assert(s != NULL);

    s->magic = magic;
    s->blocksize = BLOCKSIZE;

    strncpy(s->name, name, 496);
    s->name[495] = '\0';

    return s;
}

dnode *dnode_create(unsigned int size, uid_t user, gid_t group, mode_t mode) {
    dnode *s;
    s = (dnode *)calloc(1, sizeof(dnode));
    assert(s != NULL);

    s->size = size;
    s->user = user;
    s->group = group;
    s->mode = mode;

    return s;
}

indirect *indirect_create() {
    indirect *s;
    s = (indirect *)calloc(1, sizeof(indirect));
    assert(s != NULL);

    return s;
}

direntry direntry_create(char * name, char type, blocknum block) {
    direntry s;

    strncpy(s.name, name, 26);
    s.type = type;
    s.block = block;

    return s;
}

dirent *dirent_create() {
    dirent *s;
    s = (dirent *)calloc(1, sizeof(dirent));
    assert(s != NULL);

    return s;
}

inode *inode_create(unsigned int size, uid_t user, gid_t group, mode_t mode) {
    inode *s;
    s = (inode *)calloc(1, sizeof(inode));
    assert(s != NULL);

    s->size = size;
    s->user = user;
    s->group = group;
    s->mode = mode;

    return s;
}

db *db_create() {
    db *s;
    s = (db *)calloc(1, sizeof(db));
    assert(s != NULL);

    return s;
}

freeblock *freeblock_create(blocknum next) {
    freeblock *s;
    s = (freeblock *)calloc(1, sizeof(freeblock));
    assert(s != NULL);

    s->next = next;

    return s;
}

// Destructors
void blocknum_free(blocknum *s) {
    free(s);
}

void vcb_free(vcb *s) {
    free(s);
}

void dnode_free(dnode *s) {
    free(s);
}

void indirect_free(indirect *s) {
    free(s);
}

void dirent_free(dirent *s) {
    free(s);
}

void inode_free(inode *s) {
    free(s);
}

void db_free(db *s) {
    free(s);
}

void freeblock_free(freeblock *s) {
    free(s);
}
