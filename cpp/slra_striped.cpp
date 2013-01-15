#include "slra.h"

extern "C" {
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_errno.h>
}

/* Structure striped classes */
StripedStructure::StripedStructure( size_t blocksN, Structure **stripe  ) :
    myBlocksN(blocksN), myStripe(stripe) {
  size_t k;  
  
  for (k = 0, myN = 0, myNp = 0, myMaxNkInd = 0; k < myBlocksN; 
       myN += myStripe[k]->getN(), myNp += myStripe[k]->getNp(), k++) {

    if (myStripe[k]->getN() > myStripe[myMaxNkInd]->getN()) {
      myMaxNkInd = k;
    }
  }
}

StripedStructure::~StripedStructure()  {
  if (myStripe != NULL) {
    for (size_t k = 0; k < myBlocksN; k++) {
      if (myStripe[k] != NULL) {
        delete myStripe[k];
      }
    }
    delete[] myStripe;
  }
}

void StripedStructure::fillMatrixFromP( gsl_matrix* c, const gsl_vector* p ) {
  size_t n_row = 0, sum_np = 0;
  gsl_matrix_view sub_c;
  
  for (size_t k = 0; k < getBlocksN(); 
       sum_np += myStripe[k]->getNp(), n_row += getBlock(k)->getN(), k++) {
    sub_c = gsl_matrix_submatrix(c, n_row, 0, getBlock(k)->getN(), c->size2);    
    gsl_vector_const_view sub_p = gsl_vector_const_subvector(p, sum_np, 
        myStripe[k]->getNp());
    myStripe[k]->fillMatrixFromP(&sub_c.matrix, &sub_p.vector);
  }
}

void StripedStructure::correctP( gsl_vector* p, gsl_matrix *R, 
                                 gsl_vector *yr, long wdeg ) {
  size_t n_row = 0, sum_np = 0, D = R->size2;
  gsl_vector_view sub_p, sub_yr;
  
  for (size_t k = 0; k < getBlocksN(); 
       sum_np += myStripe[k]->getNp(), n_row += getBlock(k)->getN()*D, k++) {
    sub_yr = gsl_vector_subvector(yr, n_row, getBlock(k)->getN() * D);    
    sub_p = gsl_vector_subvector(p, sum_np, myStripe[k]->getNp());
    myStripe[k]->correctP(&sub_p.vector, R, &sub_yr.vector, wdeg);
  }
}

Cholesky *StripedStructure::createCholesky( size_t D ) const {
  return new StripedCholesky(this, D);
}

DGamma *StripedStructure::createDGamma( size_t D ) const {
  return new StripedDGamma(this, D);
}

/* Cholesky striped classes */
typedef Cholesky* pGammaCholesky;

StripedCholesky::StripedCholesky( const StripedStructure *s, size_t D ) : myS(s) {
  myD = D;
  myGamma = new pGammaCholesky[myS->getBlocksN()];
  for (size_t k = 0; k < myS->getBlocksN(); k++) {
    myGamma[k] = myS->getBlock(k)->createCholesky(D);
  }
}    

StripedCholesky::~StripedCholesky() {
  if (myGamma != NULL) {
    for (size_t k = 0; k < myS->getBlocksN(); k++) {
      if (myGamma[k] != NULL) {
        delete myGamma[k];
      }
    }
    delete[] myGamma;
  }
}

void StripedCholesky::multInvCholeskyVector( gsl_vector * yr, long trans ) {
  size_t n_row = 0, k;
  gsl_vector_view yr_b;
  
  for (k = 0; k < myS->getBlocksN(); n_row += myS->getBlock(k)->getN(), k++) {
    yr_b = gsl_vector_subvector(yr, n_row*myD, myS->getBlock(k)->getN()*myD);    
    myGamma[k]->multInvCholeskyVector(&yr_b.vector, trans);
  }
}

void StripedCholesky::multInvGammaVector( gsl_vector * yr ) {
  size_t n_row = 0, k;
  gsl_vector_view yr_b;
  
  for (k = 0; k < myS->getBlocksN(); n_row += myS->getBlock(k)->getN(), k++) {
    yr_b = gsl_vector_subvector(yr, n_row*myD, myS->getBlock(k)->getN()*myD);    
    myGamma[k]->multInvGammaVector(&yr_b.vector);
  }
}

void StripedCholesky::calcGammaCholesky( const gsl_matrix *R, double reg_gamma ) {
  for (size_t k = 0; k < myS->getBlocksN(); k++) {
    myGamma[k]->calcGammaCholesky(R, reg_gamma);  
  }
}

/* DGamma striped classes */
typedef DGamma* pDGamma;

StripedDGamma::StripedDGamma( const StripedStructure *s, size_t D  ) : 
    myS(s) {
  myTmpGrad = gsl_matrix_alloc(myS->getM(), D);  
  myLHDGamma = new pDGamma[myS->getBlocksN()];
  for (size_t k = 0; k < myS->getBlocksN(); k++) {
    myLHDGamma[k] = myS->getBlock(k)->createDGamma(D);
  }
}    

StripedDGamma::~StripedDGamma() {
  gsl_matrix_free(myTmpGrad);
  if (myLHDGamma != NULL) {
    for (size_t k = 0; k < myS->getBlocksN(); k++) {
      if (myLHDGamma[k] != NULL) {
        delete myLHDGamma[k];
      }
    }
    delete[] myLHDGamma;
  }
}

void StripedDGamma::calcYrtDgammaYr( gsl_matrix *grad, const gsl_matrix *R, 
                                     const gsl_vector *yr ) {
  size_t n_row = 0, k;
  
  gsl_matrix_set_zero(grad);
  for (k = 0; k < myS->getBlocksN(); n_row += myS->getBlock(k)->getN(), k++) {
    gsl_vector_const_view sub_yr = gsl_vector_const_subvector(yr, n_row * R->size2, 
                                  myS->getBlock(k)->getN() * R->size2);    
    myLHDGamma[k]->calcYrtDgammaYr(myTmpGrad, R, &sub_yr.vector);
    gsl_matrix_add(grad, myTmpGrad);
  }
}

void StripedDGamma::calcDijGammaYr( gsl_vector *res, gsl_matrix *R, 
                        size_t i, size_t j, gsl_vector *yr ) {
  size_t n_row = 0, k;
  gsl_vector_view sub_yr, sub_res;
  
  for (k = 0; k < myS->getBlocksN(); n_row += myS->getBlock(k)->getN(), k++) {
    sub_yr = gsl_vector_subvector(yr, n_row * R->size2, 
                                  myS->getBlock(k)->getN() * R->size2);    
    sub_res = gsl_vector_subvector(res, n_row * R->size2, 
                                   myS->getBlock(k)->getN() * R->size2);    
    myLHDGamma[k]->calcDijGammaYr(&sub_res.vector, R, i, j, &sub_yr.vector);
  }                   
}