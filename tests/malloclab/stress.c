/* Independent live-payload oracle; system allocations only belong to fixture. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "mm.h"
#include "memlib.h"
extern int mm_check(void);
static uint32_t rng;
static unsigned calls;
static uint32_t random_word(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static unsigned char value(unsigned id, size_t offset) { return (unsigned char)(id * 37u + offset * 13u + (offset >> 8)); }
static void fill(void *p, size_t n, unsigned id) { size_t i; for (i=0;i<n;i++) ((unsigned char *)p)[i]=value(id,i); }
static void verify(void *p, size_t n, unsigned id) { size_t i; for (i=0;i<n;i++) assert(((unsigned char *)p)[i]==value(id,i)); }
static void reset(void) { mem_reset_brk(); assert(mm_init()==0); assert(mm_check()); }
static void check(void) { assert(mm_check()); calls++; }
/* Each arrangement exercises a distinct neighbor-coalescing case. */
static void directed(void)
{
    void *a,*b,*c,*d,*p,*q;
    size_t h;
    unsigned mask;
    for(mask=0;mask<4;mask++) {
        reset(); a=mm_malloc(128); b=mm_malloc(128); c=mm_malloc(128); d=mm_malloc(128);
        assert(a&&b&&c&&d); fill(d,128,9);
        if(mask&1) mm_free(a);
        if(mask&2) mm_free(c);
        mm_free(b); check(); verify(d,128,9);
        if(!(mask&1)) mm_free(a);
        if(!(mask&2)) mm_free(c);
        mm_free(d); check();
        h=mem_heapsize(); p=mm_malloc(3000); assert(p); assert(mem_heapsize()==h); mm_free(p); check();
    }
    reset(); assert(mm_malloc(0)==NULL); mm_free(NULL);
    p=mm_realloc(NULL,31); assert(p); fill(p,31,1);
    q=mm_realloc(p,10000); assert(q); verify(q,31,1); fill(q,10000,2); check();
    p=mm_realloc(q,17); assert(p==q); verify(p,17,2); check();
    assert(mm_realloc(p,0)==NULL); check();
    reset(); p=mm_malloc(128); q=mm_malloc(256); a=mm_malloc(128);
    assert(p&&q&&a); fill(p,128,3); mm_free(q);
    q=mm_realloc(p,300); assert(q==p); verify(q,128,3); check();
    fill(q,300,4); p=mm_realloc(q,20000); assert(p&&p!=q); verify(p,300,4); check();
    fill(p,20000,5);
    assert(mm_realloc(p,SIZE_MAX)==NULL); verify(p,20000,5);
    assert(mm_realloc(p,30u*1024*1024)==NULL); verify(p,20000,5); check();
    assert(mm_malloc(SIZE_MAX)==NULL); assert(mm_malloc((size_t)INT_MAX)==NULL);
    mm_free(p); mm_free(a); check();
}
/* Check every byte of every live block before/after operations, with pairwise
 * nonoverlap/range/alignment checks independent of the allocator's metadata. */
static void stress(unsigned seed)
{
    enum { SLOTS=64, OPS=12000 };
    void *p[SLOTS]={0}; size_t sizes[SLOTS]={0}; unsigned ids[SLOTS]={0};
    unsigned k,i,j,index; size_t n,kept; void *q;
    reset(); rng=seed;
    for(k=0;k<OPS;k++) {
        for(i=0;i<SLOTS;i++) if(p[i]) verify(p[i],sizes[i],ids[i]);
        index=(random_word()>>16)%SLOTS;
        n=(random_word()>>8)%4097;
        if(!p[index]) {
            p[index]=mm_malloc(n); assert(p[index] || !n); sizes[index]=n;
            ids[index]=k+1; if(p[index]) fill(p[index],n,ids[index]);
        } else if(random_word()&1) {
            q=mm_realloc(p[index],n); assert(q || !n);
            kept=sizes[index]<n?sizes[index]:n;
            if(q) verify(q,kept,ids[index]);
            p[index]=q; sizes[index]=n; ids[index]=k+1;
            if(q) fill(q,n,ids[index]);
        } else { mm_free(p[index]); p[index]=NULL; sizes[index]=0; }
        check();
        for(i=0;i<SLOTS;i++) if(p[i]) {
            uintptr_t start=(uintptr_t)p[i];
            assert(start%8==0); assert(start>=(uintptr_t)mem_heap_lo());
            assert(start+sizes[i]<=(uintptr_t)mem_heap_hi()+1);
            verify(p[i],sizes[i],ids[i]);
            for(j=i+1;j<SLOTS;j++) if(p[j])
                assert(start+sizes[i]<=(uintptr_t)p[j] || (uintptr_t)p[j]+sizes[j]<=start);
        }
    }
    for(i=0;i<SLOTS;i++) if(p[i]) mm_free(p[i]);
    check();
    printf("RANDOM_OK seed=%u operations=%u heap=%zu\n",seed,OPS,mem_heapsize());
}
/* Force exhaustion, preserve old contents after failed realloc, then reuse heap. */
static void exhaustion(void)
{
    void *p[32],*q; unsigned n=0,i; size_t h;
    reset();
    while(n<32 && (p[n]=mm_malloc(1024*1024))) { fill(p[n],1024*1024,n); n++; }
    assert(n>=16&&n<20); check();
    q=mm_realloc(p[0],20u*1024*1024); assert(!q);
    for(i=0;i<n;i++) verify(p[i],1024*1024,i);
    h=mem_heapsize(); for(i=0;i<n;i++) mm_free(p[i]); check();
    q=mm_malloc(16u*1024*1024); assert(q); assert(mem_heapsize()==h);
    fill(q,16u*1024*1024,55); verify(q,16u*1024*1024,55); mm_free(q); check();
    printf("EXHAUSTION_OK allocations=%u heap=%zu\n",n,h);
}
int main(void)
{
    unsigned seeds[]={1,17,42,99,1234567},i;
    mem_init(); directed();
    for(i=0;i<sizeof(seeds)/sizeof(seeds[0]);i++) stress(seeds[i]);
    exhaustion(); mem_deinit();
    printf("MALLOCLAB_STRESS_OK operations=60000 checker_calls=%u\n",calls);
    return 0;
}
