#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "args.h"

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help", 1, AVK_NONE, AVT_BOOL, "0", "Use -help to print this message."},
	{"-v",    1, AVK_OPT,  AVT_UINT, "0", "Use -v to set the verbosity to 1; use -v=N to set it to N."}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

static	int
json_cmprs(FILE *, FILE *, int);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	verbose = 0;
	FILE	*fp = NULL;
	int	err = 0;

	a_stat = TJM_get_args(argc, argv, n_flags, flags, 0, 1, &args);
	if(a_stat != AS_OK){
		err = a_stat != AS_HELP_ONLY;
		goto CLEAN_UP;
	}

	a_val = TJM_get_flag_value(args, "-v", AVT_UINT);
	verbose = a_val->av_value.v_int;

	if(verbose > 1)
		TJM_dump_args(stderr, args);

	if(args->an_files == 0)
		fp = stdin;
	else if((fp = fopen(args->a_files[0], "r")) == NULL){
		LOG_ERROR("can't read input file %s", args->a_files[0]);
		err = 1;
		goto CLEAN_UP;
	}

	if(json_cmprs(fp, stdout, verbose)){
		LOG_ERROR("json_cmprs failed");
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	if(fp != NULL && fp != stdin)
		fclose(fp);

	TJM_free_args(args);

	exit(err);
}

static	int
json_cmprs(FILE *ifp, FILE *ofp, int verbose)
{
	int	c;
	int	in_str;
	int	err = 0;

	for(in_str = 0; (c = getc(ifp)) != EOF; ){
		if(in_str){
			putc(c, ofp);
			if(c == '"')
				in_str = 0;
			else if(c == '\\'){
				c = getc(ifp);
				if(c == EOF)
					break;
				putc(c, ofp);
			}
		}else if(c == '"'){
			in_str = 1;
			putc(c, ofp);
		}else if(!isspace(c))
			putc(c, ofp);
	}
	if(in_str){
		LOG_ERROR("json contains unterminated string");
		err = 1;
		goto CLEAN_UP;
	}
	putc('\n', ofp);

CLEAN_UP : ;

	return err;
}

