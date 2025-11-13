#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>

#define DB_FILENAME "database.db"
#define IDEAL_PAGE_SIZE_BYTES 4096

/* ======================== Data structures ======================= */

#define PAGE_TYPE_DATA    0
#define PAGE_TYPE_BTREE   1

#define STR_LEN 58
typedef struct entry {
    unsigned int id;
    char name[STR_LEN];
    char email[STR_LEN];
} entry;

#define ROWS_PER_PAGE (IDEAL_PAGE_SIZE_BYTES / sizeof(entry))
// #define BTREE_MAX_KEYS 340
#define BTREE_MAX_KEYS 4
typedef struct page {
    int type; // PAGE_TYPE_*

    /* For data nodes: count of rows actually present in the buffer,
     * len <= ROWS_PER_PAGE.
     *
     * For btree nodes: count of keys actually present in the node,
     * len <= BTREE_MAX_KEYS. */
    size_t len;

    union {
        struct {
            /* Buffer of ROWS_PER_PAGE entries. */
            entry rows[ROWS_PER_PAGE];
        } data;

        struct {
            /* Ids of entries whose pointers are stored within this node. */
            unsigned int keys[BTREE_MAX_KEYS];

            /* Indices of disk pages containing entries data.
            *
            * Entry data for 'keys[i]' will be at 'values[i]'th page on disk.
            *
            * It is up to the user to multiply this index by sizeof(page)
            * when seeking to the disk location. */
            unsigned int values[BTREE_MAX_KEYS];

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
            unsigned int children[BTREE_MAX_KEYS+1];
        } node;
    };
} page;

/* We will make sure the btree root node is always the first page on disk. */
#define BTREE_ROOT_PAGE_OFFSET 0
/* Root can never be a child, so we also use it as null child pointer. */
#define NULL_CHILD_PAGE_OFFSET BTREE_ROOT_PAGE_OFFSET

void printConfiguration(void) {
    fprintf(stdout, "DB_FILENAME: %s\n", DB_FILENAME);
    fprintf(stdout, "STR_LEN: %d\n", STR_LEN);
    fprintf(stdout, "sizeof(entry): %lu\n", sizeof(entry));
    fprintf(stdout, "IDEAL_PAGE_SIZE_BYTES: %d\n", IDEAL_PAGE_SIZE_BYTES);
    fprintf(stdout, "sizeof(page): %lu\n", sizeof(page));
    fprintf(stdout, "ROWS_PER_PAGE: %lu\n", ROWS_PER_PAGE);
    fprintf(stdout, "BTREE_MAX_KEYS: %d\n", BTREE_MAX_KEYS);
}

/* =================== Allocation wrappers ======================== */

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

/* =================== Object related functions =================== */

/* Allocate and initialize a new page object. */
page *createPage(int type) {
    page *o = xmalloc(sizeof(page));
    o->len = 0;
    o->type = type;
    return o;
}

/* ======================= Data pages operations ================== */

void printEntry(entry *o) {
    fprintf(stdout, "entry(%u, %s, %s)\n", o->id, o->name, o->email);
}

void printDataPage(page *p) {
    fprintf(stdout, "=== data page ===\n");
    for (size_t j = 0; j < p->len; j++) {
        entry e = p->data.rows[j];
        printEntry(&e);
    }
}

/* Add the new element at the end of the page 'p'. */
void dataPagePush(page *p, unsigned int id, char *name, char *email) {
    if (p->len == ROWS_PER_PAGE) {
        fprintf(stderr, "Out of space pushing entry to page\n");
        exit(1);
    }
    p->data.rows[p->len].id = id;
    snprintf(p->data.rows[p->len].name, STR_LEN, "%s", name);
    snprintf(p->data.rows[p->len].email, STR_LEN, "%s", email);
    p->len++;
}

/* Remove element with given "id" from page "p". */
void dataPageDeleteById(page *p, unsigned int id) {
    for (size_t j = 0; j < p->len; j++) {
        entry e = p->data.rows[j];
        if (e.id == id) {
            p->len--;
            for (; j < p->len; j++) {
                p->data.rows[j] = p->data.rows[j+1];
            }
            return;
        }
    }
    fprintf(stdout, "Entry %u not found in page when deleting\n", id);
}

/* ================== Btree pages operations ====================== */

void printBtreePage(page *p) {
    fprintf(stdout, "=== btree page ===\n");
    for (size_t j = 0; j < p->len; j++) {
        fprintf(stdout, "key: %u, value page: %u, child page: %u\n",
                p->node.keys[j], p->node.values[j], p->node.children[j]);
    }
}

/* Search element with given 'id' within btree node page 'p'.
 * Returns the index of the element within the page, or -1 if not present. */
int btreePageSearchById(page *p, unsigned int id) {
    for (size_t j = 0; j < p->len; j++) {
        if (p->node.keys[j] == id) {
            return j;
        }
    }
    return -1;
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

/* ============ Logical disk operations ================== */

/* Print the 'n'th page of database at 'fd'. */
void printPage(int fd, int n) {
    page *p = createPage(-1);
    fetchPage(fd, p, n);
    switch (p->type) {
    case PAGE_TYPE_DATA:
        printDataPage(p);
        break;
    case PAGE_TYPE_BTREE:
        printBtreePage(p);
        break;
    default:
        fprintf(stdout, "?");
        break;
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
    fprintf(stdout, "The database is currently %u pages big.\n", n);
    for (unsigned int j = 0; j < n; j++) printPage(fd, j);
}

/* ================ Database operations ==================
 * All database operations will perform disk I/O. */

/* Opens the database file, returns the file descriptor, and
 * loads the root in memory. If the file does not exist, it creates it
 * and dumps an empty root node at offset 0. */
int dbOpenOrCreate(void) {
    int fd;
    fd = open(DB_FILENAME, O_RDWR, 0644);
    if (fd == -1) {
        if (errno == ENOENT) {
            fd = open(DB_FILENAME, O_RDWR | O_CREAT, 0644);
            page *root = createPage(PAGE_TYPE_BTREE);
            dumpPage(fd, root, BTREE_ROOT_PAGE_OFFSET);
            free(root);
        } else {
            perror("Opening database file");
            exit(1);
        }
    }
    return fd;
}

/* btreeInsert */
// void btreeInsert(int fd, page *leaf, path *leaf_to_root_pages_idxs,
//                  unsigned int key, unsigned int value,
//                  unsigned int lchild, unsigned int rchild) {
//
//      If insertion-leaf.len < BTREE_MAX_KEYS,
//      binary insert key-value to leaf in-memory and dump it to disk
//      at root_to_leaf[0].
//
//      Else if insertion-leaf is full, create 2 new children btree nodes with
//      lhs and rhs keys+values+children, dump them on memory.
//      Then insert middle key-value plus children pointers to the 2 new nodes
//      as rightmost key of parent, recursively until you find non-full parent.
// }

/* Insert new element onto database at 'fd'. */
// void dbInsert(int fd, unsigned int id, char *name, char *email) {
    // leaf_index = ROOT
    // page *leaf = disk[leaf_index];
    // Search insertion leaf in b-tree.
    // Keep 1 node at a time in memory, store chain from root to leaf.

    // If key already exists, exit without writing anything on disk.
    // Else, allocate new data page object, initialize it with the new
    // item data, dump it on disk, free it from meory,
    // but keep page index around.
    //
    // btreeInsert(fd, leaf, leaf_to_root_pages_idxs,
    //              key: id, value: disk page index,
    //              children: NULL_CHILD_PAGE_OFFSET)
    //





    /* ========= old skeleton implementation below ======== */




    // /* Insert new node with key 'id' onto b-tree. */
    // page *p = createPage(PAGE_TYPE_BTREE);
    // fetchPage(fd, p, BTREE_ROOT_PAGE_OFFSET);
    // size_t i = 0;
    // if (p->len == BTREE_MAX_KEYS) {
    //     /* Node already stores BTREE_MAX_KEYS. */
    // }
    // /* Find sorted insertion index 'i'. */
    // while (i < p->len && p->node.keys[i] < id) { i++; }
    // if (i < p->len && p->node.keys[i] == id) {
    //     fprintf(stdout, "Key %u already exists in database.\n", id);
    // }
    // /* Shift larger elements 1 position to the right. */
    // for (size_t j = p->len; j > i; j--) {
    //     p->node.keys[j] = p->node.keys[j-1];
    //     p->node.values[j] = p->node.values[j-1];
    //     p->node.children[j] = p->node.children[j-1];
    // }
    // /* Set key at insertion point and increment the node's logical lenght. */
    // p->node.keys[i] = id;
    // p->len++;
    // /* Write data onto new page on disk.  */
    // page *p = createPage(PAGE_TYPE_DATA);
    // dataPagePush(p, id, name, email);
    // int nth = dbSize(fd);
    // dumpPage(fd, p, nth);
    // free(p);
    // /* Set the new node's data disk page pointer.  */
    // n->node.values[i] = nth;
    // /* Dump updated btree node on disk.  */
    // dumpPage(fd, n, BTREE_ROOT_PAGE_OFFSET);
// }

/* Search element with given 'id' within database at 'fd'. */
// void dbSearchById(int fd, unsigned int id) {
//     page *p = createPage(PAGE_TYPE_BTREE);
//     fetchPage(fd, p, 0);
//     int j = btreePageSearchById(p, id);
//     if (j == -1) {
//         fprintf(stdout, "Key %u not found in database.", id);
//         return;
//     }
//     unsigned int valuePage = p->node.values[j];
//     free(p);
//
//     printPage(fd, valuePage);
// }

// /* Remove element with given "id" from database at "fd". */
// void dbDeleteById(int fd, unsigned int id) {
//     page *p = createPage(PAGE_TYPE_DATA);
//     fetchPage(fd, p, 0);
//
//     dataPageDeleteById(p, id);
//     dumpPage(fd, p, 0);
//
//     free(p);
// }

/* ========================== Main ================================ */

int main(void) {
    printConfiguration();

    int fd = dbOpenOrCreate();

    // dbInsert(fd, 4, "_", "@");
    // dbInsert(fd, 5, "_", "@");
    // dbInsert(fd, 6, "_", "@");
    // dbInsert(fd, 7, "_", "@");

    diskWalk(fd);

    // dbSearchById(fd, 10);

    // dbDeleteById(fd, 103);

    close(fd);
    return 0;
}
