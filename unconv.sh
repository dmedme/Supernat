#!/bin/sh5
#
# Script to restore Supernat V2
# @(#) $Name$ $Id$
cd /usr/spool/nat
for i in ` ls -d Loc* | grep -v '\.V2' `
do
   rm -rf $i
   rm G.$i
done
for i in G.*.V2
do
    if [ "$i" = "G.*.V2" ]
    then
        break
    fi
    l=`echo $i | sed 's/G\.\([^.]*\)\.V2/\1/'`
    mv $i G.$l
    mv $l.V2 $l
done
mv Queuelist.V2 Queuelist
rm -rf log supernatdb
rm -f tmp* sacct
ls
exit
