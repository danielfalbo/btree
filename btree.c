#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DB_FILENAME "database.db"
#define PAGE_SIZE_BYTES 4096

/* ======================== Data structures ======================= */

#define STR_LEN 64
typedef struct entry {
    unsigned int id;
    char name[STR_LEN];
    char email[STR_LEN];
} entry;

#define ROWS_PER_PAGE (PAGE_SIZE_BYTES / sizeof(entry))
typedef struct page {
    /* Buffer of ROWS_PER_PAGE entries. */
    entry rows[ROWS_PER_PAGE];

    /* Count of rows actually present in the buffer. */
    unsigned int len;

} page;

#define BTREE_MIN_KEYS 4
// #define BTREE_MAX_KEYS 255
#define BTREE_MAX_KEYS 7
typedef struct btree_node {
    /* Count of keys actually present in the node.
     * BTREE_MIN_KEYS <= len <= BTREE_MAX_KEYS for all nodes except root. */
    unsigned int len;

    /* Ids of entries whose pointers are stored within this node. */
    unsigned int keys[BTREE_MAX_KEYS];

    /* Indices of disk pages containing entries data.
     *
     * Entry data for 'keys[i]' will be at 'values[i]'th page on disk.
     *
     * It is up to the consumer to multiply this index by PAGE_SIZE_BYTES
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
     * It is up to the consumer to multiply this index by PAGE_SIZE_BYTES
     * when seeking to the disk location. */
    unsigned int children[BTREE_MAX_KEYS+1];
} btree_node;

void printConfiguration(void) {
    fprintf(stdout, "DB_FILENAME: %s\n", DB_FILENAME);
    fprintf(stdout, "STR_LEN: %d\n", STR_LEN);
    fprintf(stdout, "sizeof(entry): %lu\n", sizeof(entry));
    fprintf(stdout, "PAGE_SIZE_BYTES: %d\n", PAGE_SIZE_BYTES);
    fprintf(stdout, "sizeof(page): %lu\n", sizeof(page));
    fprintf(stdout, "sizeof(btree_node): %lu\n", sizeof(btree_node));
    fprintf(stdout, "ROWS_PER_PAGE: %lu\n", ROWS_PER_PAGE);
    fprintf(stdout, "BTREE_MIN_KEYS: %d\n", BTREE_MIN_KEYS);
    fprintf(stdout, "BTREE_MAX_KEYS: %d\n", BTREE_MAX_KEYS);
    fprintf(stdout, "=============\n\n");
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

/* =================== Object related functions ===================
 * The following functions allocate objects of different types. */

/* Allocate and initialize a new page object. */
page *createPage(void) {
    page *o = xmalloc(sizeof(page));
    o->len = 0;
    return o;
}

/* ======================= Page operations ======================== */
void printEntry(entry *o) {
    fprintf(stdout, "entry(%u, %s, %s)\n", o->id, o->name, o->email);
}

void printPage(page *p) {
    fprintf(stdout, "=== page ===\n");
    for (size_t j = 0; j < p->len; j++) {
        entry e = p->rows[j];
        printEntry(&e);
    }
    fprintf(stdout, "============\n");
}

/* Add the new element at the end of the page 'p'. */
void pagePush(page *p, unsigned int id, char *name, char *email) {
    if (p->len == ROWS_PER_PAGE) {
        fprintf(stderr, "Out of space pushing entry to page\n");
        exit(1);
    }
    p->rows[p->len].id = id;
    snprintf(p->rows[p->len].name, STR_LEN, "%s", name);
    snprintf(p->rows[p->len].email, STR_LEN, "%s", email);
    p->len++;
}

/* Search element with given 'id' within page 'p'. */
void pageSearchById(page *p, unsigned int id) {
    for (size_t j = 0; j < p->len; j++) {
        entry e = p->rows[j];
        if (e.id == id) {
            printEntry(&e);
            return;
        }
    }
    fprintf(stdout, "Entry %u not found in page when searching\n", id);
}

/* Remove element with given "id" from page "p". */
void pageDeleteById(page *p, unsigned int id) {
    for (size_t j = 0; j < p->len; j++) {
        entry e = p->rows[j];
        if (e.id == id) {
            p->len--;
            for (; j < p->len; j++) {
                p->rows[j] = p->rows[j+1];
            }
            return;
        }
    }
    fprintf(stdout, "Entry %u not found in page when deleting\n", id);
}

/* ======================= Disk operations ======================== */

/* Dumps page 'p' as the 'n'th page of the 'fd' file. */
void dumpPage(int fd, page *p, unsigned int n) {
    lseek(fd, n * PAGE_SIZE_BYTES, SEEK_SET);
    write(fd, p, sizeof(page));
}

/* Loads the 'n'th page from the 'fd' file to 'p'. */
void loadPage(int fd, page *p, unsigned int n) {
    lseek(fd, n * PAGE_SIZE_BYTES, SEEK_SET);
    read(fd, p, sizeof(page));
}

/* ======================= Disk database logic ==================== */

/* Print the 'n'th page of database at 'fd'. */
void dbPrintPage(int fd, int n) {
    page *p = createPage();
    loadPage(fd, p, n);

    printPage(p);

    free(p);
}

/* Insert new element onto database at 'fd'. */
void dbPush(int fd, unsigned int id, char *name, char *email) {
    page *p = createPage();
    loadPage(fd, p, 0);

    pagePush(p, id, name, email);
    dumpPage(fd, p, 0);

    free(p);
}

/* Search element with given 'id' within database at 'fd'. */
void dbSearchById(int fd, unsigned int id) {
    page *p = createPage();
    loadPage(fd, p, 0);

    pageSearchById(p, id);

    free(p);
}

/* Remove element with given "id" from database at "fd". */
void dbDeleteById(int fd, unsigned int id) {
    page *p = createPage();
    loadPage(fd, p, 0);

    pageDeleteById(p, id);
    dumpPage(fd, p, 0);

    free(p);
}

/* ======================= Main =================================== */

int main(void) {
    printConfiguration();
    int fd = open(DB_FILENAME, O_CREAT | O_RDWR, 0644);

    // dbSearchById(fd, 0);

    // dbPush(fd, 101, "101", "101@danielfalbo.com");
    // dbPush(fd, 102, "102", "102@danielfalbo.com");
    // dbPush(fd, 103, "103", "103@danielfalbo.com");
    // dbPush(fd, 104, "104", "104@danielfalbo.com");
    // dbPush(fd, 105, "105", "105@danielfalbo.com");
    // dbPush(fd, 106, "106", "106@danielfalbo.com");
    // dbPush(fd, 107, "107", "107@danielfalbo.com");

    // dbDeleteById(fd, 102);
    // dbDeleteById(fd, 103);
    // dbDeleteById(fd, 106);

    dbPrintPage(fd, 0);

    close(fd);
    return 0;
}
