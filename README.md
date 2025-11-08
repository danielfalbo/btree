# Toy B-Tree

on-disk data structure, aimed at minimizing i/o reads and writes.

## eurekas

> my stupid questions

- in reimplementing a b tree as a toy educational project, should we assume we have access to the filesystem abstraction and can create multiple directories and files, or should we just use 1 big binary file and seek through it? in practice, do b-tree implementations use the filesystem I/O primitives from the OS or do they operate on the disk at a lower "raw" level, kinda like ignoring the disk's filesystem? can a btree-based database be considere another format to initialize a disk with, as an alternative to APFS/HFS/exFAT, or is it an abstraction above the filesystem? why do we need btrees when OS do a lot of OS filesystem caching within the disk already? do databases like postgresql and mysqlite just create "a large file"/partition within a filesystem/disk and seek-n-go within that file or do they create "directories" and use multiple files?

- b-trees are obviously great for HDDs. are they also the best data structure to perform similar operations on SSDs? or do we have something better for SSDs?

- what should we assume seek-and-go's complexity to be? should we assume every seek-and-go has the same complexity no matter the location or is the complexity proportional to the distance between the target location and the current seek position? with b-trees are we trying to minimize the seek DISTANCE between reads/writes or are we just trying to minimize the COUNT of reads/writes?
