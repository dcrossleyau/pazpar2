#!/bin/bash
#
# Simple script (and config) to get pz2 to run against yaz-ztest, and 
# calculate rankings. See how they differ for different queries
#
# (uses curl and xml-twig-tools)

DIR=`cd ..; pwd`
HDIR=$DIR/heikki
CFG="$DIR/etc/heikki-test1.cfg"

PZ="$DIR/src/pazpar2"

PIDFILE=$HDIR/pz2.pid
YAZPIDFILE=$HDIR/yaz-ztest.pid

yaz-ztest -p $YAZPIDFILE -l yaz-ztest.log &


$PZ -f $CFG  -w "$DIR/etc" -l $HDIR/pz2.log -p $PIDFILE &
sleep 0.2 # make sure it has time to start
echo "Init"
curl -s "http://localhost:9017/?command=init" > init.out
SESSION=`xml_grep --text_only "//session" init.out `
cat init.out
echo "Got session $SESSION"
SES="&session=$SESSION"


QRY="query=computer"
#SEARCH="command=search$SES&$QRY&rank=1&sort=relevance"
#SEARCH="command=search$SES&$QRY"
SEARCH="command=search$SES&$QRY&sort=relevance"
echo $SEARCH
curl -s "http://localhost:9017/?$SEARCH" > search.out
cat search.out
echo

SHOW="command=show$SES&sort=relevance"
echo $SHOW
curl -s "http://localhost:9017/?$SHOW" > show.out
echo "md-score:"
grep "md-score" show.out
echo "relevance:"
grep "relevance" show.out
echo

echo "All done"
kill `cat $PIDFILE`
kill `cat $YAZPIDFILE`