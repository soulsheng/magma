/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated s Wed Aug 14 12:16:05 2013

*/
#include "common_magma.h"

extern "C" magma_int_t
magma_sgesv(     magma_int_t n, magma_int_t nrhs,
                 float *A, magma_int_t lda,
                 magma_int_t *ipiv,
                 float *B, magma_int_t ldb,
                 magma_int_t *info)
{
/*  -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

    Purpose
    =======
    Solves a system of linear equations
       A * X = B
    where A is a general N-by-N matrix and X and B are N-by-NRHS matrices.
    The LU decomposition with partial pivoting and row interchanges is
    used to factor A as
       A = P * L * U,
    where P is a permutation matrix, L is unit lower triangular, and U is
    upper triangular.  The factored form of A is then used to solve the
    system of equations A * X = B.

    Arguments
    =========
    N       (input) INTEGER
            The order of the matrix A.  N >= 0.

    NRHS    (input) INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    A       (input/output) REAL array, dimension (LDA,N).
            On entry, the M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    LDA     (input) INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    IPIV    (output) INTEGER array, dimension (min(M,N))
            The pivot indices; for 1 <= i <= min(M,N), row i of the
            matrix was interchanged with row IPIV(i).

    B       (input/output) REAL array, dimension (LDB,NRHS)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.

    LDB     (input) INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
    =====================================================================    */

    magma_int_t num_gpus, ldda, lddb;
    
    *info = 0;
    if (n < 0) {
        *info = -1;
    } else if (nrhs < 0) {
        *info = -2;
    } else if (lda < max(1,n)) {
        *info = -4;
    } else if (ldb < max(1,n)) {
        *info = -7;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (n == 0 || nrhs == 0) {
        return *info;
    }
    
    /* If single-GPU and allocation suceeds, use GPU interface. */
    num_gpus = magma_num_gpus();
    float *dA, *dB;
    if ( num_gpus > 1 ) {
        goto CPU_INTERFACE;
    }
    ldda = ((n+31)/32)*32;
    lddb = ldda;
    if ( MAGMA_SUCCESS != magma_smalloc( &dA, ldda*n )) {
        goto CPU_INTERFACE;
    }
    if ( MAGMA_SUCCESS != magma_smalloc( &dB, lddb*nrhs )) {
        magma_free( dA );
        goto CPU_INTERFACE;
    }
    magma_ssetmatrix( n, n, A, lda, dA, ldda );
    magma_sgetrf_gpu( n, n, dA, ldda, ipiv, info );
    if ( *info == MAGMA_ERR_DEVICE_ALLOC ) {
        magma_free( dA );
        magma_free( dB );
        goto CPU_INTERFACE;
    }
    magma_sgetmatrix( n, n, dA, ldda, A, lda );
    if ( *info == 0 ) {
        magma_ssetmatrix( n, nrhs, B, ldb, dB, lddb );
        magma_sgetrs_gpu( MagmaNoTrans, n, nrhs, dA, ldda, ipiv, dB, lddb, info );
        magma_sgetmatrix( n, nrhs, dB, lddb, B, ldb );
    }
    magma_free( dA );
    magma_free( dB );
    return *info;

CPU_INTERFACE:
    /* If multi-GPU or allocation failed, use CPU interface and LAPACK.
     * Faster to use LAPACK for getrs than to copy A to GPU. */
    magma_sgetrf( n, n, A, lda, ipiv, info );
    if ( *info == 0 ) {
        lapackf77_sgetrs( MagmaNoTransStr, &n, &nrhs, A, &lda, ipiv, B, &ldb, info );
    }
    return *info;
}
