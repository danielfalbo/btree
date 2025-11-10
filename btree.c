#include <stdio.h>
#include <fcntl.h>

#define DB_FILENAME "database.db"
#define PAGE_SIZE_BYTES 4096

typedef struct Node {
    unsigned a:1;
    unsigned char c;
} Node;

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

int main(void) {
    int fd = open(DB_FILENAME, O_CREAT | O_RDWR);
    return 0;
}
