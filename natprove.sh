:
#!/bin/ksh
#!/bin/sh5
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems Limited 1992
# natprove.sh - demonstrate that the SUPERNAT installation
# has been successful
# - run as root
# - root must already have been granted rwx on the system
# - This exercises everything except the screen handler
#
. /usr/lib/nat/natenv.sh
set -vx
NATSP=/var/spool/nat
export NATSP
#NATSP=$NAT_HOME/test
#export NATSP
#rm -f ${NATSP}/supernat_fifo
#rm -f ${NATSP}/supernat_lock
#natrun -s 4 -t > ${NATSP}/natrun.log 2>&1 &
#until [ -S ${NATSP}/supernat_fifo ]
#do
#    sleep 30
#done
#
# Setting Permissions Globally
#
while :
do
nataperm rwx root
nataperm rw- test1
nataperm rw- \*
nataperm rw- test2
natalist -l
nataperm --- test1
natalist -l
nataperm --- test2
natalist -l
nataperm --- test3
natalist -l
#
# Setting Up Queues and Permissions
#
natqadd daytest1 5 3 
natgperm daytest1 rw- \*
natgperm daytest1 rw- test2
natglist -l daytest1
natgperm daytest1 --- test1
natglist -l daytest1
natgperm daytest1 --- test2
natglist -l daytest1
natqlist
natqchg daytest1 6 3 00:00 10:00 1 3 10:00 12:00 6 3 12:00 24:00 
natqlist
natqdel daytest1
natqlist
natglist -l daytest1
natqadd daytest2 6 3 00:00 10:00 1 3 10:00 12:00 6 3 12:00 24:00 
natgperm daytest2 rw- \*
natgperm daytest2 rw- test2
natglist -l daytest2
natgperm daytest2 --- test1
natglist -l daytest2
natgperm daytest2 --- test2
natglist -l daytest2
natqadd nighttest1 5 3 18:00 8:00 
natgperm nighttest1 rw- \*
natglist -l nighttest1
natqadd daytest1 5 3
natgperm daytest1 rw- \*
natqlist
natglist -l daytest1
#
# Setting Up and Executing Jobs
#
date
natq -al
for i in daytest1 daytest2 nighttest1
do
for j in 1 2 3 4 5 6
do
nat -i -th -j"$i.Imm.$j" -q $i << EOF
echo Test Immediate Job $j
sleep 300
echo End of Test Immediate Job $j
EOF
nat -m -th -j"$i.Fut.$j" -q $i 12:0$j << EOF
echo Test Future Job $j
sleep 300
echo End of Test Future Job $j
EOF
done
done 
#
# Now check out the Activity 
#
date
natq -al
for i in daytest1 daytest2 nighttest1
do
natq -l -q $i
natq -q $i
done
#
# Let the system settle down
#
sleep 100
#
# Now Try and kill some things off
#
natq -a | awk '/^ *Queue/ {q = $2}
/^ *\*/ { print "natrm -q " q " " $8 }' | sh -vx
natq -al
natq -a | awk '/^ *Queue/ {q = $2}
/^ *\*/ { print "nat -r -q  " q " -o " q " -p " $8 " -j Changed 20:00" }' | sh -vx
for i in 1 2 3 4 5
do
date
natq -al
natcmd status
sleep 300
done
natqchg daytest2 5 3
natq -al
date
natqdel daytest1
natq -al
date
natqdel daytest2
natq -al
date
natqdel nighttest1
natq -al
date
#sleep 3600
exit
done
natcmd halt
date
#
# Finished; want to collect:
# - stderr, stdout from this run
# - natrun.log
# - Activity
#
# Interesting problems comparing actual with expected, because
# of the date issues.
#
exit
