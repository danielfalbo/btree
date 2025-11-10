all: btree

btree: btree.c
	$(CC) -o btree btree.c -Wall -W -pedantic -std=c99 -O3

clean:
	rm btree
