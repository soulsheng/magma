/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @author Mark Gates
       @generated s Wed Aug 14 12:18:06 2013
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <assert.h>

#include <algorithm>  // std::swap

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing slarfb_gpu
*/
int main( int argc, char** argv )
{
    TESTING_INIT();
    
    float c_zero    = MAGMA_S_ZERO;
    float c_one     = MAGMA_S_ONE;
    float c_neg_one = MAGMA_S_NEG_ONE;
    magma_int_t M, N, K, size, ldc, ldv, ldt, ldw, nv;
    magma_int_t ione =  1;
    magma_int_t ISEED[4] = {0,0,0,1};
    float error, work[1];
    
    // test all combinations of input parameters
    const char side[]   = { MagmaLeft,       MagmaRight    };
    const char trans[]  = { MagmaTrans,  MagmaNoTrans  };
    const char direct[] = { MagmaForward,    MagmaBackward };
    const char storev[] = { MagmaColumnwise, MagmaRowwise  };

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf("    M     N     K   storev   side   direct   trans    ||R||_F / ||HC||_F\n");
    printf("========================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        M = opts.msize[i];
        N = opts.nsize[i];
        K = opts.ksize[i];
        if ( M < K || N < K || K <= 0 ) {
            printf( "skipping M %d, N %d, K %d; requires M >= K, N >= K, K >= 0.\n", (int) M, (int) N, (int) K );
            continue;
        }
        for( int istor = 0; istor < 2; ++istor ) {
        for( int iside = 0; iside < 2; ++iside ) {
        for( int idir  = 0; idir  < 2; ++idir  ) {
        for( int itran = 0; itran < 2; ++itran ) {
            
            ldc = ((M+31)/32)*32;
            ldt = ((K+31)/32)*32;
            ldw = (side[iside] == MagmaLeft ? N : M);
            // (ldv, nv) get swapped later if rowwise
            ldv = (side[iside] == MagmaLeft ? M : N);
            nv  = K;
            
            // Allocate memory for matrices
            float *C, *R, *V, *T, *W;
            TESTING_MALLOC( C, float, ldc*N );
            TESTING_MALLOC( R, float, ldc*N );
            TESTING_MALLOC( V, float, ldv*K );
            TESTING_MALLOC( T, float, ldt*K );
            TESTING_MALLOC( W, float, ldw*K );
            
            float *dC, *dV, *dT, *dW;
            TESTING_DEVALLOC( dC, float, ldc*N );
            TESTING_DEVALLOC( dV, float, ldv*K );
            TESTING_DEVALLOC( dT, float, ldt*K );
            TESTING_DEVALLOC( dW, float, ldw*K );
            
            // C is M x N.
            size = ldc*N;
            lapackf77_slarnv( &ione, ISEED, &size, C );
            //printf( "C=" );  magma_sprint( M, N, C, ldc );
            
            // V is ldv x nv. See larfb docs for description.
            // if column-wise and left,  M x K
            // if column-wise and right, N x K
            // if row-wise and left,     K x M
            // if row-wise and right,    K x N
            size = ldv*nv;
            lapackf77_slarnv( &ione, ISEED, &size, V );
            if ( storev[istor] == MagmaColumnwise ) {
                if ( direct[idir] == MagmaForward ) {
                    lapackf77_slaset( MagmaUpperStr, &K, &K, &c_zero, &c_one, V, &ldv );
                }
                else {
                    lapackf77_slaset( MagmaLowerStr, &K, &K, &c_zero, &c_one, &V[(ldv-K)], &ldv );
                }
            }
            else {
                // rowwise, swap V's dimensions
                std::swap( ldv, nv );
                if ( direct[idir] == MagmaForward ) {
                    lapackf77_slaset( MagmaLowerStr, &K, &K, &c_zero, &c_one, V, &ldv );
                }
                else {
                    lapackf77_slaset( MagmaUpperStr, &K, &K, &c_zero, &c_one, &V[(nv-K)*ldv], &ldv );
                }
            }
            //printf( "# ldv %d, nv %d\n", ldv, nv );
            //printf( "V=" );  magma_sprint( ldv, nv, V, ldv );
            
            // T is K x K, upper triangular for forward, and lower triangular for backward
            magma_int_t k1 = K-1;
            size = ldt*K;
            lapackf77_slarnv( &ione, ISEED, &size, T );
            if ( direct[idir] == MagmaForward ) {
                lapackf77_slaset( MagmaLowerStr, &k1, &k1, &c_zero, &c_zero, &T[1], &ldt );
            }
            else {
                lapackf77_slaset( MagmaUpperStr, &k1, &k1, &c_zero, &c_zero, &T[1*ldt], &ldt );
            }
            //printf( "T=" );  magma_sprint( K, K, T, ldt );
            
            magma_ssetmatrix( M,   N,  C, ldc, dC, ldc );
            magma_ssetmatrix( ldv, nv, V, ldv, dV, ldv );
            magma_ssetmatrix( K,   K,  T, ldt, dT, ldt );
            
            lapackf77_slarfb( &side[iside], &trans[itran], &direct[idir], &storev[istor],
                              &M, &N, &K,
                              V, &ldv, T, &ldt, C, &ldc, W, &ldw );
            //printf( "HC=" );  magma_sprint( M, N, C, ldc );
            
            magma_slarfb_gpu( side[iside], trans[itran], direct[idir], storev[istor],
                              M, N, K,
                              dV, ldv, dT, ldt, dC, ldc, dW, ldw );
            magma_sgetmatrix( M, N, dC, ldc, R, ldc );
            //printf( "dHC=" );  magma_sprint( M, N, R, ldc );
            
            // compute relative error |HC_magma - HC_lapack| / |HC_lapack|
            error = lapackf77_slange( "Fro", &M, &N, C, &ldc, work );
            size = ldc*N;
            blasf77_saxpy( &size, &c_neg_one, C, &ione, R, &ione );
            error = lapackf77_slange( "Fro", &M, &N, R, &ldc, work ) / error;
            printf( "%5d %5d %5d      %c       %c       %c       %c      %8.2e\n",
                    (int) M, (int) N, (int) K,
                    storev[istor], side[iside], direct[idir], trans[itran], error );
            
            TESTING_FREE( C );
            TESTING_FREE( R );
            TESTING_FREE( V );
            TESTING_FREE( T );
            TESTING_FREE( W );
            
            TESTING_DEVFREE( dC );
            TESTING_DEVFREE( dV );
            TESTING_DEVFREE( dT );
            TESTING_DEVFREE( dW );
        }}}}
        printf( "\n" );
    }
    
    TESTING_FINALIZE();
    return 0;
}
