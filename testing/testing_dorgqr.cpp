/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated d Tue Aug 13 16:46:07 2013

       @author Stan Tomov
       @author Mathieu Faverge
       @author Mark Gates
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <assert.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dorgqr
*/
int main( int argc, char** argv )
{
    TESTING_INIT();

    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double           error, work[1];
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *hA, *hR, *tau, *h_work;
    double *dA, *dT;
    magma_int_t m, n, k;
    magma_int_t n2, lda, ldda, lwork, min_mn, nb, info;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    printf("Running version %d; available are (specified through --version num):\n",
           (int) opts.version);
    printf("1 - uses precomputed dlarft matrices (default)\n");
    printf("2 - recomputes the dlarft matrices on the fly\n\n");

    printf("    m     n     k   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||R|| / ||A||\n");
    printf("=========================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ){
        for( int iter = 0; iter < opts.niter; ++iter ) {
            m = opts.msize[i];
            n = opts.nsize[i];
            k = opts.ksize[i];
            if ( m < n || n < k ) {
                printf( "skipping m %d, n %d, k %d because m < n or n < k\n", (int) m, (int) n, (int) k );
                continue;
            }
            
            lda  = m;
            ldda = ((m + 31)/32)*32;
            n2 = lda*n;
            min_mn = min(m, n);
            nb = magma_get_dgeqrf_nb( m );
            lwork  = (m + 2*n+nb)*nb;
            gflops = FLOPS_DORGQR( m, n, k ) / 1e9;
            
            TESTING_MALLOC(    hA,     double, lda*n  );
            TESTING_HOSTALLOC( h_work, double, lwork  );
            TESTING_HOSTALLOC( hR,     double, lda*n  );
            TESTING_MALLOC(    tau,    double, min_mn );
            TESTING_DEVALLOC(  dA,     double, ldda*n );
            TESTING_DEVALLOC(  dT,     double, ( 2*min_mn + ((n + 31)/32)*32 )*nb );
            
            lapackf77_dlarnv( &ione, ISEED, &n2, hA );
            lapackf77_dlacpy( MagmaUpperLowerStr, &m, &n, hA, &lda, hR, &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            // first, get QR factors
            magma_dsetmatrix( m, n, hA, lda, dA, ldda );
            magma_dgeqrf_gpu( m, n, dA, ldda, tau, dT, &info );
            if (info != 0)
                printf("magma_dgeqrf_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            magma_dgetmatrix( m, n, dA, ldda, hR, lda );
            
            gpu_time = magma_wtime();
            if (opts.version == 1)
                magma_dorgqr( m, n, k, hR, lda, tau, dT, nb, &info );
            else
                magma_dorgqr2(m, n, k, hR, lda, tau, &info );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_dorgqr_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                error = lapackf77_dlange("f", &m, &n, hA, &lda, work );
                
                lapackf77_dgeqrf( &m, &n, hA, &lda, tau, h_work, &lwork, &info );
                if (info != 0)
                    printf("lapackf77_dgeqrf returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                cpu_time = magma_wtime();
                lapackf77_dorgqr( &m, &n, &k, hA, &lda, tau, h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapackf77_dorgqr returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                // compute relative error |R|/|A| := |Q_magma - Q_lapack|/|A|
                blasf77_daxpy( &n2, &c_neg_one, hA, &ione, hR, &ione );
                error = lapackf77_dlange("f", &m, &n, hR, &lda, work) / error;
                
                printf("%5d %5d %5d   %7.1f (%7.2f)   %7.1f (%7.2f)   %8.2e\n",
                       (int) m, (int) n, (int) k,
                       cpu_perf, cpu_time, gpu_perf, gpu_time, error );
            }
            else {
                printf("%5d %5d %5d     ---   (  ---  )   %7.1f (%7.2f)     ---  \n",
                       (int) m, (int) n, (int) k,
                       gpu_perf, gpu_time );
            }
            
            TESTING_FREE(     hA     );
            TESTING_HOSTFREE( h_work );
            TESTING_HOSTFREE( hR     );
            TESTING_FREE(     tau    );
            TESTING_DEVFREE(  dA     );
            TESTING_DEVFREE(  dT     );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    TESTING_FINALIZE();
    return 0;
}
