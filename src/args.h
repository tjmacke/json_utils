#ifndef	_ARGS_H_
#define	_ARGS_H_

#define	AS_OK		0
#define	AS_HELP_ONLY	1
#define	AS_ERROR	2

#define	AVK_NONE	0
#define	AVK_OPT		1
#define	AVK_REQ		2
#define	AVK_MSG		3

#define	AVT_BOOL	0
#define	AVT_INT		1
#define	AVT_UINT	2	// >= 0
#define AVT_PINT	3	// > 0
#define	AVT_REAL	4
#define	AVT_STR		5

// Use ARGS_n2s() to convert a numeric define to a string that can used as a default value in a flag spec
#define	ARGS_n2s(s)	_ARGS_str(s)
#define	_ARGS_str(s)	#s

// Use ARGS_list(hdr, ARGS_list_elt(str, msg), ...)	 to build a multi-line msg for AVK_MSG
#define	ARGS_list(hdr, ...)		hdr "\n" __VA_ARGS__
#define	ARGS_list_elt(str, msg)		"\t\t" str "\t\t - " msg "\n"
#define	ARGS_list_elt_last(str, msg)	"\t\t" str "\t\t - " msg

typedef	struct	arg_val_t	{
	int	av_type;
	union	{
		int	v_int;
		double	v_double;
		char	*v_str;
	} av_value;
} ARG_VAL_T;

typedef	struct	flag_t	{
	const char	*f_name;
	int	f_is_opt;
	int	f_vkind;
	int	f_vtype;
	const char	*f_defval;
	const char	*f_descr;

	int	f_was_set;
	ARG_VAL_T	f_value;
} FLAG_T;

// The first 6 fields of each FLAGS are set by the user when defining the arguments
#define	N_VFLAGS	6
#define	VF_NAME		0
#define	VF_IS_OPT	1
#define	VF_VKIND	2
#define	VF_VTYPE	3
#define	VF_DEFVAL	4
#define	VF_DESCR	5

typedef	struct	args_t	{
	char	*a_progname;
	FLAG_T	*a_flags;
	FLAG_T	**a_flag_idx;
	int	an_flags;
	int	a_min_files;
	int	a_max_files;
	char	**a_files;
	int	an_files;
} ARGS_T;

#ifdef	__cplusplus
extern "C" {
#endif

int
TJM_get_args(int, char *[], int, FLAG_T *, int, int, ARGS_T **);

void
TJM_free_args(ARGS_T *);

void
TJM_dump_args(FILE *, const ARGS_T *);

const	ARG_VAL_T	*
TJM_get_flag_value(const ARGS_T *, const char *, int);

void
TJM_print_help_msg(FILE *, const ARGS_T *);

#ifdef	__cplusplus
}
#endif

#endif
