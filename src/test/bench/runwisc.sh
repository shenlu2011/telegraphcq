#!/bin/sh
# $Header: /project/eecs/db/cvsroot/postgres/src/test/bench/runwisc.sh,v 1.2 2004/03/24 08:11:37 chungwu Exp $
# 
# Note that in our published benchmark numbers, we executed the command in the
# following fashion:
#
# time $POSTGRES -texecutor -tplanner -f hashjoin -Q bench
#
if [ ! -d $1 ]; then
        echo " you must specify a valid data directory "
        exit
fi

if [ -d ./obj ]; then
	cd ./obj
fi

echo =============== vacuuming benchmark database... =================
echo "vacuum" | postgres -D${1} -Q bench > /dev/null

echo =============== running benchmark... =================
time postgres -D${1} -texecutor -tplanner -Q bench < bench.sql
