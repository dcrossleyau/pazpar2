#!/bin/sh
#

# srcdir might be set by make
srcdir=${srcdir:-"."}

TEST=`basename $0 .sh`

# look for yaz-ztest in PATH
oIFS=$IFS
IFS=:
F=''
for p in $PATH; do
    if test -x $p/yaz-ztest -a -x $p/yaz-client; then
	VERSION=`$p/yaz-client -V|awk '{print $3;}'|awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
        if test $VERSION -ge 4000000; then
            F=$p/yaz-ztest
            break
        fi
    fi
done
IFS=$oIFS

if test -z "$F"; then
    echo "yaz-ztest not found"
    exit 0
fi

rm -f ztest.pid
rm -f ${TEST}_ztest.log
$F -l ${TEST}_ztest.log -p ztest.pid -D tcp:localhost:9999
sleep 1
if test ! -f ztest.pid; then
    echo "yaz-ztest could not be started"
    exit 0
fi

E=0
if test -x ../src/pazpar2; then
    if ../src/pazpar2 -V |grep icu:enabled >/dev/null; then
	${srcdir}/run_pazpar2.sh $TEST
	E=$?
    fi
fi

kill `cat ztest.pid`
rm ztest.pid
exit $E

# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
