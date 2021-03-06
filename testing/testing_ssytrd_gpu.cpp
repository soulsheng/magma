/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @author Raffaele Solca
       @author Stan Tomov
       @author Azzam Haidar

       @generated s Wed Aug 14 12:18:08 2013

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

#define PRECISION_s

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssytrd_gpu
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t    gflops, gpu_perf, cpu_perf, gpu_time, cpu_time;
    float           eps;
    float *h_A, *h_R, *d_R, *h_Q, *h_work, *work, *dwork;
    float *tau;
    float          *diag, *offdiag;
    float           result[2] = {0., 0.};
    magma_int_t N, n2, lda, lwork, info, nb, ldwork;
    magma_int_t ione     = 1;
    magma_int_t itwo     = 2;
    magma_int_t ithree   = 3;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t status = 0;
    
    #if defined(PRECISION_z) || defined(PRECISION_c)
    float *rwork;
    #endif

    eps = lapackf77_slamch( "E" );

    magma_opts opts;
    parse_opts( argc, argv, &opts );

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("Running version %d; available are (specified through --version num):\n", 
           (int) opts.version);
    printf("1 - uses SSYMV from CUBLAS (default)\n");
    printf("2 - uses SSYMV from MAGMA BLAS that requires extra space\n\n");

    printf("  N     CPU GFlop/s (sec)   GPU GFlop/s (sec)   |A-QHQ'|/N|A|   |I-QQ'|/N\n");
    printf("===========================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            lda    = N;
            n2     = N*lda;
            nb     = magma_get_ssytrd_nb(N);
            lwork  = N*nb;  /* We suppose the magma nb is bigger than lapack nb */
            gflops = FLOPS_SSYTRD( N ) / 1e9;
            ldwork = (N*N+64-1)/64 + 2*N*nb;
            
            TESTING_MALLOC(    h_A,     float, lda*N );
            TESTING_DEVALLOC(  d_R,     float, lda*N );
            TESTING_DEVALLOC(dwork,     float, ldwork);
            TESTING_HOSTALLOC( h_R,     float, lda*N );
            TESTING_HOSTALLOC( h_work,  float, lwork );
            TESTING_MALLOC(    tau,     float, N     );
            TESTING_MALLOC(    diag,    float, N   );
            TESTING_MALLOC(    offdiag, float, N-1 );
            
            if ( opts.check ) {
                TESTING_MALLOC( h_Q,  float, lda*N );
                TESTING_MALLOC( work, float, 2*N*N );
                #if defined(PRECISION_z) || defined(PRECISION_c)
                TESTING_MALLOC( rwork, float, N );
                #endif
            }
            
            /* ====================================================================
               Initialize the matrix
               =================================================================== */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            magma_smake_symmetric( N, h_A, lda );
            magma_ssetmatrix( N, N, h_A, lda, d_R, lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            if (opts.version == 1)
                magma_ssytrd_gpu( opts.uplo, N, d_R, lda, diag, offdiag,
                                  tau, h_R, lda, h_work, lwork, &info );
            else
                magma_ssytrd2_gpu( opts.uplo, N, d_R, lda, diag, offdiag,
                                   tau, h_R, lda, h_work, lwork, dwork, ldwork, &info );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_ssytrd_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.check ) {
                magma_sgetmatrix( N, N, d_R, lda, h_R, lda );
                magma_sgetmatrix( N, N, d_R, lda, h_Q, lda );
                lapackf77_sorgtr( &opts.uplo, &N, h_Q, &lda, tau, h_work, &lwork, &info );
                
                #if defined(PRECISION_z) || defined(PRECISION_c)
                lapackf77_ssyt21( &itwo, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, rwork, &result[0] );
                
                lapackf77_ssyt21( &ithree, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, rwork, &result[1] );
                #else
                lapackf77_ssyt21( &itwo, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, &result[0] );
                
                lapackf77_ssyt21( &ithree, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, &result[1] );
                #endif
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_ssytrd( &opts.uplo, &N, h_A, &lda, diag, offdiag, tau,
                                  h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapackf77_ssytrd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Print performance and error.
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            } else {
                printf("%5d     ---   (  ---  )   %7.2f (%7.2f)",
                       (int) N, gpu_perf, gpu_time );
            }
            if ( opts.check ) {
                printf("   %8.2e        %8.2e%s", result[0]*eps, result[1]*eps,
                        ( ( (result[0]*eps < tol) && (result[1]*eps < tol) ) ? "" : "  failed")  );
                status |= ! (result[0]*eps < tol);
                status |= ! (result[1]*eps < tol);
            } else {
                printf("     ---             ---\n" );
            }
            
            TESTING_FREE( h_A );
            TESTING_FREE( tau );
            TESTING_FREE( diag );
            TESTING_FREE( offdiag );
            TESTING_HOSTFREE( h_R );
            TESTING_HOSTFREE( h_work );
            TESTING_DEVFREE ( d_R );
            TESTING_DEVFREE ( dwork );
            
            if ( opts.check ) {
                TESTING_FREE( h_Q );
                TESTING_FREE( work );
                #if defined(PRECISION_z) || defined(PRECISION_c)
                TESTING_FREE( rwork );
                #endif
            }
        }
    }

    TESTING_FINALIZE();
    return status;
}
