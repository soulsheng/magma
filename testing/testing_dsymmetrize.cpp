/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated d Tue Aug 13 16:45:53 2013
       @author Mark Gates

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dsymmetrize
   Code is very similar to testing_dtranspose.cpp
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t    gbytes, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double           error, work[1];
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *h_A, *h_R;
    double *d_A;
    magma_int_t N, size, lda, ldda;
    magma_int_t ione     = 1;
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf("    N   CPU GByte/s (sec)   GPU GByte/s (sec)   check\n");
    printf("=====================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            lda    = N;
            ldda   = ((N+31)/32)*32;
            size   = lda*N;
            // load strictly lower triangle, save strictly upper triangle
            gbytes = sizeof(double) * 1.*N*(N-1) / 1e9;
    
            TESTING_MALLOC(   h_A, double, size   );
            TESTING_MALLOC(   h_R, double, size   );
            TESTING_DEVALLOC( d_A, double, ldda*N );
            
            /* Initialize the matrix */
            for( int j = 0; j < N; ++j ) {
                for( int i = 0; i < N; ++i ) {
                    h_A[i + j*lda] = MAGMA_D_MAKE( i + j/10000., j );
                }
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_dsetmatrix( N, N, h_A, lda, d_A, ldda );
            
            gpu_time = magma_sync_wtime( 0 );
            //magmablas_dsymmetrize( opts.uplo, N-2, d_A+1+ldda, ldda );  // inset by 1 row & col
            magmablas_dsymmetrize( opts.uplo, N, d_A, ldda );
            gpu_time = magma_sync_wtime( 0 ) - gpu_time;
            gpu_perf = gbytes / gpu_time;
            
            /* =====================================================================
               Performs operation using naive in-place algorithm
               (LAPACK doesn't implement symmetrize)
               =================================================================== */
            cpu_time = magma_wtime();
            //for( int j = 1; j < N-1; ++j ) {    // inset by 1 row & col
            //    for( int i = 1; i < j; ++i ) {
            for( int j = 0; j < N; ++j ) {
                for( int i = 0; i < j; ++i ) {
                    if ( opts.uplo == MagmaLower ) {
                        h_A[i + j*lda] = MAGMA_D_CNJG( h_A[j + i*lda] );
                    }
                    else {
                        h_A[j + i*lda] = MAGMA_D_CNJG( h_A[i + j*lda] );
                    }
                }
            }
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gbytes / cpu_time;
            
            /* =====================================================================
               Check the result
               =================================================================== */
            magma_dgetmatrix( N, N, d_A, ldda, h_R, lda );
            
            blasf77_daxpy(&size, &c_neg_one, h_A, &ione, h_R, &ione);
            error = lapackf77_dlange("f", &N, &N, h_R, &lda, work);

            printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %s\n",
                   (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time,
                   (error == 0. ? "ok" : "failed") );
            
            TESTING_FREE( h_A );
            TESTING_FREE( h_R );
            TESTING_DEVFREE( d_A );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return 0;
}
