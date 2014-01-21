#ifndef	_INDEX_SET_H_
#define	_INDEX_SET_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef	struct	index_spec_t	{
	struct index_spec_t	*i_next;
	char	*i_vname;
	int	i_is_hashed;;
	int	i_is_active;
	int	i_begin;
	int	i_end;
	int	i_incr;
	int	i_current;
} INDEX_SPEC_T;

typedef	struct	index_set_t	{
	int	i_is_active;
	int	in_specs;
	INDEX_SPEC_T	**i_specs;
} INDEX_SET_T;

#ifdef	__cplusplus
extern "C" {
#endif

INDEX_SET_T	*
IS_new_iset(const INDEX_SPEC_T *);

void
IS_delete_iset(INDEX_SET_T *);

void
IS_dump_iset(FILE *, const INDEX_SET_T *);

INDEX_SPEC_T	*
IS_new_ispec(void);

void
IS_delete_ispec(INDEX_SPEC_T *);

void
IS_dump_ispec(FILE *, const INDEX_SPEC_T *);

#ifdef	__cplusplus
}
#endif

#endif
