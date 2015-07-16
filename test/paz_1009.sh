#!/bin/sh
# Test script for PAZ-1009 / AUT-258

# Start the aggw
AGDIR=../../ag-integration/gateway
AGGW=$AGDIR/aggw.pl

if [ ! -x $AGGW ]
then
  echo "AG Gateway $AGGW not found. Skipping test"
  exit 0
fi

rm -f aggw.log aggw.pid apdu.log.* # -f to shut up if not there
LOG="-l aggw.log  -a apdu.log"  # uncomment to get the gw log in a file
AGPORT="@:9998"

$AGGW $LOG -p aggw.pid  $AGPORT &

sleep 1 # let the listener start up
if [ ! -f aggw.pid ]
then
  echo "Could not start the AG gateway "
  exit 1
fi
echo "Started the AG Gateway on $AGPORT. PID=" `cat aggw.pid`

TEST=`basename $0 .sh`
# srcdir might be set by make
srcdir=${srcdir:-"."}

#exec ${srcdir}/run_pazpar2.sh --icu $TEST
${srcdir}/run_pazpar2.sh --icu $TEST


# Kill the aggw
echo "Killing the aggw, pid " `cat aggw.pid`
kill `cat aggw.pid`
rm -f aggw.pid


# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
