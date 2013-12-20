%token	TOK_EOF
%token	TOK_IDENT
%token	TOK_INT
%token	TOK_UINT
%token	TOK_STRING
%token	TOK_LCURLY
%token	TOK_RCURLY
%token	TOK_LSQUARE
%token	TOK_RSQUARE
%token	TOK_LPAREN
%token	TOK_RPAREN
%token	TOK_COMMA
%token	TOK_COLON
%token	TOK_DOLLAR
%token	TOK_MINUS
%token	TOK_STAR
%token	TOK_ATSIGN
%token	TOK_EQUAL
%token	TOK_SEMI
%token	TOK_INDEX
%token	TOK_LIST
%token	TOK_ERROR

/*
 * For examples and semantics see jg_gram.notes
 */

%%
get		: index get_list 
		;
index		: TOK_INDEX TOK_LPAREN idx_set_list TOK_RPAREN
		| empty
		;
idx_set_list	: idx_set
		| idx_set_list TOK_SEMI idx_set
		;
idx_set		: TOK_IDENT TOK_EQUAL idx_list
		;
get_list	: sel_list
		| get_list TOK_COMMA sel_list
		;
sel_list	: sel_w_meta
		| sel_list sel_w_meta
		;
sel_w_meta	: sel meta
		;
sel		: obj_sel
		| ary_sel
		;
obj_sel		: TOK_LCURLY key_list TOK_RCURLY
		| TOK_LCURLY idx_list TOK_RCURLY
		| TOK_LCURLY TOK_STAR TOK_RCURLY
		;
ary_sel		: TOK_LSQUARE idx_list TOK_RSQUARE
		| TOK_LSQUARE idx_ref TOK_RSQUARE
		| TOK_LSQUARE key_list TOK_RSQUARE
		| TOK_LSQUARE TOK_STAR TOK_RSQUARE
		;
key_list	: key
		| key_list TOK_COMMA key
		;
key		: TOK_STRING
		| TOK_IDENT
		;
idx_list	: idx
		| idx_list TOK_COMMA idx
		;
idx		: elt
		| elt TOK_COLON elt
		| elt TOK_COLON elt TOK_COLON TOK_INT
		;
elt		: TOK_INT
		| TOK_DOLLAR
		| TOK_DOLLAR TOK_MINUS TOK_UINT
		;
idx_ref		: TOK_ATSIGN TOK_IDENT
		;
meta		: TOK_ATSIGN TOK_LPAREN id_list TOK_RPAREN
		| empty
		;
id_list		: TOK_IDENT
		| id_list TOK_COMMA TOK_IDENT
		;
empty		:
		;
