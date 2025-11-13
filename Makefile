all: btree

btree: btree.c
	$(CC) -o btree btree.c -Wall -W -pedantic -O3

clean:
	rm btree
