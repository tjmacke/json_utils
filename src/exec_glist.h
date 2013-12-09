#ifndef	_EXEC_SLIST_H_
#define	_EXEC_SLIST_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <jansson.h>

#include "json_get.h"

#ifdef	__cplusplus
extern "C" {
#endif

int
JG_exec_glist(FILE *, pthread_mutex_t *, json_t *, const VALUE_T *, int);

#ifdef	__cplusplus
}
#endif

#endif
