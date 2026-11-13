#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
void add_rotate_function(lab_test_func f, char *d) {(void)f;(void)d;}
void add_smooth_function(lab_test_func f, char *d) {(void)f;(void)d;}
static uint32_t rng = 0x12345678;
static unsigned short channel(int pattern, size_t n, int c) {
    rng = rng * 1664525u + 1013904223u;
    if (pattern == 0) return 0;
    if (pattern == 1) return 65535;
    if (pattern == 2) return (n + c) % 2 ? 65535 : 0;
    if (pattern == 3) return (unsigned short)(n * 97 + c * 12345);
    return (unsigned short)(rng >> 16);
}
static int same(pixel a, pixel b) {
    return a.red==b.red && a.green==b.green && a.blue==b.blue;
}
int main(void) {
    int dims[64], nd=0, n, pattern, operation;
    size_t cases=0, compared=0;
    for(n=0;n<=35;n++)dims[nd++]=n;
    {int more[]={63,64,65,95,96,97,127,128,129,255,256,257,511,512};
     for(n=0;n<14;n++)dims[nd++]=more[n];}
    for(n=0;n<nd;n++)for(pattern=0;pattern<5;pattern++) {
        int dim=dims[n], y,x;
        size_t count=(size_t)dim*dim, k;
        pixel *src=malloc((count+1)*sizeof(pixel));
        pixel *snapshot=malloc((count+1)*sizeof(pixel));
        pixel *allocation=malloc((count+32)*sizeof(pixel)), *dst=allocation+16;
        assert(src && snapshot && allocation);
        for(k=0;k<count;k++)src[k]=(pixel){channel(pattern,k,0),channel(pattern,k,1),channel(pattern,k,2)};
        memcpy(snapshot,src,count*sizeof(pixel));
        for(operation=0;operation<2;operation++) {
            memset(allocation,0xa5,(count+32)*sizeof(pixel));
            if(operation==0)rotate(dim,src,dst);else smooth(dim,src,dst);
            assert(memcmp(src,snapshot,count*sizeof(pixel))==0);
            for(k=0;k<16*sizeof(pixel);k++) {
                assert(((unsigned char *)allocation)[k]==0xa5);
                assert(((unsigned char *)(dst+count))[k]==0xa5);
            }
            for(y=0;y<dim;y++)for(x=0;x<dim;x++) {
                pixel expected;
                if(operation==0)expected=src[(size_t)x*dim+dim-1-y];
                else {
                    unsigned r=0,g=0,b=0,total=0;int a,c;
                    for(a=y-1;a<=y+1;a++)for(c=x-1;c<=x+1;c++)
                        if(a>=0 && a<dim && c>=0 && c<dim) {
                            pixel p=src[(size_t)a*dim+c];r+=p.red;g+=p.green;b+=p.blue;total++;
                        }
                    expected=(pixel){r/total,g/total,b/total};
                }
                if(!same(dst[(size_t)y*dim+x],expected)) {
                    fprintf(stderr,"Mismatch dim=%d pattern=%d op=%d y=%d x=%d\n",dim,pattern,operation,y,x);return 1;
                }
                compared++;
            }
            cases++;
        }
        free(src);free(snapshot);free(allocation);
    }
    printf("PERFLAB_EDGES_OK dimensions=%d patterns=5 kernels=2 cases=%zu pixels=%zu source_unchanged=true guards=true\n",nd,cases,compared);
    return 0;
}
