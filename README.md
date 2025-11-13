# Toy B-Tree

on-disk data structure, aimed at performing create, read and delete operations while minimizing i/o reads and writes.

<img src="pic.jpeg" width="200px" />

## skeptical AI prompt for learning

```llm
<btree.c>

I'm the godfather of b-trees.
One of my students is implementing a toy b-tree.
This is their progress so far.
What do you see from their current implementation?
What are they doing wrong?
Do you see red flags?
Do you think they understand what they are doing?
```

## eurekas

> my stupid questions

- in reimplementing a b tree as a toy educational project, should we assume we have access to the filesystem abstraction and can create multiple directories and files, or should we just use 1 big binary file and seek through it? in practice, do b-tree implementations use the filesystem I/O primitives from the OS or do they operate on the disk at a lower "raw" level, kinda like ignoring the disk's filesystem? can a btree-based database be considere another format to initialize a disk with, as an alternative to APFS/HFS/exFAT, or is it an abstraction above the filesystem? why do we need btrees when OS do a lot of OS filesystem caching within the disk already? do databases like postgresql and mysqlite just create "a large file"/partition within a filesystem/disk and seek-n-go within that file or do they create "directories" and use multiple files?

- b-trees are obviously great for HDDs. are they also the best data structure to perform similar operations on SSDs? or do we have something better for SSDs?

- what should we assume seek-and-go's complexity to be? should we assume every seek-and-go has the same complexity no matter the location or is the complexity proportional to the distance between the target location and the current seek position? with b-trees are we trying to minimize the seek DISTANCE between reads/writes or are we just trying to minimize the COUNT of reads/writes? is there a difference between multiple sequential reads and writes and multiple random reads and writes? both on SSD and HDD?

## resources

- [antirez/otree](https://github.com/antirez/otree)
- [cstack/db_tutorial](https://cstack.github.io/db_tutorial/)
- [antoniosarosi/mkdb](https://youtube.com/playlist?list=PLdYoxziVZt9DWfdxTnXDYdc3F2TFT9jzV)
- ["(a,b)-trees" by Tom S](https://youtu.be/lifFgyB77zc)
- [Redis diskstore google group post](https://groups.google.com/g/redis-db/c/1ES8eGaJvek/m/B2BOh_khrRcJ)
- [Carnegie Mellon University Database Group](https://www.youtube.com/@CMUDatabaseGroup)
- ["Databases on SSDs" by Josiah Carlson](https://www.dr-josiah.com/2010/08/databases-on-ssds-initial-ideas-on.html)
- ["Understanding B-Trees" by Spanning Tree](https://youtu.be/K1a2Bk8NrYQ)
