/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013

       @generated c Tue Aug 13 16:43:29 2013
       @author Mark Gates
*/

#ifndef MAGMA_CGEHRD_H
#define MAGMA_CGEHRD_H

#include "magma.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cgehrd_data
{
    int ngpu;
    
    magma_int_t ldda;
    magma_int_t ldv;
    magma_int_t ldvd;
    
    magmaFloatComplex *A    [ MagmaMaxGPUs ];  // ldda*nlocal
    magmaFloatComplex *V    [ MagmaMaxGPUs ];  // ldv *nb, whole panel
    magmaFloatComplex *Vd   [ MagmaMaxGPUs ];  // ldvd*nb, block-cyclic
    magmaFloatComplex *Y    [ MagmaMaxGPUs ];  // ldda*nb
    magmaFloatComplex *W    [ MagmaMaxGPUs ];  // ldda*nb
    magmaFloatComplex *Ti   [ MagmaMaxGPUs ];  // nb*nb
    
    magma_queue_t streams[ MagmaMaxGPUs ];
};

#ifdef __cplusplus
}
#endif

#endif        //  #ifndef MAGMA_CGEHRD_H
