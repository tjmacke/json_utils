#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

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
	{"-nt",   1, AVK_REQ,  AVT_PINT, "1",  "Use -nt N to set the number of worker threads to N. N > 1 requires -n"},
	{"-n",    1, AVK_OPT,  AVT_BOOL, "0",  "Use -n to set line mode, ie 1 json blob/line. Absent, the entire input file is one json blob."},
	{"-g",    0, AVK_REQ,  AVT_STR,  NULL, "Use -g G, where G is list of selectors. Use -g @F to read the selector list from file F."}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

typedef	struct	tparm_t	{
	int	t_id;
	int	tn_workers;
} TPARM_T;

static	int	verbose;
static	VALUE_T	*vp_glist;
static	int	n_arys;

static	pthread_mutex_t	ifmutex = PTHREAD_MUTEX_INITIALIZER;;
static	const char	*ifname;
static	FILE	*ifp;
static	int	if_lcnt;

static	pthread_mutex_t	ofmutex = PTHREAD_MUTEX_INITIALIZER;
static	FILE	*ofp;

static	int	n_workers;
static	pthread_t	*workers;
static	TPARM_T	*wparms;

static	char	*
read_glist(const char *);

static	void	*
worker(void *);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	nopt = 0;
	const char	*glist = NULL;
	char	*glbuf = NULL;
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

	a_val = TJM_get_flag_value(args, "-nt", AVT_PINT);
	n_workers = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-n", AVT_BOOL);
	nopt = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-g", AVT_STR);
	glist = a_val->av_value.v_str;

	if(verbose > 1)
		TJM_dump_args(stderr, args);

	// Are the selectors in a file?
	if(*glist == '@'){
		if((glbuf = read_glist(glist)) == NULL){
			err = 1;
			goto CLEAN_UP;
		}
		glist = glbuf;
	}

	if(n_workers > 1 && !nopt){
		LOG_ERROR("multi threading (-nt N, N > 1) only works with -n");
		TJM_print_help_msg(stderr, args);
		err = 1;
		goto CLEAN_UP;
	}

	if(args->an_files == 0){
		ifp = stdin;
		ifname = "--stdin--";
	}else if((ifp = fopen(args->a_files[0], "r")) == NULL){
		LOG_ERROR("can't read json-file %s", args->a_files[0]);
		err = 1;
		goto CLEAN_UP;
	}else
		ifname = args->a_files[0];

	ofp = stdout;

	if(JG_parse_glist(glist, &vp_glist, &n_arys)){
		LOG_ERROR("problem w/key");
		err = 1;
		goto CLEAN_UP;
	}
	if(verbose > 1){
		fprintf(stderr, "glist = %s\n", glist);
		JG_value_dump(stderr, vp_glist, 0);
	}

	if(nopt){
		int	w;

		workers = (pthread_t *)malloc(n_workers * sizeof(pthread_t));
		if(workers == NULL){
			LOG_ERROR("can't allocate workers");
			err = 1;
			goto CLEAN_UP;
		}
		wparms = (TPARM_T *)malloc(n_workers * sizeof(TPARM_T));
		if(wparms == NULL){
			LOG_ERROR("can't allocate wparms");
			err = 1;
			goto CLEAN_UP;
		}
		for(w = 0; w < n_workers; w++){
			wparms[w].t_id = w + 1;
			wparms[w].tn_workers = n_workers;
			pthread_create(&workers[w], NULL, worker, &wparms[w]);
		}
		for(w = 0; w < n_workers; w++)
			pthread_join(workers[w], NULL);

	}else{		// Process 1 blob; run in main thread
		js_root = json_loadf(ifp, 0, &js_err);
		if(js_root == NULL){
			LOG_ERROR("%s:%d: couldn't load json: %s, col = %d, pos = %d",
				ifname, js_err.line, js_err.text, js_err.column, js_err.position);
			err = 1;
			goto CLEAN_UP;
		}
		if(verbose > 1){
			json_dumpf(js_root, stderr, JSON_INDENT(2));
			fputc('\n', stderr);
		}

		if(JG_exec_glist(ofp, &ofmutex, verbose, js_root, vp_glist, n_arys)){
			LOG_ERROR("%s: search failed", ifname);
			err = 1;
			goto CLEAN_UP;
		}
	}

CLEAN_UP : ;

	if(js_root != NULL)
		json_decref(js_root);

	if(wparms != NULL)
		free(wparms);

	if(workers != NULL)
		free(workers);

	if(ifp != NULL && ifp != stdin)
		fclose(ifp);

	if(vp_glist != NULL)
		JG_value_delete(vp_glist);

	if(glbuf != NULL)
		free(glbuf);

	TJM_free_args(args);

	exit(err);
}

char	*
read_glist(const char *glist)
{
	FILE	*glfp = NULL;
	struct stat	glsbuf;
	char	*glbuf = NULL;
	int	err = 0;

	if((glfp = fopen(&glist[1], "r")) == NULL){
		LOG_ERROR("can't read glist file %s", &glist[1]);
		err = 1;
		goto CLEAN_UP;
	}

	if(fstat(fileno(glfp), &glsbuf)){
		LOG_ERROR("can't stat glist file %s", &glist[1]);
		err = 1;
		goto CLEAN_UP;
	}

	glbuf = (char *)malloc((glsbuf.st_size + 1) * sizeof(char));
	if(glbuf == NULL){
		LOG_ERROR("can't allocate glbuf");
		err = 1;
		goto CLEAN_UP;
	}

	fread(glbuf, sizeof(char), glsbuf.st_size, glfp);
	glbuf[glsbuf.st_size] = '\0';

CLEAN_UP : ;

	if(err){
		if(glbuf != NULL){
			free(glbuf);
			glbuf = NULL;
		}
	}

	return glbuf;
}

static	void	*
worker(void *ptr)
{
	TPARM_T	*tparm = (TPARM_T *)ptr;
	int	done;
	int	l_if_lcnt;
	char	*line = NULL;
	size_t	s_line = 0;
	ssize_t	l_line;
	json_t	*js_root = NULL;
	json_error_t	js_err;
	int	err = 0;

	LOG_INFO("worker %d started", tparm->t_id);

	for(done = 0; ; ){

		pthread_mutex_lock(&ifmutex);
		l_line = getline(&line, &s_line, ifp);
		if(l_line > 0){
			if_lcnt++;
			l_if_lcnt = if_lcnt;
		}else
			done = 1;
		pthread_mutex_unlock(&ifmutex);
		if(done)
			break;

		if(line[l_line - 1] == '\n'){
			line[l_line -1] = '\0';
			l_line--;
			if(l_line == 0){
				LOG_WARN("%s:%d: empty line, skipping", ifname, l_if_lcnt);
				continue;
			}	
		}
		js_root = json_loads(line, 0, &js_err);
		if(js_root == NULL){
			LOG_WARN("%s:%d: couldn't load json: %s, col = %d, pos = %d",
				ifname, l_if_lcnt, js_err.text, js_err.column, js_err.position);
			err = 1;
			goto NEXT;
		}
		if(verbose){
			LOG_INFO("lcnt = %7d", l_if_lcnt);
			if(verbose > 1){
				json_dumpf(js_root, stderr, JSON_INDENT(2));
				fputc('\n', stderr);
			}
		}

		if(JG_exec_glist(stdout, &ofmutex, verbose, js_root, vp_glist, n_arys)){
			LOG_WARN("%s:%d: search failed", ifname, l_if_lcnt);
			err = 1;
			goto NEXT;
		}
	NEXT : ;

		if(js_root != NULL)
			json_decref(js_root);
		js_root = NULL;
	}

CLEAN_UP : ;

	if(js_root != NULL)
		json_decref(js_root);

	if(line != NULL)
		free(line);

	LOG_INFO("worker %d finished", tparm->t_id);

	return NULL;
}
