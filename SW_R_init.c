#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>

/* .C calls */
static const R_CallMethodDef CEntries[] = {
  {NULL, NULL, 0, NULL}
};

/* .Call calls */
extern SEXP start(SEXP, SEXP, SEXP);
extern SEXP tempError();
extern SEXP onGetInputDataFromFiles(SEXP);
extern SEXP onGetOutput(SEXP);

static const R_CallMethodDef CallEntries[] = {
  {"start",                   (DL_FUNC) &start,                   3},
  {"tempError",               (DL_FUNC) &tempError,               0},
  {"onGetInputDataFromFiles", (DL_FUNC) &onGetInputDataFromFiles, 1},
  {"onGetOutput",             (DL_FUNC) &onGetOutput,             1},
  {NULL, NULL, 0}
};

/* Register package calls with R */
void R_init_rSOILWAT2(DllInfo *dll) {
  R_registerRoutines(dll, CEntries, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_forceSymbols(dll, TRUE);
}
