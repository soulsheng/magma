/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated c Tue Aug 13 16:45:48 2013
       @author Chongxiao Cao
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cher2k
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, cublas_perf, cublas_time, cpu_perf, cpu_time;
    float          cublas_error, Cnorm, work[1];
    magma_int_t N, K;
    magma_int_t Ak, An, Bk, Bn;
    magma_int_t sizeA, sizeB, sizeC;
    magma_int_t lda, ldb, ldc, ldda, lddb, lddc;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magmaFloatComplex *h_A, *h_B, *h_C, *h_Ccublas;
    magmaFloatComplex *d_A, *d_B, *d_C;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    magmaFloatComplex alpha = MAGMA_C_MAKE(  0.29, -0.86 );
    float beta  = MAGMA_D_MAKE( -0.48,  0.38 );
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf("If running lapack (option --lapack), MAGMA and CUBLAS error are both computed\n"
           "relative to CPU BLAS result. Else, MAGMA error is computed relative to CUBLAS result.\n\n"
           "uplo = %c, transA = %c\n", opts.uplo, opts.transA );
    printf("    N     K   CUBLAS Gflop/s (ms)   CPU Gflop/s (ms)  CUBLAS error\n");
    printf("==================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.msize[i];
            K = opts.ksize[i];
            gflops = FLOPS_CHER2K(K, N) / 1e9;

            if ( opts.transA == MagmaNoTrans ) {
                lda = An = N;
                Ak = K;
                ldb = Bn = N;
                Bk = K;
            } else {
                lda = An = K;
                Ak = N;
                ldb = Bn = K;
                Bk = N;
            }
            
            ldc = N;
            
            ldda = ((lda+31)/32)*32;
            lddb = ((ldb+31)/32)*32;
            lddc = ((ldc+31)/32)*32;
            
            sizeA = lda*Ak;
            sizeB = ldb*Ak;
            sizeC = ldc*N;
            
            TESTING_MALLOC( h_A,  magmaFloatComplex, lda*Ak );
            TESTING_MALLOC( h_B,  magmaFloatComplex, ldb*Bk );
            TESTING_MALLOC( h_C,  magmaFloatComplex, ldc*N  );
            TESTING_MALLOC( h_Ccublas, magmaFloatComplex, ldc*N  );
            
            TESTING_DEVALLOC( d_A, magmaFloatComplex, ldda*Ak );
            TESTING_DEVALLOC( d_B, magmaFloatComplex, lddb*Bk );
            TESTING_DEVALLOC( d_C, magmaFloatComplex, lddc*N  );
            
            /* Initialize the matrices */
            lapackf77_clarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_clarnv( &ione, ISEED, &sizeB, h_B );
            lapackf77_clarnv( &ione, ISEED, &sizeC, h_C );
            
            /* =====================================================================
               Performs operation using CUDA-BLAS
               =================================================================== */
            magma_csetmatrix( An, Ak, h_A, lda, d_A, ldda );
            magma_csetmatrix( Bn, Bk, h_B, ldb, d_B, lddb );
            magma_csetmatrix( N, N, h_C, ldc, d_C, lddc );
            
            cublas_time = magma_sync_wtime( NULL );
            cublasCher2k( opts.uplo, opts.transA, N, K,
                         alpha, d_A, ldda,
                                d_B, lddb,
                         beta,  d_C, lddc );
            cublas_time = magma_sync_wtime( NULL ) - cublas_time;
            cublas_perf = gflops / cublas_time;
            
            magma_cgetmatrix( N, N, d_C, lddc, h_Ccublas, ldc );
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                blasf77_cher2k( &opts.uplo, &opts.transA, &N, &K,
                               &alpha, h_A, &lda,
                                       h_B, &ldb,
                               &beta,  h_C, &ldc );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.lapack ) {
                // compute relative error for both magma & cublas, relative to lapack,
                // |C_magma - C_lapack| / |C_lapack|
                Cnorm = lapackf77_clange( "M", &N, &N, h_C, &ldc, work );
                
                blasf77_caxpy( &sizeC, &c_neg_one, h_C, &ione, h_Ccublas, &ione );
                cublas_error = lapackf77_clange( "M", &N, &N, h_Ccublas, &ldc, work ) / Cnorm;
                
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)    %8.2e\n",
                       (int) N, (int) K,
                       cublas_perf, 1000.*cublas_time,
                       cpu_perf,    1000.*cpu_time,
                       cublas_error );
            }
            else {
                printf("%5d %5d   %7.2f (%7.2f)    ---   (  ---  )    ---     ---\n",
                       (int) N, (int) K,
                       cublas_perf, 1000.*cublas_time);
            }
            
            TESTING_FREE( h_A  );
            TESTING_FREE( h_B  );
            TESTING_FREE( h_C  );
            TESTING_FREE( h_Ccublas );
            
            TESTING_DEVFREE( d_A );
            TESTING_DEVFREE( d_B );
            TESTING_DEVFREE( d_C );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return 0;
}
