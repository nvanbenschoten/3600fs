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
#include <sys/types.h>
#include <sys/stat.h>

#include "3600fs.h"
    
void myformat(int size) {
    // Do not touch or move this function
    dcreate_connect();
    /* 3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
            STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
            A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */

    /* 3600: AN EXAMPLE OF READING/WRITING TO THE DISK IS BELOW - YOU'LL
            WANT TO REPLACE THE CODE BELOW WITH SOMETHING MEANINGFUL. */
    // set vcb data and write to disk
    char * n = "3600fs";
    vcb * v = vcb_create(1337, n);
    v->root = {1, 1};
    v->free = {3, 1};
    if (dwrite(0, v) < 0)
        perror("Error writing block 0 to disk.");
    free(v);

    // create first dnode block
    char * d = dnode_create(2, getuid(), getgid(), 0777);
    d->direct[0] = {2. 1};
    d->single_indirect = {NULL, 0};
    d->double_indirect = {NULL, 0};
    clock_gettime(CLOCK_REALTIME, d->create_time);
    clock_gettime(CLOCK_REALTIME, d->access_time);
    clock_gettime(CLOCK_REALTIME, d->modify_time);

    if (dwrite(1, d) < 0)
        perror("Error writing block 1 to disk.");
    free(d);

    // create fist dirent block
    // types: (not sure if these are decided by us or by inodes spec?)
    // 0 = directory
    // 1 = file
    dirent * d = dirent_create();
    d->entries[0] = * direntry_create(0, {1, 1});
    strcpy(d->entries[0].name, ".");
    d->entries[1] = * direntry_create(0, {1, 1});
    strcpy(d->entries[1].name, "..");

    if (dwrite(2, d) < 0)
        perror("Error writing block 2 to disk.");
    free(d);

    // mark rest of blocks as free
    for (int i = 3; i < size; i++) {





    //char * tmp = (char *) malloc(BLOCKSIZE);
    //memset(tmp, 0, 


    // first, create a zero-ed out array of memory  
    //char *tmp = (char *) malloc(BLOCKSIZE);
    //memset(tmp, 0, BLOCKSIZE);

    // now, write that to every block
    for (int i=0; i<size; i++) 
            if (dwrite(i, tmp) < 0) 
                    perror("Error while writing to disk");

    // voila! we now have a disk containing all zeros

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
blocknum *blocknum_create(int num, int valid){
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

direntry *direntry_create(char type, blocknum block) {
    direntry *s;
    s = (direntry *)calloc(1, sizeof(direntry));
    assert(s != NULL);

    s->type = type;
    s->block = block;

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

void direntry_free(direntry *s) {
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
