/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated c Wed Aug 14 12:16:39 2013

*/
#include "common_magma.h"

// 512 is maximum number of threads for CUDA capability 1.x
#if (GPUSHMEM < 200)
   #define BLOCK_SIZE 512
#else
   #define BLOCK_SIZE 768
#endif


__global__ void magma_ctrmv_tkernel(magmaFloatComplex *T, int ldt, magmaFloatComplex *v, 
                                    magmaFloatComplex *y);

// ----------------------------------------
// Does sum reduction of array x, leaving total in x[0].
// Contents of x are destroyed in the process.
// With k threads, can reduce array up to 2*k in size.
// Assumes number of threads <= 1024 (which is max number of threads up to CUDA capability 3.0)
// Having n as template parameter allows compiler to evaluate some conditions at compile time.
template< int n >
__device__ void zsum_reduce( /*int n,*/ int i, magmaFloatComplex* x )
{
    __syncthreads();
    if ( n > 1024 ) { if ( i < 1024 && i + 1024 < n ) { x[i] += x[i+1024]; }  __syncthreads(); }
    if ( n >  512 ) { if ( i <  512 && i +  512 < n ) { x[i] += x[i+ 512]; }  __syncthreads(); }
    if ( n >  256 ) { if ( i <  256 && i +  256 < n ) { x[i] += x[i+ 256]; }  __syncthreads(); }
    if ( n >  128 ) { if ( i <  128 && i +  128 < n ) { x[i] += x[i+ 128]; }  __syncthreads(); }
    if ( n >   64 ) { if ( i <   64 && i +   64 < n ) { x[i] += x[i+  64]; }  __syncthreads(); }
    if ( n >   32 ) { if ( i <   32 && i +   32 < n ) { x[i] += x[i+  32]; }  __syncthreads(); }
    // probably don't need __syncthreads for < 16 threads
    // because of implicit warp level synchronization.
    if ( n >   16 ) { if ( i <   16 && i +   16 < n ) { x[i] += x[i+  16]; }  __syncthreads(); }
    if ( n >    8 ) { if ( i <    8 && i +    8 < n ) { x[i] += x[i+   8]; }  __syncthreads(); }
    if ( n >    4 ) { if ( i <    4 && i +    4 < n ) { x[i] += x[i+   4]; }  __syncthreads(); }
    if ( n >    2 ) { if ( i <    2 && i +    2 < n ) { x[i] += x[i+   2]; }  __syncthreads(); }
    if ( n >    1 ) { if ( i <    1 && i +    1 < n ) { x[i] += x[i+   1]; }  __syncthreads(); }
}
// end sum_reduce

//==============================================================================

__global__ void 
magma_cgemv_kernel1(int m, const magmaFloatComplex * __restrict__ V, int ldv, 
                    const magmaFloatComplex * __restrict__ c, 
                    magmaFloatComplex *dwork)
{
        const int i = threadIdx.x;
        const magmaFloatComplex *dV = V + (blockIdx.x) * ldv;

        __shared__ magmaFloatComplex sum[ BLOCK_SIZE ];
        magmaFloatComplex lsum;

        /*  lsum := v' * C  */
        lsum = MAGMA_C_ZERO;
        for( int j = i; j < m; j += BLOCK_SIZE )
           lsum += MAGMA_C_MUL( MAGMA_C_CNJG( dV[j] ), c[j] );
        
        sum[i] = lsum;
        zsum_reduce< BLOCK_SIZE >( i, sum );

        __syncthreads();
        if (i==0)
           dwork [blockIdx.x] = sum[0];
}

//==============================================================================

__global__ void
magma_cgemv_kernel3(int m, const magmaFloatComplex * __restrict__ V, int ldv, magmaFloatComplex *c,
                    magmaFloatComplex *dwork, magmaFloatComplex *tau)
{
        const int i = threadIdx.x;
        const magmaFloatComplex *dV = V + (blockIdx.x) * ldv;

        __shared__ magmaFloatComplex sum[ BLOCK_SIZE ];
        magmaFloatComplex lsum;

        if (i==0)
           c[0] = MAGMA_C_ONE;           

        /*  lsum := v' * C  */
        lsum = MAGMA_C_ZERO;
        for( int j = i; j < m; j += BLOCK_SIZE )
           lsum += MAGMA_C_MUL( MAGMA_C_CNJG( dV[j] ), c[j] );

        sum[i] = lsum;
        zsum_reduce< BLOCK_SIZE >( i, sum );

        __syncthreads();
        if (i==0)
           dwork [blockIdx.x] = -tau[0]*sum[0];
}

//==============================================================================

__global__ void
magma_cgemv_kernel2(int m, int n, const magmaFloatComplex * __restrict__ V, int ldv, 
                    const magmaFloatComplex * __restrict__ x, magmaFloatComplex *c)
{
    const int i = threadIdx.x;
    const int j = i + BLOCK_SIZE * blockIdx.x;
    magmaFloatComplex lsum;

    V += j;

    lsum = MAGMA_C_ZERO;
    if (j < m){
       for(int k=0; k<n; k++)
          lsum += MAGMA_C_MUL( V[k*ldv], x[k]);
       
       c[j] -= lsum;
    }
}

//==============================================================================

/*
    Apply a complex block reflector H to a complex vector C from the left
    (i.e., C = H C). H is represented in the form
          H = I - V T V'
    where T is the complex k-by-k upper triangular matrix in the 
    representation of the block reflector, and V is a complex block of
    k elementary reflectors. 
*/
extern "C" void
magma_clarfbx_gpu(magma_int_t m, magma_int_t k, magmaFloatComplex *V, magma_int_t ldv,
                  magmaFloatComplex *T, magma_int_t ldt, magmaFloatComplex *c,
                  magmaFloatComplex *dwork)
{
    /* dwork = V' c                   */
    magma_cgemv_kernel1<<< k, BLOCK_SIZE, 0, magma_stream >>>(m, V, ldv, c, dwork); 

    /* dwork = T' dwork               */
    magma_ctrmv_tkernel<<< k, k, 0, magma_stream >>>( T, ldt, dwork, dwork+k);
 
    /* c = c - V dwork                */
    dim3  blocks3( (m + BLOCK_SIZE-1) / BLOCK_SIZE );
    dim3 threads3( BLOCK_SIZE );     
    magma_cgemv_kernel2<<< blocks3, threads3, 0, magma_stream >>>( m, k, V, ldv, dwork+k, c);
}

//==============================================================================
