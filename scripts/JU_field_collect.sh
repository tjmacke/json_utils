#! /bin/bash
#
. ~/etc/funcs.sh

U_MSG="usage: $0 [ -help ] [ -kl L ] -cf N [ -js S ] [ tsv-file ]"

K_LIST=1
J_STR="\n"
F_COL=
FILE=

while [ $# -gt 0 ] ; do
	case $1 in 
	-help)
		echo "$U_MSG"
		exit 0
		;;
	-kl)
		shift
		if [ $# -eq 0 ] ; then
			LOG ERROR "-kl requires comma spearated list of integers argument"
			echo "$U_MSG" 1>&2
			exit 1
		fi
		K_LIST=$1
		shift
		;;
	-cf)
		shift
		if [ $# -eq 0 ] ; then
			LOG ERROR "-cf requires integer argument"
			echo "$U_MSG" 1>&2
			exit 1
		fi
		F_COL=$1
		shift
		;;
	-*)
		LOG ERROR "unknown option $1"
		echo "$U_MSG" 1>&2
		exit 1
		;;
	*)
		FILE=$1
		shift
		break
		;;
	esac
done

if [ $# -ne 0 ] ; then
	LOG ERROR "extra arguments $*"
	echo "$U_MSG" 1>&2
	exit 1
fi

if [ -z "$F_COL" ] ; then
	LOG ERROR "missing -cf N argument"
	echo "$U_MSG" 1>&2
	exit 1
fi

awk -F'\t' 'BEGIN {
	k_list = "'"$K_LIST"'"
	n_kftab = split(k_list, kftab, ",")
	f_col = "'"$F_COL"'" + 0
	j_str = "'"$J_STR"'"
	if(j_str == "\n")
		j_str = "\\n"
}
{
	key = mk_key(n_kftab, kftab)
	if(key != l_key){
		if(l_key != ""){
			collect_recs(line_1, f_col, j_str, n_recs, recs)
			delete recs
			n_recs = 0
		}
	}
	n_recs++
	recs[n_recs] = $f_col
	if(n_recs == 1)
		line_1 = $0
	l_key = key
}
END {
	if(l_key != ""){
		collect_recs(line_1, f_col, j_str, n_recs, recs)
		delete recs
		n_recs = 0
	}
}
function mk_key(n_kftab, kftab,   k) {

	key = $(kftab[1])
	for(k = 2; k <= n_kftab; k++)
		key = key "\t" $(kftab[k])
	return key
}
function collect_recs(line_1, f_col, j_str, n_recs, recs,    f, nf, ary, j_recs) {

	j_recs = recs[1]
	for(f = 2; f <= n_recs; f++)
		j_recs = j_recs j_str recs[f]

	nf = split(line_1, ary);
	
	# print ary[1:f_col-1]
	# print j_recs
	# print ary[f_col+1:nf]

	for(f = 1; f < f_col; f++){
		if(f > 1)
			printf("\t")
		printf("%s", ary[f])
	}
	if(f > 1)
		printf("\t")
	printf("%s", j_recs)
	f++
	for( ; f <= nf; f++)
		printf("\t%s", ary[f])
	printf("\n")
}' $FILE
