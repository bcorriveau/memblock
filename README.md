# memblock - memory block management library

## Synopsis
Memory block management library that provides low overhead memory management
for applications that need to run forever without issue after initialization.

Memory is malloc'ed with mbinit() on initialization, and then mballoc() and
mbfree() are used to alloc and free memory blocks.

Memory allocation is broken into two spaces with the following block sizes
in bytes for each space:
-  small blocks :  16,  32,  48,   64,   80,   96,  112,  128
-  big blocks   : 256, 512, 768, 1024, 1280, 1536, 1792, 2048

Each memory space is managed by a block map where each block is represented
by 4 bits.

## API Reference
```
#include <mblib.h>

/** Memory Block Library Error codes */
typedef enum { MBERR_OK = 0,
               MBERR_NOMEM,
               MBERR_BIG,
               MBERR_UNKNOWN,
               MBERR_MAPCORRUPT,
               MBERR_LAST
} MBERR;

void mbinit(k_smallest_blocks_small_block_space, k_smallest_blocks_big_block_space);
void *mballoc(unsigned long size);
void mbfree(void *ptr);
MBERR mberr(void);
const char *mberrstr(MBERR err);
int mbstatget(int *blkstat[]);
void mbdumpstat();
void mbdumpmap();
int mbtestfree();
void mbterm();

The mbinit() function allocates and initializes the memory space maps and space
blocks for the requested sizes.

The mballoc() function allocates size bytes of memory rounded up to the closest
block size. Any block size that fits in the corresponding space may be allocated.

The mbfree() function frees the memory pointed to by ptr.

The mberr() function returns the last generated mblib error.

The mberrstr() function returns a pointer to an error string for the last generated
mblib error.

The mbstatget() function returns the number of blocks per space, and a pointer
to the space block statistics.

The mbdumpstat() function prints out the space block statistics.

The mbdumpmap() function prints out the space maps.

The mbtestfree() function returns 1 if no memory is allocated in either space,
                                  0 otherwise.

The mbterm() function frees the memory allocated by mbinit().
```
## Motivation
The benefits of this memory library are:
  - All memory is allocated from the OS on initialization
  - Speed. Allocation and freeing of blocks is done in User Space (no OS calls)
  - Memory fragmentation is limited as all memory blocks are rounded to the
    closest multiple of the smallest block size
  - Small overhead per memory block (4 bits)

## Code Example
```
/* allocate room for 1024 small space 16 byte blocks
 *               and 1024 large space 256 byte blocks
 */
mbinit(1 ,1);
datap = mballoc(size);
mbfree(datap);
mbterm();
```
## Running the test code
```
$ cd memblock
$ make
$ cd test
$ make
$ ./mbtest
$ ./mbtest_dyn
```
## Possible Improvements
- More memory spaces
- Thread protection
- Reduce memory block overhead to 2 bits (only 3 values are needed for mapping)
- Disallow odd number of block allocations > 1 to reduce memory fragmentation (more wasted memory)

## License
MIT License
