#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DB_FILENAME "database.db"
#define PAGE_SIZE_BYTES 4096

/* ======================== Data structures ======================= */

typedef struct entry {
    unsigned int id;
    char name[64];
    char email[64];
} entry;

#define ROWS_PER_PAGE (PAGE_SIZE_BYTES / sizeof(entry))
typedef struct page {
    unsigned int len;
    entry rows[ROWS_PER_PAGE];
} page;

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

/* Allocate and initialize a new database entry object. */
entry *createEntry(unsigned int id, char name[255], char email[255]) {
    entry *o = xmalloc(sizeof(entry));
    o->id = id;
    snprintf(o->name, sizeof(o->name), "%s", name);
    snprintf(o->email, sizeof(o->email), "%s", email);
    return o;
}

/* ===================== Page object ============================== */

page *createPage(void) {
    page *o = xmalloc(sizeof(page));
    o->len = 0;
    return o;
}

/* Add the new element at the end of the table 't'. */
void pagePush(page *p, entry *e) {
    if (p->len == ROWS_PER_PAGE) {
        fprintf(stderr, "Out of space pushing entry to page\n");
        exit(1);
    }
    void * page_end_ptr = &(p->rows[p->len]);
    memcpy(page_end_ptr, e, sizeof(entry));
    p->len++;
}

/* ======================= Use the database ======================= */
void printEntry(entry *o) {
    fprintf(stdout, "entry(%d, %s, %s)\n", o->id, o->name, o->email);
}

void printPage(page *p) {
    fprintf(stdout, "=== table ===\n");
    for (size_t j = 0; j < p->len; j++) {
        entry e = p->rows[j];
        printEntry(&e);
    }
    fprintf(stdout, "=============\n");
}

// read_node(block_id):
//      offset = block_id * PAGE_SIZE_BYTES
//      f.seek(offset)
//      data_bytes = f.read(PAGE_SIZE_BYTES)
//
//      node: Node = deserialize(data_bytes)
//      return node

// write_node(block_id, node: Node):
//      offset = block_id * PAGE_SIZE_BYTES
//      f.seek(offset)
//
//      data_bytes = serialize(node)
//      f.write(data_bytes)

// void search_by_id(unsigned int id) {
// }

void printConfiguration(void) {
    fprintf(stdout, "=============\n");
    fprintf(stdout, "DB_FILENAME: %s\n", DB_FILENAME);
    fprintf(stdout, "PAGE_SIZE_BYTES: %d\n", PAGE_SIZE_BYTES);
    fprintf(stdout, "ROWS_PER_PAGE: %lu\n", ROWS_PER_PAGE);
    fprintf(stdout, "sizeof(entry): %lu\n", sizeof(entry));
    fprintf(stdout, "sizeof(page): %lu\n", sizeof(page));
    fprintf(stdout, "=============\n\n");
}

int main(void) {
    printConfiguration();

    int fd = open(DB_FILENAME, O_CREAT | O_RDWR);

    entry *ciccio = createEntry(43, "ciccio", "ciccio@danielfalbo.com");
    entry *daniel = createEntry(11, "daniel", "hello@danielfalbo.com");
    printEntry(ciccio);
    page *mypage = createPage();
    pagePush(mypage, ciccio);
    printPage(mypage);
    pagePush(mypage, daniel);
    printPage(mypage);

    close(fd);
    return 0;
}
