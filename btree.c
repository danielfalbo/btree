#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/errno.h>

#define DB_FILENAME "database.db"
#define IDEAL_PAGE_SIZE_BYTES 4096

/* ======================== Data structures ======================= */

typedef struct list {
    unsigned int *data;
    size_t len;
} list;

#define STR_LEN 58
typedef struct dataEntry {
    unsigned int id;
    char name[STR_LEN];
    char email[STR_LEN];
} dataEntry;

#define PAGE_TYPE_DATA    0
#define PAGE_TYPE_BTREE   1

#define ROWS_PER_PAGE (IDEAL_PAGE_SIZE_BYTES / sizeof(dataEntry))

// #define BTREE_MAX_KEYS 338
#define BTREE_MAX_KEYS 4

typedef struct page {
    int type; // PAGE_TYPE_*

    /* For data nodes: count of rows actually present in the buffer,
     * len <= ROWS_PER_PAGE.
     *
     * For btree nodes: count of keys actually present in the node,
     * len <= BTREE_MAX_KEYS.
     *
     * For all btree node properties we allocate 1 more slot so when nodes
     * overflow we can still use them as temporary cache while popping the
     * middle element up to the parent node.
     * */
    size_t len;

    union {
        struct {
            /* Buffer of ROWS_PER_PAGE entries. */
            dataEntry rows[ROWS_PER_PAGE];
        } data;

        struct {

            /* Ids of entries whose pointers are stored within this node. */
            unsigned int keys[BTREE_MAX_KEYS+1];

            /* Indices of disk pages containing entries data.
            *
            * Data entry for 'keys[i]' will be at 'values[i]'th page on disk.
            *
            * It is up to the user to multiply this index by sizeof(page)
            * when seeking to the disk location. */
            unsigned int values[BTREE_MAX_KEYS+1];

            /* Indices of disk pages containing this node's children.
            *
            * 'children[i]' will be the b-subtree of keys larger than
            * 'keys[i-1]' and smaller than 'keys[i]'
            *
            *          keys[0]     keys[i]     keys[2]    keys[3]
            *       /           /           /           /          \
            *      /           /           /           /            \
            * children[0] children[1] children[2] children[3]  children[4]
            *
            * It is up to the user to multiply this index by sizeof(page)
            * when seeking to the disk location. */
            unsigned int children[BTREE_MAX_KEYS+2];
        } node;
    };
} page;

/* We will make sure the btree root node is always the first page on disk. */
#define BTREE_ROOT_PAGE_INDEX 0

/* Root can never be a child, so we also use it as null child pointer. */
#define NULL_CHILD BTREE_ROOT_PAGE_INDEX

void printConfiguration(void) {
    fprintf(stdout, "DB_FILENAME: %s\n", DB_FILENAME);
    fprintf(stdout, "STR_LEN: %d\n", STR_LEN);
    fprintf(stdout, "sizeof(dataEntry): %lu\n", sizeof(dataEntry));
    fprintf(stdout, "IDEAL_PAGE_SIZE_BYTES: %d\n", IDEAL_PAGE_SIZE_BYTES);
    fprintf(stdout, "sizeof(page): %lu\n", sizeof(page));
    fprintf(stdout, "ROWS_PER_PAGE: %lu\n", ROWS_PER_PAGE);
    fprintf(stdout, "BTREE_MAX_KEYS: %d\n", BTREE_MAX_KEYS);
}

/* =================== Graceful exit on error ===================== */

/* Print formatted string to stderr and exit with error code 1. */
void dieWithHonor(char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    exit(1);
}

/* =================== Allocation wrappers ======================== */

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        dieWithHonor("Out of memory allocating %zu bytes\n", size);
    }
    return ptr;
}

void *xrealloc(void *oldptr, size_t size) {
    void *ptr = realloc(oldptr, size);
    if (ptr == NULL) {
        dieWithHonor("Out of memory reallocating %zu bytes\n", size);
    }
    return ptr;
}

/* ================== Objects related functions =================== */

/* Allocate and initialize a new page object. */
page *createPage(int type) {
    page *o = xmalloc(sizeof(page));
    o->len = 0;
    o->type = type;
    return o;
}

page *createDataPage(void) {
    page *o = createPage(PAGE_TYPE_DATA);
    return o;
}

page *createBtreePage(void) {
    page *o = createPage(PAGE_TYPE_BTREE);
    o->node.children[0] = NULL_CHILD;
    return o;
}

list *createList(void) {
    list *o = xmalloc(sizeof(list));
    o->data = NULL;
    o->len = 0;
    return o;
}

/* ===================== List object ============================== */

/* Add the new element at the end of the list 'l'. */
void listPush(list *l, unsigned int x) {
    l->data = xrealloc(l->data, sizeof(int) * (l->len+1));
    l->data[l->len] = x;
    l->len++;
}

/* Returns the 0-indexed 'i'th element of list 'l'. */
unsigned int listAt(list *l, size_t i) {
    return l->data[i];
}

/* Returns the first element of list 'l'. */
unsigned int listFirst(list *l) {
    return listAt(l, 0);
}

/* Returns the last element of list 'l'. */
unsigned int listLast(list *l) {
    if (l->len == 0) {
        dieWithHonor("Attempted reading element from empty list.\n");
    }
    return listAt(l, l->len-1);
}

/* Removes the last element from list 'l' and returns it. */
unsigned int listPop(list *l) {
    int last = listLast(l);
    l->len--;
    return last;
}

/* Free memory storing both list data and list object.  */
void listFree(list *l) {
    free(l->data);
    free(l);
}

/* ======================= Data pages operations ================== */

void printDataEntry(dataEntry *o) {
    fprintf(stdout, "dataEntry(%u, %s, %s)\n", o->id, o->name, o->email);
}

void printDataPage(page *p) {
    // fprintf(stdout, "=== data page ===\n");
    for (size_t j = 0; j < p->len; j++) {
        dataEntry e = p->data.rows[j];
        printDataEntry(&e);
    }
}

/* Add the new element at the end of the data page 'p'. */
void dataPagePush(page *p, unsigned int id, char *name, char *email) {
    if (p->len == ROWS_PER_PAGE) {
        dieWithHonor("Out of space pushing dataEntry to page\n");
    }
    p->data.rows[p->len].id = id;
    snprintf(p->data.rows[p->len].name, STR_LEN, "%s", name);
    snprintf(p->data.rows[p->len].email, STR_LEN, "%s", email);
    p->len++;
}

/* ================== Btree nodes operations ====================== */

void printBtreePage(page *p) {
    size_t j;
    for (j = 0; j < p->len; j++) {
        fprintf(stdout, "key: %u, value: disk[%u], lchild: disk[%u]\n",
                p->node.keys[j], p->node.values[j], p->node.children[j]);
    }
    fprintf(stdout, " |_ rchild: disk[%u]\n", p->node.children[j]);
}

/* Search element with given 'id' within btree node page 'p'.
 *
 * Returns the index of the first element with key greater than or equal
 * to 'id' within the btree node 'p', or 'p->len' when the element is not
 * present and all elements have key smaller than 'id' */
size_t btreePageSearchById(page *p, unsigned int id) {
    size_t j = 0;
    while (j < p->len && p->node.keys[j] < id) j++;
    return j;
}

/* Set key, value, and both children pointers
 * for element at index 'i' in btree page 'p'. */
void btreePageSetEntry(page *p, size_t i,
                       unsigned int key, unsigned int value,
                       unsigned int lchild, unsigned int rchild) {
    p->node.keys[i]       = key;
    p->node.values[i]     = value;
    p->node.children[i]   = lchild;
    p->node.children[i+1] = rchild;
}

/* ============ Low-level disk operations ================ */

/* Dumps page 'p' as the 'n'th page of the 'fd' file. */
void dumpPage(int fd, page *p, unsigned int n) {
    lseek(fd, n * sizeof(page), SEEK_SET);
    write(fd, p, sizeof(page));
}

/* Appends page 'p' at the end of the 'fd' file. */
void appendPage(int fd, page *p) {
    lseek(fd, 0, SEEK_END);
    write(fd, p, sizeof(page));
}

/* Fetches the 'n'th page from the 'fd' file to 'p'. */
void fetchPage(int fd, page *p, unsigned int n) {
    lseek(fd, n * sizeof(page), SEEK_SET);
    read(fd, p, sizeof(page));
}

/* Print the 'n'th page of database at 'fd'. */
void printPage(int fd, int n) {
    fprintf(stdout, "disk[%d]:\n", n);
    page *p = createPage(-1);
    fetchPage(fd, p, n);
    switch (p->type) {
    case PAGE_TYPE_DATA: printDataPage(p); break;
    case PAGE_TYPE_BTREE: printBtreePage(p); break;
    default: fprintf(stdout, "?\n"); break;
    }
    free(p);
}

/* Returns the size of database at file 'fd' as number of pages. */
unsigned int dbSize(int fd) {
    lseek(fd, 0, SEEK_SET);
    off_t file_size = lseek(fd, 0, SEEK_END);
    return file_size / sizeof(page);
}

/* Print content of every page on disk, both data and btree pages. */
void diskWalk(int fd) {
    unsigned int n = dbSize(fd);
    for (unsigned int j = 0; j < n; j++) printPage(fd, j);
}

/* ================ Database operations ==================
 * All database operations will perform disk I/O. */

/* Opens the database file and returns the file descriptor.
 * If the file does not exist, it creates it
 * and dumps an empty btree root node at BTREE_ROOT_PAGE_INDEX. */
int dbOpenOrCreate(void) {
    int fd;
    fd = open(DB_FILENAME, O_RDWR, 0644);
    if (fd == -1) {
        if (errno == ENOENT) {
            fd = open(DB_FILENAME, O_RDWR | O_CREAT, 0644);
            page *root = createBtreePage();
            dumpPage(fd, root, BTREE_ROOT_PAGE_INDEX);
            free(root);
        } else {
            perror("Opening database file");
            exit(1);
        }
    }
    return fd;
}

/* Search insertion leaf for element with given 'id' in b-tree at 'fd'.
 *
 * Loads onto the provided 'bpage' object the node for btree insertion
 * of element with key 'id'.
 *
 * Starts the search from the root of the btree, and appends to the provided
 * 'path' list the explored nodes along the way, for later backtracking.
 *
 * Returns the insertion index of the given 'id' within the node loaded onto
 * 'bpage'.
 *
 * If the key is already present in the database, it will be located at the
 * returned index within the loaded node.
 *
 * It doesn't allocate memory for any other btree node other than the single
 * one provided by the caller, therefore only holding 1 btree node in memory
 * at any given time.
 *
 * It is up to the caller to free the 'bpage' node and 'path' list from memory
 * afterwards if needed. */
size_t dbSearchById(int fd, page *bpage, list *path, unsigned int id) {
    size_t i;
    listPush(path, BTREE_ROOT_PAGE_INDEX);
    while (1) {
        fetchPage(fd, bpage, listLast(path));
        i = btreePageSearchById(bpage, id);

        if (i < bpage->len && bpage->node.keys[i] == id) return i;

        unsigned int nextBpageOffset = bpage->node.children[i];

        if (nextBpageOffset == NULL_CHILD) return i;
        else listPush(path, nextBpageOffset);
    }
}

/* Inserts the given 'key' to the given btree leaf 'node',
 * at the given index 'i', splitting the node when reaching BTREE_MAX_KEYS.
 * Dumps to disk all updated node(s).
 *
 * It is up to the caller to free the 'bpage' object and 'path' list from
 * memory afterwards if needed. */
void btreeInsert(int fd, page *bpage, list *path,
                 size_t i, unsigned int key, unsigned int value,
                 unsigned int lchildPageIndex, unsigned int rchildPageIndex) {
    /* Make space for new key by shifting larger elements to the right. */
    for (size_t j = bpage->len; j > i; j--) {
        bpage->node.keys[j]       = bpage->node.keys[j-1];
        bpage->node.values[j]     = bpage->node.values[j-1];
        bpage->node.children[j+1] = bpage->node.children[j];
    }
    /* Insert new key to node at given index.*/
    btreePageSetEntry(bpage, i, key, value, lchildPageIndex, rchildPageIndex);

    bpage->len++;

    unsigned int btreeNodePageIdx = listPop(path);


    if (bpage->len <= BTREE_MAX_KEYS) {
        dumpPage(fd, bpage, btreeNodePageIdx);
        return;
    }

    /* Move right half of "bpage" elements to new node, then
     * dump it to disk and set it as rchild of "bpage" mid key. */
    page *rchild = createBtreePage();
    for (size_t j = BTREE_MAX_KEYS/2 + 1; j < bpage->len; j++) {
        btreePageSetEntry(rchild, rchild->len++,
                          bpage->node.keys[j], bpage->node.values[j],
                          bpage->node.children[j], bpage->node.children[j+1]);
    }
    unsigned int z = dbSize(fd);
    dumpPage(fd, rchild, z);
    free(rchild);

    /* Move left half of "bpage" elements to new node,
     * then set it as lchild of "bpage" mid key. */
    page *lchild = createBtreePage();
    for (size_t j = 0; j < BTREE_MAX_KEYS/2; j++) {
        btreePageSetEntry(lchild, lchild->len++,
                          bpage->node.keys[j], bpage->node.values[j],
                          bpage->node.children[j], bpage->node.children[j+1]);
    }
    dumpPage(fd, lchild, z+1);
    free(lchild);

    unsigned int newKey          = bpage->node.keys[BTREE_MAX_KEYS/2];
    unsigned int valuePageIndex  = bpage->node.values[BTREE_MAX_KEYS/2];

    if (btreeNodePageIdx == BTREE_ROOT_PAGE_INDEX) {
        /* New root. Just move mid key to beginning of "bpage". */
        btreePageSetEntry(bpage, 0, newKey, valuePageIndex, z+1, z);
        bpage->len = 1;
        dumpPage(fd, bpage, btreeNodePageIdx);
    } else {
        /* Push to parent. */
        unsigned int btreeParentPageIdx = listLast(path);
        fetchPage(fd, bpage, btreeParentPageIdx);
        size_t i = btreePageSearchById(bpage, newKey);
        btreeInsert(fd, bpage, path, i,
                    newKey, valuePageIndex,
                    lchildPageIndex, rchildPageIndex);
    }
}

/* Insert new element onto database at 'fd'. */
void dbInsert(int fd, unsigned int id, char *name, char *email) {
    list *path = createList();
    page *btreeLeaf = createBtreePage();

    size_t i = dbSearchById(fd, btreeLeaf, path, id);
    if (i < btreeLeaf->len && btreeLeaf->node.keys[i] == id) {
        fprintf(stdout, "Key %u already exists in database.\n", id);
        goto exit;
    }

    /* Dump dataEntry to new data page on disk, store page index. */
    page *dpage = createDataPage();
    dataPagePush(dpage, id, name, email);
    unsigned int diskDataPageIndex = dbSize(fd);
    dumpPage(fd, dpage, diskDataPageIndex);
    free(dpage);

    /* Insert new element at insertion leaf. */
    btreeInsert(fd, btreeLeaf, path, i,
                id, diskDataPageIndex,
                NULL_CHILD, NULL_CHILD);

exit:
    free(btreeLeaf);
    listFree(path);
}

/* ========================== Main ================================ */

int main(void) {
    printConfiguration();

    int fd = dbOpenOrCreate();

    dbInsert(fd, 4,   "4name",  "4@email.edu");
    dbInsert(fd, 6,   "6name",  "6@email.edu");
    dbInsert(fd, 7,   "7name",  "7@email.edu");
    dbInsert(fd, 8,   "8name",  "8@email.edu");
    dbInsert(fd, 5,   "5name",  "5@email.edu");
    dbInsert(fd, 9,   "9name",  "9@email.edu");
    dbInsert(fd, 10, "10name", "10@email.edu");
    dbInsert(fd, 11, "11name", "11@email.edu");
    dbInsert(fd, 0,  "daniel", "hello@danielfalbo.com");

    diskWalk(fd);

    close(fd);
    return 0;
}
