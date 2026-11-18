#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <float.h>
#include "../ext/fluxsort/src/fluxsort.h"
void w_flux_u32(void*a,size_t n){ fluxsort_prim(a,n,5); }
void w_flux_u64(void*a,size_t n){ fluxsort_prim(a,n,9); }
void w_flux_c4(void*a,size_t n,CMPFUNC*c){ fluxsort(a,n,4,c); }
void w_flux_c8(void*a,size_t n,CMPFUNC*c){ fluxsort(a,n,8,c); }
