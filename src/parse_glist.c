#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "json_get.h"
#include "parse_glist.h"

#define	TOK_EOF		0
#define	TOK_IDENT	1
#define	TOK_UINT	2
#define	TOK_STRING	3
#define	TOK_LCURLY	4
#define	TOK_RCURLY	5
#define	TOK_LBRACK	6
#define	TOK_RBRACK	7
#define	TOK_LPAREN	8
#define	TOK_RPAREN	9
#define	TOK_COMMA	10
#define	TOK_COLON	11
#define	TOK_DOLLAR	12
#define	TOK_MINUS	13
#define	TOK_STAR	14
#define	TOK_ATSIGN	15
#define	TOK_EQUAL	16
#define	TOK_SEMI	17
#define	TOK_INDEX	18	// @index?  Use @ as the strop
#define	TOK_LIST	19
#define	TOK_ERROR	20

typedef	struct	token_t	{
	int	t_tok;
	char	*t_text;
} TOKEN_T;

static	TOKEN_T	*
token_new(void);

static	void
token_get(const char **, TOKEN_T *);

static	void
token_delete(TOKEN_T *);

typedef	struct	node_t	{
	int	n_sym;
	VALUE_T	*n_value;
	struct node_t	*n_next;
} NODE_T;

static	NODE_T	*
node_new(int, VALUE_T *);

static	void
node_delete(NODE_T *);

static	void
node_delete_list(NODE_T *);

static	void
node_dump(FILE *, NODE_T *, int);

static	void
node_list_dump(FILE *, NODE_T *, int);

static	VALUE_T	*
value_new(int, const SLICE_T *, const char *, NODE_T *);

static	void
mk_indent(FILE*, int);

static	int
index_set(const char **, TOKEN_T *, INDEX_SET_T **);

static	int
index_spec(const char **, TOKEN_T *, INDEX_SPEC_T **);

static	int
selector_list(const char **, TOKEN_T *, NODE_T **);

static	int
selector(const char **, TOKEN_T *, NODE_T **);

static	int
obj_selector(const char **, TOKEN_T *, NODE_T **);

static	int
key_list(const char **, TOKEN_T *, NODE_T **);

static	int
ary_selector(const char **, TOKEN_T *, NODE_T **);

static	int
ary_index(const char **, TOKEN_T *, NODE_T **, SLICE_T *, int *, char []);

static	int
ary_elt(const char **, TOKEN_T *, int *);

static	ATTR_T	attrs[] = {
	{"index", VA_SELECTOR},
	{"key", VA_SELECTOR},
	{"size", VA_SIZE},
	{"type", VA_TYPE},
	{"value", VA_VALUE}
};
static	int	n_attrs = sizeof(attrs)/sizeof(attrs[0]);

static	int
get_attrs(const char **, TOKEN_T *, int *, char []);

static	const ATTR_T	*
find_attr(const char *, int, ATTR_T []);

static	int
count_arys(const VALUE_T *, int *);

int
JG_parse_glist(const char *glist, INDEX_SET_T **isp, VALUE_T **vp_glist, int *n_arys)
{
	const char	*sp;
	TOKEN_T	*token = NULL;
	NODE_T	*np = NULL;
	NODE_T	*np1;
	int	n_nodes = 0;
	VALUE_T	*vp = NULL;
	NODE_T	*np_first = NULL;
	NODE_T	*np_next = NULL;
	NODE_T	*np_last;
	int	err = 0;

	*vp_glist = NULL;
	*n_arys = 0;

	token = token_new();
	if(token == NULL){
		LOG_ERROR("token_new() failed");
		err = 1;
		goto CLEAN_UP;
	}

	sp = glist;
	token_get(&sp, token);
	if(token->t_tok == TOK_ATSIGN){
		token_get(&sp, token);
		if(index_set(&sp, token, isp)){
			err = 1;
			goto CLEAN_UP;
		}
	}

LOG_DEBUG("token->t_tok = %d", token->t_tok);

	if(selector_list(&sp, token, &np)){
		err = 1;
		goto CLEAN_UP;
	}
	vp = value_new(VT_VALS, NULL, NULL, np);
	if(vp == NULL){
		LOG_ERROR("value_new failed for glist %d", n_nodes + 1);
		err = 1;
		goto CLEAN_UP;
	}
	node_delete_list(np);
	np = NULL;
	np_first = np_last = node_new(TOK_LIST, vp);
	if(np_first == NULL){
		LOG_ERROR("node_new failed for glist %d", n_nodes + 1);
		err = 1;
		goto CLEAN_UP;
	}
	vp = NULL;
	n_nodes++;
	
	for( ; token->t_tok == TOK_COMMA; np_last = np_next){
		token_get(&sp, token);
		if(selector_list(&sp, token, &np)){
			err = 1;
			goto CLEAN_UP;
		}
		vp = value_new(VT_VALS, NULL, NULL, np);
		if(vp == NULL){
			LOG_ERROR("value_new failed for glist %d", n_nodes + 1);
			err = 1;
			goto CLEAN_UP;
		}
		node_delete_list(np);
		np = NULL;
		np_next = node_new(TOK_LIST, vp);
		if(np_next == NULL){
			LOG_ERROR("node_new failed for glist %d", n_nodes + 1);
			err = 1;
			goto CLEAN_UP;
		}
		vp = NULL;
		n_nodes++;

		np_last->n_next = np_next;
	}

	*vp_glist = value_new(VT_VLIST, NULL, NULL, np_first);
	if(*vp_glist == NULL){
		LOG_ERROR("value_new failed for all selectors");
		err = 1;
		goto CLEAN_UP;
	}

	if(count_arys(*vp_glist, n_arys)){
		LOG_ERROR("count_arys failed");
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	if(vp != NULL)
		JG_value_delete(vp);

	node_delete_list(np);
	node_delete_list(np_first);

	if(token != NULL)
		token_delete(token);

	return err;
}

static	int
index_set(const char **sp, TOKEN_T *tp, INDEX_SET_T **iset)
{
	INDEX_SPEC_T	*f_ispec = NULL;
	INDEX_SPEC_T	*l_ispec, *n_ispec;
	INDEX_SPEC_T	*ispec = NULL;
	int	err = 0;

	*iset = NULL;

	// "indexset" "(" "v1" "=" idx [ ";" "vi" "=" idx ] ")"
	if(tp->t_tok != TOK_IDENT){
		err = 1;
		goto CLEAN_UP;
	}else if(strcmp(tp->t_text, "indexset")){
		err = 1;
		goto CLEAN_UP;
	}
	token_get(sp, tp);
	if(tp->t_tok != TOK_LPAREN){
		err = 1;
		goto CLEAN_UP;
	}else{
		token_get(sp, tp);
		for( ; tp->t_tok == TOK_IDENT; ){
			if(index_spec(sp, tp, &ispec)){
				err = 1;
				goto CLEAN_UP;
			}
			if(f_ispec == NULL)
				f_ispec = l_ispec = ispec;
			else
				l_ispec->i_next = ispec;
			if(tp->t_tok == TOK_SEMI)
				token_get(sp, tp);
			else
				break;
		}
		if(tp->t_tok != TOK_RPAREN){
			err = 1;
			goto CLEAN_UP;
		}
		token_get(sp, tp);
		*iset = IS_new_iset(f_ispec);
		if(*iset == NULL){
			err = 1;
			goto CLEAN_UP;
		}
	}

CLEAN_UP : ;

	if(err){
		for(ispec = f_ispec; ispec; ispec = n_ispec){
			n_ispec = ispec->i_next;
			IS_delete_ispec(ispec);
		}
		if(*iset != NULL){
			IS_delete_iset(*iset);
			*iset = NULL;
		}
	}

	return err;
}

static	int
index_spec(const char **sp, TOKEN_T *tp, INDEX_SPEC_T **ispec)
{
	NODE_T	*np_first = NULL;
	SLICE_T	slice;
	int	n_attr;
	char	attr[N_VATTRS];
	int	err = 0;

	*ispec = IS_new_ispec();
	if(*ispec == NULL){
		LOG_ERROR("can't allocate ispec");
		err = 1;
		goto CLEAN_UP;
	}
	(*ispec)->i_vname = strdup(tp->t_text);
	if((*ispec)->i_vname == NULL){
		LOG_ERROR("can't strdup tp->t_text");
		err = 1;
		goto CLEAN_UP;
	}
	token_get(sp, tp);
	if(tp->t_tok != TOK_EQUAL){
		err = 1;
		goto CLEAN_UP;
	}
	token_get(sp, tp);
	if(tp->t_tok == TOK_STAR){
		(*ispec)->i_is_hashed = 1;
		token_get(sp, tp);
	}else{
		if(ary_index(sp, tp, &np_first, &slice, &n_attr, attr)){
			err = 1;
			goto CLEAN_UP;
		}
		(*ispec)->i_begin = slice.s_begin;
		(*ispec)->i_end = slice.s_end;
		(*ispec)->i_incr = slice.s_incr;
	}

CLEAN_UP : ;

	if(err){
		IS_delete_ispec(*ispec);
		*ispec = NULL;
	}

	return err;
}

static	int
selector_list(const char **sp, TOKEN_T *tp, NODE_T **np)
{
	NODE_T	*np_first = NULL;
	NODE_T	*np_next = NULL;
	NODE_T	*np_last = NULL;
	int	err = 0;

	*np = NULL;

	for(np_last = np_first = NULL; tp->t_tok == TOK_LCURLY || tp->t_tok == TOK_LBRACK; ){
		if(selector(sp, tp, &np_next)){
			err = 1;
			goto CLEAN_UP;
		}
		if(np_first == NULL)
			np_last = np_first = np_next;
		else{
			np_last->n_next = np_next;
			np_last = np_next;
		}
	}

CLEAN_UP : ;

	if(err)
		node_delete_list(np_first);
	else
		*np = np_first;

	return err;
}

static	int
selector(const char **sp, TOKEN_T *tp, NODE_T **np)
{
	NODE_T	*np_new = NULL;
	char	attr[N_VATTRS]; 
	int	n_attr;
	int	err = 0;

	*np = NULL;

	if(tp->t_tok == TOK_LCURLY){
		token_get(sp, tp);
		if(obj_selector(sp, tp, &np_new)){
			err = 1;
			goto CLEAN_UP;
		}
		if(tp->t_tok == TOK_RCURLY){
			token_get(sp, tp);
		}else{
			err = 1;
			goto CLEAN_UP;
		}
	}else if(tp->t_tok == TOK_LBRACK){
		token_get(sp, tp);
		if(ary_selector(sp, tp, &np_new)){
			err = 1;
			goto CLEAN_UP;
		}
		if(tp->t_tok == TOK_RBRACK){
			token_get(sp, tp);
		}else{
			err = 1;
			goto CLEAN_UP;
		}
	}else{
		err = 1;
		goto CLEAN_UP;
	}
	if(tp->t_tok == TOK_ATSIGN){
		token_get(sp, tp);
		if(get_attrs(sp, tp, &n_attr, attr)){
			err = 1;
			goto CLEAN_UP;
		}
		memcpy(np_new->n_value->v_attr, attr, n_attr);
		np_new->n_value->vn_attr = n_attr;
	}else{
		np_new->n_value->v_attr[0] = VA_VALUE;
		np_new->n_value->vn_attr = 1;
	}

CLEAN_UP : ;

	if(err){
		if(np_new != NULL)
			node_delete(np_new);
	}else
		*np = np_new;

	return err;
}

static	int
obj_selector(const char **sp, TOKEN_T *tp, NODE_T **np)
{
	NODE_T	*np_first = NULL;
	VALUE_T	*vp = NULL;
	int	err = 0;

	*np = NULL;

	if(tp->t_tok == TOK_IDENT || tp->t_tok == TOK_STRING){
		if(key_list(sp, tp, &np_first)){
			LOG_ERROR("key_list failed");
			err = 1;
			goto CLEAN_UP;
		}
	}else if(tp->t_tok == TOK_STAR){
		vp = value_new(VT_STAR, NULL, tp->t_text, NULL);
		if(vp == NULL){
			LOG_ERROR("value_new failed for key %s", tp->t_text);
			err = 1;
			goto CLEAN_UP;
		}
		np_first = node_new(tp->t_tok, vp);
		if(np_first == NULL){
			LOG_ERROR("node_new failed for key %s", tp->t_text);
			err = 1;
			goto CLEAN_UP;
		}
		vp = NULL;
		token_get(sp, tp);
	}else{
		err = 1;
		goto CLEAN_UP;
	}

	vp = value_new(VT_OBJ, NULL, NULL, np_first);
	if(vp == NULL){
		LOG_ERROR("value_new failed for np_first");
		err = 1;
		goto CLEAN_UP;
	};
	*np = node_new(TOK_LCURLY, vp);
	if(*np == NULL){
		LOG_ERROR("node_new failed for return value");
		err = 1;
		goto CLEAN_UP;
	}
	vp = NULL;

CLEAN_UP : ;

	if(vp != NULL)
		JG_value_delete(vp);
	if(np_first != NULL)
		node_delete_list(np_first);

	return err;
}

static	int
key_list(const char **sp, TOKEN_T *tp, NODE_T **np_first)
{
	NODE_T	*np_last = NULL;;
	NODE_T	*np_next = NULL;
	VALUE_T	*vp = NULL;
	char	attr[N_VATTRS];
	int	n_attr;
	int	err = 0;

	*np_first = NULL;

	vp = value_new(VT_KEY, NULL, tp->t_text, NULL);
	if(vp == NULL){
		LOG_ERROR("value_new failed for key %s", tp->t_text);
		err = 1;
		goto CLEAN_UP;
	}
	*np_first = node_new(tp->t_tok, vp);
	if(*np_first == NULL){
		LOG_ERROR("node_new failed for key %s", tp->t_text);
		err = 1;
		goto CLEAN_UP;
	}
	vp = NULL;
	token_get(sp, tp);
	if(tp->t_tok == TOK_ATSIGN){
		token_get(sp, tp);
		if(get_attrs(sp, tp, &n_attr, attr)){
			err = 1;
			goto CLEAN_UP;
		}
		memcpy((*np_first)->n_value->v_attr, attr, n_attr);
		(*np_first)->n_value->vn_attr = n_attr;
	}
	for(np_last = *np_first; tp->t_tok == TOK_COMMA; np_last = np_next){
		token_get(sp, tp);
		if(tp->t_tok == TOK_IDENT || tp->t_tok == TOK_STRING){
			vp = value_new(VT_KEY, NULL, tp->t_text, NULL);
			if(vp == NULL){
				LOG_ERROR("value_new failed for key %s", tp->t_text);
				err = 1;
				goto CLEAN_UP;
			}
			np_next = node_new(tp->t_tok, vp);
			if(np_next == NULL){
				LOG_ERROR("node_new failed for key %s", tp->t_text);
				err = 1;
				goto CLEAN_UP;
			}
			vp = NULL;
			np_last->n_next = np_next;
			token_get(sp, tp);
			if(tp->t_tok == TOK_ATSIGN){
				token_get(sp, tp);
				if(get_attrs(sp, tp, &n_attr, attr)){
					err = 1;
					goto CLEAN_UP;
				}
				memcpy(np_next->n_value->v_attr, attr, n_attr);
				np_next->n_value->vn_attr = n_attr;
			}
		}else{
			err = 1;
			goto CLEAN_UP;
		}
	}

CLEAN_UP : ;

	if(vp != NULL)
		JG_value_delete(vp);

	if(err){
		if(*np_first != NULL){
			node_delete_list(*np_first);
			*np_first = NULL;
		}
	}

	return err;
}

static	int
ary_selector(const char **sp, TOKEN_T *tp, NODE_T **np)
{
	NODE_T	*np_first = NULL;
	NODE_T	*np_last = NULL;
	NODE_T	*np_next = NULL;
	int	n_nodes = 0;
	VALUE_T	*vp = NULL;
	SLICE_T	slice;
	char	attr[N_VATTRS];
	int	n_attr;
	int	err = 0;

	*np = NULL;

	if(tp->t_tok == TOK_IDENT || tp->t_tok == TOK_STRING){
		if(key_list(sp, tp, &np_first)){
			LOG_ERROR("key_list failed");
			err = 1;
			goto CLEAN_UP;
		}
	}else if(tp->t_tok == TOK_STAR){
		vp = value_new(VT_STAR, NULL, tp->t_text, NULL);
		if(vp == NULL){
			LOG_ERROR("value_new failed for key %s", tp->t_text);
			err = 1;
			goto CLEAN_UP;
		}
		np_first = np_last = node_new(tp->t_tok, vp);
		if(np_first == NULL){
			LOG_ERROR("node_new failed for key %s", tp->t_text);
			err = 1;
			goto CLEAN_UP;
		}
		vp = NULL;
		n_nodes++;
		token_get(sp, tp);
	}else{
		if(ary_index(sp, tp, &np_first, &slice, &n_attr, attr)){
			err = 1;
			goto CLEAN_UP;
		}
		vp = value_new(VT_SLICE, &slice, NULL, NULL);
		if(vp == NULL){
			LOG_ERROR("value_new failed for slice %d:%d", slice.s_begin, slice.s_end);
			err = 1;
			goto CLEAN_UP;
		}
		memcpy(vp->v_attr, attr, n_attr);
		vp->vn_attr = n_attr;
		np_first = node_new(TOK_LBRACK, vp);
		if(np_first == NULL){
			LOG_ERROR("node_new failed for slice %d:%d", slice.s_begin, slice.s_end);
			err = 1;
			goto CLEAN_UP;
		}
		vp = NULL;
		n_nodes++;
		for(np_last = np_first; tp->t_tok == TOK_COMMA; np_last = np_next){
			token_get(sp, tp);
			if(ary_index(sp, tp, &np_next, &slice, &n_attr, attr)){
				err = 1;
				goto CLEAN_UP;
			}
			vp = value_new(VT_SLICE, &slice, NULL, NULL);
			if(vp == NULL){
				LOG_ERROR("value_new failed for slice %d:%d", slice.s_begin, slice.s_end);
				err = 1;
				goto CLEAN_UP;
			}
			memcpy(vp->v_attr, attr, n_attr);
			vp->vn_attr = n_attr;
			np_next = node_new(TOK_LBRACK, vp);
			if(np_next == NULL){
				LOG_ERROR("node_new failed for slice %d:%d", slice.s_begin, slice.s_end);
				err = 1;
				goto CLEAN_UP;
			}
			vp = NULL;
			n_nodes++;
			np_last->n_next = np_next;
		}
	}

	vp = value_new(VT_ARY, NULL, NULL, np_first);
	if(vp == NULL){
		LOG_ERROR("value_new failed for np_first");
		err = 1;
		goto CLEAN_UP;
	}
	*np = node_new(TOK_LBRACK, vp);
	if(*np == NULL){
		LOG_ERROR("node_new failed for return value");
		err = 1;
		goto CLEAN_UP;
	}
	vp = NULL;

CLEAN_UP : ;

	if(vp != NULL)
		JG_value_delete(vp);
	if(np_first != NULL)
		node_delete_list(np_first);

	return err;
}

static	int
ary_index(const char **sp, TOKEN_T *tp, NODE_T **np, SLICE_T *slp, int *n_attr, char attr[])
{
	int	ival;
	int	err = 0;

	*np = NULL;
	memset(slp, 0, sizeof(SLICE_T));
	*n_attr = 0;

	if(ary_elt(sp, tp, &ival)){
		err = 1;
		goto CLEAN_UP;
	}
	slp->s_begin = slp->s_end = ival;
	if(tp->t_tok == TOK_COLON){
		token_get(sp, tp);
		if(ary_elt(sp, tp, &ival)){
			err = 1;
			goto CLEAN_UP;
		}
		slp->s_end = ival;
		if(tp->t_tok == TOK_COLON){
			int	is_neg = 0;
			token_get(sp, tp);
			if(tp->t_tok == TOK_MINUS){
				is_neg = 1;
				token_get(sp, tp);
			}
			if(tp->t_tok == TOK_UINT){
				ival = atoi(tp->t_text);
				if(ival == 0){
					LOG_ERROR("slice increment can not be 0");
					err = 1; 
					goto CLEAN_UP;
				}
				slp->s_incr = is_neg ? -ival : ival;
				token_get(sp, tp);
			}else{
				err = 1;
				goto CLEAN_UP;
			}
		}
	}
	if(tp->t_tok == TOK_ATSIGN){
		token_get(sp, tp);
		if(get_attrs(sp, tp, n_attr, attr)){
			err = 1;
			goto CLEAN_UP;
		}
	}

CLEAN_UP : ;

	return err;
}

static	int
ary_elt(const char **sp, TOKEN_T *tp, int *ival)
{
	int	err = 0;

	*ival = 0;

	if(tp->t_tok == TOK_UINT){
		if((*ival = atoi(tp->t_text)) <= 0){
			LOG_ERROR("bad array index %d, must be > 0", *ival);
			err = 1;
			goto CLEAN_UP;
		}
		token_get(sp, tp);
	}else if(tp->t_tok == TOK_DOLLAR){
		token_get(sp, tp);
		if(tp->t_tok == TOK_MINUS){
			token_get(sp, tp);
			if(tp->t_tok != TOK_UINT){
				err = 1;
				goto CLEAN_UP;
			}else{
				*ival  = atoi(tp->t_text);
				if(*ival < 0){
					LOG_ERROR("bad array offset %d, must be >= 0", *ival);
					err = 1;
					goto CLEAN_UP;
				}
				*ival = -(*ival);	// $ - i is i from end, so use -i
				token_get(sp, tp);
			}
		}else
			*ival = 0;	// meaning last element
	}

CLEAN_UP : ;

	if(err)
		*ival = 0;

	return err;
}

static	int
get_attrs(const char **sp, TOKEN_T *tp, int *n_attr, char *attr)
{
	const	ATTR_T	*ap;
	int	h_value = 0;
	int	err = 0;

	memset(attr, 0, N_VATTRS);
	*n_attr = 0;

	// Valid attributes are index, key, size, type, value
	// index and key have value VA_SELECTOR
	// At least one attribute is requred, so  @() is invalid
	// if value is present, it must be the last attribute

	if(tp->t_tok == TOK_LPAREN){
		token_get(sp, tp);
		while(tp->t_tok == TOK_IDENT){
			ap = find_attr(tp->t_text, n_attrs, attrs);
			if(ap == NULL){
				err = 1;
				goto CLEAN_UP;
			}
			if(*n_attr < N_VATTRS)
				attr[*n_attr] = ap->a_value;
			(*n_attr)++;
			if(ap->a_value == VA_VALUE)
				h_value = 1;
			token_get(sp, tp);
			if(tp->t_tok == TOK_COMMA){
				token_get(sp, tp);
			}else
				break;
		}
		if(tp->t_tok == TOK_RPAREN){
			if(*n_attr < 1){
				err = 1;
				goto CLEAN_UP;
			}else if(*n_attr > N_VATTRS){
				err = 1;
				goto CLEAN_UP;
			}else if(h_value){
				if(attr[*n_attr - 1] != VA_VALUE){
					err = 1;
					goto CLEAN_UP;
				}
			}
			token_get(sp, tp);
		}else{
			err = 1;
			goto CLEAN_UP;
		}
	}else{
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	if(err){
		memset(attr, 0, N_VATTRS);
		*n_attr = 0;
	}

	return err;
}

static	const ATTR_T	*
find_attr(const char *aname, int n_atab, ATTR_T atab[])
{
	int	i, j, k, cv;
	const ATTR_T	*ap;

	for(i = 0, j = n_atab - 1; i <= j; ){
		k = (i + j) / 2;
		ap = &atab[k];
		if((cv = strcmp(ap->a_name, aname)) == 0)
			return ap;
		else if(cv < 0)
			i = k + 1;
		else
			j = k - 1;
	}
	return NULL;
}

static	TOKEN_T	*
token_new(void)
{
	TOKEN_T	*token = NULL;
	int	err = 0;

	token = (TOKEN_T *)calloc((size_t)1, sizeof(TOKEN_T));
	if(token == NULL){
		LOG_ERROR("can't allocate token");
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	if(err){
		if(token != NULL){
			token_delete(token);
			token = NULL;
		}
	}

	return token;
}

static	void
token_delete(TOKEN_T *token)
{

	if(token == NULL)
		return;

	if(token->t_text != NULL)
		free(token->t_text);

	free(token);
}

static	void
token_get(const char **str, TOKEN_T *tp)
{
	const char	*t_start, *t_end;
	
	for(t_start = *str; isspace(*t_start); t_start++)
		;
	if(*t_start == '\0'){
		tp->t_tok = TOK_EOF;
		*tp->t_text = '\0';
		*str = t_start;
		return;
	}

	if(isalpha(*t_start)){
		tp->t_tok = TOK_IDENT;
		for(t_end = t_start + 1; isalnum(*t_end) || *t_end == '_'; t_end++)
			;
		if(tp->t_text != NULL)
			free(tp->t_text);
		tp->t_text  = strndup(t_start, t_end - t_start);
	}else if(isdigit(*t_start)){
		tp->t_tok = TOK_UINT;
		for(t_end = t_start + 1; isdigit(*t_end); t_end++)
			;
		if(tp->t_text != NULL)
			free(tp->t_text);
		tp->t_text  = strndup(t_start, t_end - t_start);
	}else if(*t_start == '"'){
		tp->t_tok = TOK_STRING;
		for(t_end = ++t_start; *t_end; t_end++){
			if(*t_end == '"')
				break;
			else if(*t_end == '\\')
				t_end++;
		}
		if(*t_end != '"'){
			LOG_ERROR("unterminated string");
			tp->t_tok = TOK_ERROR;
			return;
		}else{
			char	*sp, *dp;

			if(tp->t_text != NULL)
				free(tp->t_text);
			tp->t_text  = strndup(t_start, t_end - t_start);
			for(dp = sp = tp->t_text; *sp; sp++){
				// process the escapes
				if(*sp == '\\'){
					sp++;
					switch(*sp){
					case 'b' :
						*dp++ = '\b';
						break;
					case 'f' :
						*dp++ = '\f';
						break;
					case 'n' :
						*dp++ = '\n';
						break;
					case 'r' :
						*dp++ = '\r';
						break;
					case 't' :
						*dp++ = '\t';
						break;
					case 'u' :	// TODO: deal with unicode literal
						*dp++ = '\\';
						*dp++ = 'u';
						break;
					case '"' :
					case '\\' :
					case '/' :
					default :
						*dp++ = *sp;
						break;
					}
				}else
					*dp++ = *sp;
			}
			*dp = '\0';
			t_end++;
		}
	}else{ 
		switch(*t_start){
		case '{' :
			tp->t_tok = TOK_LCURLY;
			break;
		case '}' :
			tp->t_tok = TOK_RCURLY;
			break;
		case '[' :
			tp->t_tok = TOK_LBRACK;
			break;
		case ']' :
			tp->t_tok = TOK_RBRACK;
			break;
		case '(' :
			tp->t_tok = TOK_LPAREN;
			break;
		case ')' :
			tp->t_tok = TOK_RPAREN;
			break;
		case ',' :
			tp->t_tok = TOK_COMMA;
			break;
		case ':' :
			tp->t_tok = TOK_COLON;
			break;
		case '$' :
			tp->t_tok = TOK_DOLLAR;
			break;
		case '-' :
			tp->t_tok = TOK_MINUS;
			break;
		case '*' :
			tp->t_tok = TOK_STAR;
			break;
		case '@' :
			tp->t_tok = TOK_ATSIGN;
			break;
		case '=' :
			tp->t_tok = TOK_EQUAL;
			break;
		case ';' :
			tp->t_tok = TOK_SEMI;
			break;
		default :
			tp->t_tok = TOK_ERROR;
			break;
		}
		t_end = t_start + 1;
		if(tp->t_text != NULL)
			free(tp->t_text);
		tp->t_text = strndup(t_start, t_end - t_start);
	}
	*str = t_end;
}

static	NODE_T	*
node_new(int n_sym, VALUE_T *n_value)
{
	NODE_T	*np = NULL;
	int	err = 0;

	np = (NODE_T *)calloc((size_t)1, sizeof(NODE_T));
	if(np == NULL){
		LOG_ERROR("can't allocate np");
		err = 1;
		goto CLEAN_UP;
	}
	np->n_sym = n_sym;
	np->n_value = n_value;
	np->n_next = NULL;

CLEAN_UP : ;

	if(err){
		if(np != NULL){
			node_delete(np);
			np = NULL;
		}
	}

	return np;
}

static	void
node_delete(NODE_T *np)
{

	if(np == NULL)
		return;

	if(np->n_value != NULL)
		JG_value_delete(np->n_value);
	free(np);
}

static	void
node_delete_list(NODE_T *np)
{
	NODE_T	*np1, *np_next;

	if(np == NULL)
		return;

	for(np1 = np; np1; np1 = np_next){
		np_next = np1->n_next;
		node_delete(np1);
	}
}

static	void
node_dump(FILE *fp, NODE_T *np, int ilev)
{
	int	i;

	if(np == NULL){
		mk_indent(fp, ilev);
		fprintf(fp, "node = NULL\n");
		return;
	}

	mk_indent(fp, ilev);
	fprintf(fp, "node = %p {\n", np);
	mk_indent(fp, ilev);
	fprintf(fp, "  sym   = %d\n", np->n_sym);
	JG_value_dump(fp, np->n_value, ilev + 1);
	if(np->n_next == NULL){
		mk_indent(fp, ilev);
		fprintf(fp, "  next = NULL\n");
	}else{
		mk_indent(fp, ilev);
		fprintf(fp, "  next = %p\n", np->n_next);
	}
	mk_indent(fp, ilev);
	fprintf(fp, "}\n");
}

static	void
node_list_dump(FILE *fp, NODE_T *np, int ilev)
{
	NODE_T	*np1;

	if(np == NULL){
		mk_indent(fp, ilev);
		fprintf(fp, "nlist = NULL\n");
		return;
	}

	mk_indent(fp, ilev);
	fprintf(fp, "nlist = %p {\n", np);
	for(np1 = np; np1; np1 = np1->n_next){
		node_dump(fp, np1, ilev + 1);
	}
	mk_indent(fp, ilev);
	fprintf(fp, "}\n");
}

static	VALUE_T	*
value_new(int type, const SLICE_T *sp, const char *str, NODE_T *nodes)
{
	VALUE_T	*vp = NULL;
	VTAB_T	*vtab = NULL;
	int	err = 0;

	vp = (VALUE_T *)calloc((size_t)1, sizeof(VALUE_T));
	if(vp == NULL){
		err = 1;
		goto CLEAN_UP;
	}
	vp->v_type = type;
	if(type == VT_SLICE){
		vp->v_value.v_slice.s_begin = sp->s_begin;
		vp->v_value.v_slice.s_end = sp->s_end;
		vp->v_value.v_slice.s_incr = sp->s_incr;
	}else if(type == VT_KEY){
		vp->v_value.v_key = strdup(str);
		if(vp->v_value.v_key == NULL){
			err = 1;
			goto CLEAN_UP;
		}
	}else if(type == VT_STAR){
		vp->v_value.v_key = strdup("*");
		if(vp->v_value.v_key == NULL){
			err = 1;
			goto CLEAN_UP;
		}
	}else if(type == VT_OBJ || type == VT_ARY || type == VT_VALS || type == VT_VLIST){
		NODE_T	*np;
		int	n_nodes;
		int	i;

		for(n_nodes = 0, np = nodes; np; n_nodes++, np = np->n_next)
			;
		if(n_nodes == 0)
			goto CLEAN_UP;
		vtab = (VTAB_T *)calloc((size_t)1, sizeof(VTAB_T));
		if(vtab == NULL){
			err = 1;
			goto CLEAN_UP;
		}
		vp->v_value.v_vtab = vtab;
		vtab->v_vtab = (VALUE_T **)calloc((size_t)n_nodes, sizeof(VALUE_T *));
		if(vtab->v_vtab == NULL){
			err = 1;
			goto CLEAN_UP;
		}
		vtab->vn_vtab = n_nodes;
		for(i = 0, np = nodes; np; i++, np = np->n_next){
			vtab->v_vtab[i] = np->n_value;
			np->n_value = NULL;
		}
	}else{
		LOG_ERROR("unknown type %d", type);
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	if(err){
		if(vp != NULL){
			JG_value_delete(vp);
			vp = NULL;
		}
	}

	return vp;
}

void
JG_value_delete(VALUE_T	*vp)
{

	if(vp == NULL)
		return;

	if(vp->v_type == VT_OBJ || vp->v_type == VT_ARY || vp->v_type == VT_VALS || vp->v_type == VT_VLIST ){
		if(vp->v_value.v_vtab != NULL){
			if(vp->v_value.v_vtab->v_vtab != NULL){
				int	i;

				for(i = 0; i < vp->v_value.v_vtab->vn_vtab; i++)
					JG_value_delete(vp->v_value.v_vtab->v_vtab[i]);
				free(vp->v_value.v_vtab->v_vtab);
			}
			free(vp->v_value.v_vtab);
		}
	}else if(vp->v_type == VT_KEY || vp->v_type == VT_STAR){
		if(vp->v_value.v_key != NULL)
			free(vp->v_value.v_key);
	}
	free(vp);
}

void
JG_value_dump(FILE *fp, const VALUE_T *vp, int ilev)
{

	if(vp == NULL){
		mk_indent(fp, ilev);
		fprintf(fp, "value = NULL\n");
	}else{
		int	i;

		mk_indent(fp, ilev);
		fprintf(fp, "value = %p {\n", vp);
		mk_indent(fp, ilev);
		fprintf(fp, "  type = %d\n", vp->v_type);
		mk_indent(fp, ilev);
		fprintf(fp, "  attr = %d {", vp->vn_attr);
		for(i = 0; i < vp->vn_attr; i++)
			fprintf(fp, " %d", vp->v_attr[i]);
		fprintf(fp, "}\n");
		if(vp->v_type == VT_SLICE){
			mk_indent(fp, ilev);
			fprintf(fp, "  low  = ");
			if(vp->v_value.v_slice.s_begin < 0)
				fprintf(fp, "$%d", vp->v_value.v_slice.s_begin);
			else if(vp->v_value.v_slice.s_begin == 0)
				fprintf(fp, "$");
			else
				fprintf(fp, "%d", vp->v_value.v_slice.s_begin);
			fprintf(fp, "\n");
			mk_indent(fp, ilev);
			fprintf(fp, "  high = ");
			if(vp->v_value.v_slice.s_end < 0)
				fprintf(fp, "$%d", vp->v_value.v_slice.s_end);
			else if(vp->v_value.v_slice.s_end == 0)
				fprintf(fp, "$");
			else
				fprintf(fp, "%d", vp->v_value.v_slice.s_end);
			fprintf(fp, "\n");
			mk_indent(fp, ilev);
			fprintf(fp, "  incr = %d (0 means set at runtime)\n", vp->v_value.v_slice.s_incr);
		}else if(vp->v_type == VT_KEY || vp->v_type == VT_STAR){
			mk_indent(fp, ilev);
			fprintf(fp, "  key  = %p, %s\n", vp->v_value.v_key, vp->v_value.v_key ? vp->v_value.v_key : "NULL");
		}else if(vp->v_type == VT_OBJ || vp->v_type == VT_ARY || vp->v_type == VT_VALS || vp->v_type == VT_VLIST){
			if(vp->v_value.v_vtab == NULL){
				mk_indent(fp, ilev);
				fprintf(fp, "  vtab = NULL\n");
			}else{
				int	i;

				mk_indent(fp, ilev);
				fprintf(fp, "  vtab = %p, %d {\n", vp->v_value.v_vtab, vp->v_value.v_vtab->vn_vtab);
				for(i = 0; i < vp->v_value.v_vtab->vn_vtab; i++)
					JG_value_dump(fp, vp->v_value.v_vtab->v_vtab[i], ilev + 2);
				mk_indent(fp, ilev);
				fprintf(fp, "  }\n");
			}
		}else{
			mk_indent(fp, ilev);
			fprintf(fp, "  value = Unknown, bad v_type\n");
		}
		mk_indent(fp, ilev);
		fprintf(fp, "}\n");
	}
}

static	void
mk_indent(FILE *fp, int lev)
{
	int	i;

	for(i = 0; i < lev * ND_INDENT; i++)
		fputc(' ', fp);
}

static	int
count_arys(const VALUE_T *vp_glist, int *n_arys)
{
	int	g, s;
	const VTAB_T	*g_vtab;
	const VALUE_T	*vp_get;
	const VTAB_T	*s_vtab;
	const VALUE_T	*vp_sel;
	int	err = 0;

	*n_arys = 0;
	if(vp_glist == NULL)
		return err;

	g_vtab = vp_glist->v_value.v_vtab;
	for(g = 0; g < g_vtab->vn_vtab; g++){
		vp_get = g_vtab->v_vtab[g];
		s_vtab = vp_get->v_value.v_vtab;
		for(s = 0; s < s_vtab->vn_vtab; s++){
			vp_sel = s_vtab->v_vtab[s];
			if(vp_sel->v_type == VT_ARY)
				(*n_arys)++;
		}
	}

CLEAN_UP : ;

	if(err)
		*n_arys = 0;

	return err;
}
