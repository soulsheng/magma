/*
    -- MAGMA (version 1.4.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       August 2013
 
       @author Mathieu Faverge
 
       Macro to transpose matrices before and after computation
       in LU kernels
*/

#ifndef MAGMA_TRANSPOSE_H
#define MAGMA_TRANSPOSE_H

#define magmablas_sgetmo_in( dA, dAT, ldda, m, n )              \
  dAT = dA;                                                     \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_stranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    cublasStatus_t status = cublasAlloc( (m)*(n), sizeof(float), (void**)&dAT); \
    if (status != CUBLAS_STATUS_SUCCESS)                                \
      return -7;                                                        \
    magmablas_stranspose2( dAT, ldda, dA, ldda, m, n );                 \
  }

#define magmablas_sgetmo_out( dA, dAT, ldda, m, n )             \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_stranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    magmablas_stranspose2( dA, ldda, dAT, ldda, n, m );         \
    cublasFree(dAT);                                            \
  }

#define magmablas_dgetmo_in( dA, dAT, ldda, m, n )              \
  dAT = dA;                                                     \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_dtranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    cublasStatus_t status = cublasAlloc( (m)*(n), sizeof(double), (void**)&dAT); \
    if (status != CUBLAS_STATUS_SUCCESS)                                \
      return -7;                                                        \
    magmablas_dtranspose2( dAT, ldda, dA, ldda, m, n );                 \
  }

#define magmablas_dgetmo_out( dA, dAT, ldda, m, n )             \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_dtranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    magmablas_dtranspose2( dA, ldda, dAT, ldda, n, m );         \
    cublasFree(dAT);                                            \
  }

#define magmablas_cgetmo_in( dA, dAT, ldda, m, n )              \
  dAT = dA;                                                     \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_ctranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    cublasStatus_t status = cublasAlloc( (m)*(n), sizeof(magmaFloatComplex), (void**)&dAT); \
    if (status != CUBLAS_STATUS_SUCCESS)                                \
      return -7;                                                        \
    magmablas_ctranspose2( dAT, ldda, dA, ldda, m, n );                 \
  }

#define magmablas_cgetmo_out( dA, dAT, ldda, m, n )             \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_ctranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    magmablas_ctranspose2( dA, ldda, dAT, ldda, n, m );         \
    cublasFree(dAT);                                            \
  }

#define magmablas_zgetmo_in( dA, dAT, ldda, m, n )              \
  dAT = dA;                                                     \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_ztranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    cublasStatus_t status = cublasAlloc( (m)*(n), sizeof(magmaDoubleComplex), (void**)&dAT); \
    if (status != CUBLAS_STATUS_SUCCESS)                                \
      return -7;                                                        \
    magmablas_ztranspose2( dAT, ldda, dA, ldda, m, n );                 \
  }

#define magmablas_zgetmo_out( dA, dAT, ldda, m, n )             \
  if ( ( (m) == (n) ) && ( (m)%32 == 0) && ( (ldda)%32 == 0) ){ \
    magmablas_ztranspose_inplace( ldda, dAT, ldda );            \
  } else {                                                      \
    magmablas_ztranspose2( dA, ldda, dAT, ldda, n, m );         \
    cublasFree(dAT);                                            \
  }

#endif /* MAGMA_TRANSPOSE_H */
