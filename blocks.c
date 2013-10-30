#include "3600fs.h"

#define CACHE_SIZE 100

const int DISKNUMBER = 13371337;

// Cache!!!
blocknum cacheBlockNum [CACHE_SIZE];
char cacheBlock [CACHE_SIZE*512];
int cacheOrder [CACHE_SIZE];

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

    strncpy(s->name, name, 491);
    s->name[490] = '\0';

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

    setName(&s, name);
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
char *getName(direntry direntry) {
    char *name = (char *)calloc(28, sizeof(char));
    assert(name != NULL);

    strncpy(name, direntry.name, 27);
    name[27] = '\0';

    return name;
}

void setName(direntry *direntry, char *name) {
    strncpy(direntry->name, name, 27);
}

void disk_crash() {
    printf("ERROR: Your disk image crashed\n");
    exit(1);
}

int bufdread(int blocknum, char * buf, int size) {
    char tmp[BLOCKSIZE];
    memset(tmp, 0, BLOCKSIZE);

    int ret = BLOCKSIZE;

    // Deal with cache
    int i;
    int hit = 0;
    for(i = 0; i < CACHE_SIZE; i++) {
        if (cacheBlockNum[i].valid && cacheBlockNum[i].block == blocknum) {
            // Hit
            memcpy(buf, &cacheBlock[i*BLOCKSIZE], size);

            int j;
            for (j = 0; j < CACHE_SIZE; j++) {
                if (j != i && cacheOrder[j] < cacheOrder[i])
                    cacheOrder[j]++;
            }

            cacheOrder[i] = 0;

            hit = 1;
            break;
        }
    }

    if (hit == 0) {
        // Miss
        ret = dread(blocknum, tmp);
        if (ret < 0)
            disk_crash();

        memcpy(buf, tmp, size);

        // Add to cache
        int j = 0;
        while(cacheOrder[j] != CACHE_SIZE-1) {
            j++;
        }

        memcpy(&cacheBlock[j*BLOCKSIZE], buf, size);

        cacheBlockNum[j] = blocknum_create(blocknum, 1);

        int k = 0;
        for (k = 0; k < CACHE_SIZE; k++) {
            if (k != j && cacheOrder[k] < cacheOrder[j])
                cacheOrder[k]++;
        }

        cacheOrder[j] = 0;
    }

    return ret;
}

int bufdwrite(int blocknum, const char * buf, int size) {
    char tmp[BLOCKSIZE];
    memset(tmp, 0, BLOCKSIZE);
    memcpy(tmp, buf, size);

    int ret = dwrite(blocknum, tmp);
    if (ret < 0)
        disk_crash();

    int i;
    for(i = 0; i < CACHE_SIZE; i++) {
        if (cacheBlockNum[i].valid && cacheBlockNum[i].block == blocknum) {
            // Hit
            memcpy(&cacheBlock[i*BLOCKSIZE], buf, size);
            break;
        }
    }

    return ret;
}

void initCache() {
    int i;
    for (i = 0; i < CACHE_SIZE; i++)
        cacheOrder[i] = CACHE_SIZE - 1 - i;
}
