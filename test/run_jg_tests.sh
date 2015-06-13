#! /bin/bash
#
function LOG {
	LEV=$1
	shift
	echo $(date '+%Y%m%d %H%M%S') T000000: $LEV: ${FUNCNAME[1]}:${BASH_SOURCE[1]}:${BASH_LINENO[0]}: "$*"
}

U_MSG="usage: $0 [ -help ] [ -ref ] [ test-list-file ]"

if [ "$JG_HOME" == "" ] ; then
	CWD=$(pwd)
	JG_HOME=$(dirname $CWD)
	LOG INFO "using JG_HOME  = $JG_HOME"
fi

BUILD_DIR=$JG_HOME/src
BINDIR=$JG_HOME/bin

MK_REF="no"
TL_FILE=

while [ $# -gt 0 ] ; do
	case $1 in
	-help)
		echo "$U_MSG"
		exit 0
		;;
	-ref)
		MK_REF="yes"
		shift
		;;
	-*)
		LOG ERROR "unknown option $1"
		echo "$U_MSG"
		exit 1
		;;
	*)
		TL_FILE=$1
		shift
		break
		;;
	esac
done

if [ $# -gt 0 ] ; then
	LOG ERROR "extra arguments $*"
	echo "$U_MSG"
	exit 1
fi

grep -v '^#' $TL_FILE	|\
awk -F'\t' 'BEGIN {
	mk_ref = "'"$MK_REF"'" == "yes"
	if(mk_ref){
		bindir = "'"$BINDIR"'"
		sfx = "ref"
	}else{
		bindir = "'"$BUILD_DIR"'"
		sfx = "cand"
	}
	printf("#! /bin/bash\n")
	printf("#\n")
	printf("BINDIR=%s\n", bindir)
	printf("#\n")
}
NF > 0 {
	if(NF == 3 || NF == 4){
		if(!mk_ref){
			printf("echo \"=====================================================\"\n")
			printf("echo    \"Running %s test\"\n", $1)
			printf("echo\n")
		}
		printf("%s/json_get -g %s %s", bindir, $2, $3)
		if(NF == 4)
			printf(" %s", $4)
		printf(" > %s.%s\n", $1, sfx)
		if(!mk_ref){
			printf("diff %s.cand %s.ref && echo \"  PASSED\" || echo \"    FAILED (possibly; see if diffs above look OK)\"\n", $1, $1)
			printf("rm -f %s.cand\n", $1)
		}
	}else{
		printf("ERROR: line %d: wrong number of fields %d, expect 3 or 4\n", NR, NF) > "/dev/stderr"
		exit 1
	}
}' > /tmp/jg_tests.$$
chmod +x /tmp/jg_tests.$$
/tmp/jg_tests.$$
rm /tmp/jg_tests.$$
