# Toy B-Tree

on-disk data structure, aimed at performing create, read and delete operations while minimizing i/o reads and writes.

| day 5                               | day 6                               |
|-------------------------------------|-------------------------------------|
|<img src="pic1.jpeg" width="200px" />|<img src="pic2.jpeg" width="200px" />|

## skeptical AI prompt for learning

usually prompted to thinking model.

```llm
<student's known limitations and TODOs>
</student's known limitations and TODOs>

<student's btree.c>
</student's btree.c>

I'm the godfather of b-trees and I am here with a C programming expert.
One of our students is implementing a toy b-tree in pure C.
This is their progress so far.
What do you see from their current implementation?
What are they doing wrong?
Do you see red flags?
Do you think they understand what they are doing?
Do you see any correctness issue?
Do you see any bug?
Could the code be more elegant and better commented?
Could the code be more clear?
Can the code be refactored to be more beautiful?
Can the code be refactored to be more simple?
Can the code be refactored to have better abstractions,
never repeating blocks of code?
```

## eurekas

> my stupid questions

- in reimplementing a b tree as a toy educational project, should we assume we have access to the filesystem abstraction and can create multiple directories and files, or should we just use 1 big binary file and seek through it? in practice, do b-tree implementations use the filesystem I/O primitives from the OS or do they operate on the disk at a lower "raw" level, kinda like ignoring the disk's filesystem? can a btree-based database be considere another format to initialize a disk with, as an alternative to APFS/HFS/exFAT, or is it an abstraction above the filesystem? why do we need btrees when OS do a lot of OS filesystem caching within the disk already? do databases like postgresql and mysqlite just create "a large file"/partition within a filesystem/disk and seek-n-go within that file or do they create "directories" and use multiple files?

- b-trees are obviously great for HDDs. are they also the best data structure to perform similar operations on SSDs? or do we have something better for SSDs?

- what should we assume seek-and-go's complexity to be? should we assume every seek-and-go has the same complexity no matter the location or is the complexity proportional to the distance between the target location and the current seek position? with b-trees are we trying to minimize the seek DISTANCE between reads/writes or are we just trying to minimize the COUNT of reads/writes? is there a difference between multiple sequential reads and writes and multiple random reads and writes? both on SSD and HDD?

## resources

1. CLRS algorithms book B-trees section.

- [antirez/otree](https://github.com/antirez/otree)
- [cstack/db_tutorial](https://cstack.github.io/db_tutorial/)
- [antoniosarosi/mkdb](https://youtube.com/playlist?list=PLdYoxziVZt9DWfdxTnXDYdc3F2TFT9jzV)
- ["(a,b)-trees" by Tom S](https://youtu.be/lifFgyB77zc)
- [Redis diskstore google group post](https://groups.google.com/g/redis-db/c/1ES8eGaJvek/m/B2BOh_khrRcJ)
- [Carnegie Mellon University Database Group](https://www.youtube.com/@CMUDatabaseGroup)
- ["Databases on SSDs" by Josiah Carlson](https://www.dr-josiah.com/2010/08/databases-on-ssds-initial-ideas-on.html)
- ["Understanding B-Trees" by Spanning Tree](https://youtu.be/K1a2Bk8NrYQ)
- [B-Tree Visualization by University of San Francisco](https://www.cs.usfca.edu/~galles/visualization/BTree.html)

## known limitations

- we use 1 entire data page per row, when ROWS_PER_PAGE would fit in a page,
therefore using much more disk space than necessary. this is because the goal
of this project is just to implement a toy b-tree whose goal is to minimize
the number of I/O operations, not disk space.
- we use the same struct with union overlap for both btree nodes and data
pages, and attempt to reach IDEAL_PAGE_SIZE_BYTES by tweaking BTREE_MAX_KEYS
and rows string fields length STR_LEN. this could probably be improved in
practice for both primary and disk memory efficiency as well as safety and
precision about the page size. for the sake of learning, thinking in terms of
page indices instead of raw disk offets came more natural to me while learning.
I also just wanted to play around and practice with anonymous `union`s: they
are really cool to me.
- we iterate through the keys in btree nodes instead of binary searching. this
is beacause, once again, I wanted to focus on building a b-tree to minimize
disk I/O operations, the real bottleneck, for this project so I didn't bother
with the in-memory binary search.
- we do syscalls to deal with files instead of using the more portable C lib.
we still didn't do anyting fancy, so search and replacing
`read(fd, p, sizeof(page));`s with `fread(p, 1, sizeof(page), f)`s would
probably make everything stdc-compatible.
- I'm not sure everything works fine with an odd BTREE_MAX_KEYS, I've only
tested even BTREE_MAX_KEYS for now.
- we leak disk storage every time an insertion splits a node, creating 2 new nodes and leaking the space allocated to original on disk forever. instead of creating 2 new pages, we should just create 1 new page for the right child and overwrite the original page for the left child. while we do 1 extra disk I/O, the asymptotic is still logarithmic, so I'm happy with this for now.

# TODO

- it would be cool to make the database file entirely human-readable.
- `--feel-the-pain` flag that sleeps 5 extra seconds at every disk read/write.
- implement `DELETE` operation
- always keep the root in main memory.
- build a repl.
