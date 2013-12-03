%%
[\ \t\f]+		; // skip white space

[a-zA-Z][_a-zA-Z0-9]*	; // an identifier
[0-9]+			; // an integer
\"[^"]*			; // a string. \" doesn't terminate a string

\{			; // LCURLY
\}			; // RCURLY
\[			; // LBRACK
\]			; // RBRACK
\,			; // COMMA
:			; // COLON
\$			; // DOLLAR
\-			; // MINUS
\*			; // STAR
