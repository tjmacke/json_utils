GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always)

INCS=	-I../include

CC=	cc
CFLAGS=	-g $(INCS) -DVERSION=\"$(GIT_VERSION)\"

LEX=flex
YACC=bison -y

LIBS=	../lib/libjansson.a -lpthread

PROGRAMS=	\
	json_fmt	\
	json_get	#\
#	jg2

all:	$(PROGRAMS)

json_fmt:	json_fmt.o args.o log.o
	$(CC) $(CFLAGS) -o json_fmt json_fmt.o args.o log.o

json_get:	json_get.o parse_glist.o exec_glist.o index_set.o jg_result.o args.o log.o
	$(CC) $(CFLAGS) $(INCS) -o json_get json_get.o parse_glist.o exec_glist.o index_set.o jg_result.o args.o log.o $(LIBS)

jg2:	jg2.o jg2_gram.o args.o log.o
	$(CC) $(CFLAGS) -o jg2 jg2.o jg2_gram.o args.o log.o

json_get.o:	json_get.h parse_glist.h exec_glist.h args.h log.h

parse_glist.o:	parse_glist.h json_get.h log.h

exec_glist.o:	exec_glist.h jg_result.h json_get.h log.h

index_set.o:	index_set.h log.h

jg_result.o:	jg_result.h log.h

jg2_gram.h:	jg2_gram.y
	$(YACC) -d -o jg2_gram.c jg2_gram.y

jg2_lex.c:	jg2_lex.l
	$(LEX) -o jg2_lex.c jg2_lex.l

jg2_gram.o: jg2_gram.h jg2_lex.c

args.o:	args.h log.h

log.o:	log.h

install:	all
	mkdir -p ../bin
	cp $(PROGRAMS) ../bin
	rm -f *.o core core.* junk.* jg2_lex.c jg2_gram.h jg2_gram.c

clean:
	rm -f $(PROGRAMS) *.o core core.* junk.* jg2_lex.c jg2_gram.h jg2_gram.c
