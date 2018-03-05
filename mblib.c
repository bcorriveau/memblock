/**
 * \brief
 * Memory Block Management Library
 *
 * \details
 * Memory is allocated in blocks rounded up to the closest 4 words for small blocks
 * and 64 words for large blocks.
 *
 * See mblib.h for more details
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mblib.h"

/** Memblock base types */
typedef unsigned char   mbbyte_t;
typedef unsigned short  uint16;

/** \brief Unit used for memory map allocation */
typedef unsigned int    mbword_t;

/** Local constants */
#define     MB_DEBUG                    0           /* 0 for no debug output */

#define     MB_MAPWORD_SIZE             sizeof(mbword_t)
#define     MB_MAP_NIB_PERWORD          (sizeof(mbword_t) << 1)
#define     MB_MAP_BITS_PERNIB          4

#define     MB_SMALLBLOCKS              0
#define     MB_BIGBLOCKS                1
#define     MB_SPACES                   2

#define     MB_MAP_ALLOC_MIN_WORDS      4
#define     MB_MAP_ALLOC_MAX_WORDS      32

#define     MB_SBMAP_BYTES_PERNIB       16
#define     MB_SBMAP_BYTES_PERWORD      (MB_SBMAP_BYTES_PERNIB * MB_MAP_NIB_PERWORD)

#define     MB_BBMAP_BYTES_PERNIB       (MB_SBMAP_BYTES_PERWORD << 1)
#define     MB_BBMAP_BYTES_PERWORD      (MB_BBMAP_BYTES_PERNIB * MB_MAP_NIB_PERWORD)

#define     MB_MAP_ALLOC_LFN_MAP        0xF0000000          /* left most nibble  */
#define     MB_MAP_ALLOC_RTN_MAP        0x0000000F          /* right nost nibble */
#define     MB_MAP_ALLOC_MARK           0xF0000000
#define     MB_MAP_ALLOC_END            0x10000000
#define     MB_MAP_ALLOC_END_VAL        0x1

/** Memory Block libraary error strings */
const char *mb_errorstr[] = {   "OK",
                                "No available memory for last allocation",
                                "Requested memory allocation to big for memory spaces",
                                "Referenced memory not in mblib space",
                                "Map space is corrupted"};

/**
 * \brief
 * Memory block map and block space
 *
 * \details
 * Each Memory block space is mapped with a nibble map of words
 *
 * Each 4 bits in the small block map represents 4 words of small block memory (16 bytes)
 * Each word in the small block map tracks 32 words of small block memory (128 bytes)
 *
 * Each 4 bits in the large block map represents 64 words of big block memory (256 bytes)
 * Each word in the big block map tracks 256 words (2k)
 *
 * The small block area allocates sizes from 16 to 128 bytes, rounding up to the closest 16 bytes (4 words)
 *
 * The big block area allocates sizes from 256 bytes to 2k, rounding up to the closest 256 bytes (64 words)
 *
 * Free memory is marked with 0 (4 bits for 4 words)
 *   A single allocated block (4 words) is marked allocated with a 1
 *
 *   More than 4 allocated words are marked allocated beginning 
 *   with an F and ending with a 1, with F's marked between
 *
 * Memory allocation is only done within map words, and not across them. This can
 * lead to some framentation if odd numbers of the minimum block size for a space
 * are allocated. Any number of block sizes for a space may be allocated from any 
 * map word
 *
 *  bytes_pernib    - bytes reserved per map nibble (4 bits)
 *  bytes_perword   - bytes reserved per map word
 *  mapwords        - number of map words for this space
 *  mi              - current index into map
 *  bmap            - block map
 *  block           - memory for blocks
 */
typedef struct {
    const uint16     bytes_pernib;
    const uint16     bytes_perword;
    uint16           mapwords;
    uint16           mi;
    mbword_t        *bmap;
    mbbyte_t        *block;
} mbspace_t;

/** \brief 
  * Memory block libary control block type
  * \details
  * err     - Last recorded error
  * space   - Array of memory spaces 
  */
typedef struct {
    MBERR       err;
    mbspace_t   space[MB_SPACES];
} mbcb_t;

/**
 * \brief
 * Memory block libary control block
 *
 * \details
 * Initialize control block last error, and for each memory space
 * the map's reserved bytes per nibble and word for convenience
 */
mbcb_t mbcb = { MBERR_OK, { {MB_SBMAP_BYTES_PERNIB, MB_SBMAP_BYTES_PERWORD},
                            {MB_BBMAP_BYTES_PERNIB, MB_BBMAP_BYTES_PERWORD} } };

/** \brief
 *  Array used to hold block allocations per block size per space
 */
int mbblkstat[MB_SPACES * MB_MAP_NIB_PERWORD];


/** \brief
 *  print debug message to stderr if debug is on
 */
#if MB_DEBUG == 1
    #define MB_DEBUG_PRINT(fmt, args...) \
            fprintf(stderr, "MBLIB_DEBUG: %s:%.3d "fmt, __FILE__, __LINE__, ## args)
#else
    #define MB_DEBUG_PRINT(fmt, args...)
#endif


/**
 * \brief
 * Get mblib error  code
 *
 * \return  error code for last mblib operation
 */
MBERR
mberr(void)
{
    return mbcb.err;
}

/**
 * \brief
 * Get mblib error string for err code
 *
 * \param[in] err   the error code to map to an error string
 *
 * \return          error string for given err code
 */
const char *
mberrstr(MBERR err)
{
    if (err < MBERR_LAST) {
        return mb_errorstr[err];
    } else {
        return NULL;
    }
}


/**
 * \brief
 * Get nibble value
 *
 * \details
 * Return the value of the given nibble of the given map word
 * Map nibbles are numbered from left to right starting from zero
 */
static mbword_t
mbnibval(mbword_t   mword,
         int        nib)
{
    mbword_t nibval;

    nibval = mword & (MB_MAP_ALLOC_LFN_MAP >> (nib << 2));
    return (nibval >> ((MB_MAP_NIB_PERWORD - 1 - nib) << 2));
}


/**
 * \brief
 * Calculate block allocation stats for space
 *
 * \details
 * Scans through map for given space and writes stats into given array
 * to the caller.
 *
 * \param[in]  space    block space to calculate stats
 * \param[out] blkstat  array of allocation statistics per block
 *
 * \return              number of statistics per space
 */
static MBERR
mbstatcalc(mbspace_t *space, int *blkstat)
{
    int         mi, wi, blk;
    mbword_t    amask;

    /* scan through the map words */
    for (mi=0; mi < space->mapwords; mi++) {
        amask = MB_MAP_ALLOC_LFN_MAP;
        wi = 0;
        /* scan through a map word for allocated blocks */
        while (wi < MB_MAP_NIB_PERWORD) {
            if (space->bmap[mi] & amask) {
                /* found a block, now figure out its size */
                blk = 0;
                while (mbnibval(space->bmap[mi], wi) != MB_MAP_ALLOC_END_VAL) {
                    wi++;
                    blk++;
                    if (wi >= MB_MAP_NIB_PERWORD) {
                        if (MB_DEBUG) {
                            assert(0);
                        }
                        return MBERR_MAPCORRUPT;
                    }
                }
                MB_DEBUG_PRINT("amask %.8X mi %d wi %d blk %d\n", amask, mi, wi, blk);
                blkstat[blk]++;
            }
            wi++;
            amask >>= (MB_MAP_BITS_PERNIB * (blk+1));
        }
    }
    return MBERR_OK;
}

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
mbstatget(int *blkstat[])
{
    int i;
 
    /* clear out allocation stats */
    memset(mbblkstat, 0, sizeof(mbblkstat));

    for (i=0; i < MB_SPACES; i++) {
        if (mbstatcalc(&mbcb.space[i], &mbblkstat[i * MB_MAP_NIB_PERWORD])) {
            mbcb.err = MBERR_MAPCORRUPT;
            return 0;
        }
    }

    *blkstat = mbblkstat;
    return (MB_MAP_NIB_PERWORD);
}


/**
 * \brief
 * Return an incremented map index
 *
 * \details
 * Increment the given map index, and wrap if needed 
 */
static inline uint16
mbimapinc(uint16 mi, uint16 mapwords)
{
    return (++mi < mapwords ? mi : 0);
}


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
mbinit(int k_sb_smallest, int k_bb_smallest)
{
    mbspace_t *space, *prevspace;

    /* set up mapwords for spaces */
    mbcb.space[MB_SMALLBLOCKS].mapwords = k_sb_smallest * 1024 / MB_MAP_NIB_PERWORD;
    mbcb.space[MB_BIGBLOCKS].mapwords = k_bb_smallest * 1024 / MB_MAP_NIB_PERWORD;

    /* Malloc all required memory for space maps and block areas contiguously */
    mbcb.space[MB_SMALLBLOCKS].bmap = 
            malloc((mbcb.space[MB_SMALLBLOCKS].mapwords * (MB_MAPWORD_SIZE + MB_SBMAP_BYTES_PERWORD) +
                   (mbcb.space[MB_BIGBLOCKS].mapwords *   (MB_MAPWORD_SIZE + MB_BBMAP_BYTES_PERWORD)) ));

    /* Clear out all the map and block areas */
    memset(mbcb.space[MB_SMALLBLOCKS].bmap, 0,
           (mbcb.space[MB_SMALLBLOCKS].mapwords * (MB_MAPWORD_SIZE + MB_SBMAP_BYTES_PERWORD) +
           (mbcb.space[MB_BIGBLOCKS].mapwords   * (MB_MAPWORD_SIZE + MB_BBMAP_BYTES_PERWORD)) ));

    /* Set up space for small blocks */
    space = &mbcb.space[MB_SMALLBLOCKS];
    space->block = (mbbyte_t *)(space->bmap + space->mapwords);
    space->mi = 0;

    /* Set up space for big blocks */
    prevspace = space;
    space++;
    space->bmap = (mbword_t *)(prevspace->block + (prevspace->mapwords * prevspace->bytes_perword));
    space->block = (mbbyte_t *)(space->bmap + space->mapwords);
    space->mi = 0;

    /* clear out allocation stats */
    memset(mbblkstat, 0, sizeof(mbblkstat));
}

/**
 * \brief
 * Terminate memory block management
 *
 * \details
 * Releases memory used for block management back to the OS
 */
void
mbterm()
{
    free(mbcb.space[MB_SMALLBLOCKS].bmap);
}


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
mballoc(int size)
{
    int         i, nwords, wi;
    uint16      mi;
    mbword_t    smask, amask;   /* start mask, current alloc mask */
    mbspace_t   *space;
    void        *ret;

    space = NULL;
    for (i=0; i < MB_SPACES; i++) {
        if (size <= mbcb.space[i].bytes_perword) {
            /* found block space to use */
            space = &mbcb.space[i];
            break;
        }
    }

    /* if no pace found return NULL */
    if (space == NULL) {
        MB_DEBUG_PRINT("Cannot allocate %d bytes, only up to %d bytes at a time\n",
                       size, (int)(MB_BBMAP_BYTES_PERWORD));
        mbcb.err = MBERR_BIG;
        return NULL;
    }

    nwords = (size + space->bytes_pernib - 1) / space->bytes_pernib;

    /* generate allocation mask to use */
    smask = MB_MAP_ALLOC_END;
    for (i=1; i < nwords; i++) {
        smask >>= MB_MAP_BITS_PERNIB;
        smask |= MB_MAP_ALLOC_MARK;
    }

    /* Scan left to right through word map with alloc mask for space */
    mi = space->mi;
    wi = 0;
    amask = smask;
    while (space->bmap[mi]) {
        if (amask & space->bmap[mi]) {
            if (amask & MB_MAP_ALLOC_RTN_MAP) {
                mi = mbimapinc(mi, space->mapwords);
                wi = 0;
                amask = smask;
                /* if back to where the serch started then no space */
                if (mi == space->mi) {
                    MB_DEBUG_PRINT("No space found for %d bytes!\n", size);
                    mbcb.err = MBERR_NOMEM;
                    return NULL;
                }
            } else {
                amask >>= MB_MAP_ALLOC_MIN_WORDS;
                wi++;
            }
        } else {
            /* found free block */
            break;
        }
    }

    /* Mark space allocated on map and increment map index if at end of word */
    space->bmap[mi] |= amask;
    space->mi = mi;
    if (space->bmap[mi] & MB_MAP_ALLOC_RTN_MAP) {
        space->mi = mbimapinc(mi, space->mapwords);
    }

    mbcb.err = MBERR_OK;
    ret =  &space->block[(mi * space->bytes_perword) + (wi * space->bytes_pernib)];
    MB_DEBUG_PRINT("Allocating %d words for %d bytes at %p:mi %d wi %d amask %.8X\n", 
                   nwords, size, ret, mi, wi, amask);
    return ret;
}


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
mbfree(void *mbp)
{
    int         i,found;
    uint16      mi, wi;
    mbword_t    fmask;
    mbspace_t   *space;

    MB_DEBUG_PRINT("Trying to free memory at %p\n", mbp);

    /* Find which space the memory being freed is in */
    space = &mbcb.space[MB_SMALLBLOCKS];
    found = 0;
    for (i=0; i < MB_SPACES; i++) {
        if (mbp < (void *) (space->block + (space->mapwords * space->bytes_perword))) {
            found = 1;
            break;
        }
        space++;
    }

    if (!found) {
        MB_DEBUG_PRINT("Tried to free memory not owned by mblib at %p\n", mbp);
        mbcb.err = MBERR_UNKNOWN;
        return;
    }

    mi = (mbp - (void *)space->block) / space->bytes_perword;
    wi = ((mbp - (void *)space->block) % space->bytes_perword) / space->bytes_pernib;
    fmask = MB_MAP_ALLOC_LFN_MAP >> (wi * MB_MAP_ALLOC_MIN_WORDS);
    while (mbnibval(space->bmap[mi], wi++) != MB_MAP_ALLOC_END_VAL) {
        if (wi >= MB_MAP_NIB_PERWORD) {
            mbcb.err = MBERR_MAPCORRUPT;
            if (MB_DEBUG) {
                assert(0);
            }
            return;
        }
        fmask |= fmask >> MB_MAP_BITS_PERNIB;
    }
    MB_DEBUG_PRINT("Freed memory at space %i mi %d wi %d fmask %.8X\n", i, mi, wi, fmask);

    space->bmap[mi] &= ~fmask;
}

/**
 * \brief
 * Dump memory space block usage stats
 *
 * \details
 * Prints out the memory block usage statistics for the memory spaces. This is for
 * debugging.
 */
void
mbdumpstat()
{
    int i, *stats, num;

    printf("\n---- Block Allocation Statistics ----\n");

    num = mbstatget(&stats);
    printf("-- small blocks : ");
    for (i=0; i < num; i++) {
        printf("%.6d ", *stats++);
    }
    printf("\n--   big blocks : ");
    for (i=0; i < num; i++) {
        printf("%.6d ", *stats++);
    }
    printf("\n");
}


/**
 * \brief
 * Dump memory space maps
 *
 * \details
 * Prints out the memory maps for the memory spaces. This is for
 * debugging.

 */
void
mbdumpmap()
{
    int i;

    printf("-------- Small Block Map --------\n");
    for (i=0; i < mbcb.space[MB_SMALLBLOCKS].mapwords; i++) {
        printf("%.8X ",mbcb.space[MB_SMALLBLOCKS].bmap[i]);
        if (((i+1) % 8) == 0) printf("\n");
    }

    printf("-------- Big Block Map --------\n");
    for (i=0; i < mbcb.space[MB_BIGBLOCKS].mapwords; i++) {
        printf("%.8X ",mbcb.space[MB_BIGBLOCKS].bmap[i]);
        if (((i+1) % 8) == 0) printf("\n");
    }
}


/**
 * \brief
 * Test that all memory blocks are free
 *
 * \details
 * Tests that both memory spaces are unused. This is for testing purposes.
 *
 * \return
 * 1 if all memory block space is free
 * 0 if some memory block space is allocated
 */
int
mbtestfree()
{
    int i,sp;
    mbspace_t *space;

    space = &mbcb.space[MB_SMALLBLOCKS];

    for (sp=0; sp < MB_SPACES; sp++) {
        for (i=0; i < space->mapwords; i++)
        {
            if (space->bmap[i] != 0) {
                if (MB_DEBUG) {
                    assert(0);
                }
                return 0;
            }
        }
        space++;
    }
    return 1;
}
