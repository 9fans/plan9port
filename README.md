This is a port of many Plan 9 libraries and programs to Unix.
Originally authored by Russ Cox.

This fork of plan9port adds:
- mventi memory indexed venti server compatible with venti arenas
- vgate vbackup file served as geom per ggate

Some other changes were needed to make servers work for me on FreeBSD,
notably 9pfuse, vnfs, vacfs.

WB Kloke
