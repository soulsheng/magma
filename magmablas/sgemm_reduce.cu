/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated s Wed Aug 14 12:16:37 2013

*/
#include "common_magma.h"

// 512 is maximum number of threads for CUDA capability 1.x
#if (GPUSHMEM < 200)
    #define NUM_THREADS 512
#else
    #define NUM_THREADS 1024
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// size of work for a thread block
#define BLK_M 16
#define BLK_N 16

#define BLK_K (NUM_THREADS / (BLK_M * BLK_N))


///////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------
// Does sum reduction of array x, leaving total in x[0].
// Contents of x are destroyed in the process.
// With k threads, can reduce array up to 2*k in size.
// Assumes number of threads <= 1024 (which is max number of threads up to CUDA capability 3.0)
// Having n as template parameter allows compiler to evaluate some conditions at compile time.

template< int n >
__device__ void sum_reduce2( /*int n,*/ int j, int k, int i, float x[][ BLK_N +1][ BLK_K +1] )
{
    __syncthreads();
/*
    if ( n > 1024 ) { if ( i < 1024 && i + 1024 < n ) { x[j][k][i] += x[j][k][i+1024]; }  __syncthreads(); }
    if ( n >  512 ) { if ( i <  512 && i +  512 < n ) { x[j][k][i] += x[j][k][i+ 512]; }  __syncthreads(); }
    if ( n >  256 ) { if ( i <  256 && i +  256 < n ) { x[j][k][i] += x[j][k][i+ 256]; }  __syncthreads(); }
    if ( n >  128 ) { if ( i <  128 && i +  128 < n ) { x[j][k][i] += x[j][k][i+ 128]; }  __syncthreads(); }
    if ( n >   64 ) { if ( i <   64 && i +   64 < n ) { x[j][k][i] += x[j][k][i+  64]; }  __syncthreads(); }
*/
    if ( n >   32 ) { if ( i <   32 && i +   32 < n ) { x[j][k][i] += x[j][k][i+  32]; }  __syncthreads(); }
    // probably don't need __syncthreads for < 16 threads
    // because of implicit warp level synchronization.
    if ( n >   16 ) { if ( i <   16 && i +   16 < n ) { x[j][k][i] += x[j][k][i+  16]; }  __syncthreads(); }
    if ( n >    8 ) { if ( i <    8 && i +    8 < n ) { x[j][k][i] += x[j][k][i+   8]; }  __syncthreads(); }
    if ( n >    4 ) { if ( i <    4 && i +    4 < n ) { x[j][k][i] += x[j][k][i+   4]; }  __syncthreads(); }
    if ( n >    2 ) { if ( i <    2 && i +    2 < n ) { x[j][k][i] += x[j][k][i+   2]; }  __syncthreads(); }
    if ( n >    1 ) { if ( i <    1 && i +    1 < n ) { x[j][k][i] += x[j][k][i+   1]; }  __syncthreads(); }
}
// end sum_reduce


//==============================================================================

__global__
void magmablas_sgemm_reduce_kernel(int m, int n, int k, 
                                   float alpha, 
                                   const float * __restrict__ d_A, int lda,
                                   const float * __restrict__ d_B, int ldb,
                                   float beta,
                                   float *d_C, int ldc)
{
    const int i = threadIdx.x;
    
    if (blockIdx.x*BLK_M + threadIdx.y < m && blockIdx.y*BLK_N + threadIdx.z < n){
    
        const float *dA = d_A + (blockIdx.x*BLK_M + threadIdx.y) * lda;
        const float *dB = d_B + (blockIdx.y*BLK_N + threadIdx.z) * ldb;
        float *dC = d_C + blockIdx.x*BLK_M + blockIdx.y*BLK_N * ldc;
        
        __shared__ float sum[BLK_M][BLK_N+1][ BLK_K +1];
        float lsum;
        
        /*  w := v' * C  */
        lsum = MAGMA_S_ZERO;
        for( int j = i; j < k; j += BLK_K )
            lsum += MAGMA_S_CNJG( dA[j] )* dB[j];
        
        sum[threadIdx.y][threadIdx.z][i] = lsum;
        sum_reduce2< BLK_K >( threadIdx.y, threadIdx.z, i, sum );
        
        /*  C := C - v * w  */
        __syncthreads();
        if (threadIdx.x == 0) {
            if (MAGMA_S_EQUAL(beta, MAGMA_S_ZERO))
                dC[threadIdx.y + threadIdx.z*ldc] = alpha*sum[threadIdx.y][threadIdx.z][0];
            else
                dC[threadIdx.y + threadIdx.z*ldc] = beta* dC[threadIdx.y + threadIdx.z*ldc] + 
                                                    alpha*sum[threadIdx.y][threadIdx.z][0];
        }
    }
}

//==============================================================================

extern "C" void
magmablas_sgemm_reduce(magma_int_t m, magma_int_t n, magma_int_t k,
                       float alpha,
                       const float *d_A, magma_int_t lda,
                       const float *d_B, magma_int_t ldb,
                       float beta,
                       float *d_C, magma_int_t ldc )
{
/*  -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

    Purpose
    =======
    SGEMM_REDUCE  performs one of the matrix-matrix operations
    
       C := alpha* A' B  + beta*C,
    
    where alpha and beta are scalars, and A, B and C are matrices, with A
    a k-by-m matrix, B a k-by-n matrix, and C an m-by-n matrix. 
    
    This routine is tuned for m, n << k. Typically, m and n are expected
    to be less than 128. 
    =====================================================================    */

    dim3  blocks( (m+BLK_M-1)/BLK_M, (n+BLK_N-1)/BLK_N );
    dim3 threads( BLK_K, BLK_M, BLK_N );
    magmablas_sgemm_reduce_kernel<<<blocks,threads, 0, magma_stream >>>(
        m, n, k, alpha, d_A, lda,  d_B, ldb, beta, d_C, ldc );
}

//==============================================================================
