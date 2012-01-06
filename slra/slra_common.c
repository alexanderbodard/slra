#include "slra.h"

int slraMatrix2Struct( data_struct* ps, double *s_matr, 
                       int q, int s_matr_cols ) {
  ps->q  = q;
  int l;

  int n_plus_d = 0;
  for (l = 0; l < ps->q; l++) {
    ps->a[l].blocks_in_row = *(s_matr + l);
    ps->a[l].nb = (s_matr_cols > 1) ? *(s_matr + ps->q + l): 1;
    ps->a[l].exact = (s_matr_cols > 2) ? *(s_matr + 2 * ps->q + l): 0;
    ps->a[l].toeplitz = (s_matr_cols > 3) ? *(s_matr + 3 * ps->q + l): 0;
    n_plus_d += ps->a[l].blocks_in_row * ps->a[l].nb;
  }

  return n_plus_d;
}

char meth_codes[] = "lqn";
char submeth_codes_lm[] = "ls";
char submeth_codes_qn[] = "b2pf";
char submeth_codes_nm[] = "n2r";
char *submeth_codes[] = {submeth_codes_lm, submeth_codes_qn, submeth_codes_nm};

void slraString2Method( const char *str_buf, opt_and_info *popt )  {
  int submeth_codes_max[] = { 
    sizeof(submeth_codes_lm) / sizeof(submeth_codes_lm[0]) - 1, 
    sizeof(submeth_codes_qn) / sizeof(submeth_codes_qn[0]) - 1, 
    sizeof(submeth_codes_nm) / sizeof(submeth_codes_nm[0]) - 1 
  };

  int i;

  for (i = sizeof(meth_codes)/sizeof(meth_codes[0]) - 1; i >= 0; i--)  {
    if (str_buf[0] == meth_codes[i]) {
      (*popt).method = i;
      break;
    }
  } 
	  
  if (i < 0)  {
    WARNING("Ignoring optimization option 'method'. Unrecognized value.");
    slraAssignDefOptValue((*popt), method);
    slraAssignDefOptValue((*popt), submethod);
  }

  for (i = submeth_codes_max[(*popt).method] - 1; i >= 0; i--)  {
    if (str_buf[1] == submeth_codes[(*popt).method][i]) {
      (*popt).submethod = i;
      break;
    }
  } 
  if (i < 0 && str_buf[1] != 0)  {
    WARNING("Unrecognized submethod - using default.");
    slraAssignDefOptValue((*popt), submethod);
  }
}

int slraString2Disp( const char *str_value )  {
  char *str_disp[] = {"notify", "final", "iter", "off" };
  int i;

  for (i = 0; i < sizeof(str_disp) / sizeof(str_disp[0]); i++) {
    if (strcmp(str_disp[i], str_value) == 0) {
      return i;
    }
  }
    
  if (i < 0) {
    WARNING("Ignoring optimization option 'disp'. Unrecognized value.");
  }
   
  return SLRA_DEF_disp;
}

/* ************************************************ */
/* m_to_gsl_matrix: convert the Matlab style column */
/* major mxn matrix array to a GSL matrix           */
/* ************************************************ */
void m_to_gsl_matrix(gsl_matrix* a_gsl, double* a_m) 
{
  int i, j;

  for (i = 0; i < a_gsl->size1; i++) {
    for (j = 0; j < a_gsl->size2; j++) {
      gsl_matrix_set(a_gsl, i, j, a_m[i + j * a_gsl->size1]);
    }
  }
}

/* ************************************************ */
/* gsl_to_m_matrix: convert the GSL mxn matrix to a */
/* Matlab style column major matrix array           */
/* ************************************************ */
void gsl_to_m_matrix(double* a_m, gsl_matrix* a_gsl) 
{
  int i, j;

  for (i = 0; i < a_gsl->size1; i++) {
    for (j = 0; j < a_gsl->size2; j++) {
      a_m[i + j * a_gsl->size1] = gsl_matrix_get(a_gsl, i, j);
    }
  }
}

/* gsl_matrix_vectorize: vectorize column-wise a gsl_matrix */
void gsl_matrix_vectorize(double* v, gsl_matrix* m)
{
  int i, j;

  for (j = 0; j < m->size2; j++)
    for (i = 0; i < m->size1; i++)
      v[i+j*m->size1] = gsl_matrix_get(m,i,j);
}


/* gsl_matrix_vec_inv: gsl_matrix from an array */
void gsl_matrix_vec_inv(gsl_matrix* m, double* v)
{
  int i, j;

  for (i = 0; i < m->size1; i++)
    for (j = 0; j < m->size2; j++)
      gsl_matrix_set(m,i,j,v[i+j*m->size1]);
}


/* print matrix */
void print_mat(const gsl_matrix* m)
{
  int i, j;

  PRINTF("\n");
  for (i = 0; i < m->size1; i++) {
    for (j = 0; j < m->size2; j++)
      PRINTF("%16.14f ", gsl_matrix_get(m, i, j));
    PRINTF("\n");
  }
  PRINTF("\n");
}


/* print matrix */
void print_mat_tr(const gsl_matrix* m)
{
  int i, j;

  PRINTF("\n");
  for (j = 0; j < m->size2; j++) {
    for (i = 0; i < m->size1; i++) {
      PRINTF("%16.14f ", gsl_matrix_get(m, i, j));
    }
    PRINTF("\n");
  }
  PRINTF("\n");
}


/* print_arr: print array */
void print_arr(double* a, int n)
{
  int i;

  PRINTF("\n");
  for (i = 0; i < n; i++)
    PRINTF("%f ",*(a+i));
  PRINTF("\n");
}