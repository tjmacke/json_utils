%token	SYM_LCURLY
%token	SYM_RCURLY
%token	SYM_LSQUARE
%token	SYM_RSQUARE
%token	SYM_LPAREN
%token	SYM_RPAREN
%token	SYM_COMMA
%token	SYM_COLON
%token	SYM_DOLLAR
%token	SYM_MINUS
%token	SYM_STAR
%token	SYM_ATSIGN
%token	SYM_EQUAL
%token	SYM_SEMI
%token	SYM_INDEX
%token	SYM_IDENT
%token	SYM_INT
%token	SYM_UINT
%token	SYM_STRING
%token	SYM_ERROR

/*
 * For examples and semantics see jg_gram.notes
 */

%%
get		: index get_list 
		;
index		: SYM_INDEX SYM_LPAREN idx_set_list SYM_RPAREN
		| empty
		;
idx_set_list	: idx_set
		| idx_set_list SYM_SEMI idx_set
		;
idx_set		: SYM_IDENT SYM_EQUAL idx_list
		;
get_list	: sel_list
		| get_list SYM_COMMA sel_list
		;
sel_list	: sel_w_meta
		| sel_list sel_w_meta
		;
sel_w_meta	: sel meta
		;
sel		: obj_sel
		| ary_sel
		;
obj_sel		: SYM_LCURLY key_list SYM_RCURLY
		| SYM_LCURLY idx_list SYM_RCURLY
		| SYM_LCURLY SYM_STAR SYM_RCURLY
		;
ary_sel		: SYM_LSQUARE idx_list SYM_RSQUARE
		| SYM_LSQUARE idx_ref SYM_RSQUARE
		| SYM_LSQUARE key_list SYM_RSQUARE
		| SYM_LSQUARE SYM_STAR SYM_RSQUARE
		;
key_list	: key
		| key_list SYM_COMMA key
		;
key		: SYM_STRING
		| SYM_IDENT
		;
idx_list	: idx
		| idx_list SYM_COMMA idx
		;
idx		: elt
		| elt SYM_COLON elt
		| elt SYM_COLON elt SYM_COLON SYM_INT
		;
elt		: SYM_INT
		| SYM_DOLLAR
		| SYM_DOLLAR SYM_MINUS SYM_UINT
		;
idx_ref		: SYM_ATSIGN SYM_IDENT
		;
meta		: SYM_ATSIGN SYM_LPAREN id_list SYM_RPAREN
		| empty
		;
id_list		: SYM_IDENT
		| id_list SYM_COMMA SYM_IDENT
		;
empty		:
		;
