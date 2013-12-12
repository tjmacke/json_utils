#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "log.h"
#include "jg_result.h"

// These 2 functions handle strings w/tabs, newlines, makeing them safe to be fields in tsv files.
// check for tab, newline and count each as 2 chars.
static	size_t
tsv_strlen(const char *);

// check for tab, newline and copy each as \t, \n.
static	char	*
tsv_strcpy(char *, const char *);


JG_RESULT_T	*
JG_result_new(FILE *fp, pthread_mutex_t *fp_mutex, int n_arys)
{
	JG_RESULT_T	*jgr = NULL;
	int	i;
	int	err = 0;

	jgr = (JG_RESULT_T *)calloc((size_t)1, sizeof(JG_RESULT_T));
	if(jgr == NULL){
		err = 1;
		goto CLEAN_UP;
	}
	jgr->j_fp = fp;
	jgr->j_fp_mutex = fp_mutex;
	jgr->j_first = 1;
	jgr->j_bufs = (BUF_T *)calloc((size_t)(n_arys + 1), sizeof(BUF_T));
	if(jgr->j_bufs == NULL){
		err = 1;
		goto CLEAN_UP;
	}
	jgr->jn_bufs = (n_arys + 1);
	for(i = 0; i < jgr->jn_bufs; i++){
		jgr->j_bufs[i].b_buf = (char *)malloc(S_BUF * sizeof(char));
		if(jgr->j_bufs[i].b_buf == NULL){
			err = 1;
			goto CLEAN_UP;
		}
		*jgr->j_bufs[i].b_buf = '\0';
		jgr->j_bufs[i].bs_buf = S_BUF;
		jgr->j_bufs[i].bn_buf = 0;
		jgr->j_bufs[i].b_bp = jgr->j_bufs[i].b_buf;
	}

CLEAN_UP : ;


	if(err){
		JG_result_delete(jgr);
		jgr = NULL;
	}

	return jgr;
}

void
JG_result_delete(JG_RESULT_T *jgr)
{

	if(jgr == NULL)
		return;

	if(jgr->j_bufs != NULL){
		int	i;

		for(i = 0; i < jgr->jn_bufs; i++){
			if(jgr->j_bufs[i].b_buf != NULL)
				free(jgr->j_bufs[i].b_buf);
		}
		free(jgr->j_bufs);
	}
	free(jgr);
}

int
JG_result_add(JG_RESULT_T *jgr, const char *str)
{
	BUF_T	*buf;
	size_t	l_str;
	size_t	need;
	size_t	have;
	int	err = 0;

	// Make sure str will fit, realloc if it won't, return err if realloc fails
	// size is l_str + 1 for first (str + '\0') and l_str + 2 for others ('\t' + str + '\0')
	// tsv_strlen() counts each tab, newline as 2 chars
	l_str = tsv_strlen(str);
	buf = &jgr->j_bufs[jgr->jc_buf];
	l_str = jgr->j_first ? l_str : l_str + 1;	// all but first need an initial '\t'
	need = l_str + 1;				// don't forget the \0
	have = buf->bs_buf - buf->bn_buf;
	if(have < need){
		size_t	n_size;
		char	*tmp = NULL;

		n_size = 1.5 * buf->bs_buf;
		tmp = realloc(buf->b_buf, n_size);
		if(tmp == NULL){
			LOG_ERROR("realloc failed for buf[%d]", jgr->jc_buf);
			err = 1;
			goto CLEAN_UP;
		}
		buf->b_buf = tmp;
		buf->bs_buf = n_size;
		buf->b_bp = &buf->b_buf[buf->bn_buf];
	}
	if(jgr->j_first){
		jgr->j_first = 0;
		// tsv_strcpy() copies tab, newline as \t, \n
		tsv_strcpy(buf->b_bp, str);
		buf->b_bp += l_str;
	}else{
		*buf->b_bp++ = '\t';
		// tsv_strcpy() copies tab, newline as \t, \n
		tsv_strcpy(buf->b_bp, str);
		buf->b_bp += l_str - 1;
	}
	buf->bn_buf += l_str;

CLEAN_UP : ;

	return err;
}

int
JG_result_array_push(JG_RESULT_T *jgr)
{
	int	err = 0;

	if(jgr->jc_buf >= jgr->jn_bufs - 1){
		LOG_ERROR("buf stk overflow, limit = %d", jgr->jn_bufs - 1);
		err = 1;
		goto CLEAN_UP;
	}
	jgr->jc_buf++;
	jgr->j_bufs[jgr->jc_buf].bn_buf = 0;
	jgr->j_bufs[jgr->jc_buf].b_bp = jgr->j_bufs[jgr->jc_buf].b_buf;
	*(jgr->j_bufs[jgr->jc_buf].b_bp) = '\0';

CLEAN_UP:

	return err;
}

void
JG_result_array_init(JG_RESULT_T *jgr)
{
	int	i;
	int	first;

	jgr->j_bufs[jgr->jc_buf].bn_buf = 0;
	jgr->j_bufs[jgr->jc_buf].b_bp = jgr->j_bufs[jgr->jc_buf].b_buf;
	*(jgr->j_bufs[jgr->jc_buf].b_bp) = '\0';
	for(first = 1, i = jgr->jc_buf; i >= 0; i--){
		if(jgr->j_bufs[i].bn_buf != 0){
			first = 0;
			break;
		}
	}
	jgr->j_first = first;
}

int
JG_result_array_pop(JG_RESULT_T *jgr)
{
	int	err = 0;

	if(jgr->jc_buf <= 0){
		LOG_ERROR("buf stk underflow");
		err = 1;
		goto CLEAN_UP;
	}
	jgr->jc_buf--;

CLEAN_UP : ;

	return err;
}

int
JG_result_print(const JG_RESULT_T *jgr)
{
	int	i;
	int	err = 0;

	pthread_mutex_lock(jgr->j_fp_mutex);
	for(i = 0; i < jgr->jn_bufs; i++)
		fputs(jgr->j_bufs[i].b_buf, jgr->j_fp);
	fputc('\n', jgr->j_fp);
	pthread_mutex_unlock(jgr->j_fp_mutex);

	return err;
}

void
JG_result_dump(FILE *fp, const char *msg, const JG_RESULT_T *jgr, int verbose)
{
	int	i;

	if(msg != NULL && *msg != '\0')
		fprintf(fp, "%s\n", msg);

	if(jgr == NULL){
		fprintf(fp, "jgr is NULL\n");
		return;
	}

	fprintf(fp, "result = {\n");
	fprintf(fp, "\tfp     = %p\n", jgr->j_fp);
	fprintf(fp, "\tfirst  = %d\n", jgr->j_first);
	fprintf(fp, "\tn_bufs = %d\n", jgr->jn_bufs);
	fprintf(fp, "\tc_buf  = %d\n", jgr->jc_buf + 1);
	fprintf(fp, "\tbufs   = {\n");
	for(i = 0; i < jgr->jn_bufs; i++){
		fprintf(fp, "\t\tbuf = %d%s {\n", i+1, i == jgr->jc_buf ? "*" : "");
		fprintf(fp, "\t\t\ts_buf = %ld\n", jgr->j_bufs[i].bs_buf);
		fprintf(fp, "\t\t\tn_buf = %ld\n", jgr->j_bufs[i].bn_buf);
		fprintf(fp, "\t\t\tbp    = %p\n", jgr->j_bufs[i].b_bp);
		fprintf(fp, "\t\t\tbuf   = %p", jgr->j_bufs[i].b_buf);
		if(verbose)
			fprintf(fp, " %s", jgr->j_bufs[i].b_buf);
		fprintf(fp, "\n");
		fprintf(fp, "\t\t}\n");
	}
	fprintf(fp, "\t}\n");
	fprintf(fp, "}\n");
}

static	size_t
tsv_strlen(const char *str)
{
	const char	*sp;
	int	c;
	size_t	l_str;

	if(str == NULL){
		// TODO: I think that I'm going to use a NULL src to indicate a missing element and return "\\0"
		LOG_ERROR("str is NULL");
		return 0;
	}

	for(l_str = 0, sp = str; *sp; sp++, l_str++){
		c = *sp & 0xff;
		if(c < 0x20){		// the C0 controls
			switch(*sp){
			case '\b' :
			case '\f' :
			case '\n' :
			case '\r' :
			case '\t' :
				l_str++;
				break;
			default :
				l_str += 5;	// -> \u%04x of *sp
				break;
			}
		}else if(c == 0x7f){	// delete
			l_str += 5;	// -> \u007f
		}else if(c == 0xc2){	// check for C1 controls
			int	c1;

			c1 = sp[1] & 0xff;
			if(c1 >= 0x80 && c1 <= 0x9f){
				l_str += 5;	// -> \u0080 : \u009f
				sp++;
			}
		}
	}
	return l_str;
}

static	char	*
tsv_strcpy(char *dst, const char *src)
{
	char	*dp;
	const char	*sp;
	int	c;

	if(dst == NULL){
		LOG_ERROR("dst is NULL");
		return NULL;
	}else if(src == NULL){
		// TODO: I think that I'm going to use a NULL src to indicate a missing element and return "\\0"
		LOG_ERROR("src is NULL");
		*dst = '\0';
		return dst;
	}

	for(dp = dst, sp = src; *sp; sp++){
		c = *sp & 0xff;
		if(c < 0x20){		// the C0 controls
			switch(*sp){
			case '\b' :
				*dp++ = '\\';
				*dp++ = 'b';
				break;
			case '\f' :
				*dp++ = '\\';
				*dp++ = 'f';
				break;
			case '\n' :
				*dp++ = '\\';
				*dp++ = 'n';
				break;
			case '\r' :
				*dp++ = '\\';
				*dp++ = 'r';
				break;
			case '\t' :
				*dp++ = '\\';
				*dp++ = 'r';
				break;
			default :
				sprintf(dp, "\\u%04x", c);
				dp += 6;
				break;
			}
		}else if(c == 0x7f){	// delete
			strcpy(dp, "\\u007f");
			dp += 6;
		}else if(c == 0xc2){	// check for C1 controls
			int	c1;

			c1 = sp[1] & 0xff;
			if(c1 >= 0x80 && c1 <= 0x9f){	// Got one
				sprintf(dp, "\\u%04x", c1);
				dp += 6;
				sp++;
			}else
				*dp++ = *sp;
		}else
			*dp++ = *sp;
	}
	*dp = '\0';
	return dst;
}
