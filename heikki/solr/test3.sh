#!/bin/bash
#
# Simple script (and config) to get pz2 to run against DBC's OpenSearch, and
# calculate rankings. See how they differ for different queries
#

if [ "$1" == "clean" ]
then
  echo "Cleaning up"
  rm -f $PIDFILE $YAZPIDFILE *.out *.log *.data *~ plot.cmd
  exit
fi
killall pazpar2 

rm -f *.out *.log

URL="http://localhost:9017/"
CFG="test3.cfg"

PZ="../../src/pazpar2"
if [ ! -x $PZ ]
then
  echo "$PZ2 not executable. Panic"
  exit 1
fi

PIDFILE=pz2.pid

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

if [ -z "$2" ]
then
  HEADLINE="$Q"
else
  HEADLINE="$2: $Q"
fi

QRY=`echo $Q | sed 's/ /+/g' `

#SORT="sort=score"
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

# Plot the lines created by the code
grep plotline show.out > scores.data
echo "Client numbers"
cat scores.data | cut -d' ' -f2 | sort -u
head -10 scores.data

exit 1

echo "
  set term png
  set out \"plot.png\"
  set title \"$HEADLINE\"
" > plot.cmd
echo '
  plot "scores.data" using 0:($2==0?$6:1/0) with points title "db-1", \
       "scores.data" using 0:($2==1?$6:1/0) with points title "db-2", \
       "scores.data" using 0:($2==2?$6:1/0) with points title "db-3", \
       "scores.data" using 0:($2==3?$6:1/0) with points title "db-4", \
       "scores.data" using 0:($2==4?$6:1/0) with points title "db-5", \
       "scores.data" using 0:($2==5?$6:1/0) with points title "db-6" \
' >> plot.cmd
cat plot.cmd | gnuplot



echo "All done"
kill `cat $PIDFILE`
rm -f $PIDFILE 

