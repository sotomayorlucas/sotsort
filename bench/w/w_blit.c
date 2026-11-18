#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <float.h>
#include "../ext/blitsort/src/blitsort.h"
void w_blit_u32(void*a,size_t n){ blitsort_prim(a,n,5); }
void w_blit_u64(void*a,size_t n){ blitsort_prim(a,n,9); }
void w_blit_c4(void*a,size_t n,CMPFUNC*c){ blitsort(a,n,4,c); }
void w_blit_c8(void*a,size_t n,CMPFUNC*c){ blitsort(a,n,8,c); }
