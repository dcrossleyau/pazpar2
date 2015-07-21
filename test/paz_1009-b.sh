#!/bin/sh
# Test script for PAZ-1009 / AUT-258

# Second script, this starts pazpar2 by itself, and uses curl
# Also, posts a whole service

# If $AGPORT is set, uses that as the AG gateway address
# You can start one with something like
#  ssh -L 9998:localhost:9998 somemachine
#    .../aggw.pl @:9998
# If not, starts one locally

if [ -z $AGPORT ]
then 
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
    AGPORT="@:9999"

    $AGGW $LOG -p aggw.pid  $AGPORT &

    sleep 1 # let the listener start up
    if [ ! -f aggw.pid ]
    then
        echo "Could not start the AG gateway "
        exit 1
    fi
    echo "Started the AG Gateway on $AGPORT. PID=" `cat aggw.pid`
else
    echo "Assuming AG gateway is running on $AGPORT"
fi

# Start pz2
rm -f pazpar2.log
../src/pazpar2 -v debug -d -X -l pazpar2.log -f paz_1009-b.cfg &
PZ2PID=$!
echo "Started pazpar2. PID=$PZ2PID"

echo "Init"
curl -X POST -H "Content-type: text/xml" --data-binary @paz_1009_service4.xml  \
"localhost:9763?command=init"
echo; echo

echo "First Search"
curl "localhost:9763?session=1&command=search&query=water"
echo; echo

echo "Bytarget"
curl "localhost:9763?session=1&command=bytarget&block=1"
echo; echo

echo "Second Search"
curl "localhost:9763?session=1&command=search&query=water&limit=publisher%3DU.S.%20G.P.O"
echo; echo

echo "Bytarget"
curl "localhost:9763?session=1&command=bytarget&block=1"
echo; echo

echo "Termlist"
curl "localhost:9763?session=1&command=termlist&name=xtargets%2Cseries%2Cpublisher"
echo; echo

# Kill PZ2
kill $PZ2PID
echo "Killed pz2 $PZ2PID"

# Kill the aggw
if [ -f aggw.pid ]
then 
    echo "Killing the aggw, pid " `cat aggw.pid`
    kill `cat aggw.pid`
    rm -f aggw.pid
fi


# Local Variables:
# mode:shell-script
# sh-indentation: 2
# sh-basic-offset: 4
# End:
