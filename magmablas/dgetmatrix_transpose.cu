/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated d Tue Aug 13 16:45:20 2013

*/
#include "common_magma.h"
#define PRECISION_d
#include "commonblas.h"


//
//      m, n - dimensions in the output (ha) matrix.
//             This routine copies the dat matrix from the GPU
//             to ha on the CPU. In addition, the output matrix
//             is transposed. The routine uses a buffer of size
//             2*lddb*nb pointed to by dB (lddb > m) on the GPU. 
//             Note that lda >= m and lddat >= n.
//
extern "C" void 
magmablas_dgetmatrix_transpose( magma_int_t m, magma_int_t n,
                                const double *dat, magma_int_t ldda,
                                double       *ha,  magma_int_t lda,
                                double       *dB,  magma_int_t lddb, magma_int_t nb )
{
    magma_int_t i = 0, j = 0, ib;

    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    if (lda < m || ldda < n || lddb < m){
        printf("Wrong arguments in dgetmatrix_transpose.\n");
        return;
    }

    magma_queue_t stream[2];
    magma_queue_create( &stream[0] );
    magma_queue_create( &stream[1] );

    for(i=0; i<n; i+=nb){
       /* Move data from GPU to CPU using 2 buffers; 1st transpose the data on the GPU */
       ib   = min(n-i, nb);

       //magmablas_dtranspose2 ( dB + (j%2)*nb*lddb, lddb, dat+i, ldda, ib, m);
       magmablas_dtranspose2s( dB + (j%2)*nb*lddb, lddb, dat+i, ldda, ib, m, stream[j%2]);
       magma_dgetmatrix_async( m, ib,
                               dB + (j%2) * nb * lddb, lddb,
                               ha+i*lda,               lda, stream[j%2] );
       j++;
    }

    magma_queue_destroy( stream[0] );
    magma_queue_destroy( stream[1] );
}

//===========================================================================
//  This version is similar to the above but for multiGPUs. The distribution
//  is 1D block cyclic. The input arrays are pointers for the corresponding
//  GPUs. The streams are passed as argument, in contrast to the single GPU
//  routine.
//  NOTE: see magmablas_dgetmatrix_transpose_mgpu.
//===========================================================================
extern "C" void
magmablas_dgetmatrix_transpose2( magma_int_t m, magma_int_t n,
                                 const double **dat, magma_int_t *ldda,
                                 double         *ha, magma_int_t  lda,
                                 double        **dB, magma_int_t  lddb, magma_int_t nb,
                                 magma_int_t num_gpus, magma_queue_t stream[][2] )
{
    magma_int_t i = 0, j[4] = {0, 0, 0, 0}, ib, k;

    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    if (lda < m || lddb < m){
        printf("Wrong arguments in dgetmatrix_transpose2.\n");
        return;
    }

    for(i=0; i<n; i+=nb){
       /* Move data from GPU to CPU using 2 buffers; 1st transpose the data on the GPU */
       k = (i/nb)%num_gpus;
       ib   = min(n-i, nb);
       magma_setdevice(k);

       //magma_queue_sync( stream[k][j[k]%2] );
       //magmablas_dtranspose2( dB[k] + (j[k]%2)*nb*lddb, lddb, 
       //                       dat[k]+i/(nb*num_gpus)*nb, ldda[k], ib, m);
       magmablas_dtranspose2s(dB[k] + (j[k]%2)*nb*lddb, lddb,
                              dat[k]+i/(nb*num_gpus)*nb, ldda[k], 
                              ib, m, stream[k][j[k]%2]);
       magma_dgetmatrix_async( m, ib,
                               dB[k] + (j[k]%2) * nb * lddb, lddb,
                               ha+i*lda,                     lda, stream[k][j[k]%2] );
       j[k]++;
    }
}

