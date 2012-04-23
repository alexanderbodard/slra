extern "C" {
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>
}
#include "slra.h"

SDependentCholesky::SDependentCholesky( const SDependentStructure *s,
     int D, double reg_gamma ) : myW(s), myD(D), my_reg_gamma(reg_gamma) {
  /* Preallocate arrays */
  myPackedCholesky = (double *)malloc(myW->getM() * myD * myD * myW->getS() * sizeof(double));
  myTempR = gsl_matrix_alloc(myW->getNplusD(), myD);
  myTempWktR = gsl_matrix_alloc(myW->getNplusD(), myD);
  myTempGammaij = gsl_matrix_alloc(myD, myD);

  /* Calculate variables for FORTRAN routines */     
  s_minus_1 =  myW->getS() - 1;
  d_times_s =  myD * myW->getS();
  d_times_Mg = myW->getM() * myD;
  d_times_s_minus_1 = myD * myW->getS() - 1;
}
  
SDependentCholesky::~SDependentCholesky() {
  free(myPackedCholesky);
  gsl_matrix_free(myTempR);
  gsl_matrix_free(myTempWktR);
  gsl_matrix_free(myTempGammaij);
}

void SDependentCholesky::multiplyInvPartCholeskyArray( double * yr,
         int trans, size_t size, size_t chol_size ) {
  size_t info, total_cols = size / chol_size;
  dtbtrs_("U", (trans ? "T" : "N"), "N", 
          &chol_size, &d_times_s_minus_1, &total_cols, 
	  myPackedCholesky, &d_times_s, yr, &chol_size, &info);
}
  
void SDependentCholesky::multiplyInvPartGammaArray( double * yr, size_t size,
         size_t chol_size ) {
  size_t info, total_cols = size / chol_size; 
  dpbtrs_("U", &chol_size, &d_times_s_minus_1, &total_cols, 
          myPackedCholesky, &d_times_s, yr, &chol_size, &info);  
}

void SDependentCholesky::multiplyInvCholeskyVector( gsl_vector * yr, int trans ) {
  if (yr->stride != 1) {
    throw new Exception("Cannot multiply vectors with stride != 1\n");
  }
  multiplyInvPartCholeskyArray(yr->data, trans, yr->size, d_times_Mg);
}

void SDependentCholesky::multiplyInvGammaVector( gsl_vector * yr ) {
  if (yr->stride != 1) {
    throw new Exception("Cannot multiply vectors with stride != 1\n");
  }
  multiplyInvPartGammaArray(yr->data, yr->size, d_times_Mg);
}

void SDependentCholesky::multiplyInvCholeskyTransMatrix( gsl_matrix * yr_matr,
         int trans ) {
  if (yr_matr->size2 != yr_matr->tda) {
    Cholesky::multiplyInvCholeskyTransMatrix(yr_matr, trans);
  } else {
    multiplyInvPartCholeskyArray(yr_matr->data, trans, 
        yr_matr->size1 * yr_matr->size2, d_times_Mg);
  }
}

void SDependentCholesky::calcGammaCholesky( gsl_matrix *R ) {
  size_t info = 0;
  PRINTF("HelloC1!\n");
  computeGammaUpperPart(R);
  PRINTF("HelloC2!\n");

  dpbtrf_("U", &d_times_Mg, &d_times_s_minus_1, myPackedCholesky, &d_times_s, &info);
  PRINTF("HelloC3!\n");
  if (info) { 
    PRINTF("Error: info = %d", info); /* TODO: add regularization */
  }
}

void SDependentCholesky::computeGammaUpperPart( gsl_matrix *R ) {
  gsl_matrix gamma_ij;
  double *diagPtr =  myPackedCholesky;
  
  DEBUGINT(getM());
  DEBUGINT(getD());
  DEBUGINT(getS());
  DEBUGINT(R->size1);
  DEBUGINT(R->size2);
  
  for (size_t i = 0; i < getM(); ++i, diagPtr += getS() * getD() * getD()) {
    DEBUGINT(i);
    if (getS() > 1) {
      gsl_matrix blk_row = gsl_matrix_view_array_with_tda(diagPtr + d_times_s_minus_1, 
          (getS() + 1) * getD(), getD(), d_times_s_minus_1).matrix;
      for (size_t j = 0; (j <= getS()) && (j < getM() - i); j++) {
        DEBUGINT(j);
        gamma_ij = gsl_matrix_submatrix(&blk_row, j * getD(), 0, getD(), getD()).matrix;
        if (j < getS()) {
          PRINTF("Hello1\n");
          myW->AtWijB(myTempGammaij, i, i+j, R, R, myTempWktR);  
          PRINTF("Hello12\n");
        } else {
          gsl_matrix_set_zero(myTempGammaij);
        }
        if (j == 0) {
          copyLowerTrg(&gamma_ij, myTempGammaij);
        } else {
          gsl_matrix_memcpy(&gamma_ij, myTempGammaij);
        }
      }
    } else {
      myW->AtWijB(myTempGammaij, i, i, R, R, myTempWktR);  
      gamma_ij = gsl_matrix_view_array(diagPtr, getD(), getD()).matrix;
      shiftLowerTrg(&gamma_ij, myTempGammaij);
    }
    DEBUGINT(i);
  }
}
