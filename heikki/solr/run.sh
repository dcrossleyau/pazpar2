#!/bin/bash
#
# Run the test with a number of queries, plot the results
# 

if [ "$1" == "" ]
then
  echo "Need an argument, the name of this test run"
  echo "It will be in the title of all plots, together with the query"
  exit 1
fi
(cd ../../src; rm -f pazpar2; make; grep '###' relevance.c )
TITLE="$1"
OUTFILE=`echo $1.txt | sed 's/ /_/g'`
echo "Run $TITLE" > $OUTFILE
./test3.sh clean
rm *.png

function onerun() {
    QRY="$1"
    echo "" >> $OUTFILE
    echo "Query: $QRY" >> $OUTFILE
    PNG=`echo "$TITLE $QRY.png" | sed 's/ /_/g' `
    echo "Graph: $PNG" >> $OUTFILE
    ./test3.sh "$QRY" "$TITLE"
    cat stat.line >> $OUTFILE
    grep "plotline" show.out | head -10 >> $OUTFILE
    cp plot.png $PNG
}

onerun "harry potter"
onerun "vietnam war"
onerun "water or fire or ice"
onerun "zen and motorcycle"
echo "" >> $OUTFILE
echo "client#, position, tf/idf, roundrobin, solr, normalized # database # title" >> $OUTFILE

rm plot.png
