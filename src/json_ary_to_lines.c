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
json_ary_to_lines(FILE *, int);

static	int
json_copy_str(FILE *, int *, int *);

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

	json_ary_to_lines(fp, verbose);

CLEAN_UP : ;

	if(fp != NULL && fp != stdin)
		fclose(fp);

	TJM_free_args(args);

	exit(err);
}

static	int
json_ary_to_lines(FILE *fp, int verbose)
{
	int	c;
	int	ccnt, rcnt;
	int	err = 0;

	for(ccnt = 0 ; (c = getc(fp)) != EOF; ){
		ccnt++;
		if(!isspace(c))
			break;
	}
	if(c == EOF){
		if(ccnt != 0){
			LOG_ERROR("file contains only white space");
			err = 1;
		}
		goto CLEAN_UP;	// empty file is OK
	}else if(c != '['){
		LOG_ERROR("file does not contain a JSON array");
		err = 1;
		goto CLEAN_UP;
	}else{
		c = getc(fp);
		ccnt++;
	}

	for(rcnt = 0; c != EOF; ){
		for( ; isspace(c); ){
			c = getc(fp);
			ccnt++;
		}
		if(c == '{'){
			if(json_copy_str(fp, &c, &ccnt)){
				err = 1;
				goto CLEAN_UP;
			}
			if(c == '}'){
				c = getc(fp);
				for( ; isspace(c); ){
					c = getc(fp);
					ccnt++;
				}
				if(c == ','){
					c = getc(fp);
					ccnt++;
				}else if(c == ']')
					break;
				else{
					LOG_ERROR("pos %10d: unexpected char '%c'", ccnt, c);
					err = 1;
					goto CLEAN_UP;
				}
			}
		}else{
			LOG_ERROR("pos %10d: unexpected char '%c'", ccnt, c);
			err = 1;
			goto CLEAN_UP;
		}
	}
	if(c != ']'){
		LOG_ERROR("pos %10d: unclosed array", ccnt);
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	return err;
}

static	int
json_copy_str(FILE *fp, int *c, int *ccnt)
{
	int	in_str;
	int	n_lev, ul_char, dl_char;
	int	err = 0;

	// is { or [
	putchar(*c);

	ul_char = *c;
	dl_char = *c == '{' ? '}' : ']';
	n_lev = 1;
	for(in_str = 0; (*c = getc(fp)) != EOF; ){
		(*ccnt)++;
		if(in_str){
			putchar(*c);
			if(*c == '\\'){
				*c = getc(fp);
				(*ccnt)++;
				putchar(*c);
			}else if(*c == '"'){
				in_str = 0;
			}
		}else if(*c == '"'){
			putchar(*c);
			in_str = 1;
		}else if(*c == ul_char){
			putchar(*c);
			n_lev++;
		}else if(*c == dl_char){
			putchar(*c);
			n_lev--;
			if(n_lev == 0)
				break;
		}else if(!isspace(*c))
			putchar(*c);
	}
	putchar('\n');

	return err;
}
