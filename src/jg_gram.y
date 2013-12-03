%token	SYM_LCURLY
%token	SYM_RCURLY
%token	SYM_LSQUARE
%token	SYM_RSQUARE
%token	SYM_COMMA
%token	SYM_COLON
%token	SYM_DOLLAR
%token	SYM_MINUS
%token	SYM_STAR
%token	SYM_IDENT
%token	SYM_INT
%token	SYM_STRING
%token	SYM_ERROR

/*
 * For examples and semantics see jg_gram.notes
 */

%%
get_list	: sel_list
		| get_list SYM_COMMA sel_list
		;
sel_list	: sel
		| sel_list sel
		;
sel		: obj_sel
		| ary_sel
		;
obj_sel		: SYM_LCURLY key_list SYM_RCURLY
		| SYM_LCURLY SYM_STAR SYM_RCURLY
		;
ary_sel		: SYM_LSQUARE idx_list SYM_RSQUARE
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
		;
elt		: SYM_INT
		| SYM_DOLLAR
		| SYM_DOLLAR SYM_MINUS SYM_INT
		;
