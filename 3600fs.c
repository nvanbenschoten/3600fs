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
	fprintf(stderr, "vfs_mount called\n");

	// Do not touch or move this code; connects the disk
	dconnect();

	/* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
		AND LOAD ANY DATA STRUCTURES INTO MEMORY */

	v = vcb_create(0, "");

	bufdread(0, (char *)v, sizeof(vcb));

	// Check integrity of vcb
	if (v->magic != DISKNUMBER) {
		printf("%s\n", "The disk connected was not the correct disk");
		dunconnect();
	}


	return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
	fprintf(stderr, "vfs_unmount called\n");

	/* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
		ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
		KEEP DATA CACHED THAT'S NOT ON DISK */

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

	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
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
			return -1;
		}
	}

	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);

	blocknum block;
	int ret = getNODE(d, name, matchd, matchi, &block, 0);
	dnode_free(d);

	// Check to see if match is valid
	if (ret < 0) {
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
/*
 * NOTE: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE 
 *       EXTRA CREDIT PORTION OF THE PROJECT.  IF SO, YOU SHOULD
 *       UN-COMMENT THIS METHOD.
static int vfs_mkdir(const char *path, mode_t mode) {

  return -1;
} */

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
	if (path[0] != '/')
		return -1;

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
			return -1;
		}
	}

	// Check in direct
	unsigned int count = offset;
	while (count < 110*16) {

		if (d->direct[count/16].valid) {

			dirent *de = dirent_create();
			bufdread(d->direct[count/16].block, (char *)de, sizeof(dirent));

			int j;
			for (j = count%16; j < 16; j++) {
				// j = direntry entry
				count++;
				printf("Direct nodes %i : %i\n", count, j);
				if (de->entries[j].block.valid) {
					// If the entry is valid
					if(filler(buf, de->entries[j].name, NULL, count)) {
						dirent_free(de);
						dnode_free(d);
						return 1;
					}
				}
			}

			dirent_free(de);
		}
		else {
			count += 16;
		}
	}


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
				printf("Indirect nodes %i : %i\n", count, j);
				if (de->entries[j].block.valid) {
					// If the entry is valid
					if(filler(buf, de->entries[j].name, NULL, count)) {
						indirect_free(ind);
						dirent_free(de);
						dnode_free(d);
						return 1;
					}
				}
			}

			dirent_free(de);
		}
		else {
			count += 16;
		}
	}

	indirect_free(ind);



	indirect *firstind = indirect_create();
	bufdread(d->double_indirect.block, (char *)firstind, sizeof(indirect));
	while(count < 128*128*16+128*16+110*16) {
		
		if (firstind->blocks[(count-110*16-128*16)/128].valid) {

			indirect *secind = indirect_create();
			bufdread(firstind->blocks[(count-110*16-128*16)/128].block, (char *)secind, sizeof(indirect));

			int k;
			for (k = (count-110*16-128*16)%128; k < 128; k++) {

				if (secind->blocks[(count-110*16-128*16)/128/16].valid) {

					dirent *de = dirent_create();
					bufdread(secind->blocks[(count-110*16-128*16)/128/16].block, (char *)de, sizeof(dirent));

					int j;
					for (j = (count-110*16-128*16-k*128)%16; j < 16; j++) {
						// j = direntry entry
						count++;
						printf("Double Indirect nodes %i : %i : %i\n", count, k, j);
						if (de->entries[j].block.valid) {
							// If the entry is valid
							if(filler(buf, de->entries[j].name, NULL, count)) {
								indirect_free(firstind);
								indirect_free(secind);
								dirent_free(de);
								dnode_free(d);
								return 1;
							}
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

	dnode_free(d);

	return 0;
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Move down path
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
			printf("Error seperating the path and filename\n");
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
					// If ditectory could not be found
					printf("Could not find directory\n");
					return -1;
			}
	}

	// check if file already exists
	dnode * temp_d = dnode_create(0, 0, 0, 0);
	inode * temp_i = inode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, temp_d, temp_i, &block, 0);
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

	// What if the dirent is only half full of valid direntries?
	// Should we fix that to have more efficient memory storage?
	// Also, what if there are no dirents with empty slots and we need to allocate a new one?
	int d_last_eb = 0;
	while (d->direct[d_last_eb].valid) { // find last directory entry block
		d_last_eb++;
	}
	d_last_eb--;

	// read directory entry block
	dirent *de = dirent_create();
	bufdread(d->direct[d_last_eb].block, (char *) de, sizeof(dirent));

	int d_last_ent = 0;
	while (((unsigned int)d_last_ent < (unsigned int)sizeof(dirent)/sizeof(direntry)) &&
		de->entries[d_last_ent].block.valid) {
		d_last_ent++;
	}
	if (d_last_ent == sizeof(dirent)/sizeof(direntry)) { // if need to allocate a new dirent block
		// modify d_last_eb and d_last_ent to point to new block & entry
		while (d->direct[d_last_eb].valid) {
			// COMMENT This is going to have to be recursive for it to truly work
			d_last_eb++;
		}
		d->direct[d_last_eb] = v->free; // set next dirent blocknum to be next free block
		dirent *de_new = dirent_create(); // create new dirent
		free(de); // free the old dirent as we are replacing it
		de = de_new; // reassign old dirent to new dirent
		d_last_ent = 0; // set last entry to be first entry because new dirent
		d->size += 1; // add new block to size
		
		if (getNextFree(v) < 0) {
			printf("No free blocks available\n");
			return -1;
		}
		
		// if all those are valid go to next indirect etc
		// could probably use find next invalid function, will run into problems with reusing
		// dirent blocks as well...
	}
	
	// write info to dirent block
	strcpy(de->entries[d_last_ent].name, name);
	de->entries[d_last_ent].type = 1;
	de->entries[d_last_ent].block = blocknum_create(v->free.block, 1);
	bufdwrite(d->direct[d_last_eb].block, (char *) de, sizeof(dirent));

	// Update dnode
	d->size += 1;
	bufdwrite(dirBlock.block, (char *) d, sizeof(dnode));

	int writenum;// = getNextFree(v);
	if ((writenum = getNextFree(v)) < 0) {
		// Could not get next free block
		printf("No free blocks available\n");
	return -1;
	}
	
	inode * i = inode_create(0, geteuid(), getegid(), mode);
	clock_gettime(CLOCK_REALTIME, &(d->create_time));
	clock_gettime(CLOCK_REALTIME, &(d->access_time));
	clock_gettime(CLOCK_REALTIME, &(d->modify_time));
	i->direct[0] = v->free;
	i->single_indirect = blocknum_create(0, 0);
	i->double_indirect = blocknum_create(0, 0);
	bufdwrite(writenum, (char *) i, sizeof(inode));

	getNextFree(v);

	// update vcb
	bufdwrite(0, (char *) v, sizeof(vcb));

	// write new file info to directory

	// free all the stuff
	dnode_free(d);
	dirent_free(de);
	inode_free(i);

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

	return 0;
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

	/* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
		MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */

	return 0;
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
	char *pathcpy = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(pathcpy != NULL);
	strcpy(pathcpy, path);

	char *name = (char *)calloc(strlen(path) + 1, sizeof(char));
	assert(name != NULL);

	// Seperating the directory name from the file/directory name
	if (seperatePathAndName(pathcpy, name)) {
			printf("Error seperating the path and filename\n");
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
					return -1;
			}
	}

	inode *i_node = inode_create(0, 0, 0, 0);
	dnode *d_temp = dnode_create(0, 0, 0, 0);
	blocknum block;
	int ret = getNODE(d, name, d_temp, i_node, &block, 1);
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
	unsigned int count = 0;
	for (i = 0; count < i_node->size && i < 110; i++) {
		// i = direct blocks
		// Count number of valid while comparing until all are acocunted for
		if (i_node->direct[i].valid) {
			count++;   
			// Free data here
			releaseFree(v, i_node->direct[i]);
		}
	}

	// -> then need to also do same for single and double indirects...


	// Frees inode block itself
	releaseFree(v, block);

	// update vcb
	bufdwrite(0, (char *) v, sizeof(vcb));

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
static int vfs_chmod(const char *file, mode_t mode)
{

	return 0;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{

	return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{

	return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{

	/* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
		   BE AVAILABLE FOR OTHER FILES TO USE. */

	return 0;
}


/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
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

	restOfPath[i-strlen(searchPath)-1] = '\0';

	// Check in direct
	unsigned int count = 0;
	for (i = 0; count < directory->size && i < 110; i++) {
		// i = direct blocks
		// Count number of valid while comparing until all are acocunted for
		dirent *de = dirent_create();

		bufdread(directory->direct[i].block, (char *)de, sizeof(dirent));

		int j;
		for (j = 0; count < directory->size && j < 16; j++) {
			// j = direntry entry
			if (de->entries[j].block.valid) {
				count++;
				if ((de->entries[j].type = 0) && (!strcmp(de->entries[j].name, searchPath))) {
					// If match found, overwrite current dnode
					block->block = de->entries[j].block.block;
					block->block = de->entries[j].block.valid;

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
			}
		}

		dirent_free(de);
	}

	if (directory->size > 16*110) {
		// Single indirect
		indirect *ind = indirect_create();
		bufdread(directory->single_indirect.block, (char *)ind, sizeof(indirect));

		for (i = 0; count < directory->size && i < 128; i++) {
			// i = direct blocks
			// Count number of valid while comparing until all are acocunted for

			dirent *de = dirent_create();
			bufdread(ind->blocks[i].block, (char *)de, sizeof(dirent));

			int j;
			for (j = 0; count < directory->size && j < 16; j++) {
				// j = direntry entry
				if (de->entries[j].block.valid) {
					count++;
					if ((de->entries[j].type = 0) && (!strcmp(de->entries[j].name, searchPath))) {
						// If match found, overwrite current dnode
						block->block = de->entries[j].block.block;
						block->block = de->entries[j].block.valid;

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
				}
			}

			dirent_free(de);
		}

		indirect_free(ind);
	}

	if (directory->size > 16*110+16*128) {
		// Double indirect
		indirect *firstind = indirect_create();
		bufdread(directory->double_indirect.block, (char *)firstind, sizeof(indirect));
		
		for (i = 0; count < directory->size && i < 128; i++) {
			// i = direct blocks
			// Count number of valid while comparing until all are acocunted for

			indirect *secind = indirect_create();
			bufdread(firstind->blocks[i].block, (char *)secind, sizeof(indirect));

			int k;
			for (k = 0; count < directory->size && k < 128; k++) {
				dirent *de = dirent_create();
				bufdread(secind->blocks[k].block, (char *)de, sizeof(dirent));

				int j;
				for (j = 0; count < directory->size && j < 16; j++) {
					// j = direntry entry
					if (de->entries[j].block.valid) {
						count++;
						if ((de->entries[j].type = 0) && (!strcmp(de->entries[j].name, searchPath))) {
							// If match found, overwrite current dnode

							block->block = de->entries[j].block.block;
							block->block = de->entries[j].block.valid;
							
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
					}
				}

				dirent_free(de);
			}

			indirect_free(secind);
			
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
int getNODE(dnode *directory, char *name, dnode *searchDnode, inode *searchInode, blocknum *retBlock, int deleteFlag) {
	*retBlock = blocknum_create(0, 0);

	direntry dir;
	dir.block.valid = 0;

	// Check in directory
	int i;
	unsigned int count = 0;
	for (i = 0; count < directory->size && i < 110; i++) {
		// i = direct blocks
		// Count number of valid while comparing until all are acocunted for
		dirent *de = dirent_create();

		bufdread(directory->direct[i].block, (char *)de, sizeof(dirent));

		int j;
		for (j = 0; count < directory->size && j < 16; j++) {
			// j = direntry entry
			if (de->entries[j].block.valid) {
				count++;
				if (!strcmp(name, de->entries[j].name))
					// Found it!
					dir = de->entries[j];
					dirent_free(de);

					if (deleteFlag)
						dir.block.valid = 0;

					if (dir.type == 0) {
						// If the dirent is for a directory
						bufdread(dir.block.block, (char *)searchDnode, sizeof(dnode));
						retBlock->block = dir.block.block;
						retBlock->valid = 1;
						return 0;
					}
					else {
						// If the dirent is for a file
						bufdread(dir.block.block, (char *)searchInode, sizeof(inode));
						retBlock->block = dir.block.block;
						retBlock->valid = 1;
						return 1;
					}
			}
		}

		dirent_free(de);
	}

	if (directory->size > 16*110) {
		// Single indirect
	}

	if (directory->size > 16*110+16*128) {
		// Double indirect
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
	bufdread(v->free.block, (char *) f, sizeof(freeblock));
	v->free = f->next;
	free(f);
	return writenum;
}

int releaseFree(vcb *v, blocknum block) {
	freeblock *f = freeblock_create(v->free);
	bufdwrite(block.block, (char *) f, sizeof(freeblock));
	block.valid = 1;
	v->free = block;
	freeblock_free(f);
	return 0;
}
