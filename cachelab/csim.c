#define _POSIX_C_SOURCE 200809L
#include "cachelab.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
typedef struct { uint64_t tag, age; int valid; } Line;
static Line *cache;
static unsigned s, b;
static int ways, hits, misses, evictions, verbose;
static uint64_t tick;
static void access_cache(uint64_t address) {
    size_t set = (address >> b) & (((uint64_t)1 << s) - 1);
    uint64_t tag = address >> (s + b);
    Line *lines = cache + set * ways;
    int victim = 0;
    ++tick;
    for (int i = 0; i < ways; ++i) {
        if (lines[i].valid && lines[i].tag == tag) {
            ++hits; lines[i].age = tick;
            if (verbose) printf(" hit");
            return;
        }
    }
    ++misses;
    if (verbose) printf(" miss");
    for (int i = 0; i < ways; ++i) {
        if (!lines[i].valid) { victim = i; break; }
        if (lines[i].age < lines[victim].age) victim = i;
    }
    if (lines[victim].valid) {
        ++evictions;
        if (verbose) printf(" eviction");
    }
    lines[victim] = (Line){tag, tick, 1};
}
static int integer(const char *text, unsigned *value) {
    char *end;
    unsigned long n = strtoul(text, &end, 10);
    if (!*text || *end || *text == '-' || n > INT_MAX) return 0;
    *value = (unsigned)n;
    return 1;
}
int main(int argc, char **argv) {
    char *trace = NULL;
    int opt, have_s = 0, have_b = 0;
    unsigned e = 0;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h': printf("Usage: %s [-v] -s bits -E ways -b bits -t trace\n", argv[0]); return 0;
        case 'v': verbose = 1; break;
        case 's': have_s = integer(optarg, &s); break;
        case 'E': if (!integer(optarg, &e)) return 1; break;
        case 'b': have_b = integer(optarg, &b); break;
        case 't': trace = optarg; break;
        default: return 1;
        }
    }
    if (!have_s || !have_b || !e || !trace || s >= 63 || b >= 64 || s + b >= 64) return 1;
    size_t sets = (uint64_t)1 << s;
    if (sets > SIZE_MAX / e / sizeof(Line)) return 1;
    ways = (int)e;
    cache = calloc(sets * ways, sizeof(Line));
    if (!cache) return 1;
    FILE *file = fopen(trace, "r");
    if (!file) { perror(trace); free(cache); return 1; }
    char operation;
    unsigned long long address;
    int size;
    while (fscanf(file, " %c %llx,%d", &operation, &address, &size) == 3) {
        if (operation == 'I') continue;
        if (operation != 'L' && operation != 'S' && operation != 'M') continue;
        if (verbose) printf("%c %llx,%d", operation, address, size);
        access_cache(address);
        if (operation == 'M') access_cache(address);
        if (verbose) printf("\n");
    }
    fclose(file); free(cache);
    printSummary(hits, misses, evictions);
    return 0;
}
