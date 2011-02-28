#!/bin/bash
DELAY=0.001
WAIT=5
NUMBER=5
ROUNDS=5
PORT=9004
SERVICE=perf_t
SHUTDOWN=1
HOST=127.0.0.1
if test -n "$1"; then
	. $1
fi
let r=0
while test $r -lt $ROUNDS; do
	echo "$r"
	let i=0
	while test $i -lt $NUMBER; do
		./pp2client.sh --outfile=$r.$i --prefix=http://$HOST:${PORT}/search.pz2 --service=$SERVICE >$r.$i.log 2>&1 &
		sleep $DELAY
		let i=$i+1
	done
	sleep $WAIT
	let r=$r+1
done
wait
if [ "$SHUTDOWN" == "1" ] ; then 
    wget -O x "http://localhost:${PORT}/search.pz2?command=exit"
fi
