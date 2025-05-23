#ifndef	_PARSE_SLIST_H_
#define	_PARSE_SLIST_H_

#include "index_set.h"
#include "json_get.h"

#ifdef	__cplusplus
extern "C" {
#endif

int
JG_parse_glist(const char *, INDEX_SET_T **, VALUE_T **, int *);

void
JG_value_delete(VALUE_T *);

void
JG_value_dump(FILE *, const VALUE_T *, int);

#ifdef	__cplusplus
}
#endif

#endif
