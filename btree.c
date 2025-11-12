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
    unsigned int len;
    entry rows[ROWS_PER_PAGE];
} page;

void printConfiguration(void) {
    fprintf(stdout, "DB_FILENAME: %s\n", DB_FILENAME);
    fprintf(stdout, "STR_LEN: %d\n", STR_LEN);
    fprintf(stdout, "sizeof(entry): %lu\n", sizeof(entry));
    fprintf(stdout, "PAGE_SIZE_BYTES: %d\n", PAGE_SIZE_BYTES);
    fprintf(stdout, "sizeof(page): %lu\n", sizeof(page));
    fprintf(stdout, "ROWS_PER_PAGE: %lu\n", ROWS_PER_PAGE);
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
    fprintf(stdout, "Entry %u not found in page\n", id);
}

/* Remove element with given "id" from page "p". */
void pageDeleteById(page *p, unsigned int id) {
    for (size_t j = 0; j < p->len; j++) {
        entry e = p->rows[j];
        if (e.id == id) {
            p->len--;
            for (size_t i = j+1; i < p->len; i++) {
                p->rows[i] = p->rows[i+1];
            }
            return;
        }
    }
    fprintf(stdout, "Entry %u not found in page\n", id);
}

/* ======================= Disk operations ======================== */

/* Dumps page 'p' as the 'n'th page of the 'fd' file. */
void dumpPage(int fd, page *p, int n) {
    lseek(fd, n * PAGE_SIZE_BYTES, SEEK_SET);
    write(fd, p, sizeof(page));
}

/* Loads the 'n'th page from the 'fd' file to 'p'. */
void loadPage(int fd, page *p, int n) {
    lseek(fd, n * PAGE_SIZE_BYTES, SEEK_SET);
    read(fd, p, sizeof(page));
}

/* ======================= Disk database logic ==================== */

// TODO: disk INSERT

// TODO: disk SELECT

// TODO: disk DELETE

/* ======================= Main =================================== */

int main(void) {
    printConfiguration();

    int fd = open(DB_FILENAME, O_CREAT | O_RDWR, 0644);
    page *p = createPage();
    loadPage(fd, p, 0);
    printPage(p);

    // pagePush(p, 0, "hello", "hello@danielfalbo.com");
    // printPage(p);
    // pageSearchById(p, 100);
    // pageSearchById(p, 43);
    // pageDeleteById(p, 0);
    // printPage(p);

    dumpPage(fd, p, 0);
    free(p);
    close(fd);

    return 0;
}
