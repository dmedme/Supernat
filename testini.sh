#!/bin/sh
# Initialise a test system
. ./testenv.sh
rm -rf test
mkdir test
mkdir test/log
chmod 0770 test/log
cp Queuelist.V3 test/Queuelist
chmod 0664 test/Queuelist
cat /dev/null > test/Activity
chmod 0660 test/Activity
echo rwxefin > test/Userlist
echo rwxroot >> test/Userlist
chmod 0664 test/Userlist
natcalc -n $NAT_HOME/test/supernatdb -c < /dev/null
