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

	direntry dir;
	dir.block.valid = 0;

	// Read vcb
	dnode *d = dnode_create(0, 0, 0, 0);

	int ret = bufdread(v->root.block, (char *)d, sizeof(dnode));

	// Check in direct
	int i;
	unsigned int count = 0;
	for (i = 0; count < d->size && i < 110 && !dir.block.valid; i++) {
		// i = direct blocks
		// Count number of valid while comparing until all are acocunted for
		dirent *de = dirent_create();

		bufdread(d->direct[i].block, (char *)de, sizeof(dirent));

		int j;
		for (j = 0; j < 16; j++) {
			// j = direntry entry
			if (de->entries[i].block.valid) {
				count++;
				if (!strcmp(path, de->entries[i].name))
					dir = de->entries[i];
					break;
			}
		}

		dirent_free(de);
	}

	if (d->size > 16*110 && !dir.block.valid) {
		// Single indirect
	}

	if (d->size > 16*110+16*128 && !dir.block.valid) {
		// Double indirect
	} 

	dnode_free(d);

	// Check to see if match is valid
	if (!dir.block.valid) {
		return ENOENT;
	}

	dnode *matchd = dnode_create(0, 0, 0, 0);
	inode *matchi = inode_create(0, 0, 0, 0);

	if (dir.type == 0) {
		// If the dirent is for a directory
		bufdread(dir.block.block, (char *)matchd, sizeof(dnode));

		stbuf->st_mode    = matchd->mode | S_IFDIR; // Directory node
		stbuf->st_uid     = matchd->user; // directory uid
		stbuf->st_gid     = matchd->group; // directory gid
		stbuf->st_atime   = matchd->access_time.tv_sec; // access time
		stbuf->st_mtime   = matchd->modify_time.tv_sec; // modify time
		stbuf->st_ctime   = matchd->create_time.tv_sec; // create time
		stbuf->st_size    = BLOCKSIZE*matchd->size; // directory size
		stbuf->st_blocks  = matchd->size; // directory size in blocks
	}
	else {
		// If the dirent is for a file
		bufdread(dir.block.block, (char *)matchi, sizeof(inode));

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

	return 0;
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    //int p = 0; // path dir index
    blocknum root = v->root; // root block number
    dnode *d = dnode_create(0, 0, 0, 0);
    bufdread(root.block, (char *) d, BLOCKSIZE);
    int d_last_eb = 0;
    while (d->direct[d_last_eb].valid) { // find last directory entry block
        d_last_eb++;
    }
    d_last_eb--;
    // read directory entry block
    dirent *de = dirent_create();
    bufdread(d->direct[d_last_eb].block, (char *) de, BLOCKSIZE);

    int d_last_ent = 0;
    while (de->entries[d_last_ent].block.valid) {
        d_last_ent++;
    }
    path++; // move up path pointer to remove inital /
    strcpy(de->entries[d_last_ent].name, path);
    de->entries[d_last_ent].type = 1;
    de->entries[d_last_ent].block = blocknum_create(v->free.block, 1);
    bufdwrite(d->direct[d_last_eb].block, (char *) de, BLOCKSIZE);

    d->size =+ 1;

    int writenum = v->free.block; // this could be turned into a function which returns
    freeblock * f = freeblock_create(blocknum_create(0, 0)); // the next free blocknum
    bufdread(v->free.block, (char *) f, BLOCKSIZE);
    v->free = f->next;
    free(f);
    
    inode * i = inode_create(0, geteuid(), getegid(), mode);
    clock_gettime(CLOCK_REALTIME, &(d->create_time));
    clock_gettime(CLOCK_REALTIME, &(d->access_time));
    clock_gettime(CLOCK_REALTIME, &(d->modify_time));
    i->direct[0] = v->free;
    i->single_indirect = blocknum_create(0, 0);
    i->double_indirect = blocknum_create(0, 0);
    bufdwrite(writenum, (char *) i, BLOCKSIZE);


    //writenum = v->free.block; // this could be turned into a function which returns
    f = freeblock_create(blocknum_create(0, 0)); // the next free blocknum
    bufdread(v->free.block, (char *) f, BLOCKSIZE);
    v->free = f->next;
    free(f);

    bufdwrite(0, (char *) v, BLOCKSIZE);
    // write new file info to directory

    // write file to free blocks

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
