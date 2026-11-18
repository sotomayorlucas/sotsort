#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <float.h>
#define cmp(a,b) ((*(const unsigned long long*)(a)>>32) > (*(const unsigned long long*)(b)>>32))
#include "../ext/fluxsort/src/fluxsort.h"
void w_flux_p8(void*a,size_t n){ fluxsort(a,n,8,NULL); }
