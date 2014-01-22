#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "args.h"

static	int
cmp_flag_name(const void *, const void *);

static	int
get_flag_defval(FLAG_T *, char **);

static	int
set_flag_value(FLAG_T *, const char *);

static	FLAG_T	*
find_flag_name(const char *, int, FLAG_T *[]);

/*
static	void
pr_help_msg(FILE *, ARGS_T *);
*/

// TODO: decide if options end after 1st non -x arg?
int
TJM_get_args(int argc, char *argv[], int n_flags, FLAG_T flags[], int min_files, int max_files, ARGS_T **args)
{
	int	i;
	FLAG_T	*fp;
	int	u_defval;
	char	*defval = NULL;
	size_t	s_work = 0;
	char	*w_name = NULL;
	char	*w_value = NULL;
	int	a_stat = AS_OK;

	*args = NULL;
	if(argc == 0 || argv == NULL){
		LOG_ERROR("argc is 0 or argv is NULL");
		a_stat = AS_ERROR;
		goto CLEAN_UP;
	}

	*args = (ARGS_T *)calloc((size_t)1, sizeof(ARGS_T));
	if(*args == NULL){
		LOG_ERROR("can't alloxate args");
		a_stat = AS_ERROR;
		goto CLEAN_UP;
	}

	(*args)->a_progname = argv[0];

	if(n_flags > 0){
		(*args)->an_flags = n_flags;
		(*args)->a_flags = flags;
		(*args)->a_flag_idx = (FLAG_T **)malloc(n_flags * sizeof(FLAG_T *));
		if((*args)->a_flag_idx == NULL){
			LOG_ERROR("can't allocate a_flag_idx");
			a_stat = AS_ERROR;
			goto CLEAN_UP;
		}
		for(i = 0; i < n_flags; i++){
			if(get_flag_defval(&((*args)->a_flags)[i], &defval)){
				LOG_ERROR("%s: can't get defval", (*args)->a_flags[i].f_name);
				a_stat = AS_ERROR;
				goto CLEAN_UP;
			}
			if(set_flag_value(&((*args)->a_flags)[i], defval)){
				LOG_ERROR("can't set def value for %s", (*args)->a_flags[i].f_name);
				a_stat = AS_ERROR;
				goto CLEAN_UP;
			}
			(*args)->a_flag_idx[i] = &((*args)->a_flags)[i];
			if(defval != NULL){
				free(defval);
				defval = NULL;
			}
		}
		qsort((*args)->a_flag_idx, (*args)->an_flags, sizeof(FLAG_T *), cmp_flag_name);
	}

	if(min_files < 0){
		LOG_ERROR("min_files (%d) must >= 0", min_files);
		a_stat = AS_ERROR;
		goto CLEAN_UP;
	}
	// Use -1 to indicate no max_files
	if(max_files != -1){
		if(max_files < 0){
			LOG_ERROR("max_files (%d) must >= 0 or -1 for no max_files", max_files);
			a_stat = AS_ERROR;
			goto CLEAN_UP;
		}else if(max_files != 0){
			if(max_files < min_files){
				LOG_ERROR("max_files (%d) must be >= min_files (%d)", max_files, min_files);
				a_stat = AS_ERROR;
				goto CLEAN_UP;
			}
		}
	}
	(*args)->a_min_files = min_files;
	(*args)->a_max_files = max_files;

	(*args)->a_files = (char **)malloc(argc * sizeof(char *));
	if((*args)->a_files == NULL){
		LOG_ERROR("can't allocate a_files\n");
		a_stat = AS_ERROR;
		goto CLEAN_UP;
	}
	for(s_work = 0, i = 1; i < argc; i++){
		size_t	l_arg;

		// -help always works, short ciruits arg processing
		if(!strcmp(argv[i], "-help")){
			TJM_print_help_msg(stderr, *args);
			a_stat = AS_HELP_ONLY;
			goto CLEAN_UP;
		}
#ifdef	VERSION
		if(!strcmp(argv[i], "-version")){
			fprintf(stderr, "Version: %s\n", VERSION);
			a_stat = AS_HELP_ONLY;
			goto CLEAN_UP;
		}
#endif
		l_arg = strlen(argv[i]);
		if(l_arg > s_work)
			s_work = l_arg;
	}
	if(s_work > 0){
		s_work++;
		w_name = (char *)malloc(s_work * sizeof(char));
		if(w_name == NULL){
			LOG_ERROR("can't allocate w_name");
			a_stat = AS_ERROR;
			goto CLEAN_UP;
		}
		w_value = (char *)malloc(s_work * sizeof(char));
		if(w_value == NULL){
			LOG_ERROR("can't allocate w_value");
			a_stat = AS_ERROR;
			goto CLEAN_UP;
		}

		for(i = 1; i < argc; i++){
			if(*argv[i] == '-'){
				char	*eq;

				if((eq = strchr(argv[i], '=')) != NULL){
					strncpy(w_name, argv[i], eq - argv[i]);
					w_name[eq - argv[i]] = '\0';
					strcpy(w_value, &eq[1]);
					u_defval = 0;
				}else{
					strcpy(w_name, argv[i]);
					strcpy(w_value, "1");
					u_defval = 1;
				}
				if((fp = find_flag_name(w_name, (*args)->an_flags, (*args)->a_flag_idx)) == NULL){
					LOG_ERROR("unknown flag %s", argv[i]);
					TJM_print_help_msg(stderr, *args);
					a_stat = AS_ERROR;
					goto CLEAN_UP;
				}else{
					fp->f_was_set = 1;
					if(fp->f_vkind == AVK_REQ){
						i++;
						if(i >= argc){
							LOG_ERROR("flag %s requires value", fp->f_name);
							TJM_print_help_msg(stderr, *args);
							a_stat = AS_ERROR;
							goto CLEAN_UP;
						}else
							strcpy(w_value, argv[i]);
					}else if(fp->f_vkind == AVK_OPT){
						// Strings w/opt value are special.  Use "" as "was set w/no value"
						if(fp->f_vtype == AVT_STR && fp->f_defval == NULL && u_defval)
							strcpy(w_value, "");
					}
					if(set_flag_value(fp, w_value)){
						LOG_ERROR("can't set value for %s", (*args)->a_flags[i].f_name);
						TJM_print_help_msg(stderr, *args);
						a_stat = AS_ERROR;
						goto CLEAN_UP;
					}
				}
			}else{
				(*args)->a_files[(*args)->an_files] = argv[i];
				(*args)->an_files++;
			}
		}
	}
	// Check for flags that were not set
	for(i = 0; i < (*args)->an_flags; i++){
		fp = &((*args)->a_flags)[i];
		if(!fp->f_was_set && !fp->f_is_opt){
			LOG_ERROR("flag %s was not set", fp->f_name);
			TJM_print_help_msg(stderr, *args);
			a_stat = AS_ERROR;
			goto CLEAN_UP;
		}
	}
	/* TODO: Check for flags have limited values to make sure they're legal
	for(i = 0; i < (*args)->an_flags; i++){
	}
	*/

	if((*args)->an_files < (*args)->a_min_files){
		LOG_ERROR("Missing files: have %d, need at least %d", (*args)->an_files, (*args)->a_min_files);
		TJM_print_help_msg(stderr, *args);
		a_stat = AS_ERROR;
		goto CLEAN_UP;
	}
	if((*args)->a_max_files > 0){
		if((*args)->an_files > (*args)->a_max_files){
			LOG_ERROR("Too many files: have %d, need at most %d", (*args)->an_files, (*args)->a_max_files);
			TJM_print_help_msg(stderr, *args);
			a_stat = AS_ERROR;
			goto CLEAN_UP;
		}
	}

CLEAN_UP : ;

	if(w_value != NULL)
		free(w_value);

	if(w_name != NULL)
		free(w_name);

	if(defval != NULL)
		free(defval);

	if(a_stat != AS_OK){
		TJM_free_args(*args);
		*args = NULL;
	}

	return a_stat;
}

void
TJM_free_args(ARGS_T *args)
{
	int	i;
	FLAG_T	*fp;

	if(args == NULL)
		return;

	if(args->a_flag_idx != NULL)
		free(args->a_flag_idx);
	for(i = 0; i < args->an_flags; i++){
		fp = &args->a_flags[i];
		if(fp->f_value.av_type == AVT_STR && fp->f_value.av_value.v_str != NULL)
			free(fp->f_value.av_value.v_str);
	}

	if(args->a_files != NULL)
		free(args->a_files);

	free(args);
}

void
TJM_dump_args(FILE *fp, const ARGS_T *args)
{

	if(args == NULL){
		fprintf(fp, "args is NULL\n");
		return;
	}

	fprintf(fp, "args = {\n");
	fprintf(fp, "\tprogname = %s\n", args->a_progname ? args->a_progname : "NULL");
	if(args->an_flags == 0)
		fprintf(fp, "\tflags    = NULL\n");
	else{
		int	i;

		if(args->a_flags == NULL)
			fprintf(fp, "\tflags    = NULL\n");
		else{
			size_t	maxl_sname, maxl_defvals, l_str;
			int	n_msg;
			FLAG_T	*p_flag;

			for(n_msg = 0, maxl_defvals = maxl_sname = 0, i = 0; i < args->an_flags; i++){
				if(args->a_flags[i].f_vkind == AVK_MSG){
					n_msg++;
					continue;
				}
				if((l_str = strlen(args->a_flags[i].f_name)) > maxl_sname)
					maxl_sname = l_str;
				if(args->a_flags[i].f_defval != NULL){
					if((l_str = strlen(args->a_flags[i].f_defval)) > maxl_defvals)
						maxl_defvals = l_str;
				}else if((l_str = strlen("NULL")) > maxl_defvals)
					maxl_defvals = l_str;
			}

			fprintf(fp, "\tflags    = %d {\n", args->an_flags - n_msg);
			for(i = 0; i < args->an_flags; i++){
				p_flag = &args->a_flags[i];
				if(p_flag->f_vkind == AVK_MSG)
					continue;
				fprintf(fp, "\t\tname = %-*s", (int)maxl_sname, p_flag->f_name);
				fprintf(fp, " | isopt = %d", p_flag->f_is_opt);
				fprintf(fp, " | was_set = %d", p_flag->f_was_set);
				fprintf(fp, " | vkind = ");
				switch(p_flag->f_vkind){
				case AVK_NONE :
					fprintf(fp, "NONE");
					break;
				case AVK_OPT :
					fprintf(fp, "OPT ");
					break;
				case AVK_REQ :
					fprintf(fp, "REQ ");
					break;
				case AVK_MSG :
					fprintf(fp, "MSG ");
					break;
				default :
					fprintf(fp, "ERROR: invalid vkind %d", p_flag->f_vkind);
					break;
				}
				fprintf(fp, " | vtype = ");
				switch(p_flag->f_vtype){
				case AVT_BOOL :
					fprintf(fp, "BOOL");
					break;
				case AVT_INT :
					fprintf(fp, "INT ");
					break;
				case AVT_UINT :
					fprintf(fp, "UINT");
					break;
				case AVT_PINT :
					fprintf(fp, "PINT");
					break;
				case AVT_REAL :
					fprintf(fp, "REAL");
					break;
				case AVT_STR :
					fprintf(fp, "STR ");
					break;
				default :
					fprintf(fp, "ERROR: invalid vtype %d", p_flag->f_vtype);
					break;
				}
				fprintf(fp, " | defval = %-*s", (int)maxl_defvals, p_flag->f_defval ? p_flag->f_defval : "NULL");
				fprintf(fp, " | value = ");
				switch(p_flag->f_value.av_type){
				case AVT_INT :
					fprintf(fp, "%d", p_flag->f_value.av_value.v_int);
					break;
				case AVT_REAL :
					fprintf(fp, "%g", p_flag->f_value.av_value.v_double);
					break;
				case AVT_STR :
					fprintf(fp, "%s", p_flag->f_value.av_value.v_str ? p_flag->f_value.av_value.v_str : "NULL");
					break;
				default :
					fprintf(fp, "ERROR: invalid v_type %d, value = %p", p_flag->f_value.av_type, p_flag->f_value.av_value.v_str);
					break;
				}
				fprintf(fp, "\n");
			}
			fprintf(fp, "\t}\n");
		}
	}
	fprintf(fp, "\tmin_files = %d\n", args->a_min_files);
	fprintf(fp, "\tmax_files = %d\n", args->a_max_files);
	if(args->an_files == 0)
		fprintf(fp, "\tfiles     = NONE\n");
	else{
		int	i;

		fprintf(fp, "\tfiles     = %d {\n", args->an_files);
			for(i = 0; i < args->an_files; i++)
				fprintf(fp, "\t\t%s\n", args->a_files[i] ? args->a_files[i] : "NULL");
		fprintf(fp, "\t}\n");
	}
	fprintf(fp, "}\n");
}

const ARG_VAL_T	*
TJM_get_flag_value(const ARGS_T *args, const char *fname, int vtype)
{
	FLAG_T	*fp = NULL;;
	const ARG_VAL_T	*avp = NULL;
	int	err = 0;

	if(args == NULL){
		LOG_ERROR("args is NULL");
		err = 1;
		goto CLEAN_UP;
	}
	if(args->an_flags == 0){
		LOG_ERROR("no flags");
		err = 1;
		goto CLEAN_UP;
	}
	if(args->a_flag_idx == NULL){
		LOG_ERROR("args has not been initialized: call TJM_get_args() before any calls to TJM_get_flag_value()");
		err = 1;
		goto CLEAN_UP;
	}
	if(fname == NULL || *fname == '\0'){
		LOG_ERROR("fname is NULL or empty");
		err = 1;
		goto CLEAN_UP;
	}
	if((fp = find_flag_name(fname, args->an_flags, args->a_flag_idx)) == NULL){
		LOG_ERROR("unknown flag %s", fname);
		err = 1;
		goto CLEAN_UP;
	}
	avp = &fp->f_value;
	if(vtype == AVT_BOOL || vtype == AVT_INT || vtype == AVT_UINT || vtype == AVT_PINT){
		if(avp->av_type != AVT_INT){
			LOG_ERROR("flag %s has wrong type: expect %d, got %d", fname, vtype, avp->av_type);
			err = 1;
			goto CLEAN_UP;
		}
	}else if(vtype != avp->av_type){
		LOG_ERROR("flag %s has wrong type: expect %d, got %d", fname, vtype, avp->av_type);
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	if(err)
		avp = NULL;

	return avp;
}

static	int
cmp_flag_name(const void *p1, const void *p2)
{
	const	FLAG_T	**fp1 = (const FLAG_T **)p1;
	const	FLAG_T	**fp2 = (const FLAG_T **)p2;
	int	cv;

	if((cv = strcmp((*fp1)->f_name, (*fp2)->f_name)) == 0)
		return 0;
	else if(cv < 0)
		return -1;
	else
		return 1;
}

static	int
get_flag_defval(FLAG_T *fp, char **defval)
{
	const char	*vp, *e_vp;
	int	err = 1;

	*defval = NULL;

	if(fp->f_defval == NULL || *fp->f_defval == '\0'){
		err = 0;
		goto CLEAN_UP;
	}

	if(strchr(fp->f_defval, '|') == NULL){	// | indicates this string contains choices
		*defval = strdup(fp->f_defval);
		if(*defval == NULL)
			LOG_ERROR("%s: can't strdup defval", fp->f_name);
		else
			err = 0;
		goto CLEAN_UP;
	}

	// string is at least "|"
	for(vp = e_vp = fp->f_defval; *e_vp; e_vp++){
		if(*e_vp == '|'){
			if(e_vp == vp){
				LOG_ERROR("%s: empty choice", fp->f_name);
				goto CLEAN_UP;
			}
			if(e_vp[-1] == '*'){
				if(e_vp == vp + 1){
					LOG_ERROR("%s: default (*) value is empty", fp->f_name);
					goto CLEAN_UP;
				}
				*defval = strndup(vp, e_vp - 1 - vp);
				if(*defval == NULL)
					LOG_ERROR("%s: can't strndup defval", fp->f_name);
				else
					err = 0;
				goto CLEAN_UP;
			}
			vp = e_vp + 1;
		}
	}
	if(*vp == '\0'){
		LOG_ERROR("%s: empty choice", fp->f_name);
		goto CLEAN_UP;
	}
	if(e_vp[-1] == '*'){
		if(e_vp == vp + 1){
			LOG_ERROR("%s: default (*) value is empty", fp->f_name);
			goto CLEAN_UP;
		}
		*defval = strndup(vp, e_vp - 1 - vp);
		if(*defval == NULL)
			LOG_ERROR("%s: can't strndup defval", fp->f_name);
		else
			err = 0;
		goto CLEAN_UP;
	}else
		err = 0;

CLEAN_UP : ;

	return err;
}

static	int
set_flag_value(FLAG_T *fp, const char *value)
{
	int	err = 0;

	switch(fp->f_vtype){
	case AVT_BOOL :
	case AVT_INT :
	case AVT_UINT :
	case AVT_PINT :
		fp->f_value.av_type = AVT_INT;
		if(value == NULL || *value == '\0')
			fp->f_value.av_value.v_int = 0;
		else{
			int	ival;

			ival = atoi(value);
			if(fp->f_vtype == AVT_UINT){
				if(ival < 0){
					LOG_WARN("flag %s has type VT_UINT, but value (%d) < 0, using 0 instead", fp->f_name, ival);
					ival = 0;
				}
			}else if(fp->f_vtype  == AVT_PINT){
				if(ival < 1){
					LOG_WARN("flag %s has type VT_PINT, but value (%d) < 1, using r instead", fp->f_name, ival);
					ival = 1;
				}
			}
			fp->f_value.av_value.v_int = ival;
		}
		break;
	case AVT_REAL :
		fp->f_value.av_type = AVT_REAL;
		if(value == NULL || *value == '\0')
			fp->f_value.av_value.v_double = 0.0;
		else
			fp->f_value.av_value.v_double = atof(value);
		break;
	case AVT_STR :
		fp->f_value.av_type = AVT_STR;
		if(value == NULL){
			fp->f_value.av_value.v_str = NULL;
		}else{
			fp->f_value.av_value.v_str = strdup(value);
			if(fp->f_value.av_value.v_str == NULL){
				LOG_ERROR("flag %s: can't strdup value %s", fp->f_name, value);
				err = 1;
				goto CLEAN_UP;
			}
		}
		break;
	default :
		LOG_ERROR("unknown vtype %d", fp->f_vtype);
		err = 1;
		goto CLEAN_UP;
		break;
	}

CLEAN_UP : ;

	return err;
}

static	FLAG_T	*
find_flag_name(const char *name, int n_ftab, FLAG_T *ftab[])
{
	int	i, j, k, cv;
	FLAG_T	*fp;

	for(i = 0, j = n_ftab - 1; i <= j; ){
		k = (i + j) / 2;
		fp = ftab[k];
		if((cv = strcmp(fp->f_name, name)) == 0)
			return fp;
		else if(cv < 0)
			i = k + 1;
		else
			j = k - 1;
	}
	return NULL;
}

void
TJM_print_help_msg(FILE *fp, const ARGS_T *args)
{

	fprintf(fp, "usage: %s", args->a_progname);
	if(args->an_flags > 0)
		fprintf(fp, " flags ");
	if(args->a_max_files != 0){
		if(args->a_min_files == 0)
			fprintf(fp, "[ ");
		if(args->a_max_files == 1)
			fprintf(fp, "file");
		else if(args->a_max_files == 2)
			fprintf(fp, "file-1 file-2");
		else if(args->a_max_files != -1)
			fprintf(fp, "file-1 ... file-%d", args->a_max_files);
		else
			fprintf(fp, "file-1 ...");
		if(args->a_min_files == 0)
			fprintf(fp, " ]");
	}
	fprintf(fp, "\n");
	if(args->an_flags > 0){
		int	i, f;
		int	l_vflags[N_VFLAGS];
		size_t	l_vflag;
		FLAG_T	*p_flag;

		fprintf(fp, "\n");

		// compute the max widths of each field
		// nit l_vflags[f] to the length of the fields's name
		for(f = 0; f < N_VFLAGS; f++){
			switch(f){
			case VF_NAME :
				l_vflags[f] = strlen("Name");
				break;
			case VF_IS_OPT :
				l_vflags[f] = strlen("Opt?");
				break;
			case VF_VKIND :
				l_vflags[f] = strlen("Val?");
				break;
			case VF_VTYPE :
				l_vflags[f] = strlen("Type");
				break;
			case VF_DEFVAL :
				l_vflags[f] = strlen("DefVal?");
				break;
			case VF_DESCR :
				l_vflags[f] = strlen("Descr?");
				break;
			}
		}
		for(i = 0; i < args->an_flags; i++){
			p_flag = &args->a_flags[i];
			if(p_flag->f_vkind == AVK_MSG)
				continue;
			for(f = 0; f < N_VFLAGS; f++){
				switch(f){
				case VF_NAME :
					l_vflag = strlen(p_flag->f_name);
					break;
				case VF_IS_OPT :
					l_vflag = p_flag->f_is_opt ? strlen("Yes") : strlen("No");
					break;
				case VF_VKIND :
					switch(p_flag->f_vkind){
					case AVK_NONE :
						l_vflag = strlen("None");
						break;
					case AVK_OPT :
						l_vflag = strlen("Opt");
						break;
					case AVK_REQ :
						l_vflag = strlen("Req");
						break;
					}
					break;
				case VF_VTYPE :
					switch(p_flag->f_vtype){
					case AVT_BOOL :
						l_vflag = strlen("Bool");
						break;
					case AVT_INT :
						l_vflag = strlen("Int");
						break;
					case AVT_UINT :
						l_vflag = strlen("Int>=0");
						break;
					case AVT_PINT :
						l_vflag = strlen("Int>0");
						break;
					case AVT_REAL :
						l_vflag = strlen("Real");
						break;
					case AVT_STR :
						l_vflag = strlen("Str");
						break;
					}
					break;
				case VF_DEFVAL :
					l_vflag = p_flag->f_defval ? strlen(p_flag->f_defval) : strlen("NULL");
					break;
				case VF_DESCR :
					l_vflag = p_flag->f_descr ? strlen(p_flag->f_descr) : strlen("NULL");
					break;
				}
				if(l_vflag > l_vflags[f])
					l_vflags[f] = l_vflag;
			}
		}

		// print the flag field names
		fprintf(fp, "Flags\t");
		for(f = 0; f < N_VFLAGS; f++){
			const char 	*str;

			fprintf(fp, "  ");
			switch(f){
			case VF_NAME :
				str = "Name";
				break;
			case VF_IS_OPT :
				str = "Opt?";
				break;
			case VF_VKIND :
				str = "Val?";
				break;
			case VF_VTYPE :
				str = "Type";
				break;
			case VF_DEFVAL :
				str = "DefVal?";
				break;
			case VF_DESCR :
				str = "Descr";
				break;
			}
			fprintf(fp, "%-*s", l_vflags[f], str);
		}
		fprintf(fp, "\n");

		// print the flags
		for(i = 0; i < args->an_flags; i++){
			p_flag = &args->a_flags[i];
			if(p_flag->f_vkind == AVK_MSG){
				fprintf(fp, "\n\t%s\n\n", p_flag->f_descr);
				continue;
			}
			fprintf(fp, "\t");
			for(f = 0; f < N_VFLAGS; f++){
				const char	*str;

				fprintf(fp, "  ");
				switch(f){
				case VF_NAME :
					str = p_flag->f_name;
					break;
				case VF_IS_OPT :
					str = p_flag->f_is_opt ? "Yes" : "No";
					break;
				case VF_VKIND :
					switch(p_flag->f_vkind){
					case AVK_NONE :
						str = "None";
						break;
					case AVK_OPT :
						str = "Opt";
						break;
					case AVK_REQ :
						str = "Req";
						break;
					}
					break;
				case VF_VTYPE :
					switch(p_flag->f_vtype){
					case AVT_BOOL :
						str = "Bool";
						break;
					case AVT_INT :
						str = "Int";
						break;
					case AVT_UINT :
						str = "Int>=0";
						break;
					case AVT_PINT :
						str = "Int>0";
						break;
					case AVT_REAL :
						str = "Real";
						break;
					case AVT_STR :
						str = "Str";
						break;
					}
					break;
				case VF_DEFVAL :
					str = p_flag->f_defval ? p_flag->f_defval : "NULL";
					break;
				case VF_DESCR :
					str = p_flag->f_descr ? p_flag->f_descr : "NULL";
					break;
				}
				fprintf(fp, "%-*s", l_vflags[f], str);
			}
			fprintf(fp, "\n");
		}
	}
}
