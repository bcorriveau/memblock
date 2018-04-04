/**
 * \brief
 * Memory Block Allocation
 *
 * \details
 * This memory management library is intended for embedded applications that need to
 * run forever without memory allocation problems.
 *
 * The benefits of this memory library are:
 *      - All memory is allocated from the OS on initialization
 *      - Speed. Allocation and freeing of blocks is done in User Space (no OS calls)
 *      - Memory fragmentation is limited as all memory blocks are rounded to the
 *          closest multiple of the smallest block size
 *      - Small overhead per memory block (4 bits)
 *
 * Memory is malloc'ed with mbinit() on initialization, and then mballoc() and 
 * mbfree() are used to alloc and free memory blocks.
 *
 * Memory allocation is broken into two spaces with the following block sizes
 * in bytes for each space:
 *   small blocks :  16,  32,  48,   64,   80,   96,  112,  128
 *   big blocks   : 256, 512, 768, 1024, 1280, 1536, 1792, 2048
 *
 * The initialization values passed into mbinit() sets the maximum number of
 * smallest sized blocks that can be allocated for each space, and also the
 * maximum memory allocated for the space. Any block size for a space may
 * be allocated if there is room in the space.
 *
 * Each space is managed by its own map. Allocations are rounded up to the
 * closest fitting block size and take up that amount of space. The map overhead is 
 * very small, only 4 bits per smallest block size in the multiple that is allocated. 
 * This can lead to some framentation if lots of memory is allocated that in odd multiples
 * of the smallest block size (e.g. 3 or 5 or 7). If memory is not a big concern the
 * allocation could be changed not allow odd smallest block size multiples for allocation.
 *
 */

/** Memory Block Library Error codes */
typedef enum { MBERR_OK = 0,
    MBERR_NOMEM,
    MBERR_BIG,
    MBERR_UNKNOWN,
    MBERR_MAPCORRUPT,
    MBERR_LAST
} MBERR;


/**
 * \brief
 * Get mblib error  code
 *
 * \return  error code for last mblib operation
 */
MBERR
mberr(void);

/**
 * \brief
 * Get mblib error string for err code
 *
 * \param[in] err   the error code to map to an error string
 *
 * \return          error string for given err code
 */
const char *
mberrstr(MBERR err);

/**
 * \brief
 * Initialize memory block library
 *
 * \details
 * Mallocs map and block spaces contiguously
 *
 * Allocates the number of k (1024) of smallest blocks for the small block space and
 *                         k (1024) of smallest blocks for the big block space.
 *
 * \param[in] k_sb_smallest   k of smallest blocks for small block space
 * \param[in] k_bb_smallest   k of smallest blocks for big block space 
 *
 */
void
mbinit(int k_sb_smallest, int k_bb_smallest);

/**
 * \brief
 * Terminate memory block management
 *
 * \details
 * Releases memory used for block management back to the OS
 */
void
mbterm();

/**
 * \brief
 * Allocate memory block space
 *
 * \details
 * Rounds the given size up to the closest block size in the
 * appropriate block space, and marks that space used on the
 * corresponding space map.
 *
 * \param[in] size    number of bytes requested
 *
 * \returns   pointer to bytes allocate or NULL if not available
 *            If NULL returned check mberr code for reason
 */
void *
mballoc(unsigned long);

/**
 * \brief
 * Free memory allocated by mballoc
 *
 * \details
 * Frees all the memory that was allocated starting from this address.
 * Do not pass in an address that was not returned from mballoc().
 * If the address given is not in the memory spaces then mberr will be set.
 *
 * \param[in]   mbp     memory block pointer
 *
 * \note
 * If there is a problem freeing the mblib mberr will be set
 */
void
mbfree(void *);

/**
 * \brief
 * Get mblib space block allocation stats
 *
 * \details
 * Scans through space maps and writes stats into an array that is returned
 * to the caller.
 *
 * \param[out] blkstat   array of allocation statistics per block
 *
 * \return              number of statistics per space
 */
int
mbstatget(int *blkstat[]);

/**
 * \brief
 * Dump memory space block usage stats
 *
 * \details
 * Prints out the memory block usage statistics for the memory spaces. This is for
 * debugging.
 */
void
mbdumpstat();

/**
 * \brief
 * Dump memory space maps
 *
 * \details
 * Prints out the memory maps for the memory spaces. This is for
 * debugging.

 */
void
mbdumpmap();

/**
 * \brief
 * Test that all memory blocks are free
 *
 * \details
 * Tests that both memory spaces are unused. This is for testing purposes.
 *
 * \return
 * TRUE    if all memory block space is free
 * FALSE  if some memory block space is allocated
 */
int
mbtestfree();
