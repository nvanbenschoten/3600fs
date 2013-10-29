/*
 * CS3600, Fall 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This file contains all of the basic functions that you will need 
 * to implement for this project.  Please see the project handout
 * for more details on any particular function, and ask on Piazza if
 * you get stuck.
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <fuse.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"

// Global vcb
vcb *v;

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */
static void* vfs_mount(struct fuse_conn_info *conn) {
	UNUSED(conn);

	fprintf(stderr, "vfs_mount called\n");

	// Do not touch or move this code; connects the disk
	dconnect();

	/* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
		AND LOAD ANY DATA STRUCTURES INTO MEMORY */

	initCache();

	v = vcb_create(0, "");

	bufdread(0, (char *)v, sizeof(vcb));

	// Check integrity of vcb
	if (v->magic != DISKNUMBER) {
		printf("%s\n", "The disk connected was not the correct disk");
		dunconnect();
	}

	if (v->dirty) {
		// Perform integrity check
	}

	// update vcb with dirty bit
	v->dirty = 1;
	bufdwrite(0, (char *) v, sizeof(vcb));

	return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
	UNUSED(private_data);

	fprintf(stderr, "vfs_unmount called\n");

	/* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
		ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
		KEEP DATA CACHED THAT'S NOT ON DISK */

	// update vcb with dirty bit
	v->dirty = 0;
	bufdwrite(0, (char *) v, sizeof(vcb));

	// Free vcb
	vcb_free(v);

	// Do not touch or move this code; unconnects the disk
	dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
	fprintf(stderr, "vfs_getattr called\n");

	// Do not mess with this code 
	stbuf->st_nlink = 1; // hard links
	stbuf->st_rdev  = 0;
	stbuf->st_blksize = BLOCKSIZE;

	// Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		printf("Error seperating the path and filename\n");
		free(pathcpy);
		free(name);
		return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			dnode_free(d);
			free(pathcpy);
			free(name);
			return -1;
		}
	}

	if (!strcmp(name, "")) {
		// If name is blank, it means that it is refering to the current directory
		strcpy(name, ".");
	}

	// Get the node
	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, matchd, matchi, &block, 0, 0, 0, "");
	dnode_free(d);

	// Check to see if match is valid
	if (ret < 0) {
		dnode_free(matchd);
		inode_free(matchi);
		free(pathcpy);
		free(name);
		return -ENOENT;
	}
	else if (ret == 0) {
		// If the dirent is for a directory
		stbuf->st_mode    = matchd->mode | S_IFDIR; // Directory node
		stbuf->st_uid     = matchd->user; // directory uid
		stbuf->st_gid     = matchd->group; // directory gid
		stbuf->st_atime   = matchd->access_time.tv_sec; // access time
		stbuf->st_mtime   = matchd->modify_time.tv_sec; // modify time
		stbuf->st_ctime   = matchd->create_time.tv_sec; // create time
		stbuf->st_size    = matchd->size; // directory size
		stbuf->st_blocks  = (matchd->size + 16 - 1)/(16); // directory size in blocks
	}
	else if (ret == 1) {
		// If the dirent is for a file
		stbuf->st_mode    = matchi->mode | S_IFREG;
		stbuf->st_uid     = matchi->user; // file uid
		stbuf->st_gid     = matchi->group; // file gid
		stbuf->st_atime   = matchi->access_time.tv_sec; // access time 
		stbuf->st_mtime   = matchi->modify_time.tv_sec; // modify time
		stbuf->st_ctime   = matchi->create_time.tv_sec; // create time
		stbuf->st_size    = matchi->size; // file size
		stbuf->st_blocks  = (matchi->size + BLOCKSIZE - 1)/(BLOCKSIZE); // file size in blocks
	}

	dnode_free(matchd);
	inode_free(matchi);
	free(pathcpy);
	free(name);

	return 0;
}

/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory, and will create it with the specified initial mode.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory.
 */

static int vfs_mkdir(const char *path, mode_t mode) {

	// Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	// Handle last char being a slash
	if (pathcpy[strlen(pathcpy) - 1] == '/')
		pathcpy[strlen(pathcpy) - 1] = '\0';

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		printf("Error seperating the path and filename\n");
		free(pathcpy);
		free(name);
		return -1;
	}

	if(strlen(name) > 27) {
		printf("File name too long\n");
		free(pathcpy);
		free(name);
		return -ENAMETOOLONG;
	}

	//Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			dnode_free(d);
			free(pathcpy);
			free(name);
			return -1;
		}
	}

	// check if file already exists
	dnode * temp_d = dnode_create(0, 0, 0, 0);
	inode * temp_i = inode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, temp_d, temp_i, &block, 0, 0, 0, "");
	if (ret >= 0) {
		// COMMENT Made it so it file or dir match, it returns
		dnode_free(d);
		dnode_free(temp_d);
		inode_free(temp_i);
		// do pathcpy and name need to be freed?
		free(pathcpy);
		free(name);
		return -EEXIST;
	}
	else {
		dnode_free(temp_d);
		inode_free(temp_i);
	}
        
    // while we havent found a dirent
    int dnode_block = -1;
    int i = 0, j = 0, ent_b = 0, lvl = 0;
    int dirents = 110;
    int fr;
    indirect *indr = indirect_create();
    indirect *indr2 = indirect_create();
    dirent *de = dirent_create();
    dirent *de_new = dirent_create();
    blocknum * dbs = d->direct;
    dnode * d_new = dnode_create(2, geteuid(), getegid(), mode);

    // create new inode metadata block, WITHOUT DATA block to go with it
    clock_gettime(CLOCK_REALTIME, &(d_new->create_time));
    clock_gettime(CLOCK_REALTIME, &(d_new->access_time));
    clock_gettime(CLOCK_REALTIME, &(d_new->modify_time));
    d_new->direct[0] = blocknum_create(0, 0);
    d_new->single_indirect = blocknum_create(0, 0);
    d_new->double_indirect = blocknum_create(0, 0);

    while (dnode_block == -1) { // while we havent found a dirent for the inode
            if (ent_b < dirents) { // if we have more dirents to look through
                    if (dbs[ent_b].valid) { // if the entry is valid we have to look through it
                            bufdread(dbs[ent_b].block, (char *) de, sizeof(dirent)); // get the next dirent
                            for (i = 0; i < 16; i++) { // look through the direntries
                                    if (!de->entries[i].block.valid) { // if any of the entries are invalid
                                            // meaning we can use them to write to
                                            //strcpy(de->entries[i].name, name); 
                                            setName(&de->entries[i], name); // write to them
                                            de->entries[i].type = 0; // 0 = dir
                                            if ((fr = getNextFree(v)) >= 0) {
                                                    de->entries[i].block = blocknum_create(fr, 1);
                                            }
                                            else {
                                                    printf("Error: No disk space available.\n");
                                                    free(d);
                                                    free(indr);
                                                    free(indr2);
                                                    free(de);
                                                    free(de_new);
                                                    free(d_new);
                                                    free(pathcpy);
                                                    free(name);
                                                    return -ENOSPC;
                                            }
                                            dnode_block = de->entries[i].block.block; // set inode
                                            bufdwrite(dbs[ent_b].block, (char *) de, sizeof(dirent));
                                            break;
                                    }
                            }
                    }
                    else { // we need to use this dirent for our inode
                            // set to valid and make it an actual blocknum cause no guarantees
                            if ((fr = getNextFree(v)) >= 0) {
                                    dbs[ent_b] = blocknum_create(fr, 1); // write new blocknum as valid
                            }
                            else {
                                    printf("Error: No disk space available.\n");
                                    free(d);
                                    free(indr);
                                    free(indr2);
                                    free(de);
                                    free(de_new);
                                    free(d_new);
                                    free(pathcpy);
                                    free(name);
                                    return -ENOSPC;
                            }
                            //strcpy(de_new->entries[0].name, name); // write new direnty to new dirent
                            setName(&de_new->entries[0], name); // write to them
                            de_new->entries[0].type = 0;
                            if ((fr = getNextFree(v)) >= 0) {
                                    de_new->entries[0].block = blocknum_create(fr, 1);
                            }
                            else {
                                    printf("Error: No disk space available.\n");
                                    free(d);
                                    free(indr);
                                    free(indr2);
                                    free(de);
                                    free(de_new);
                                    free(d_new);
                                    free(pathcpy);
                                    free(name);
                                    return -ENOSPC;
                            }
                            // write dirent
                            bufdwrite(dbs[ent_b].block, (char *) de_new, sizeof(dirent));
                            dnode_block = de_new->entries[0].block.block; // set inode blocknum 
                    }
                    ent_b++; 
            }
            else if (lvl == 0) { // we need to move to single_indirect block
                    if (d->single_indirect.valid) { // blocknum is valid so can just use it
                            bufdread(d->single_indirect.block, (char *) indr, sizeof(indirect));
                    }
                    else { // need to actually create blocknum etc
                            if ((fr = getNextFree(v)) >= 0) {
                                    d->single_indirect = blocknum_create(fr, 1);
                            }
                            else {
                                    printf("Error: No disk space available.\n");
                                    free(d);
                                    free(indr);
                                    free(indr2);
                                    free(de);
                                    free(de_new);
                                    free(d_new);
                                    free(pathcpy);
                                    free(name);
                                    return -ENOSPC;
                            }
                            indr->blocks[0] = blocknum_create(0, 0);
                    }
                    ent_b = 0;
                    dirents = 128;
                    dbs = indr->blocks;
                    lvl++;
            }
            else if (lvl == 1) { // we need to move to double_indirect block
                    if (d->double_indirect.valid) { // if blocknum valid
                            bufdread(d->double_indirect.block, (char *) indr2, sizeof(indirect)); // can use it
                            if (indr2->blocks[0].valid) { // if 2nd level valid
                                    bufdread(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                            }
                            else { // need to update both levels
                                    if ((fr = getNextFree(v)) >= 0) {
                                            indr2->blocks[0] = blocknum_create(fr, 1);
                                    }
                                    else {
                                            printf("Error: No disk space available.\n");
                                            free(d);
                                            free(indr);
                                            free(indr2);
                                            free(de);
                                            free(de_new);
                                            free(d_new);
                                            free(pathcpy);
                                            free(name);
                                            return -ENOSPC;
                                    }
                                    free(indr);
                                    indr = indirect_create();
                                    indr->blocks[0] = blocknum_create(0, 0);
                                    bufdwrite(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                            }
                    }
                    else { // not valid so need to setup 1st and 2nd levels
                            if ((fr = getNextFree(v)) >= 0) {
                                    d->double_indirect = blocknum_create(fr, 1);
                            }
                            else {
                                    printf("Error: No disk space available.\n");
                                    free(d);
                                    free(indr);
                                    free(indr2);
                                    free(de);
                                    free(de_new);
                                    free(d_new);
                                    free(pathcpy);
                                    free(name);
                                    return -ENOSPC;
                            }
                            if ((fr = getNextFree(v)) >= 0) {
                                    indr2->blocks[0] = blocknum_create(fr, 1);
                            }
                            else {
                                    printf("Error: No disk space available.\n");
                                    free(d);
                                    free(indr);
                                    free(indr2);
                                    free(de);
                                    free(de_new);
                                    free(d_new);
                                    free(pathcpy);
                                    free(name);
                                    return -ENOSPC;
                            }
                            free(indr);
                            indr = indirect_create();
                            indr->blocks[0] = blocknum_create(0, 0);
                            bufdwrite(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                    }
                    ent_b = 0;
                    j = 0;
                    dirents = 128;
                    dbs = indr->blocks;
                    lvl++;
            }
            else if (lvl == 2) { // else we are already in second level of indirection
                    j++;
                    if (j < 128) { // if there are still indirects to check
                            if (indr2->blocks[j].valid) { // if next first level indirect is usable
                                    bufdread(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                            }
                            else { // otherwise need to setup blank indr
                                    if ((fr = getNextFree(v)) >= 0) {
                                            indr2->blocks[j] = blocknum_create(fr, 1);
                                    }
                                    else {
                                            printf("Error: No disk space available.\n");
                                            free(d);
                                            free(indr);
                                            free(indr2);
                                            free(de);
                                            free(de_new);
                                            free(d_new);
                                            free(pathcpy);
                                            free(name);
                                            return -ENOSPC;
                                    }
                                    free(indr);
                                    indr = indirect_create();
                                    indr->blocks[0] = blocknum_create(0, 0);
                                    bufdwrite(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                            }
                            ent_b = 0;
                            dirents = 128;
                            dbs = indr->blocks;
                    }
                    else { // we have run out of indirection space
                            lvl++;
                    }
            }
            else { // we need to return an error because there is no more room for dirents
                    // free stuff
                    free(d);
                    free(indr);
                    free(indr2);
                    free(de);
                    free(de_new);
                    free(d_new);
                    free(pathcpy);
		    free(name);
                    // error and return
                    printf("Error no directory entry available to create file.\n");
                    return -1;
            }
    }
    if (ent_b != 0) { // if we changed ent_b its off by one for final dbs comparison 
            ent_b--;
    }
    

    // New directory dirent
    if ((fr = getNextFree(v)) >= 0) {
            d_new->direct[0] = blocknum_create(fr, 1);
    }
    else {
            printf("Error: No disk space available.\n");
            free(d);
            free(indr);
            free(indr2);
            free(de);
            free(de_new);
            free(d_new);
            free(pathcpy);
            free(name);
            return -ENOSPC;
    }
    dirent * de_in = dirent_create();
    de_in->entries[0] = direntry_create(".", 0, blocknum_create(dnode_block, 1));
    de_in->entries[1] = direntry_create("..", 0, blocknum_create(dirBlock.block, 1));

    bufdwrite(d_new->direct[0].block, (char *)de_in, sizeof(dirent));
    dirent_free(de_in);

    bufdwrite(dnode_block, (char *) d_new, sizeof(dnode));

    d->size += 1;
    // write DBS dependent on lvl... thank god this is the end
    switch (lvl) {
            case 0:
                    d->direct[ent_b] = dbs[ent_b];
                    bufdwrite(dirBlock.block, (char *) d, sizeof(dnode)); 
                    break;
            case 1:
                    indr->blocks[ent_b] = dbs[ent_b];
                    bufdwrite(dirBlock.block, (char *) d, sizeof(dnode));
                    bufdwrite(d->single_indirect.block, (char *) indr, sizeof(indirect));
                    break;

            case 2: // j could be off by 1, but i dont think it is atm
                    indr->blocks[ent_b] = dbs[ent_b];
                    bufdwrite(dirBlock.block, (char *) d, sizeof(dnode));
                    bufdwrite(d->double_indirect.block, (char *) indr2, sizeof(indirect));
                    bufdwrite(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                    break;
            // case 3 is probably necessary as if lvl = 3
            // it means there was no space so should have returned with error already
    }
	// update vcb
	bufdwrite(0, (char *) v, sizeof(vcb));

	// then free stuff and return
	dnode_free(d);
	indirect_free(indr);
	indirect_free(indr2);
	dirent_free(de);
	dirent_free(de_new);
	dnode_free(d_new);
	free(pathcpy);
	free(name);

  	return 0;
}

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
					   off_t offset, struct fuse_file_info *fi)
{
	UNUSED(fi);

	if (path[0] != '/')
		return -1;

	// Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum block = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &block)) {
			// If ditectory could not be found
			printf("Could not find directory\n");
			dnode_free(d);
			free(pathcpy);
			return -1;
		}
	}

	// Check in directs
	unsigned int count = offset;
	while (count < 110*16) {

		if (d->direct[count/16].valid) {

			dirent *de = dirent_create();
			bufdread(d->direct[count/16].block, (char *)de, sizeof(dirent));

			int j;
			for (j = count%16; j < 16; j++) {
				// j = direntry entry
				count++;
				if (de->entries[j].block.valid) {
					// If the entry is valid
					char *dname = getName(de->entries[j]);
					if(filler(buf, dname, NULL, count)) {
						dirent_free(de);
						dnode_free(d);
						free(pathcpy);
						free(dname);
						return 0;
					}
					free(dname);
				}
			}

			dirent_free(de);
		}
		else {
			count += 16;
		}
	}

	// Single indirection
	if (d->single_indirect.valid) {
		indirect *ind = indirect_create();
		bufdread(d->single_indirect.block, (char *)ind, sizeof(indirect));
		while(count < 128*16+110*16) {

			if (ind->blocks[(count-110*16)/16].valid) {

				dirent *de = dirent_create();
				bufdread(ind->blocks[(count-110*16)/16].block, (char *)de, sizeof(dirent));

				int j;
				for (j = count%16; j < 16; j++) {
					// j = direntry entry
					count++;
					if (de->entries[j].block.valid) {
						// If the entry is valid
						char *dname = getName(de->entries[j]);
						if(filler(buf, dname, NULL, count)) {
							indirect_free(ind);
							dirent_free(de);
							dnode_free(d);
							free(pathcpy);
							free(dname);
							return 0;
						}
						free(dname);
					}
				}

				dirent_free(de);
			}
			else {
				count += 16;
			}
		}
		
		indirect_free(ind);
	}
	else {
		count += 128*16;
	}

	// Double indirection
	if (d->double_indirect.valid) {

		indirect *firstind = indirect_create();
		bufdread(d->double_indirect.block, (char *)firstind, sizeof(indirect));
		while(count < 128*128*16+128*16+110*16) {
			// While below limit of direct, first indirect, and second indirect
			
			if (firstind->blocks[(count-110*16-128*16)/(16*128)].valid) {
				// If first indirect is valid

				// Read in second indirect
				indirect *secind = indirect_create();
				bufdread(firstind->blocks[(count-110*16-128*16)/(16*128)].block, (char *)secind, sizeof(indirect));

				int k;
				for (k = (count-110*16-128*16)/16; k < 128; k++) {
					// K is equal to the index in the second indirect block

					if (secind->blocks[k].valid) {

						dirent *de = dirent_create();
						bufdread(secind->blocks[k].block, (char *)de, sizeof(dirent));

						int j;
						for (j = (count-110*16-128*16)%16; j < 16; j++) {
							// j = direntry entry
							count++;
							if (de->entries[j].block.valid) {
								// If the entry is valid
								char *dname = getName(de->entries[j]);
								if(filler(buf, dname, NULL, count)) {
									indirect_free(firstind);
									indirect_free(secind);
									dirent_free(de);
									dnode_free(d);
									free(pathcpy);
									free(dname);
									return 0;
								}
								free(dname);
							}
						}

						dirent_free(de);
					}
					else {
						count += 16;
					}
				}

				indirect_free(secind);
			}
			else {
				count += 128*16;
			}
		}
		indirect_free(firstind);
	}
	else {
		count += 128*128*16;
	}

	dnode_free(d);
	free(pathcpy);

	return 0;
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	UNUSED(fi);

	// Move down path
	// Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		printf("Error seperating the path and filename\n");
		free(pathcpy);
		free(name);
		return -1;
	}

	if(strlen(name) > 27) {
		printf("File name too long\n");
		free(pathcpy);
		free(name);
		return -ENAMETOOLONG;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(path, "")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			dnode_free(d);
			free(pathcpy);
			free(name);
			return -1;
		}
	}

	// check if file already exists
	dnode * temp_d = dnode_create(0, 0, 0, 0);
	inode * temp_i = inode_create(0, 0, 0, 0);
	blocknum block;
        int fr;
	int ret = getNODE(d, name, temp_d, temp_i, &block, 0, 0, 0, "");
	if (ret >= 0) { // there was an error so free and return
		dnode_free(d);
		dnode_free(temp_d);
		inode_free(temp_i);
		free(pathcpy);
		free(name);
		return -EEXIST;
	}
	else {
		dnode_free(temp_d);
		inode_free(temp_i);
	}
        
        // while we havent found a dirent
        int inode_block = -1;
        int i = 0, j = 0, ent_b = 0, lvl = 0;
        int dirents = 110;
        indirect *indr = indirect_create();
        indirect *indr2 = indirect_create();
        dirent *de = dirent_create();
        dirent *de_new = dirent_create();
        blocknum * dbs = d->direct;
        inode * i_new = inode_create(0, geteuid(), getegid(), mode);

        // create new inode metadata block, WITHOUT DATA block to go with it
        clock_gettime(CLOCK_REALTIME, &(i_new->create_time));
        clock_gettime(CLOCK_REALTIME, &(i_new->access_time));
        clock_gettime(CLOCK_REALTIME, &(i_new->modify_time));
        i_new->direct[0] = blocknum_create(0, 0);
        i_new->single_indirect = blocknum_create(0, 0);
        i_new->double_indirect = blocknum_create(0, 0);

        while (inode_block == -1) { // while we havent found a dirent for the inode
                if (ent_b < dirents) { // if we have more dirents to look through
                        if (dbs[ent_b].valid) { // if the entry is valid we have to look through it
                                bufdread(dbs[ent_b].block, (char *) de, sizeof(dirent)); // get the next dirent
                                for (i = 0; i < 16; i++) { // look through the direntries
                                        if (!de->entries[i].block.valid) { // if any of the entries are invalid
                                                // meaning we can use them to write to
                                                setName(&de->entries[i], name); // write to them
                                                de->entries[i].type = 1; // 1 = file
                                                if ((fr = getNextFree(v)) >= 0) {
                                                        de->entries[i].block = blocknum_create(fr, 1);
                                                }
                                                else {
                                                        printf("Error: No disk space available.\n");
                                                        free(d);
                                                        free(indr);
                                                        free(indr2);
                                                        free(de);
                                                        free(de_new);
                                                        free(i_new);
                                                        free(pathcpy);
                                                        free(name);
                                                        return -ENOSPC;
                                                }
                                                inode_block = de->entries[i].block.block; // set inode
                                                bufdwrite(dbs[ent_b].block, (char *) de, sizeof(dirent));
                                                break;
                                        }
                                }
                        }
                        else { // we need to use this dirent for our inode
                                // set to valid and make it an actual blocknum cause no guarantees
                                if ((fr = getNextFree(v)) >= 0) {
                                        dbs[ent_b] = blocknum_create(fr, 1); // write new blocknum as valid
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        free(d);
                                        free(indr);
                                        free(indr2);
                                        free(de);
                                        free(de_new);
                                        free(i_new);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }
                                setName(&de_new->entries[0], name); // write new direnty to new dirent
                                de_new->entries[0].type = 1;
                                if ((fr = getNextFree(v)) >= 0) {
                                        de_new->entries[0].block = blocknum_create(fr, 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        free(d);
                                        free(indr);
                                        free(indr2);
                                        free(de);
                                        free(de_new);
                                        free(i_new);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }
                                // write dirent
                                bufdwrite(dbs[ent_b].block, (char *) de_new, sizeof(dirent));
                                inode_block = de_new->entries[0].block.block; // set inode blocknum 
                        }
                        ent_b++; 
                }
                else if (lvl == 0) { // we need to move to single_indirect block
                        if (d->single_indirect.valid) { // blocknum is valid so can just use it
                                bufdread(d->single_indirect.block, (char *) indr, sizeof(indirect));
                        }
                        else { // need to actually create blocknum etc
                                if ((fr = getNextFree(v)) >= 0) {
                                        d->single_indirect = blocknum_create(fr, 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        free(d);
                                        free(indr);
                                        free(indr2);
                                        free(de);
                                        free(de_new);
                                        free(i_new);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }
                                indr->blocks[0] = blocknum_create(0, 0);
                        }
                        ent_b = 0;
                        dirents = 128;
                        dbs = indr->blocks;
                        lvl++;
                }
                else if (lvl == 1) { // we need to move to double_indirect block
                        if (d->double_indirect.valid) { // if blocknum valid
                                bufdread(d->double_indirect.block, (char *) indr2, sizeof(indirect)); // can use it
                                if (indr2->blocks[0].valid) { // if 2nd level valid
                                        bufdread(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                                }
                                else { // need to update both levels
                                        if ((fr = getNextFree(v)) >= 0) {
                                                indr2->blocks[0] = blocknum_create(fr, 1);
                                        }
                                        else {
                                                printf("Error: No disk space available.\n");
                                                free(d);
                                                free(indr);
                                                free(indr2);
                                                free(de);
                                                free(de_new);
                                                free(i_new);
                                                free(pathcpy);
                                                free(name);
                                                return -ENOSPC;
                                        }
                                        free(indr);
                                        indr = indirect_create();
                                        indr->blocks[0] = blocknum_create(0, 0);
                                        bufdwrite(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                                }
                        }
                        else { // not valid so need to setup 1st and 2nd levels
                                if ((fr = getNextFree(v)) >= 0) {
                                        d->double_indirect = blocknum_create(fr, 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        free(d);
                                        free(indr);
                                        free(indr2);
                                        free(de);
                                        free(de_new);
                                        free(i_new);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }
                                if ((fr = getNextFree(v)) >= 0) {
                                        indr2->blocks[0] = blocknum_create(fr, 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        free(d);
                                        free(indr);
                                        free(indr2);
                                        free(de);
                                        free(de_new);
                                        free(i_new);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }
                                free(indr);
                                indr = indirect_create();
                                indr->blocks[0] = blocknum_create(0, 0);
                                bufdwrite(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                        }
                        ent_b = 0;
                        j = 0;
                        dirents = 128;
                        dbs = indr->blocks;
                        lvl++;
                }
                else if (lvl == 2) { // we need to move within the second indirect entries
                        j++;
                        if (j != 128) {
                                if (indr2->blocks[j].valid) {
                                        bufdread(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                }
                                else { // need to setup blank indr
                                        if ((fr = getNextFree(v)) >= 0) {
                                                indr2->blocks[j] = blocknum_create(fr, 1);
                                        }
                                        else {
                                                printf("Error: No disk space available.\n");
                                                free(d);
                                                free(indr);
                                                free(indr2);
                                                free(de);
                                                free(de_new);
                                                free(i_new);
                                                free(pathcpy);
                                                free(name);
                                                return -ENOSPC;
                                        }
                                        free(indr);
                                        indr = indirect_create();
                                        indr->blocks[0] = blocknum_create(0, 0);
                                        bufdwrite(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                }
                                ent_b = 0;
                                dirents = 128;
                                dbs = indr->blocks;
                        }
                        else {
                                lvl++;
                        }
                }
                else { // we need to return an error because there is no more room for dirents
                        // free stuff
                        free(d);
                        free(indr);
                        free(indr2);
                        free(de);
                        free(de_new);
                        free(i_new);
                        free(pathcpy);
			free(name);
                        // error and return
                        printf("Error no directory entry available to create file.\n");
                        return -1;
                }
        }
        if (ent_b != 0) { // if we changed ent_b its off by one for final dbs comparison 
                ent_b--;
        }
        
        bufdwrite(inode_block, (char *) i_new, sizeof(inode));

        d->size += 1;
        // WRITE DBS dependent on lvl
        switch (lvl) {
                case 0:
                        d->direct[ent_b] = dbs[ent_b];
                        bufdwrite(dirBlock.block, (char *) d, sizeof(dnode)); 
                        break;
                case 1:
                        indr->blocks[ent_b] = dbs[ent_b];
                        bufdwrite(dirBlock.block, (char *) d, sizeof(dnode));
                        bufdwrite(d->single_indirect.block, (char *) indr, sizeof(indirect));
                        break;

                case 2: 
                        indr->blocks[ent_b] = dbs[ent_b];
                        bufdwrite(dirBlock.block, (char *) d, sizeof(dnode));
                        bufdwrite(d->double_indirect.block, (char *) indr2, sizeof(indirect));
                        bufdwrite(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                        break;
                // case 3 is not necessary as if lvl = 3
                // it means there was no space so should have returned with error already
        }
	// update vcb
	bufdwrite(0, (char *) v, sizeof(vcb));

	// then free stuff and return
	free(d);
	free(indr);
	free(indr2);
	free(de);
	free(de_new);
	free(i_new);
	free(pathcpy);
	free(name);
		
	return 0;
}

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes starting at the specified
 * offset (offset) from the specified file (path)
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
					struct fuse_file_info *fi)
{
    UNUSED(fi);
	// Move down path
    // Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
			printf("Error seperating the path and filename\n");
			free(pathcpy);
			free(name);
			return -1;
	}

	if (strlen(name) > 26) {
		printf("Error new filename too long\n");
		free(pathcpy);
		free(name);
		return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));

	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(path, "")) {
			// If path isnt the root directory
			// Transforms d to the correct directory
			if(findDNODE(d, pathcpy, &dirBlock)) {
					// If directory could not be found
					printf("Could not find directory\n");
					dnode_free(d);
					free(pathcpy);
					free(name);
					return -1;
			}
	}

	// Get correct node
	inode *i_node = inode_create(0, 0, 0, 0);
	dnode *d_temp = dnode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, d_temp, i_node, &block, 0, 0, 0, "");
	if (ret != 1) { // if didnt find matching file node
		inode_free(i_node);
		dnode_free(d_temp);
		dnode_free(d);
		free(name);
		free(pathcpy);
		printf("Could not find file to read or file is a directory");
		return -1;
	}
	
        unsigned int copied = 0;
	int blocks = ((int)offset)/sizeof(db); // calculate num blocks to look in
	int block_offset = ((int)offset) % sizeof(db); // calculate offset into 
	char tmp[sizeof(db)];
        indirect *indr = indirect_create();
        indirect *indr2 = indirect_create();
        int cur_b = 0, blocks_read = 110;
        int j = 0, lvl = 0;
        blocknum * dbs;

        
        // calculate the starting block to look in and get dbs, set j, and set lvl accordingly
        if (blocks < 110) { // in normal direct blocks
                dbs = i_node->direct; 
                cur_b = blocks;
        }
        else if (blocks >= 110 && blocks < 238) { // need to look in single indirects
                lvl++;
                if (i_node->single_indirect.valid) {
                        bufdread(i_node->single_indirect.block, (char*) indr, sizeof(indirect));
                        dbs = indr->blocks;
                        cur_b = blocks - 110;
                        blocks_read = 128;
                }
                else { // free and return
                        inode_free(i_node);
                        dnode_free(d);
                        dnode_free(d_temp);
                        indirect_free(indr);
                        indirect_free(indr2);
                        free(name);
                        free(pathcpy);
                        printf("Error: Invalid file offset");
                        return -1;
                }
        }
        else if (blocks < 16622) { // need to look in double indirects
                lvl++; // \    / |_| \ /
                lvl++; //  \/\/  | |  |
                if (i_node->double_indirect.valid) {
                        bufdread(i_node->double_indirect.block, (char *) indr2, sizeof(indirect));
                        j = (blocks - 238) / 128;
                        if (indr2->blocks[j].valid) {
                                bufdread(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                dbs = indr->blocks;
                                cur_b = blocks - 238 - (j*128);
                                blocks_read = 128;
                        }
                        else { // free and return
                                inode_free(i_node);
                                dnode_free(d);
                                dnode_free(d_temp);
                                indirect_free(indr);
                                indirect_free(indr2);
                                free(name);
                                free(pathcpy);
                                printf("Error: Invalid file offset");
                                return -1;
                        }
                }
                else { // free and return
                        inode_free(i_node);
                        dnode_free(d);
                        dnode_free(d_temp);
                        indirect_free(indr);
                        indirect_free(indr2);
                        free(name);
                        free(pathcpy);
                        printf("Error: Invalid file offset");
                        return -1;
                }
        }
        else { // Error 
                inode_free(i_node);
                dnode_free(d);
                dnode_free(d_temp);
                indirect_free(indr);
                indirect_free(indr2);
                free(name);
                free(pathcpy);
                printf("Error: File offset too large\n");
                return -1; 
        }

        while (copied < size) {
                // read data from valid blocks in a blocknum[]
                if (cur_b < blocks_read) { // if we havent read all the blocks needed in the current dbs
                        if (dbs[cur_b].valid) { // if the block is valid so we can read data from it
                                bufdread(dbs[cur_b].block, tmp, sizeof(db)); // read in current block
                                if (size - copied >= sizeof(db)) { // if we need to copy until end of block
                                        // copy data taking into account potential offsets
                                        memcpy((char *)buf+copied, tmp+block_offset, sizeof(db)-block_offset);
                                        copied += sizeof(db)-block_offset; // add to copied
                                }
                                else { // otherwise we stop copying before the end of the block
                                        // take into account how far we actually need to copy
                                        memcpy((char*)buf+copied, tmp+block_offset, size-copied);
                                        copied += size-copied; // add to copied
                                }
                                block_offset = 0;
                                cur_b++;
                        }
                        else { // free and return
                                inode_free(i_node);
                                dnode_free(d);
                                dnode_free(d_temp);
                                indirect_free(indr);
                                indirect_free(indr2);
                                free(name);
                                free(pathcpy);

                                return copied;
                        }
                }
                else if (lvl == 0) { // if all of the direct blocks have been read switch to next blocknum
                        lvl++;
                        if (i_node->single_indirect.valid) {
                                bufdread(i_node->single_indirect.block, (char*) indr, sizeof(indirect));
                                dbs = indr->blocks;
                                cur_b = 0;
                                blocks_read = 128;
                        }
                        else { // free and return
                                inode_free(i_node);
                                dnode_free(d);
                                dnode_free(d_temp);
                                indirect_free(indr);
                                indirect_free(indr2);
                                free(name);
                                free(pathcpy);

                                return copied;
                        }
                }
                
                else if (lvl == 1) { // if next level then move indirection level
                        lvl++;
                        if (i_node->double_indirect.valid) {
                                bufdread(i_node->double_indirect.block, (char *) indr2, sizeof(indirect));
                                if (indr2->blocks[j].valid) {
                                        bufdread(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                        dbs = indr->blocks;
                                        cur_b = 0;
                                        blocks_read = 128;
                                        j++;
                                }
                                else { // free and return
                                        inode_free(i_node);
                                        dnode_free(d);
                                        dnode_free(d_temp);
                                        indirect_free(indr);
                                        indirect_free(indr2);
                                        free(name);
                                        free(pathcpy);

                                        return copied;
                                }
                        }
                        else { // free and return
                                inode_free(i_node);
                                dnode_free(d);
                                dnode_free(d_temp);
                                indirect_free(indr);
                                indirect_free(indr2);
                                free(name);
                                free(pathcpy);
                                return copied;
                        }   
                }
                // if need to move within double indirect
                else if (lvl == 2) { // if we need to move within indirection levels 
                        if (j < 128 && indr2->blocks[j].valid) {
                                if (indr2->blocks[j].valid) {
                                        bufdread(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                        dbs = indr->blocks;
                                        cur_b = 0;
                                        blocks_read = 128;
                                        j++;
                                }
                                else { // free and return
                                        inode_free(i_node);
                                        dnode_free(d);
                                        dnode_free(d_temp);
                                        indirect_free(indr);
                                        indirect_free(indr2);
                                        free(name);
                                        free(pathcpy);

                                        return copied;
                                }
                        }
                        else { // free + return
                                inode_free(i_node);
                                dnode_free(d);
                                dnode_free(d_temp);
                                indirect_free(indr);
                                indirect_free(indr2);
                                free(name);
                                free(pathcpy);
                                return copied;
                        }
                }
        }

        // free and return
        inode_free(i_node);
        dnode_free(d);
        dnode_free(d_temp);
        indirect_free(indr);
        indirect_free(indr2);
	free(name);
	free(pathcpy);

        return copied;
}

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'.
 * It should do so starting at the specified offset 'offset'.  If
 * offset is beyond the current size of the file, you should pad the
 * file with 0s until you reach the appropriate length.
 *
 * You should return the number of bytes written.
 *
 * HINT: Ignore 'fi'
 */
static int vfs_write(const char *path, const char *buf, size_t size,
					 off_t offset, struct fuse_file_info *fi)
{
    UNUSED(fi);
	// Move down path
	// Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
			printf("Error seperating the path and filename\n");
			free(pathcpy);
			free(name);
			return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));

	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(path, "")) {
			// If path isnt the root directory
			// Transforms d to the correct directory
			if(findDNODE(d, pathcpy, &dirBlock)) {
					// If directory could not be found
					printf("Could not find directory\n");
					dnode_free(d);
					free(pathcpy);
					free(name);
					return -1;
			}
	}

	// Find node
	inode *i_node = inode_create(0, 0, 0, 0);
	dnode *d_temp = dnode_create(0, 0, 0, 0);
	blocknum block; 
	int ret = getNODE(d, name, d_temp, i_node, &block, 0, 0, 0, "");
	if (ret != 1) { // if didnt find matching file node
		inode_free(i_node);
		dnode_free(d_temp);
		dnode_free(d);
		free(name);
		free(pathcpy);
		printf("Could not find file to write to or file is a directory");
		return -1;
	}
	dnode_free(d_temp);
        // Actual unique write code starts here

        unsigned int written = 0;
	int blocks = ((int)offset)/sizeof(db); // calculate num blocks to look in
	int block_offset = ((int)offset) % sizeof(db); // calculate offset into 
	char tmp[sizeof(db)] = {0};
        indirect *indr = indirect_create();
        indirect *indr2 = indirect_create();
        int cur_b = 0, blocks_write = 110;
        int j = 0, lvl = 0, i = 0;
        int fr;
        blocknum * dbs;
        unsigned int usable_offset;

        // calculate real offsets
        if (offset > i_node->size) { // if our offset is past the end of our file
                usable_offset = i_node->size; // we need to start from the end of the file instead
                for (i = 0; i < (int)sizeof(db); i++) { // set tmp to all zeroes
                        tmp[i] = 0;
                }
                blocks = (i_node->size)/sizeof(db);
                block_offset = (i_node->size)%sizeof(db);
        }
        else { // otherwise we just keep the give offset
                usable_offset = offset;
        }

        // calculate starting blocknum, set cur_b, dbs, lvl, j, etc accordingly
        if (blocks < 110) { // in normal direct blocks
                dbs = i_node->direct;
                cur_b = blocks;
        }
        else if (blocks >= 110 && blocks < 238) { // in single indirects
                lvl++;
                if (i_node->single_indirect.valid) {
                        bufdread(i_node->single_indirect.block, (char *) indr, sizeof(indirect));
                        dbs = indr->blocks;
                        cur_b = blocks - 110;
                        blocks_write = 128;
                }
                else { // there should never be an else?
                        // free and return???
                }
        }
        else if (blocks < 16622) { // double indirect magic numbers
                lvl++; // these two lines are the best in the program
                lvl++; // lol
                if (i_node->double_indirect.valid) {
                        bufdread(i_node->double_indirect.block, (char *) indr2, sizeof(indirect));
                        j = (blocks - 238) / 128;
                        if (indr2->blocks[j].valid) {
                                bufdread(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                dbs = indr->blocks;
                                cur_b = blocks - 238 - (j * 128);
                                blocks_write = 128;
                        }
                        else { // once again an else that should never happen
                        }
                }
                else { // should never happen 
                }
        }
        else {
                printf("Error: File offset too large, file size not supported\n");
                return -ENOSPC;
        }

        // write data blocks
        while (usable_offset + written < offset + size) { // while there are still more blocks to write
                if (cur_b < blocks_write) { // if there are more blocks to check in dbs
                        if (dbs[cur_b].valid) { // if the current block is valid we can use it
                                if (usable_offset < offset) { // only read in part that needs to be nonzero
                                        bufdread(dbs[cur_b].block, (char *) tmp, block_offset);
                                }
                                else {
                                        bufdread(dbs[cur_b].block, (char *) tmp, sizeof(db)); // read in data block
                                }
                        }
                        else { // otherwise need to get a free block to write to
                                if ((fr = getNextFree(v)) >= 0) {
                                        dbs[cur_b] = blocknum_create(getNextFree(v), 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        dnode_free(d);
                                        inode_free(i_node);
                                        indirect_free(indr);
                                        indirect_free(indr2);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }
                        }

                        if (usable_offset + written < offset) { // if we need to write zeroes
                                if ((offset - (usable_offset + written)) >= (int)sizeof(db)) {
                                // if we need to copy tmp to end of block
                                        bufdwrite(dbs[cur_b].block, (char *) tmp, sizeof(db));
                                        written += sizeof(db) - block_offset;
                                        // reset block_offset and reset tmp to all zeroes
                                        for (i = 0; i < block_offset+1; i++) {
                                                tmp[i] = 0;
                                        }
                                        block_offset = 0;
                                }
                                else { // otherwise just copy partially into block
                                        bufdwrite (dbs[cur_b].block, (char *) tmp, offset-usable_offset);
                                        written += offset-usable_offset;
                                        // and reset our block offset
                                        block_offset = ((int)offset) % sizeof(db);
                                }
                        } 
                        else { // can copy actual data instead
                                if (size - written >= sizeof(db)) { // if we have to write until end of block
                                        memcpy(tmp+block_offset, buf+written, sizeof(db)-block_offset);
                                        bufdwrite(dbs[cur_b].block, (char *) tmp, sizeof(db));
                                        written += sizeof(db)-block_offset;
                                }
                                else { // otherwise we just copy a set number of bytes we need
                                        memcpy(tmp+block_offset, buf+written, size-written);
                                        bufdwrite(dbs[cur_b].block, (char *) tmp, sizeof(db));
                                        written += size-written;
                                }
                                block_offset = 0;
                        }
                        cur_b++;
                }
                else if (lvl == 0) {
                        lvl++;
                        // write current dbs data to inode block 
                        for (i = 0; i < 110; i++) {
                                i_node->direct[i] = dbs[i]; 
                        }
                        if (i_node->single_indirect.valid) {
                        // if the single indirect is valid we can use it and dont have to create one
                                bufdread(i_node->single_indirect.block, (char *) indr, sizeof(indirect));
                        }
                        else { // need to set blocks and write for new indirect
                                if ((fr = getNextFree(v)) >= 0) {
                                        i_node->single_indirect = blocknum_create(fr, 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        dnode_free(d);
                                        inode_free(i_node);
                                        indirect_free(indr);
                                        indirect_free(indr2);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }
                                indr->blocks[0] = blocknum_create(0, 0);
                        }
                        dbs = indr->blocks;
                        cur_b = 0; 
                        blocks_write = 128;
                }
                else if (lvl == 1) {
                        lvl++;
                        // write current dbs data to indirect block
                        for (i = 0; i < 128; i++) {
                                indr->blocks[i] = dbs[i];
                        }
                        bufdwrite(i_node->single_indirect.block, (char *) indr, sizeof(indirect));
                        // setup new level of indirection
                        if (i_node->double_indirect.valid) {
                                bufdread(i_node->double_indirect.block, (char *) indr2, sizeof(indirect));
                                if (indr2->blocks[0].valid) {
                                        bufdread(indr2->blocks[0].block ,(char *) indr, sizeof(indirect));
                                }
                                else {
                                        if ((fr = getNextFree(v)) >= 0) {
                                                indr2->blocks[0] = blocknum_create(fr, 1);
                                        }
                                        else {
                                                printf("Error: No disk space available.\n");
                                                dnode_free(d);
                                                inode_free(i_node);
                                                indirect_free(indr);
                                                indirect_free(indr2);
                                                free(pathcpy);
                                                free(name);
                                                return -ENOSPC;
                                        }

                                        free(indr);
                                        indr = indirect_create();
                                        for (i = 0; i < 128; i++) {
                                                indr->blocks[i] = blocknum_create(0, 0);
                                        }
                                        bufdwrite(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                                }
                        }
                        else {
                                if ((fr = getNextFree(v)) >= 0) {
                                        i_node->double_indirect = blocknum_create(fr, 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        dnode_free(d);
                                        inode_free(i_node);
                                        indirect_free(indr);
                                        indirect_free(indr2);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }

                                if ((fr = getNextFree(v)) >= 0) {
                                        indr2->blocks[0] = blocknum_create(fr, 1);
                                }
                                else {
                                        printf("Error: No disk space available.\n");
                                        dnode_free(d);
                                        inode_free(i_node);
                                        indirect_free(indr);
                                        indirect_free(indr2);
                                        free(pathcpy);
                                        free(name);
                                        return -ENOSPC;
                                }

                                free(indr);
                                indr = indirect_create();
                                for (i = 0; i < 128; i++) {
                                        indr->blocks[i] = blocknum_create(0, 0);
                                }
                                bufdwrite(indr2->blocks[0].block, (char *) indr, sizeof(indirect));
                        }

                        dbs = indr->blocks;
                        cur_b = 0;
                        blocks_write = 128;

                }
                else if (lvl == 2) {
                        // write current dbs data to finished single indirect
                        for (i = 0; i < 128; i++) {
                                indr->blocks[i] = dbs[i];
                        }
                        bufdwrite(indr2->blocks[j].block, (char *) indr, sizeof(indirect));

                        j++;

                        if (j < 128) {
                                if (indr2->blocks[j].valid) { // if we have an existing single indirect to use
                                        bufdread(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                }
                                else {
                                        if ((fr = getNextFree(v)) >= 0) {
                                                indr2->blocks[j] = blocknum_create(fr, 1);
                                        }
                                        else {
                                                printf("Error: No disk space available.\n");
                                                dnode_free(d);
                                                inode_free(i_node);
                                                indirect_free(indr);
                                                indirect_free(indr2);
                                                free(pathcpy);
                                                free(name);
                                                return -ENOSPC;
                                        }
                                        free(indr);
                                        indr = indirect_create();
                                        for (i = 0; i < 128; i++) {
                                                indr->blocks[i] = blocknum_create(0, 0);
                                        }
                                        bufdwrite(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                                }
                                cur_b = 0;
                                blocks_write = 128;
                                dbs = indr->blocks;
                        }
                        else { // NO ROOM SO CAN ERROR AND FREE OR WHATEVER
                                lvl++;
                        }
                }
                else { // no room so need to error and free etc.
                        // write level 2 indirect block
                        bufdwrite(i_node->double_indirect.block, (char *) indr2, sizeof(indirect));
                        dnode_free(d);
                        inode_free(i_node);
                        indirect_free(indr);
                        indirect_free(indr2);
                        free(pathcpy);
                        free(name);
                        return -ENOSPC;
                }
        }

        i_node->size += written;

        // handle final writes etc based on level of indirection

        switch (lvl) {
                case 0:
                        // probably dont need this for loop...
                        for (i = 0; i < 110; i++) {
                                i_node->direct[i] = dbs[i]; 
                        }

                        bufdwrite(block.block, (char *) i_node, sizeof(inode));
                        break;

                case 1:
                        bufdwrite(block.block, (char *) i_node, sizeof(inode));
                        // write current dbs data to indirect block
                        for (i = 0; i < 128; i++) {
                                indr->blocks[i] = dbs[i];
                        }
                        bufdwrite(i_node->single_indirect.block, (char *) indr, sizeof(indirect));

                        break;

                case 2:
                        // write current dbs data to finished single indirect
                        for (i = 0; i < 128; i++) {
                                indr->blocks[i] = dbs[i];
                        }
                        bufdwrite(indr2->blocks[j].block, (char *) indr, sizeof(indirect));
                        bufdwrite(i_node->double_indirect.block, (char *) indr2, sizeof(indirect));
                        bufdwrite(block.block, (char *) i_node, sizeof(inode));

                        break;
        }

        // free and return
        
        dnode_free(d);
        inode_free(i_node);
        indirect_free(indr);
        indirect_free(indr2);
        free(pathcpy);
        free(name);

	return written;
}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{
	/* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
			AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */

	// Move down path
	// Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
			printf("Error seperating the path and filename\n");
			free(pathcpy);
			free(name);
			return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));

	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(path, "")) {
			// If path isnt the root directory
			// Transforms d to the correct directory
			if(findDNODE(d, pathcpy, &dirBlock)) {
					// If directory could not be found
					printf("Could not find directory\n");
					dnode_free(d);
					free(pathcpy);
					free(name);
					return -1;
			}
	}

	// Get correct node
	inode *i_node = inode_create(0, 0, 0, 0);
	dnode *d_temp = dnode_create(0, 0, 0, 0);
	blocknum block;
	// References to inodes are deleted in getNODE function!
	int ret = getNODE(d, name, d_temp, i_node, &block, 1, dirBlock.block, 0, "");
	if (ret != 1) { // if didnt find matching file node
		// what does findNODE return if both match??
		inode_free(i_node);
		dnode_free(d_temp);
		dnode_free(d);
		free(name);
		free(pathcpy);
		printf("Could not find file to delete or file is a directory");
		return -1;
	}

	// Frees data blocks
	int i;
	for (i = 0; i < 110; i++) {
		// i = direct blocks
		// Count number of valid while comparing until all are acocunted for
		if (i_node->direct[i].valid) { 
			// Free data here
			releaseFree(v, i_node->direct[i]);
		}
	}

	if (i_node->single_indirect.valid) {
		// Single indirect
		indirect *ind = indirect_create();
		bufdread(i_node->single_indirect.block, (char *)ind, sizeof(indirect));

		for (i = 0; i < 128; i++) {
			// i = direct blocks
			// Count number of valid while comparing until all are acocunted for

			if (ind->blocks[i].valid) {
				// Free data here
				releaseFree(v, ind->blocks[i]);
			}
		}

		// Free single indirect node
		releaseFree(v, i_node->single_indirect);
		indirect_free(ind);
	}

	if (i_node->double_indirect.valid) {
		// Double indirect
		indirect *firstind = indirect_create();
		bufdread(i_node->double_indirect.block, (char *)firstind, sizeof(indirect));
		
		for (i = 0; i < 128; i++) {
			// i = direct blocks
			// Count number of valid while comparing until all are acocunted for

			if (firstind->blocks[i].valid) {

				indirect *secind = indirect_create();
				bufdread(firstind->blocks[i].block, (char *)secind, sizeof(indirect));

				int k;
				for (k = 0; k < 128; k++) {

					if (secind->blocks[k].valid) {
						// Free data here
						releaseFree(v, secind->blocks[k]);
					}
				}

				// Free inner indirect nodes
				releaseFree(v, firstind->blocks[i]);
				indirect_free(secind);	
			}
		}

		// Free outer indirect nodes
		releaseFree(v, i_node->double_indirect);
		indirect_free(firstind);
	} 

	// Frees inode block itself
	releaseFree(v, block);

	// update vcb
	bufdwrite(0, (char *) v, sizeof(vcb));

	// Free variables
	inode_free(i_node);
	dnode_free(d_temp);
	dnode_free(d);
	free(name);
	free(pathcpy);

	return 0;
}

/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{
	vfs_delete(to);

	// Create modifiable strings
	char *pathcpy = (char *)calloc(strlen(from) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, from);

	char *name = (char *)calloc(strlen(from) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		printf("Error seperating the path and filename\n");
		free(pathcpy);
		free(name);
		return -1;
	}

	// Seperating for the to
	char *pathcpy2 = (char *)calloc(strlen(to) + 1, sizeof(char));
	assert(pathcpy2 != NULL);
	strcpy(pathcpy2, to);

	char *newName = (char *)calloc(strlen(to) + 1, sizeof(char));
	assert(newName != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy2, newName)) {
		printf("Error seperating the path and filename\n");
		free(pathcpy);
		free(name);
		free(pathcpy2);
		free(newName);
		return -1;
	}

	// Make sure paths are the same
	if (strcmp(pathcpy, pathcpy2)) {
		printf("Error paths of old and new file names not the same\n");
		free(pathcpy);
		free(name);
		free(pathcpy2);
		free(newName);
		return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			free(pathcpy);
			free(name);
			dnode_free(d);
			free(pathcpy2);
			free(newName);
			return -1;
		}
	}

	if (strlen(newName) > 27) {
		printf("Error new filename too long\n");
		free(pathcpy);
		free(name);
		dnode_free(d);
		free(pathcpy2);
		free(newName);
		return -ENAMETOOLONG;
	}

	// Get correct node
	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);	
	blocknum block;
	int ret = getNODE(d, name, matchd, matchi, &block, 0, 0, 1, newName);
	
	// Free malloced variables
	dnode_free(d);
	dnode_free(matchd);
	inode_free(matchi);
	free(pathcpy);
	free(name);
	free(pathcpy2);
	free(newName);

	if (ret < 0) {
		return -ENOENT;
	}

	return 0;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *path, mode_t mode)
{
	// Determine correct file and path from the name
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		printf("Error seperating the path and filename\n");
		free(pathcpy);
		free(name);
		return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			dnode_free(d);
			free(pathcpy);
			free(name);
			return -1;
		}
	}

	if (!strcmp(name, "")) {
		// If name is blank, it means that it is refering to the current directory
		strcpy(name, ".");
	}

	// Get correct node
	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, matchd, matchi, &block, 0, 0, 0, "");
	dnode_free(d);
	free(pathcpy);
	free(name);

	// Check to see if match is valid
	if (ret < 0) {
		printf("Could not find file\n");
		dnode_free(matchd);
		inode_free(matchi);
		return -1;
	}
	else if (ret == 0) {
		// If the dirent is for a directory
		matchd->mode = mode; // Directory node
		bufdwrite(block.block, (char *) matchd, sizeof(dnode));
	}
	else if (ret == 1) {
		// If the dirent is for a file
		matchi->mode = mode;
		bufdwrite(block.block, (char *) matchi, sizeof(inode));
	}

	dnode_free(matchd);
	inode_free(matchi);
	return 0;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *path, uid_t uid, gid_t gid)
{
	// Determine correct file and path from the name
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		free(pathcpy);
		free(name);
		printf("Error seperating the path and filename\n");
		return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			free(pathcpy);
			free(name);
			dnode_free(d);
			return -1;
		}
	}

	if (!strcmp(name, "")) {
		// If name is blank, it means that it is refering to the current directory
		strcpy(name, ".");
	}

	// Get correct node
	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, matchd, matchi, &block, 0, 0, 0, "");
	dnode_free(d);
	free(pathcpy);
	free(name);

	// Check to see if match is valid
	if (ret < 0) {
		printf("Could not find file\n");
		dnode_free(matchd);
		inode_free(matchi);
		return -1;
	}
	else if (ret == 0) {
		// If the dirent is for a directory
		matchd->user = uid; // Directory node
		matchd->group = gid;
		bufdwrite(block.block, (char *) matchd, sizeof(dnode));
	}
	else if (ret == 1) {
		// If the dirent is for a file
		matchi->user = uid; // File node
		matchi->group = gid;
		bufdwrite(block.block, (char *) matchi, sizeof(inode));
	}

	dnode_free(matchd);
	inode_free(matchi);
	return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *path, const struct timespec ts[2])
{
	// Determine correct file and path from the name
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		free(pathcpy);
		free(name);
		printf("Error seperating the path and filename\n");
		return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			free(pathcpy);
			free(name);
			dnode_free(d);
			return -1;
		}
	}

	if (!strcmp(name, "")) {
		// If name is blank, it means that it is refering to the current directory
		strcpy(name, ".");
	}

	// Get correct node
	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, matchd, matchi, &block, 0, 0, 0, "");
	dnode_free(d);
	free(pathcpy);
	free(name);

	// Check to see if match is valid
	if (ret < 0) {
		printf("Could not find file\n");
		dnode_free(matchd);
		inode_free(matchi);
		return -1;
	}
	else if (ret == 0) {
		// If the dirent is for a directory
		matchd->access_time = ts[0]; // Directory node
		matchd->modify_time = ts[1];
		bufdwrite(block.block, (char *) matchd, sizeof(dnode));
	}
	else if (ret == 1) {
		// If the dirent is for a file
		matchi->access_time = ts[0]; // File node
		matchi->modify_time = ts[1];
		bufdwrite(block.block, (char *) matchi, sizeof(inode));
	}

	dnode_free(matchd);
	inode_free(matchi);
	return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *path, off_t offset)
{
	// Determine correct file and path from the name
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
		free(pathcpy);
		free(name);
		printf("Error seperating the path and filename\n");
		return -1;
	}

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);
	bufdread(v->root.block, (char *)d, sizeof(dnode));
	blocknum dirBlock = blocknum_create(v->root.block, 1);

	if (strcmp(pathcpy, "/")) {
		// If path isnt the root directory
		// Transforms d to the correct directory
		if(findDNODE(d, pathcpy, &dirBlock)) {
			// If directory could not be found
			printf("Could not find directory\n");
			free(pathcpy);
			free(name);
			dnode_free(d);
			return -1;
		}
	}

	if (!strcmp(name, "")) {
		// If name is blank, it means that it is refering to the current directory
		strcpy(name, ".");
	}

	// Get correct node
	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, matchd, matchi, &block, 0, 0, 0, "");
	dnode_free(d);
	dnode_free(matchd);
	free(pathcpy);
	free(name);

	// Check to see if match is valid
	if (ret < 1) {
		printf("Could not find file\n");
		inode_free(matchi);
		return -1;
	}
	else if (ret == 1) {
		// If the dirent is for a file

		// If size is greater than the file size
		if (matchi->size < offset) {
			printf("Offset is larger than the file\n");
			return -1;
		}
		
		//If the trunction is good
		matchi->size = offset;

		// Count = to last data block valid
		unsigned int count = (offset + BLOCKSIZE - 1)/(BLOCKSIZE);
		while (count < 110) {
			if (matchi->direct[count].valid) {
				// If valid and above cutoff point
				releaseFree(v, matchi->direct[count]);
				matchi->direct[count].valid = 0;
			}
			count++;
		}

		// Single indirection
		if (matchi->single_indirect.valid && count < 128+110) {
			// Will be
			int deleteInd = (count <= 110);

			indirect *ind = indirect_create();
			bufdread(matchi->single_indirect.block, (char *)ind, sizeof(indirect));

			while(count < 128+110) {
				if (ind->blocks[count-110].valid) {
					releaseFree(v, ind->blocks[count-110]);
					ind->blocks[count-110].valid = 0;
				}	
				count++;
			}

			if (deleteInd) {
				releaseFree(v, matchi->single_indirect);
				matchi->single_indirect.valid = 0;
			} else {
				bufdwrite(matchi->single_indirect.block, (char *)ind, sizeof(indirect));
			}

			indirect_free(ind);
		}
		else {
			count += 128;
		}

		// Double indirection
		if (matchi->double_indirect.valid && count < 128*128+128+110) {

			// Will be
			int deleteFirstInd = (count <= 110+128);

			indirect *firstind = indirect_create();
			bufdread(matchi->double_indirect.block, (char *)firstind, sizeof(indirect));

			while(count < 128*128+128+110) {
				// While below limit of direct, first indirect, and second indirect

				if (firstind->blocks[(count-110-128)/128].valid) {
					// If first indirect is valid
					int deleteSecInd = (count <= ((count-110-128)/128)*128 + 110 + 128);

					// Read in second indirect
					indirect *secind = indirect_create();
					bufdread(firstind->blocks[(count-110-128)/128].block, (char *)secind, sizeof(indirect));

					int k;
					for (k = (count-110-128)%128; k < 128; k++) {
						// K is equal to the index in the second indirect block
						if (secind->blocks[k].valid) {
							releaseFree(v, secind->blocks[k]);
						}
						
						count++;
					}

					if (deleteSecInd) {
						releaseFree(v, firstind->blocks[(count-110-128)/128 - 1]);
						firstind->blocks[(count-110-128)/128 - 1].valid = 0;
					} else {
						bufdwrite(firstind->blocks[(count-110-128)/128 - 1].block, (char *)secind, sizeof(indirect));
					}

					indirect_free(secind);
				}
				else {
					count += 128;
				}
			}

			if (deleteFirstInd) {
				releaseFree(v, matchi->double_indirect);
				matchi->double_indirect.valid = 0;
			} else {
				bufdwrite(matchi->double_indirect.block, (char *)firstind, sizeof(indirect));
			}

			indirect_free(firstind);
		}
		else {
			count += 128*128;
		}

		bufdwrite(block.block, (char *) matchi, sizeof(inode));
	}

	inode_free(matchi);
	return 0;
}


/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     
 */
static struct fuse_operations vfs_oper = {
	.init    = vfs_mount,
	.destroy = vfs_unmount,
	.getattr = vfs_getattr,
	.readdir = vfs_readdir,
	.create	 = vfs_create,
	.read	 = vfs_read,
	.write	 = vfs_write,
	.unlink	 = vfs_delete,
	.rename	 = vfs_rename,
	.chmod	 = vfs_chmod,
	.chown	 = vfs_chown,
	.utimens	 = vfs_utimens,
	.truncate	 = vfs_truncate,
	.mkdir	 = vfs_mkdir,
};

int main(int argc, char *argv[]) {
	/* Do not modify this function */
	umask(0);
	if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
		printf("Usage: ./3600fs -s -d <dir>\n");
		exit(-1);
	}
	return fuse_main(argc, argv, &vfs_oper, NULL);
}

// HELPER FUNCTIONS

// Params
// 	path = path name in string that should be passed in and changed
// 	name = char * that will be populated with file name
// Returns
// 	-1 if error
// 	0 if no error
int seperatePathAndName(char *path, char *name) {
	unsigned int i;
	for (i = strlen(path)-1; path[i] != '/'; i--) {
		if (i == 0)
			return -1;
	} 

	unsigned int j;
	for (j = 0; j+i+1 < strlen(path); j++) {
		// Copys string after last backslash to name
		name[j] = path[j+i+1];
	}

	// Null terminates each string
	name[j] = '\0';
	path[i+1] = '\0';

	return 0;
}

// Params
// 	directory = root dnode that will be changed to match
// 	path = path name in string
// Returns
// 	-1 if no match found
// 	0 if match found
int findDNODE(dnode *directory, char *path, blocknum *block) {
	if (path[0]  != '/')
		return -1;
	if (!strcmp(path, "/"))
		return 0;

	char *searchPath = (char *)calloc(strlen(path)+1, sizeof(char));
	assert(searchPath != NULL);

	char *restOfPath = (char *)calloc(strlen(path)+1, sizeof(char));
	assert(restOfPath != NULL);
	restOfPath[0] = '/';

	unsigned int i;
	int hitFirstBackslashFlag = 0;
	for (i = 1; i < strlen(path); i++) {
		if (!hitFirstBackslashFlag) {
			// Add to searchPath
			if (path[i] == '/') {
				// Hit first backslash
				searchPath[i-1] = '\0';
				hitFirstBackslashFlag = 1;
			}
			else {
				// Didnt
				searchPath[i-1] = path[i];
			}
		}
		else {
			// Add to restOfPath
			restOfPath[i-strlen(searchPath)-1] = path[i];
		}
	} 

	if (hitFirstBackslashFlag)
		restOfPath[i-strlen(searchPath)-1] = '\0';
	else
		restOfPath[i-strlen(searchPath)] = '\0';

	// Check in direct
	unsigned int count = 0;
	for (i = 0; count < directory->size && i < 110; i++) {
		// i = direct blocks
		// Count number of valid while comparing until all are acocunted for
		if (directory->direct[i].valid) {
			dirent *de = dirent_create();
			bufdread(directory->direct[i].block, (char *)de, sizeof(dirent));

			int j;
			for (j = 0; count < directory->size && j < 16; j++) {
				// j = direntry entry
				if (de->entries[j].block.valid) {
					count++;
					char *dname = getName(de->entries[j]);
					if ((de->entries[j].type == 0) && (!strcmp(dname, searchPath))) {
						free(dname);
						// If match found, overwrite current dnode
						block->block = de->entries[j].block.block;
						block->valid = de->entries[j].block.valid;

						bufdread(de->entries[j].block.block, (char *)directory, sizeof(dnode));
						free(searchPath);
						dirent_free(de);
						if (hitFirstBackslashFlag) {
							// More to path
							int ret = findDNODE(directory, restOfPath, block);
							free(restOfPath);
							return ret;
						}
						else {
							// Found correct one
							free(restOfPath);
							return 0;
						}
					}
					free(dname);
				}
			}

			dirent_free(de);
		}
	}

	if (directory->single_indirect.valid) {
		// Single indirect
		indirect *ind = indirect_create();
		bufdread(directory->single_indirect.block, (char *)ind, sizeof(indirect));

		for (i = 0; count < directory->size && i < 128; i++) {
			// i = direct blocks
			// Count number of valid while comparing until all are acocunted for
			if (ind->blocks[i].valid) {

				dirent *de = dirent_create();
				bufdread(ind->blocks[i].block, (char *)de, sizeof(dirent));

				int j;
				for (j = 0; count < directory->size && j < 16; j++) {
					// j = direntry entry
					if (de->entries[j].block.valid) {
						count++;
						char *dname = getName(de->entries[j]);
						if ((de->entries[j].type == 0) && (!strcmp(dname, searchPath))) {
							free(dname);
							// If match found, overwrite current dnode
							block->block = de->entries[j].block.block;
							block->valid = de->entries[j].block.valid;

							bufdread(de->entries[j].block.block, (char *)directory, sizeof(dnode));
							free(searchPath);
							dirent_free(de);
							indirect_free(ind);
							if (hitFirstBackslashFlag) {
								// More to path
								int ret = findDNODE(directory, restOfPath, block);
								free(restOfPath);
								return ret;
							}
							else {
								// Found correct one
								free(restOfPath);
								return 0;
							}
						}
						free(dname);
					}
				}

				dirent_free(de);
			}
		}

		indirect_free(ind);
	}

	if (directory->double_indirect.valid) {
		// Double indirect
		indirect *firstind = indirect_create();
		bufdread(directory->double_indirect.block, (char *)firstind, sizeof(indirect));
		
		for (i = 0; count < directory->size && i < 128; i++) {
			// i = direct blocks
			// Count number of valid while comparing until all are acocunted for

			if (firstind->blocks[i].valid) {

				indirect *secind = indirect_create();
				bufdread(firstind->blocks[i].block, (char *)secind, sizeof(indirect));

				int k;
				for (k = 0; count < directory->size && k < 128; k++) {
					if (secind->blocks[k].valid) {
						dirent *de = dirent_create();
						bufdread(secind->blocks[k].block, (char *)de, sizeof(dirent));

						int j;
						for (j = 0; count < directory->size && j < 16; j++) {
							// j = direntry entry
							if (de->entries[j].block.valid) {
								count++;
								char *dname = getName(de->entries[j]);
								if ((de->entries[j].type == 0) && (!strcmp(dname, searchPath))) {
									free(dname);
									// If match found, overwrite current dnode

									block->block = de->entries[j].block.block;
									block->valid = de->entries[j].block.valid;
									
									bufdread(de->entries[j].block.block, (char *)directory, sizeof(dnode));
									// Free variables
									free(searchPath);
									dirent_free(de);
									indirect_free(firstind);
									indirect_free(secind);

									if (hitFirstBackslashFlag) {
										// More to path
										int ret = findDNODE(directory, restOfPath, block);
										free(restOfPath);
										return ret;
									}
									else {
										// Found correct one
										free(restOfPath);
										return 0;
									}
								}
								free(dname);
							}
						}
						dirent_free(de);
					}
				}
				indirect_free(secind);
			}
		}
		indirect_free(firstind);
	} 

	free(searchPath);
	free(restOfPath);

	return -1;
}

// Params
// 	directory = root dnode
// 	name = name of node looking for
// 	searchDnode = dnode will be filled in if found
// 	searchInode = inode will be filled in if found
//  blocknum = blocknum if valid
//  deleteFlag = if match reference should be set to invalid
// Returns
// -1 for not found
// 0 for directory
// 1 for file
int getNODE(dnode *directory, char *name, dnode *searchDnode, inode *searchInode, blocknum *retBlock, int deleteFlag, int directoryBlock, int renameFlag, char *newName) {
	*retBlock = blocknum_create(0, 0);

	direntry dir;
	dir.block.valid = 0;

	// Check in directory
	int i;
	unsigned int count = 0;
	for (i = 0; count < directory->size && i < 110; i++) {
		// i = direct blocks
		// Count number of valid while comparing until all are acocunted for
		if (directory->direct[i].valid) {
			dirent *de = dirent_create();
			bufdread(directory->direct[i].block, (char *)de, sizeof(dirent));

			int j;
			for (j = 0; count < directory->size && j < 16; j++) {
				// j = direntry entry
				if (de->entries[j].block.valid) {
					count++;
					char *dname = getName(de->entries[j]);
					if (!strcmp(name, dname)) {
						// Found it!
						dir = de->entries[j];

						if (dir.type == 0) {
							// If the dirent is for a directory
							bufdread(dir.block.block, (char *)searchDnode, sizeof(dnode));
							retBlock->block = dir.block.block;
							retBlock->valid = 1;
						}
						else {
							// If the dirent is for a file
							bufdread(dir.block.block, (char *)searchInode, sizeof(inode));
							retBlock->block = dir.block.block;
							retBlock->valid = 1;
						}

						if (deleteFlag) {
							int empty = 0;
							int l;
							// Set direntry to invalid
							de->entries[j].block.valid = 0;
							for (l = 0; l < 16; l ++) {
								if (de->entries[l].block.valid) {
									empty++;
								}
							}

							if (empty < 1) {
								// Only direntry is the one that is being deleted
								releaseFree(v, directory->direct[i]);
								directory->direct[i].valid = 0;
							}

							else {
								// More than one valid direntry in dirent
								bufdwrite(directory->direct[i].block, (char *)de, sizeof(dirent));
							}

							directory->size--;
							bufdwrite(directoryBlock, (char *)directory, sizeof(dnode));
						}
						else if (renameFlag) {
							strcpy(dname, newName);
							bufdwrite(directory->direct[i].block, (char *)de, sizeof(dirent));
						}

						dirent_free(de);
						free(dname);

						if (dir.type == 0) {
							// If the dirent is for a directory
							return 0;
						}
						else {
							// If the dirent is for a file
							return 1;
						}
					}
					free(dname);
				}
			}

			dirent_free(de);
		}
	}

	if (directory->single_indirect.valid) {
		// Single indirect
		indirect *ind = indirect_create();
		bufdread(directory->single_indirect.block, (char *)ind, sizeof(indirect));

		for (i = 0; count < directory->size && i < 128; i++) {
			// Cycles through indirect block

			if (ind->blocks[i].valid) {

				dirent *de = dirent_create();
				bufdread(ind->blocks[i].block, (char *)de, sizeof(dirent));

				int j;
				for (j = 0; count < directory->size && j < 16; j++) {
					// j = direntry entry
					if (de->entries[j].block.valid) {
						count++;
						char *dname = getName(de->entries[j]);
						if (!strcmp(dname, name)) {
							// Found it!
							dir = de->entries[j];
							indirect_free(ind);

							if (dir.type == 0) {
								// If the dirent is for a directory
								bufdread(dir.block.block, (char *)searchDnode, sizeof(dnode));
								retBlock->block = dir.block.block;
								retBlock->valid = 1;
							}
							else {
								// If the dirent is for a file
								bufdread(dir.block.block, (char *)searchInode, sizeof(inode));
								retBlock->block = dir.block.block;
								retBlock->valid = 1;
							}

							if (deleteFlag) {
								int empty = 0;
								int l;
								// Set direntry to invalid
								de->entries[j].block.valid = 0;

								// Cycle over dirent to count direntries
								for (l = 0; l < 16; l ++) {
									if (de->entries[l].block.valid) {
										empty++;
									}
								}

								if (empty < 1) {
									// Only direntry is the one that is being deleted
									releaseFree(v, ind->blocks[i]);
									ind->blocks[i].valid = 0;

									// Make sure indirect isn't empty
									int indEmpty = 0;
									int m;
									for (m = 0; m < 128; m++) {
										if (ind->blocks[m].valid) {
											indEmpty++;
										}
									}

									if (indEmpty < 1) {
										// only block in ind was deleted
										releaseFree(v, directory->single_indirect);
										directory->single_indirect.valid = 0;
									}
									else {
										// More blocks in ind
										bufdwrite(directory->single_indirect.block, (char *)ind, sizeof(indirect));
									}

								}
								else {
									// More than one valid direntry in dirent
									bufdwrite(ind->blocks[i].block, (char *)de, sizeof(dirent));
								}

								directory->size--;
								bufdwrite(directoryBlock, (char *)directory, sizeof(dnode));
							}
							else if (renameFlag) {
								strcpy(dname, newName);
								bufdwrite(directory->direct[i].block, (char *)de, sizeof(dirent));
							}

							dirent_free(de);
							free(dname);

							if (dir.type == 0) {
								// If the dirent is for a directory
								return 0;
							}
							else {
								// If the dirent is for a file
								return 1;
							}
						}
						free(dname);
					}
				}

				dirent_free(de);
			}
		}

		indirect_free(ind);
	}

	if (directory->double_indirect.valid) {
		// Double indirect
		indirect *firstind = indirect_create();
		bufdread(directory->double_indirect.block, (char *)firstind, sizeof(indirect));
		
		for (i = 0; count < directory->size && i < 128; i++) {
			// i = direct blocks
			// Count number of valid while comparing until all are acocunted for

			if (firstind->blocks[i].valid) {

				indirect *secind = indirect_create();
				bufdread(firstind->blocks[i].block, (char *)secind, sizeof(indirect));

				int k;
				for (k = 0; count < directory->size && k < 128; k++) {

					if (secind->blocks[k].valid) {

						dirent *de = dirent_create();
						bufdread(secind->blocks[k].block, (char *)de, sizeof(dirent));

						int j;
						for (j = 0; count < directory->size && j < 16; j++) {
							// j = direntry entry
							if (de->entries[j].block.valid) {
								count++;
								char *dname = getName(de->entries[j]);
								if (!strcmp(dname, name)) {
									// Found it!
									dir = de->entries[j];
									indirect_free(firstind);
									indirect_free(secind);

									if (dir.type == 0) {
										// If the dirent is for a directory
										bufdread(dir.block.block, (char *)searchDnode, sizeof(dnode));
										retBlock->block = dir.block.block;
										retBlock->valid = 1;
									}
									else {
										// If the dirent is for a file
										bufdread(dir.block.block, (char *)searchInode, sizeof(inode));
										retBlock->block = dir.block.block;
										retBlock->valid = 1;
									}

									if (deleteFlag) {
										int empty = 0;
										int l;
										// Set direntry to invalid
										de->entries[j].block.valid = 0;

										for (l = 0; l < 16; l ++) {
											if (de->entries[l].block.valid) {
												empty++;
											}
										}

										if (empty < 1) {
											// Only direntry is the one that is being deleted
											releaseFree(v, secind->blocks[k]);
											secind->blocks[k].valid = 0;

											// Make sure second indirect isn't empty
											int indEmpty = 0;
											int m;
											for (m = 0; m < 128; m++) {
												if (secind->blocks[m].valid) {
													indEmpty++;
												}
											}

											if (indEmpty < 1) {
												// only block in ind was deleted
												releaseFree(v, firstind->blocks[i]);
												firstind->blocks[i].valid = 0;

												// Make sure second indirect isn't empty
												int firstindEmpty = 0;
												int n;
												for (n = 0; n < 128; n++) {
													if (firstind->blocks[n].valid) {
														firstindEmpty++;
													}
												}

												if (firstindEmpty < 1) {
													// only block in ind was deleted
													releaseFree(v, directory->double_indirect);
													directory->double_indirect.valid = 0;
												}
												else {
													// More blocks in firstind
													bufdwrite(directory->double_indirect.block, (char *)firstind, sizeof(indirect));
												}
											}
											else {
												// More blocks in secind
												bufdwrite(firstind->blocks[i].block, (char *)secind, sizeof(indirect));
											}

										}
										else {
											// More than one valid direntry in dirent
											de->entries[j].block.valid = 0;
											bufdwrite(directory->direct[i].block, (char *)de, sizeof(dirent));
										}

										directory->size--;
										bufdwrite(directoryBlock, (char *)directory, sizeof(dnode));
									}
									else if (renameFlag) {
										strcpy(dname, newName);
										bufdwrite(directory->direct[i].block, (char *)de, sizeof(dirent));
									}

									dirent_free(de);
									free(dname);

									if (dir.type == 0) {
										// If the dirent is for a directory
										return 0;
									}
									else {
										// If the dirent is for a file
										return 1;
									}
								}
								free(dname);
							}
						}

						dirent_free(de);
					}
				}

				indirect_free(secind);
			}
			
		}

		indirect_free(firstind);
	} 

	return -1;
}

// Params
// 	v = vcb block
// Returns
// 	-1 if no available freeblocks
// 	0-n if freeblock
int getNextFree(vcb *v) {
	int writenum = v->free.block;
	freeblock * f = freeblock_create(blocknum_create(0, 0)); // the next free blocknum
	if (v->free.valid)
		bufdread(v->free.block, (char *) f, sizeof(freeblock));
	else
		return -1;
	v->free = f->next;
	free(f);
	return writenum;
}


// Frees the block and adds it to the front of the vcb's free list
int releaseFree(vcb *v, blocknum block) {
	freeblock *f = freeblock_create(v->free);
	bufdwrite(block.block, (char *) f, sizeof(freeblock));
	block.valid = 1;
	v->free = block;
	freeblock_free(f);
	return 0;
}
