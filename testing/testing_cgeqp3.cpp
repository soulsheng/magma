/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated c Tue Aug 13 16:46:08 2013

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

#define PRECISION_c

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cgeqp3
*/
int main( int argc, char** argv)
{
    TESTING_INIT();
    
    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf=0, cpu_time=0;
    magmaFloatComplex *h_A, *h_R, *tau, *h_work;
    magma_int_t *jpvt;
    magma_int_t M, N, n2, lda, lwork, j, info, min_mn, nb;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf("  M     N     CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||A*P - Q*R||_F\n");
    printf("=====================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[i];
            N = opts.nsize[i];
            min_mn = min(M, N);
            lda    = M;
            n2     = lda*N;
            nb     = magma_get_cgeqp3_nb( min_mn );
            gflops = FLOPS_CGEQRF( M, N ) / 1e9;
            
            lwork = ( N+1 )*nb;
            #if defined(PRECISION_d) || defined(PRECISION_s)
            lwork += 2*N;
            #endif
            if ( opts.check )
                lwork = max( lwork, M*N + N );
            
            #if defined(PRECISION_z) || defined(PRECISION_c)
            float *rwork;
            TESTING_MALLOC(    rwork,  float,          2*N    );
            #endif
            TESTING_MALLOC(    jpvt,   magma_int_t,     N      );
            TESTING_MALLOC(    tau,    magmaFloatComplex, min_mn );
            TESTING_MALLOC(    h_A,    magmaFloatComplex, n2     );
            TESTING_HOSTALLOC( h_R,    magmaFloatComplex, n2     );
            TESTING_HOSTALLOC( h_work, magmaFloatComplex, lwork  );
            
            /* Initialize the matrix */
            lapackf77_clarnv( &ione, ISEED, &n2, h_A );
            lapackf77_clacpy( MagmaUpperLowerStr, &M, &N, h_A, &lda, h_R, &lda );
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                for( j = 0; j < N; j++)
                    jpvt[j] = 0;
                
                cpu_time = magma_wtime();
                #if defined(PRECISION_z) || defined(PRECISION_c)
                lapackf77_cgeqp3(&M, &N, h_R, &lda, jpvt, tau, h_work, &lwork, rwork, &info);
                #else
                lapackf77_cgeqp3(&M, &N, h_R, &lda, jpvt, tau, h_work, &lwork, &info);
                #endif
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapack_cgeqp3 returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            lapackf77_clacpy( MagmaUpperLowerStr, &M, &N, h_A, &lda, h_R, &lda );
            for( j = 0; j < N; j++)
                jpvt[j] = 0;
            
            gpu_time = magma_wtime();
            #if defined(PRECISION_z) || defined(PRECISION_c)
            magma_cgeqp3(M, N, h_R, lda, jpvt, tau, h_work, lwork, rwork, &info);
            #else
            magma_cgeqp3(M, N, h_R, lda, jpvt, tau, h_work, lwork, &info);
            #endif
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_cgeqp3 returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (int) M, (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            }
            else {
                printf("%5d %5d     ---   (  ---  )   %7.2f (%7.2f)",
                       (int) M, (int) N, gpu_perf, gpu_time );
            }
            if ( opts.check ) {
                float error, ulp;
                
                magma_int_t minmn = min(M, N);
                ulp = lapackf77_slamch( "P" );
                
                // Compute norm( A*P - Q*R )
                error = lapackf77_cqpt01( &M, &N, &minmn, h_A, h_R, &lda,
                                          tau, jpvt, h_work, &lwork );
                error *= ulp;
                
                printf("   %8.2e\n", error );
            }
            else {
                printf("     ---  \n");
            }
            
            #if defined(PRECISION_z) || defined(PRECISION_c)
            TESTING_FREE( rwork );
            #endif
            TESTING_FREE( jpvt );
            TESTING_FREE( tau );
            TESTING_FREE( h_A );
            TESTING_HOSTFREE( h_R );
            TESTING_HOSTFREE( h_work );
        }
    }

    TESTING_FINALIZE();
    return 0;
}
