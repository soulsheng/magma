/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @author Mark Gates
       @author Azzam Haidar
       @precisions normal z -> s d c
*/
#include "common_magma.h"

extern "C" magma_int_t
magma_zlarfb_gpu_gemm( char side, char trans, char direct, char storev,
                  magma_int_t m, magma_int_t n, magma_int_t k,
                  const magmaDoubleComplex *dV,    magma_int_t ldv,
                  const magmaDoubleComplex *dT,    magma_int_t ldt,
                  magmaDoubleComplex *dC,          magma_int_t ldc,
                  magmaDoubleComplex *dwork,       magma_int_t ldwork,
                  magmaDoubleComplex *dworkvt,     magma_int_t ldworkvt)
{
/*  -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Univ. of California Berkeley
       August 2013

    Purpose
    =======
    ZLARFB applies a complex block reflector H or its transpose H' to a
    COMPLEX_16 m by n matrix C, from the left.
    NOTE THAT THIS FUNCTION ASSUME THAT THE UPPER PART OF dV IS 0 BECAUSE IT IS REFERENCED.
    SAME FOR UPPEr/LOWER PART OF dT

    Arguments
    =========
    SIDE    (input) CHARACTER
            = 'L': apply H or H' from the Left
            = 'R': apply H or H' from the Right

    TRANS   (input) CHARACTER
            = 'N': apply H   (No transpose)
            = 'C': apply H' (Conjugate transpose)

    DIRECT  (input) CHARACTER
            Indicates how H is formed from a product of elementary
            reflectors
            = 'F': H = H(1) H(2) . . . H(k) (Forward)
            = 'B': H = H(k) . . . H(2) H(1) (Backward)

    STOREV  (input) CHARACTER
            Indicates how the vectors which define the elementary
            reflectors are stored:
            = 'C': Columnwise
            = 'R': Rowwise

    M       (input) INTEGER
            The number of rows of the matrix C.

    N       (input) INTEGER
            The number of columns of the matrix C.

    K       (input) INTEGER
            The order of the matrix T (= the number of elementary
            reflectors whose product defines the block reflector).

    DV      (input) COMPLEX_16 array on the GPU, dimension
                (LDV,K) if STOREV = 'C'
                (LDV,M) if STOREV = 'R' and SIDE = 'L'
                (LDV,N) if STOREV = 'R' and SIDE = 'R'
            The matrix V. See further details.

    LDV     (input) INTEGER
            The leading dimension of the array V.
            If STOREV = 'C' and SIDE = 'L', LDV >= max(1,M);
            if STOREV = 'C' and SIDE = 'R', LDV >= max(1,N);
            if STOREV = 'R', LDV >= K.

    DT      (input) COMPLEX_16 array on the GPU, dimension (LDT,K)
            The triangular k by k matrix T in the representation of the
            block reflector.

    LDT     (input) INTEGER
            The leading dimension of the array T. LDT >= K.

    DC      (input/output) COMPLEX_16 array on the GPU, dimension (LDC,N)
            On entry, the m by n matrix C.
            On exit, C is overwritten by H*C, or H'*C, or C*H, or C*H'.

    LDC     (input) INTEGER
            The leading dimension of the array C. LDA >= max(1,M).

    WORK    (workspace) COMPLEX_16 array, dimension (LDWORK,K)

    LDWORK  (input) INTEGER
            The leading dimension of the array WORK.
            If SIDE == 'L', LDWORK >= max(1,N);
            if SIDE == 'R', LDWORK >= max(1,M);

    WORKVT  (workspace) COMPLEX_16 array, dimension (LDWORKT,K)

    LDWORKVT(input) INTEGER
            The leading dimension of the array WORKVT.
            LDWORKVT >= max(1,min(M,N));

    Further Details
    ===============
    The shape of the matrix V and the storage of the vectors which define
    the H(i) is best illustrated by the following example with n = 5 and
    k = 3.
    All elements including 0's and 1's are stored, unlike LAPACK.

    DIRECT = 'F' and STOREV = 'C':         DIRECT = 'F' and STOREV = 'R':

                 V = (  1  0  0 )                 V = (  1 v1 v1 v1 v1 )
                     ( v1  1  0 )                     (  0  1 v2 v2 v2 )
                     ( v1 v2  1 )                     (  0  0  1 v3 v3 )
                     ( v1 v2 v3 )
                     ( v1 v2 v3 )

    DIRECT = 'B' and STOREV = 'C':         DIRECT = 'B' and STOREV = 'R':

                 V = ( v1 v2 v3 )                 V = ( v1 v1  1  0  0 )
                     ( v1 v2 v3 )                     ( v2 v2 v2  1  0 )
                     (  1 v2 v3 )                     ( v3 v3 v3 v3  1 )
                     (  0  1 v3 )
                     (  0  0  1 )

    ===================================================================      */

    magmaDoubleComplex c_zero    = MAGMA_Z_ZERO;
    magmaDoubleComplex c_one     = MAGMA_Z_ONE;
    magmaDoubleComplex c_neg_one = MAGMA_Z_NEG_ONE;

    magmaDoubleComplex *dwVT = dwork;           // size = m*k
    magmaDoubleComplex *dwQ  = dworkvt;         // size = m*m
    magmaDoubleComplex *dwC  = dwQ + m*m;       // size = m*n


    /* Function Body */
    if (m <= 0 || n <= 0) {
        return MAGMA_SUCCESS;
    }

    // opposite of trans
    char transt;
    if (trans == 'N' || trans == 'n')
        transt = MagmaConjTrans;
    else
        transt = MagmaNoTrans;
    
    // whether T is upper or lower triangular
    char uplo;
    if (direct == 'F' || direct == 'f')
        uplo = MagmaUpper;
    else
        uplo = MagmaLower;
    
    // whether V is stored transposed or not
    char notransV, transV;
    if (storev == 'C' || storev == 'c') {
        notransV = MagmaNoTrans;
        transV   = MagmaConjTrans;
    }
    else {
        notransV = MagmaConjTrans;
        transV   = MagmaNoTrans;
    }
        printf("check that workspace dworkvt is of good size \n");

    if ( side  == 'l' || side  == 'L' ) {
        // Form H C or H' C
        // Comments assume H C.
        // When forming H' C, T gets transposed via transt for m>=n or by trans for m<n.
        
        // dwVT = V T
        magma_zgemm( notransV, trans,
                     m, k, k,
                     c_one,  dV, ldv,
                             dT, ldt,
                     c_zero, dwVT, m);
        // dwQ = dwVT * V' = V T V'
        magma_zgemm( MagmaNoTrans, transV,
                     m, m, k,
                     c_one,  dwVT,   m,
                             dV,    ldv,
                     c_zero, dwQ,   m);
        // copy C to Wc then do a gemm C = (I-VTV')*C = C - dwQ * dwC
         magma_zcopymatrix( m, n, dC, ldc, dwC, m );

        // C = C - dwQ*dwC = C - V T V'C
        magma_zgemm( MagmaNoTrans, MagmaNoTrans,
                     m, n, m,
                     c_neg_one, dwQ,   m,
                                dwC,   m,
                     c_one,     dC, ldc);
    }
    else {
        // Form C H or C H'
        // Comments assume C H.
        // When forming C H', T gets transposed via trans.
        printf("not implemented\n");
    }

    return MAGMA_SUCCESS;
} /* magma_zlarfb */
