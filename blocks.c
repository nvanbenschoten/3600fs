#include "3600fs.h"

const int DISKNUMBER = 13371337;

// Constructors
blocknum blocknum_create(int num, unsigned int valid) {
    blocknum s;

    s.block = num;
    s.valid = valid;

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
    s.name[26] = '\0';
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


// Helper functions
void disk_crash() {
    printf("ERROR: Your disk image crashed\n");
    exit(1);
}

int bufdread(int blocknum, char * buf, int size) {
    char tmp[BLOCKSIZE];
    memset(tmp, 0, BLOCKSIZE);

    int ret = dread(blocknum, tmp);
    if (ret < 0)
        disk_crash();

    memcpy(buf, tmp, size);

    return ret;
}

int bufdwrite(int blocknum, const char * buf, int size) {
    char tmp[BLOCKSIZE];
    memset(tmp, 0, BLOCKSIZE);
    memcpy(tmp, buf, size);

    int ret = dwrite(blocknum, tmp);
    if (ret < 0)
        disk_crash();

    return ret;
}
