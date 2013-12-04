#!/bin/bash
#
# Simple script (and config) to get pz2 to run against DBC's OpenSearch, and
# calculate rankings. See how they differ for different queries
#

if [ "$1" == "clean" ]
then
  echo "Cleaning up"
  rm -f $PIDFILE $YAZPIDFILE *.out *.log *.data *~ 
  exit
fi
killall pazpar2 dbc-opensearch-gw

rm -f *.out *.log

URL="http://localhost:9017/"
CFG="test2.cfg"

PZ="../../src/pazpar2"
if [ ! -x $PZ ]
then
  echo "$PZ2 not executable. Panic"
  exit 1
fi

PIDFILE=pz2.pid

# Start the gateway.
  ./dbc-opensearch-gw.pl -1 \
      -c dbc-opensearch-gw.cfg \
      -l dbc-opensearch-gw.log \
      @:9994 &


$PZ -f $CFG  -l pz2.log -p $PIDFILE &
sleep 0.2 # make sure it has time to start
echo "Init"
curl -s "$URL?command=init" > init.out
SESSION=`xml_grep --text_only "//session" init.out `
# cat init.out; echo
echo "Got session $SESSION"
SES="&session=$SESSION"


if [ -z "$1" ]
then
  Q="computer"
else
  Q=$1
fi
QRY=`echo $Q | sed 's/ /+/g' `

SORT="sort=relevance_h"
#SEARCH="command=search$SES&$QRY&rank=1&sort=relevance"
#SEARCH="command=search$SES&$QRY"
#SEARCH="command=search$SES&query=$QRY&sort=relevance"
SEARCH="command=search$SES&query=$QRY&$SORT"
echo $SEARCH
curl -s "$URL?$SEARCH" > search.out
cat search.out | grep search
echo
sleep 0.5 # let the search start working

STAT="command=stat&$SES"
echo "" > stat.out
LOOPING=1
while [ $LOOPING = 1 ]
do
  sleep 0.5
  curl -s "$URL?$STAT" > stat.out
  ACT=`xml_grep --text_only "//activeclients" stat.out`
  HIT=`xml_grep --text_only "//hits" stat.out`
  REC=`xml_grep --text_only "//records" stat.out`
  echo "$ACT $HIT $REC"
  if grep -q "<activeclients>0</activeclients>" stat.out
  then
    LOOPING=0
  fi
  echo >> stats.out
  cat stat.out >> stats.out
done


SHOW="command=show$SES&start=0&num=100&$SORT"
echo $SHOW
curl -s "http://localhost:9017/?$SHOW" > show.out
#grep "relevance" show.out | grep += | grep -v "(0)"
#grep "round-robin" show.out
grep '^ <md-title>' show.out | head -11
grep 'Received' dbc-opensearch-gw.log | head -1 >> titles.out
grep '^ <md-title>' show.out >> titles.out

# Plot it
DF=`echo $QRY | sed 's/@//g' | sed 's/[+"]/_/g' | sed s"/'//g "`
grep "round-robin" show.out |
  cut -d' ' -f 6,7 |
  sed 's/[^0-9 ]//g' |
  awk '{print FNR,$0}'> $DF.data



echo '\
  set term png
  set out "plot.png"
  #set yrange [0:300000]
  set logscale y
  plot \' > plot.cmd
for F in *.data
do
  BF=`basename $F .data | sed 's/_/ /g' `
  echo -n " \"$F\" using 1:2  with points  title \"$BF\", " >> plot.cmd
done
echo "0 notitle" >> plot.cmd

gnuplot < plot.cmd

echo

echo "All done"
kill `cat $PIDFILE`
rm -f $PIDFILE 

