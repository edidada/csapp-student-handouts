/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following team struct 
 */
team_t team = {
    "edidada",            /* Self-study team ID */

    "edidada",            /* Self-study author */
    "",                   /* No personal email supplied */

    "",                   /* Second member full name (leave blank if none) */
    ""                    /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/

/* 
 * naive_rotate - The naive baseline version of rotate 
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/* 
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_descr[] = "rotate: 16x16 tiles with contiguous destination writes";
void rotate(int dim, pixel *src, pixel *dst) 
{
    int row, col, i, j;
    /* 16x16 tiles bound the working set; inner stores are contiguous. */
    for (row = 0; row < dim; row += 16) {
        int endrow = row + 16 < dim ? row + 16 : dim;
        for (col = 0; col < dim; col += 16) {
            int endcol = col + 16 < dim ? col + 16 : dim;
            for (j = col; j < endcol; j++) {
                pixel *out = dst + (dim - 1 - j) * dim + row;
                for (i = row; i < endrow; i++) {
                    *out++ = src[i * dim + j];
                }
            }
        }
    }
}

/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);   
    add_rotate_function(&rotate, rotate_descr);   
    /* ... Register additional test functions here */
}


/***************
 * SMOOTH KERNEL
 **************/

/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_sum(pixel_sum *sum, pixel p) 
{
    sum->red += (int) p.red;
    sum->green += (int) p.green;
    sum->blue += (int) p.blue;
    sum->num++;
    return;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
    current_pixel->red = (unsigned short) (sum.red/sum.num);
    current_pixel->green = (unsigned short) (sum.green/sum.num);
    current_pixel->blue = (unsigned short) (sum.blue/sum.num);
    return;
}

/* 
 * avg - Returns averaged pixel value at (i,j) 
 */
static pixel avg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for(ii = max(i-1, 0); ii <= min(i+1, dim-1); ii++) 
	for(jj = max(j-1, 0); jj <= min(j+1, dim-1); jj++) 
	    accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
}

/*
 * smooth - Your current working version of smooth. 
 * IMPORTANT: This is the version you will be graded on
 */
char smooth_descr[] = "smooth: rolling three-row column sums and separate borders";
void smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;
    if (dim < 3) {
        naive_smooth(dim, src, dst);
        return;
    }
    /* Handle the O(dim) border separately: interior always has 9 samples. */
    for (j = 0; j < dim; j++) {
        dst[j] = avg(dim, 0, j, src);
        dst[(dim - 1) * dim + j] = avg(dim, dim - 1, j, src);
    }
    for (i = 1; i < dim - 1; i++) {
        pixel *top = src + (i - 1) * dim;
        pixel *mid = src + i * dim;
        pixel *bottom = src + (i + 1) * dim;
        int lr = top[0].red + mid[0].red + bottom[0].red;
        int lg = top[0].green + mid[0].green + bottom[0].green;
        int lb = top[0].blue + mid[0].blue + bottom[0].blue;
        int cr = top[1].red + mid[1].red + bottom[1].red;
        int cg = top[1].green + mid[1].green + bottom[1].green;
        int cb = top[1].blue + mid[1].blue + bottom[1].blue;
        dst[i * dim] = avg(dim, i, 0, src);
        dst[i * dim + dim - 1] = avg(dim, i, dim - 1, src);
        for (j = 1; j < dim - 1; j++) {
            int rr = top[j + 1].red + mid[j + 1].red + bottom[j + 1].red;
            int rg = top[j + 1].green + mid[j + 1].green + bottom[j + 1].green;
            int rb = top[j + 1].blue + mid[j + 1].blue + bottom[j + 1].blue;
            pixel *out = dst + i * dim + j;
            out->red = (unsigned short)((lr + cr + rr) / 9);
            out->green = (unsigned short)((lg + cg + rg) / 9);
            out->blue = (unsigned short)((lb + cb + rb) / 9);
            lr = cr; lg = cg; lb = cb;
            cr = rr; cg = rg; cb = rb;
        }
    }
}


/********************************************************************* 
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&smooth, smooth_descr);
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    /* ... Register additional test functions here */
}

