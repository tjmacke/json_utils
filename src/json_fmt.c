#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "args.h"

#define	FMT_IN_SIZE	2

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help",    1, AVK_NONE, AVT_BOOL, "0", "Use -help to print this message."},
	{"-version", 1, AVK_NONE, AVT_BOOL, "0", "Use -version to print the version."},
	{"-v",       1, AVK_OPT,  AVT_UINT, "0", "Use -v to set verbosity to 1, -v=N to it to N."},
	{"-n",       1, AVK_NONE, AVT_BOOL, "0", "Use -n when each line is a complete json object."},
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

typedef	struct	fstate_t	{
	int	f_verbose;
	const char	*f_buf;
	size_t	fl_buf;
	int	f_instr;
	int	f_ilev;
	int	f_addnl;
} FSTATE_T;

static	int
fmtjs(FILE *, FSTATE_T *);

static	int	verbose;
static	int	nopt;

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	FILE 	*fp = NULL;
	char	*line = NULL;
	size_t	s_line = 0;
	ssize_t	l_line;
	int	lcnt;
	FSTATE_T	state;
	char	*lp;
	int	i, ilev;
	int	instr;
	int	err = 0;

	a_stat = TJM_get_args(argc, argv, n_flags, flags, 0, 1, &args);
	if(a_stat != AS_OK){
		err = a_stat != AS_HELP_ONLY;
		goto CLEAN_UP;
	}

	a_val = TJM_get_flag_value(args, "-v", AVT_UINT);
	verbose = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-n", AVT_BOOL);
	nopt = a_val->av_value.v_int;

	if(args->an_files == 0)
		fp = stdin;
	else if((fp = fopen(args->a_files[0], "r")) == NULL){
		LOG_ERROR("can't read js-file %s", args->a_files[0]);
		err = 1;
		goto CLEAN_UP;
	}

	memset(&state, 0, sizeof(FSTATE_T));
	for(ilev = 0, instr = 0, lcnt = 0; (l_line = getline(&line, &s_line, fp)) > 0; ){
		state.f_buf = line;
		state.fl_buf = l_line;
		err |= fmtjs(stdout, &state);
		if(nopt)
			putchar('\n');
	}
	putchar('\n');

CLEAN_UP : ;

	if(line != NULL)
		free(line);

	if(fp != NULL && fp != stdin)
		fclose(fp);

	TJM_free_args(args);

	exit(err);
}

static	int
fmtjs(FILE *fp, FSTATE_T *state)
{
	const char	*bp;
	const char	*e_bp;
	int	i;
	int	err = 0;

	e_bp = &state->f_buf[state->fl_buf];
	for(bp = state->f_buf; bp < e_bp; ){
		if(state->f_instr){
			if(*bp == '"'){
				state->f_instr = 0;
				putc(*bp, fp);
				bp++;
			}else if(*bp == '\\'){
				putc(*bp, fp);
				bp++;
				putc(*bp, fp);
				bp++;
			}else{
				putc(*bp, fp);
				bp++;
			}
		}else if(*bp == '"'){
			state->f_instr = 1;
			putc(*bp, fp);
			bp++;
		}else if(*bp == ','){
			putc(*bp, fp);
			bp++;
			putc('\n', fp);
			for(i = 0; i < FMT_IN_SIZE * state->f_ilev; i++)	
				putc(' ', fp);
		}else if(*bp == '{' || *bp == '['){
/*
			if(state->f_ilev > 0)
				putc('\n', fp);
			for(i = 0; i < FMT_IN_SIZE * state->f_ilev; i++)	
				putc(' ', fp);
*/
			putc(*bp, fp);
			bp++;
			state->f_ilev++;
			putc('\n', fp);
			for(i = 0; i < FMT_IN_SIZE * state->f_ilev; i++)	
				putc(' ', fp);
		}else if(*bp == '}' || *bp == ']'){
			putc('\n', fp);
			state->f_ilev--;
			for(i = 0; i < FMT_IN_SIZE * state->f_ilev; i++)	
				putc(' ', fp);
			putc(*bp, fp);
			bp++;
		}else if(*bp == '\\'){
		}else if(!isspace(*bp)){
			putc(*bp, fp);
			bp++;
		}else
			bp++;
	}
	if(state->f_addnl)
		fputc('\n', fp);

	return err;
}
