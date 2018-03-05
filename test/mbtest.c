/**
 * \brief
 * Memory Block Management Library Test
 *
 * \details
 * Run a bunch of tests to make sure the mb library is working
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../mblib.h"

/**
 * \brief Fill space with a known data pattern
 */
void
fill(unsigned char *dp, int size)
{
    while (size) {
        /* Pattern should not be a multiple of space allocated */
        *dp++ = size-- % 100;
    }
}


/**
 * \brief Verify know data pattern in space
 */
void
verify(unsigned char *dp, int size)
{
    while (size) {
        assert(*dp++ == size-- % 100);
    }
}


int main()
{
    int i,j, cursize;
    int mballocsz[20] = {128, 64, 48, 48, 64, 128, 16,   64,  48, 128,
        48, 48, 64, 64, 80,  80, 256, 300, 129, 9000};
    static void *p[2048];
    static int sz[2048];

    mbinit(2,1);
    mbdumpmap();
    mbdumpstat();
    assert(mbtestfree());

    printf("Test 1 - Do some basic allcation, writing, and free of blocks\n");
    printf("allocating and writing...\n");
    for (i=0; i < 20; i++)
    {
        p[i] = mballoc(mballocsz[i]);
        if (p[i]) {
            fill(p[i], mballocsz[i]);
        } else {
            printf("Alloc size: %d  Error:%s\n", mballocsz[i], mberrstr(mberr()));
        }
    }
    mbdumpmap();
    mbdumpstat();
    printf("verifying and freeing...\n");
    for (i=0; i < 20; i++)
    {
        /* verify memory writes  before freeing */
        if (p[i]) {
            verify(p[i], mballocsz[i]);
            mbfree(p[i]);
        }
    }
    mbdumpmap();
    mbdumpstat();
    assert(mbtestfree());

    printf("\nTest 2 - Allocate max smallest blocks in small and big block space verify\n");
    printf("allocating and writing...\n");
    for (i=0,j=0; i < 1024; i++)
    {
        p[j] = mballoc(16);
        fill(p[j++], 16);
        p[j] = mballoc(256);
        fill(p[j++], 256);
    }
    mbdumpmap();
    mbdumpstat();
    printf("verifying and freeing...\n");
    for (i=0,j=0; i < 1024; i++)
    {
        verify(p[j], 16);
        mbfree(p[j++]);
        verify(p[j], 256);
        mbfree(p[j++]);
    }
    mbdumpmap();
    mbdumpstat();
    assert(mbtestfree());

    printf("\nTest 3 - Allocate array of different sizes and then fill in the gaps for small and big block space. \nAlso write and verify blocks before freeing\n");
    printf("allocating and writing...\n");

    i = 0;
    printf("Allocate lots of blocks of different sizes across both spaces\n");
    while (1) {
        sz[i] = 16 * ((i % 8) +1);
        p[i] = mballoc(sz[i]);
        if (p[i]) {
            fill(p[i], sz[i]);
            i++;
        } else {
            break;
        }
    }
    while (1) {
        sz[i] = 256 * ((i % 8) +1);
        p[i] = mballoc(sz[i]);
        if (p[i]) {
            fill(p[i], sz[i]);
            i++;
        } else {
            break;
        }
    }
    mbdumpmap();
    mbdumpstat();

    printf("Now fill in the gaps with smaller and smaller blocks\n");
    cursize = 2048;
    while (cursize) {
        p[i] = mballoc(cursize);
        if (p[i]) {
            sz[i] = cursize;
            fill(p[i], cursize);
            i++;
        } else {
            cursize -= 256;
        }
    }
    cursize = 128;
    while (cursize) {
        p[i] = mballoc(cursize);
        if (p[i]) {
            sz[i] = cursize;
            fill(p[i], cursize);
            i++;
        } else {
            cursize -= 16;
        }
    }
    mbdumpmap();
    mbdumpstat();

    printf("\nFree all blocks\n");
    i = 0;
    while (p[i]) {
        mbfree(p[i++]);
        printf(".");
    }
    printf("\n");
    mbdumpmap();
    mbdumpstat();
    assert(mbtestfree());

    mbterm();
}