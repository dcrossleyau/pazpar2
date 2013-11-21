#!/bin/bash
#
# Simple script (and config) to get pz2 to run against yaz-ztest, and 
# calculate rankings. See how they differ for different queries
#
# (uses curl and xml-twig-tools)

if [ "$1" == "clean" ]
then
  echo "Cleaning up"
  rm -f $PIDFILE $YAZPIDFILE *.out *.log *~ 
  exit
fi

CFG="test1.cfg"

PZ="../src/pazpar2"

PIDFILE=pz2.pid
YAZPIDFILE=yaz-ztest.pid

yaz-ztest -p $YAZPIDFILE -l yaz-ztest.log &

rm -f *.out *.log

$PZ -f $CFG  -l pz2.log -p $PIDFILE &
sleep 0.2 # make sure it has time to start
echo "Init"
curl -s "http://localhost:9017/?command=init" > init.out
SESSION=`xml_grep --text_only "//session" init.out `
# cat init.out; echo
echo "Got session $SESSION"
SES="&session=$SESSION"


QRY="query=computer"
#SEARCH="command=search$SES&$QRY&rank=1&sort=relevance"
#SEARCH="command=search$SES&$QRY"
SEARCH="command=search$SES&$QRY&sort=relevance"
echo $SEARCH
curl -s "http://localhost:9017/?$SEARCH" > search.out
cat search.out | grep search
echo

SHOW="command=show$SES&sort=relevance"
echo $SHOW
curl -s "http://localhost:9017/?$SHOW" > show.out
grep "relevance" show.out | grep += | grep -v "(0)"
echo

echo "All done"
kill `cat $PIDFILE`
kill `cat $YAZPIDFILE`
rm -f $PIDFILE $YAZPIDFILE

