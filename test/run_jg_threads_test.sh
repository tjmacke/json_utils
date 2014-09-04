#! /bin/bash
#
function LOG {
	LEV=$1
	shift
	echo $(date '+%Y%m%d %H%M%S') T000000: $LEV: ${FUNCNAME[1]}:${BASH_SOURCE[1]}:${BASH_LINENO[0]}: "$*"
}

U_MSG="usage: $0 [ -help ] [ -ref ] [ -nt N ] [ threads-test-list-file ]"

JG_HOME=$HOME/json_utils
BUILD_DIR=$JG_HOME/src
BINDIR=$JG_HOME/bin

# move the data to test
DATADIR=$HOME/json_utils/data

MK_REF="no"
N_THREADS=4
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
	-nt)
		shift
		if [ $# -eq 0 ] ; then
			LOG ERROR "-nt requires integer argument"
			echo "$U_MSG"
			exit 1
		fi
		N_THREADS=$1
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

if [ $# -ne 0 ] ; then
	LOG ERROR "extra arguments $*"
	echo "$U_MSG"
	exit 1
fi

grep -v '^#' $TL_FILE	|\
awk -F'\t' 'BEGIN {
	mk_ref = "'"$MK_REF"'" == "yes"
	n_threads = "'"$N_THREADS"'" + 0
	bindir = mk_ref ? "'"$BINDIR"'" : "'"$BUILD_DIR"'"
	printf("#! /bin/bash\n")
	printf("#\n")
	printf("BINDIR=%s\n", bindir)
	printf("N_THREADS=%d\n", n_threads)
	printf("#\n")
}
NF > 0 {
	if(NF == 3 || NF == 4){
		if(!mk_ref){
			printf("echo \"=====================================================\"\n")
			printf("echo    \"Running %s test single threaded\"\n", $1)
			printf("echo\n")
			if($3 ~ /.gz$/)
				printf("gunzip -c %s | %s/json_get -n -g %s", $3, bindir, $2)
			else
				printf("%s/json_get -g %s %s", bindir, $2, $3)
			if(NF == 4)
				printf(" %s", $4)
			printf(" > %s.single\n", $1)
			printf("diff %s.single %s.ref && echo \"  PASSED\" || echo \"    FAILED (possibly; see if diffs above look OK)\"\n", $1, $1)
			printf("rm -f %s.single\n", $1)
			printf("echo \"=====================================================\"\n")
			printf("echo    \"Running %s test with %d threads\"\n", $1, n_threads)
			printf("echo\n")
			if($3 ~ /.gz$/)
				printf("gunzip -c %s | %s/json_get -n -nt %d -g %s", $3, bindir, n_threads, $2)
			else
				printf("%s/json_get -g %s %s", bindir, $2, $3)
			if(NF == 4)
				printf(" %s", $4)
			printf(" > %s.multi\n", $1)
			printf("diff %s.multi %s.ref && echo \"  PASSED\" || echo \"    FAILED (possibly; see if diffs above look OK)\"\n", $1, $1)
			printf("rm -f %s.multi\n", $1)
		}else{
			if($3 ~ /.gz$/)
				printf("gunzip -c %s | %s/json_get -n -g %s", $3, bindir, $2)
			else
				printf("%s/json_get -g %s %s", bindir, $2, $3)
			if(NF == 4)
				printf(" %s", $4)
			printf(" > %s.ref\n", $1)
		}
	}else{
		printf("ERROR: line %d: wrong number of fields %d, expect 3 or 4\n", NR, NF) > "/dev/stderr"
		exit 1
	}
}' > /tmp/jg_thread_tests.$$
chmod +x /tmp/jg_thread_tests.$$
/tmp/jg_thread_tests.$$
rm /tmp/jg_thread_tests.$$
