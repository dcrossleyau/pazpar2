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

URL="http://localhost:9017/"
CFG="test1.cfg"

PZ="../src/pazpar2"

PIDFILE=pz2.pid
YAZPIDFILE=yaz-ztest.pid

yaz-ztest -p $YAZPIDFILE -l yaz-ztest.log &

rm -f *.out *.log

$PZ -f $CFG  -l pz2.log -p $PIDFILE &
sleep 0.2 # make sure it has time to start
echo "Init"
curl -s "$URL?command=init" > init.out
SESSION=`xml_grep --text_only "//session" init.out `
# cat init.out; echo
echo "Got session $SESSION"
SES="&session=$SESSION"


QRY="query=computer"
#SEARCH="command=search$SES&$QRY&rank=1&sort=relevance"
#SEARCH="command=search$SES&$QRY"
SEARCH="command=search$SES&$QRY&sort=relevance"
echo $SEARCH
curl -s "$URL?$SEARCH" > search.out
cat search.out | grep search
echo

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


SHOW="command=show$SES&sort=relevance_h&start=0&num=100"
echo $SHOW
curl -s "http://localhost:9017/?$SHOW" > show.out
#grep "relevance" show.out | grep += | grep -v "(0)"
grep "round-robin" show.out

# Plot it
grep "round-robin" show.out |
  cut -d' ' -f 6,7 |
  sed 's/[^0-9 ]//g' |
  awk '{print FNR,$0}'> plot.data

echo '\
  set term png
  set out "plot.png"
  plot "plot.data" using 1:2  with points  title "tf/idf", \
       "plot.data" using 1:($3*300)  with points  title "round-robin"
 ' | gnuplot

echo

echo "All done"
kill `cat $PIDFILE`
kill `cat $YAZPIDFILE`
rm -f $PIDFILE $YAZPIDFILE

