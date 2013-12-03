#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <jansson.h>

#include "log.h"
#include "args.h"
#include "json_get.h"
#include "parse_glist.h"
#include "exec_glist.h"

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help", 1, AVK_NONE, AVT_BOOL, "0",  "Use -help to print this message."},
	{"-v",    1, AVK_OPT,  AVT_UINT, "0",  "Use -v to set the verbosity to 1, use -v=N to set it to N."},
	{"-n",    1, AVK_OPT,  AVT_BOOL, "0",  "Use -n to set line mode, ie 1 json blob/line. Absent, the entire input file is one json blob."},
	{"-g",    0, AVK_REQ,  AVT_STR,  NULL, "Use -g G, G a sequence of json selectors to get values."}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	verbose = 0;
	int	nopt = 0;
	const char	*glist = NULL;
	VALUE_T	*vp_glist = NULL;
	int	n_arys = 0;
	const char	*fname = NULL;
	FILE	*fp = NULL;
	char	*line = NULL;
	size_t	s_line = 0;
	ssize_t	l_line;	
	int	lcnt;
	json_t	*js_root = NULL;
	json_error_t	js_err;
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

	a_val = TJM_get_flag_value(args, "-g", AVT_STR);
	glist = a_val->av_value.v_str;

	if(verbose > 1)
		TJM_dump_args(stderr, args);

	if(args->an_files == 0){
		fp = stdin;
		fname = "--stdin--";
	}else if((fp = fopen(args->a_files[0], "r")) == NULL){
		LOG_ERROR("can't read json-file %s", args->a_files[0]);
		err = 1;
		goto CLEAN_UP;
	}else
		fname = args->a_files[0];

	if(JG_parse_glist(glist, &vp_glist, &n_arys)){
		LOG_ERROR("problem w/key");
		err = 1;
		goto CLEAN_UP;
	}
	if(verbose){
		fprintf(stderr, "glist = %s\n", glist);
		JG_value_dump(stderr, vp_glist, 0);
	}

	if(nopt){
		for(lcnt = 0; (l_line = getline(&line, &s_line, fp)) > 0; ){
			lcnt++;
			if(line[l_line - 1] == '\n'){
				line[l_line -1] = '\0';
				l_line--;
				if(l_line == 0){
					LOG_WARN("%s:%d: empty line, skipping", fname, lcnt);
					continue;
				}	
			}
			js_root = json_loads(line, 0, &js_err);
			if(js_root == NULL){
				LOG_WARN("%s:%d: couldn't load json: %s, col = %d, pos = %d",
					fname, lcnt, js_err.text, js_err.column, js_err.position);
				err = 1;
				goto NEXT;
			}
			if(verbose){
				LOG_INFO("lcnt = %7d", lcnt);
				if(verbose > 1){
					json_dumpf(js_root, stderr, JSON_INDENT(2));
					fputc('\n', stderr);
				}
			}

			if(JG_exec_glist(stdout, js_root, vp_glist, n_arys)){
				LOG_WARN("%s:%d: search failed", fname, lcnt);
				err = 1;
				goto NEXT;
			}
		NEXT : ;

			if(js_root != NULL)
				json_decref(js_root);
			js_root = NULL;
		}
	}else{
		js_root = json_loadf(fp, 0, &js_err);
		if(js_root == NULL){
			LOG_ERROR("%s:%d: couldn't load json: %s, col = %d, pos = %d",
				fname, js_err.line, js_err.text, js_err.column, js_err.position);
			err = 1;
			goto CLEAN_UP;
		}
		if(verbose > 1){
			json_dumpf(js_root, stderr, JSON_INDENT(2));
			fputc('\n', stderr);
		}

		if(JG_exec_glist(stdout, js_root, vp_glist, n_arys)){
			LOG_ERROR("%s: search failed", fname);
			err = 1;
			goto CLEAN_UP;
		}
	}

CLEAN_UP : ;

	if(js_root != NULL)
		json_decref(js_root);

	if(line != NULL)
		free(line);

	if(fp != NULL && fp != stdin)
		fclose(fp);

	if(vp_glist != NULL)
		JG_value_delete(vp_glist);

	TJM_free_args(args);

	exit(err);
}
