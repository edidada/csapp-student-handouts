/* Boundary-tag allocator with an explicit doubly-linked free list.
 * Every block has a size/allocation header and footer; free payloads hold
 * previous/next links. Immediate coalescing prevents adjacent free blocks.
 * Split only when the remainder can hold both links and both tags.
 * No external allocation or global compound state is used.
 */
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
#include "config.h"

team_t team = {"edidada", "edidada", "", "", ""};
#define W sizeof(size_t)
#define MINBLOCK (2 * W + 2 * sizeof(void *))
#define CHUNK 4096
static char *first;
static char *free_head;

/* Tags use low bit for allocation; all block sizes are multiples of eight. */
static size_t tag(const char *p) { return *(const size_t *)p; }
static size_t bytes(const char *p) { return tag(p - W) & ~(size_t)7; }
static int used(const char *p) { return (int)(tag(p - W) & 1); }
static char *next(const char *p) { return (char *)p + bytes(p); }
static char *prev(const char *p) { return (char *)p - (tag(p - 2 * W) & ~(size_t)7); }
static char **back(char *p) { return (char **)p; }
static char **forward(char *p) { return (char **)(p + sizeof(void *)); }
static void mark(char *p, size_t n, int allocated)
{
    *(size_t *)(p - W) = n | (size_t)allocated;
    *(size_t *)(p + n - 2 * W) = n | (size_t)allocated;
}

/* Remove a free block before changing its tags or payload. */
static void unlink_free(char *p)
{
    char *a = *back(p), *b = *forward(p);
    if (a) *forward(a) = b; else free_head = b;
    if (b) *back(b) = a;
}

/* Coalesce both neighbors, then insert exactly one free-list node. */
static char *coalesce(char *p)
{
    size_t n = bytes(p);
    char *a = prev(p), *b = next(p);
    if (!used(a)) { unlink_free(a); n += bytes(a); p = a; }
    if (!used(b)) { unlink_free(b); n += bytes(b); }
    mark(p, n, 0);
    *back(p) = NULL;
    *forward(p) = free_head;
    if (free_head) *back(free_head) = p;
    free_head = p;
    return p;
}

/* Extend only within the simulated heap and int-sized mem_sbrk interface. */
static char *extend(size_t n)
{
    char *p;
    if (n > (size_t)INT_MAX || n > MAX_HEAP - mem_heapsize()) return NULL;
    p = mem_sbrk((int)n);
    if (p == (void *)-1) return NULL;
    mark(p, n, 0);
    *(size_t *)(p + n - W) = 1;
    return coalesce(p);
}

/* Reject overflow before adding metadata/alignment. malloc(0) returns NULL. */
static size_t adjusted(size_t n)
{
    if (!n || n > SIZE_MAX - 2 * W - 7) return 0;
    n = (n + 2 * W + 7) & ~(size_t)7;
    return n < MINBLOCK ? MINBLOCK : n;
}

/* Place an allocated block, merging any split tail with its free neighbor. */
static void place(char *p, size_t n, size_t total)
{
    if (total - n >= MINBLOCK) {
        mark(p, n, 1);
        mark(p + n, total - n, 0);
        coalesce(p + n);
    } else mark(p, total, 1);
}

/* Reinitialize all state after the driver's mem_reset_brk. */
int mm_init(void)
{
    char *p;
    first = free_head = NULL;
    p = mem_sbrk((int)(4 * W));
    if (p == (void *)-1) return -1;
    *(size_t *)p = 0;
    mark(p + 2 * W, 2 * W, 1);
    *(size_t *)(p + 3 * W) = 1;
    first = p + 4 * W;
    return extend(CHUNK) ? 0 : -1;
}

/* First-fit search of free blocks only, with bounded chunk extension. */
void *mm_malloc(size_t size)
{
    size_t n = adjusted(size), total;
    char *p;
    if (!n) return NULL;
    for (p = free_head; p && bytes(p) < n; p = *forward(p)) {}
    if (!p) {
        size_t growth = n > CHUNK ? n : CHUNK;
        p = extend(growth);
        if (!p && growth != n) p = extend(n);
        if (!p) return NULL;
    }
    total = bytes(p);
    unlink_free(p);
    place(p, n, total);
    return p;
}

/* NULL is a no-op; valid live blocks are immediately reusable. */
void mm_free(void *ptr)
{
    char *p = ptr;
    if (!p) return;
    mark(p, bytes(p), 0);
    coalesce(p);
}

/* Shrink in place; grow into a free successor or the wilderness when possible.
 * Otherwise copy only the old payload capacity, keeping old data on failure. */
void *mm_realloc(void *ptr, size_t size)
{
    char *p = ptr, *q, *replacement;
    size_t n, old, total, copy;
    if (!p) return mm_malloc(size);
    if (!size) { mm_free(p); return NULL; }
    n = adjusted(size);
    if (!n) return NULL;
    old = bytes(p);
    if (n <= old) { place(p, n, old); return p; }
    q = next(p);
    total = old + (used(q) ? 0 : bytes(q));
    if (total < n && (!bytes(q) || (!used(q) && !bytes(next(q))))) {
        size_t growth = n - total;
        if (growth < CHUNK) growth = CHUNK;
        if (!extend(growth) && growth != n - total) extend(n - total);
        q = next(p);
        total = old + (used(q) ? 0 : bytes(q));
    }
    if (!used(q) && total >= n) {
        unlink_free(q);
        place(p, n, total);
        return p;
    }
    replacement = mm_malloc(size);
    if (!replacement) return NULL;
    copy = old - 2 * W;
    if (copy > size) copy = size;
    memcpy(replacement, p, copy);
    mm_free(p);
    return replacement;
}

/* Verify physical partition, alignment, matching tags, no adjacent free blocks,
 * valid reciprocal list links, no cycles, and exact free-block membership.
 * Called by independent tests, never by the timed allocation path. */
int mm_check(void)
{
    char *p, *q, *previous = NULL;
    char *limit = (char *)mem_heap_lo() + mem_heapsize();
    size_t physical = 0, listed = 0, count, n;
    int previous_free = 0;
    if (!first) return 0;
    for (p = first; p < limit; p += n) {
        if ((uintptr_t)p % 8) return 0;
        n = bytes(p);
        if (!n) { if (p != limit || !used(p)) return 0; break; }
        if (n < MINBLOCK || n % 8 || n > (size_t)(limit - p)) return 0;
        if (tag(p - W) != tag(p + n - 2 * W)) return 0;
        if (!used(p)) { if (previous_free) return 0; physical++; }
        previous_free = !used(p);
    }
    if (p != limit || tag(limit - W) != 1) return 0;
    for (q = free_head; q; q = *forward(q)) {
        if (++listed > physical) return 0;
        for (p = first; p < limit && p != q; p += bytes(p)) {}
        if (p == limit || used(q) || *back(q) != previous) return 0;
        previous = q;
    }
    if (listed != physical) return 0;
    for (p = first; p < limit; p += bytes(p)) if (!used(p)) {
        count = 0;
        for (q = free_head; q; q = *forward(q)) if (p == q) count++;
        if (count != 1) return 0;
    }
    return 1;
}
