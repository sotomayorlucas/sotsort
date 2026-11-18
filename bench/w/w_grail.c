#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#define inline static inline
typedef struct { uint64_t k, p[3]; } r32_t;
#if defined(CMPF)
extern int CMPF(const void*,const void*);
#define SORT_CMP(a,b) CMPF((const void*)(a),(const void*)(b))
#elif defined(KEYEXPR)
#define KX(e) KEYEXPR(e)
#define SORT_CMP(a,b) ((KX(a)>KX(b))-(KX(a)<KX(b)))
#else
#define SORT_CMP(a,b) ((*(a)>*(b))-(*(a)<*(b)))
#endif
#include "../ext/GrailSort/GrailSort.h"
#define CAT2(a,b) a##b
#define CAT(a,b) CAT2(a,b)
void CAT(w_grail_,TAG)(void*a,size_t n){ GrailSort((SORT_TYPE*)a,(int)n); }
void CAT(w_grailb_,TAG)(void*a,size_t n){ GrailSortWithBuffer((SORT_TYPE*)a,(int)n); }
