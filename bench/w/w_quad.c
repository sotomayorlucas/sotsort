#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <float.h>
#include "../ext/quadsort/src/quadsort.h"
void w_quad_u32(void*a,size_t n){ quadsort_prim(a,n,5); }
void w_quad_u64(void*a,size_t n){ quadsort_prim(a,n,9); }
void w_quad_c4(void*a,size_t n,CMPFUNC*c){ quadsort(a,n,4,c); }
void w_quad_c8(void*a,size_t n,CMPFUNC*c){ quadsort(a,n,8,c); }
