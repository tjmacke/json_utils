#ifndef	_JG_RESULT_H_
#define	_JG_RESULT_H_

#include <stdio.h>

#define	S_BUF	100000

typedef	struct	buf_t	{
	char	*b_buf;
	size_t	bs_buf;
	size_t	bn_buf;
	char	*b_bp;
} BUF_T;

typedef	struct	jg_result_t	{
	FILE	*j_fp;
	int	j_first;
	BUF_T	*j_bufs;
	int	jn_bufs;
	int	jc_buf;
} JG_RESULT_T;

#ifdef	__cplusplus
extern "C" {
#endif

JG_RESULT_T	*
JG_result_new(FILE *, int);

void
JG_result_delete(JG_RESULT_T *);

int
JG_result_add(JG_RESULT_T *, const char *);

int
JG_result_array_push(JG_RESULT_T *);

void
JG_result_array_init(JG_RESULT_T *);

int
JG_result_array_pop(JG_RESULT_T *);

int
JG_result_print(const JG_RESULT_T *);

void
JG_result_dump(FILE *, const char *, const JG_RESULT_T *, int);

#ifdef	__cplusplus
}
#endif

#endif
