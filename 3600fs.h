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

// Defining structures
typedef struct blocknum_t {
	int block:31;
  	int valid:1;
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
  	char name[(BLOCKSIZE - 2*sizeof(int) - 2*sizeof(blocknum))/sizeof(char)];
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
  blocknum direct[(BLOCKSIZE - sizeof(unsigned int) - sizeof(uid_t) 
  	- sizeof(gid_t) - sizeof(mode_t) 
  	- 3*sizeof(struct timespec) - 2*sizeof(blocknum))/sizeof(blocknum)]; 
  blocknum single_indirect;
  blocknum double_indirect;
} dnode;

typedef struct indirect_t {
  blocknum blocks[BLOCKSIZE/sizeof(blocknum)];
} indirect;

typedef struct direntry_t {
  char name[(32-sizeof(char)-sizeof(blocknum))/sizeof(char)];
  char type;
  blocknum block;
} direntry;

typedef struct dirent_t {
  direntry entries[BLOCKSIZE/sizeof(direntry)];
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
  blocknum direct[(BLOCKSIZE - sizeof(unsigned int) - sizeof(uid_t) 
  	- sizeof(gid_t) - sizeof(mode_t) 
  	- 3*sizeof(struct timespec) - 2*sizeof(blocknum))/sizeof(blocknum)];
  blocknum single_indirect;
  blocknum double_indirect;
} inode;

typedef struct db_t {
  char data[BLOCKSIZE];
} db;

typedef struct free_t {
  blocknum next;
  char junk[(BLOCKSIZE-sizeof(blocknum))/sizeof(char)];
} free;

#endif
