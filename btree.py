DB_FILENAME = "database.db"

PAGE_SIZE_BYTES = 4096

# f = open(DB_FILENAME, binary read write)

# class Node

# def read_node(block_id):
#     offset = block_id * PAGE_SIZE_BYTES
#     f.seek(offset)
#     data_bytes = f.read(PAGE_SIZE_BYTES)
#
#     node: Node = deserialize(data_bytes)
#     return node
#
# def write_node(block_id, node: Node):
#     offset = block_id * PAGE_SIZE_BYTES
#     f.seek(offset)
#
#     data_bytes = serialize(node)
#     f.write(data_bytes)
