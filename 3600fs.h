/*
 * CS3600, Fall 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "disk.h"

#define UNUSED(x) (void)(x)

#define DIRENT_NUM 16
#define DIRECTS 110
#define INDIRECTS 128

extern const int DISKNUMBER;

// Defining structures
typedef struct blocknum_t {
	int block:31;
	unsigned int valid:1;
} blocknum;

typedef struct vcb_s {
	// a magic number of identify your disk
	int magic;
	// the block size
	int blocksize;
	// the location of the root DNODE
	blocknum root;
	// the location of the first free block
	blocknum free;
	// the name of your disk
	char name[491];
	// a dirty bit to check if it was removed correctly
	char dirty;
    // the size of the disk in blocks
    int blocks;
} vcb;

typedef struct dnode_t {
	// directory node metadata
	unsigned int size;
	uid_t user;
	gid_t group;
	mode_t mode;
	struct timespec access_time;
	struct timespec modify_time;
	struct timespec create_time;
	// the locations of the directory entry blocks
	blocknum direct[DIRECTS]; 
	blocknum single_indirect;
	blocknum double_indirect;
} dnode;

typedef struct indirect_t {
	blocknum blocks[INDIRECTS];
} indirect;

typedef struct direntry_t {
	// types:
        // 0 = directory
        // 1 = file
	char name[27];
	char type;
	blocknum block;
} direntry;

typedef struct dirent_t {
	direntry entries[DIRENT_NUM];
} dirent;

typedef struct inode_t {
	// the file metadata
	unsigned int size;
	uid_t user;
	gid_t group;
	mode_t mode;
	struct timespec access_time;
	struct timespec modify_time;
	struct timespec create_time;
	// the locations of the file data blocks
	blocknum direct[DIRECTS];
	blocknum single_indirect;
	blocknum double_indirect;
} inode;

typedef struct db_t {
	char data[512];
} db;

typedef struct free_t {
	blocknum next;
	char junk[508];
} freeblock;

// Constructors
blocknum blocknum_create(int num, unsigned int valid);
vcb *vcb_create(int magic, char *name);
dnode *dnode_create(unsigned int size, uid_t user, gid_t group, mode_t mode);
indirect *indirect_create();
direntry direntry_create(char * name, char type, blocknum block);
dirent *dirent_create();
inode *inode_create(unsigned int size, uid_t user, gid_t group, mode_t mode);
db *db_create();
freeblock *freeblock_create(blocknum next);

// Destructors
void vcb_free(vcb *s);
void dnode_free(dnode *s);
void indirect_free(indirect *s);
void dirent_free(dirent *s);
void inode_free(inode *s);
void db_free(db *s);
void freeblock_free(freeblock *s);

// Global helper Functions
char *getName(direntry direntry);
void setName(direntry *direntry, char *name);
void disk_crash();
int bufdread(int blocknum, char * buf, int size);
int bufdwrite(int blocknum, const char * buf, int size);
void initCache();

// File system specific helper functions
int seperatePathAndName(char *path, char *name);
int findDNODE(dnode *directory, char *path, blocknum *block);
int getNODE(dnode *directory, char *name, dnode *searchDnode, inode *searchInode, blocknum *block, int deleteFlag, int directoryBlock, int renameFlag, char *newName);
int getNextFree(vcb *v);
int releaseFree(vcb *v, blocknum block);
int checkIntegrity();
int checkDNODE(dnode *d, int block);
int checkINODE(inode *i);

#endif
