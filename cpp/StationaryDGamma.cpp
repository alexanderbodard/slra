#include <memory.h>
extern "C" {
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>
}
#include "slra.h"

StationaryDGamma::StationaryDGamma( const StationaryStructure *s, size_t D ) :
    myD(D), myW(s) {
  myTempVkColRow = gsl_vector_alloc(myW->getM());
  myDGammaVec = gsl_vector_alloc(myD * (2 * myW->getMu() - 1));
  myDGammaTrMat = gsl_matrix_alloc(myD, 2 * myW->getMu() - 1);
  myDGamma = gsl_matrix_alloc(myD, myD * (2 * myW->getMu() - 1));
  myTmpCol = gsl_vector_alloc(myW->getN());
  myVk_R =  gsl_matrix_alloc(myW->getM(), myD);
  myN_k = gsl_matrix_alloc(myD, myD);
  myEye = gsl_matrix_alloc(myW->getM(), myW->getM());
  gsl_matrix_set_identity(myEye);
}

StationaryDGamma::~StationaryDGamma() {
  gsl_vector_free(myTempVkColRow);
  gsl_vector_free(myTmpCol);
  gsl_vector_free(myDGammaVec);
  gsl_matrix_free(myDGammaTrMat);
  gsl_matrix_free(myDGamma);
  gsl_matrix_free(myVk_R);
  gsl_matrix_free(myN_k);
  gsl_matrix_free(myEye);
}

void StationaryDGamma::calcYrtDgammaYr( gsl_matrix *mgrad_r, 
         const gsl_matrix *R, const gsl_vector *yr ) {
  size_t n = yr->size / myD;
  gsl_matrix Yr = gsl_matrix_const_view_vector(yr, n, myD).matrix, YrB, YrT;

  gsl_matrix_set_zero(mgrad_r);
  
  for (size_t k = 0; k < myW->getMu(); k++) {
    YrT = gsl_matrix_submatrix(&Yr, 0, 0, n - k, myD).matrix;
    YrB = gsl_matrix_submatrix(&Yr, k, 0, n - k, myD).matrix;
    gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, &YrB, &YrT, 0.0, myN_k);

    myW->VkB(myVk_R, k, R);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 2.0, myVk_R, myN_k, 1.0, mgrad_r);

    if (k > 0) {
      myW->VkB(myVk_R, -k, R);
      gsl_blas_dgemm(CblasNoTrans, CblasTrans, 2.0, myVk_R, myN_k, 1.0, mgrad_r);
    }
  }    
}

void StationaryDGamma::calcDijGammaYr( gsl_vector *res, const gsl_matrix *R, 
         size_t i, size_t j,  const gsl_vector *yr ) {
  gsl_vector gv_sub, perm_col = gsl_matrix_column(myEye, i).vector, dgammajrow,
             res_stride, yr_stride;
  long S = myW->getMu();           

  for (long k = 1 - S; k < S; k++) {
    gv_sub = gsl_vector_subvector(myDGammaVec, (k + S - 1) * myD, 
              myD).vector;
    myW->AtVkV(&gv_sub, -k, R, &perm_col, myTempVkColRow);
    gsl_matrix_set_col(myDGammaTrMat, -k + myW->getMu() - 1, &gv_sub);
  }

  size_t n = yr->size / myD;
  if (myD == 1) {
    dgammajrow = gsl_matrix_row(myDGammaTrMat, 0).vector;
    gsl_vector_add (myDGammaVec, &dgammajrow);
    tmv_prod_vector(myDGammaVec, myW->getMu(), yr, n, res);  
  } else {
    res_stride = gsl_vector_subvector_with_stride(res, j, myD, n).vector;       
    yr_stride = gsl_vector_const_subvector_with_stride(yr, j, myD, n).vector;       
    gsl_matrix_view gamma_vec_mat = gsl_matrix_view_vector(myDGammaVec, 1, 
                                                           myDGammaVec->size);
    gsl_vector_memcpy(myTmpCol, &yr_stride);
  
    gsl_vector_set_zero(res);
    tmv_prod_vector(myDGammaVec, myW->getMu(), yr, n, &res_stride);  
    tmv_prod_new(myDGammaTrMat, myW->getMu(), myTmpCol, n, res, 1.0);  
  }
}


