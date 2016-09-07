#!/bin/sh
# natover.sh - control the coercion of report programs to batch.
#
# Merge lists of /u2/cfacs/binlive and /u2/cfacs/over_ride
#
# Allow users to toggle between them
#
#set -x
pid=$$
export pid
ls -1 /u2/cfacs/binlive | egrep -v '\.' | grep '_' > reps$pid.lis
#
# Function to put the names of the queues in a list
#
all_queues() {
QUEUES=`natqlist | awk -F= '/^DEFINE=/ {
    if ($2 != "GLOBAL")
        print $2
}'`
export QUEUES
return
}
#
# Function to select a single queue from a list
#
queue_select () {
head=$1
all_queues
QUEUES=`(
echo HEAD=$head
echo PROMPT=Select Queues, and Press Return
echo COMMAND= 
echo SEL_YES/COMM_NO/MENU
echo SCROLL
for i in $QUEUES
do
    echo $i/$i
done
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty `
if [  "$QUEUES" = "EXIT:" -o "$QUEUES" = " " ]
then
    QUEUES=
else
    set -- $QUEUES
    QUEUES="$*"
fi
export QUEUES
return
}
#
# Function to reset the list of CFACS reports
#
reset_lists() {
ls -1 /u2/cfacs/over_ride | grep -v '\.'  > overs$pid.lis
fgrep -v -f overs$$.lis reps$pid.lis > normals$pid.lis
return
}
. /u1/natman/bin/natenv.sh
reset_lists
# ****************************************************************************
# Main control - process options until exit requested
#
while :
do

opt=`natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=MAIN: Over-ride Maintenance Menu
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
COERCE:    Coerce the program into Batch/COERCE:
RESTORE:   Restore interactive job execution capabilities/RESTORE:
DEFAULTS:  Set Report Queue Defaults/DEFAULTS:
REVIEW:    List Current Report Queue Defaults/cat /u2/cfacs/over_ride/rep_queues.dat | sort
EXIT:      Exit/EXIT:

EOF
`
set -- $opt
case $1 in
EXIT:)
break
;;
COERCE:)
FILE_LIST=`(
echo HEAD=COERCE: Force batch execution of the selected reports
echo PROMPT=Select Files, and Press Return
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
sed 's=.*=&/&=' normals$pid.lis
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
if [ ! "$FILE_LIST" = " " -o "$FILE_LIST" = "EXIT:" ]
then
    for i in $FILE_LIST
    do
        ln /u2/cfacs/over_ride/natclone.sh /u2/cfacs/over_ride/$i
    done
    reset_lists
fi
 ;;
RESTORE:)
FILE_LIST=`(
echo HEAD=RESTORE: Restore interactive execution for the selected reports
echo PROMPT=Select Files, and Press Return
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
sed 's=.*=&/&=' overs$pid.lis
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
if [ ! "$FILE_LIST" = " " -o "$FILE_LIST" = "EXIT:" ]
then
    for i in $FILE_LIST
    do
        rm -f /u2/cfacs/over_ride/$i
        reset_lists
    done
fi
 ;;
DEFAULTS:)
FILE_LIST=`(
echo HEAD=DEFAULTS: Assign a default queue to a set of reports
echo PROMPT=Select Files, and Press Return
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
sed 's=.*=&/&=' reps$pid.lis
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
if [ ! "$FILE_LIST" = " " -o "$FILE_LIST" = "EXIT:" ]
then
    queue_select "Choose a queue to assign these reports to"    
    set -- "$QUEUES"
    QUEUES=$1
    if [ ! "$QUEUES" = " " -o "$QUEUES" = "EXIT:" -o "$QUEUES" = All ]
    then
        
        cp /u2/cfacs/binlive/cfxbatch.dat bat1$pid.dat
        cp /u2/cfacs/over_ride/rep_queues.dat rep1$pid.dat
        for i in $FILE_LIST
        do
            sed "/:$i:/ d
/^:::/ d" bat1$pid.dat >bat2$pid.dat
            sed "/^$i:/ d" rep1$pid.dat >rep2$pid.dat
            echo ":$i::/u1/natman/bin/nat -q $QUEUES -j ':\$:' -x 'cfxrun :\$:' -i -r:" >> bat2$pid.dat
            echo "$i:$QUEUES" >> rep2$pid.dat
            mv bat2$pid.dat bat1$pid.dat
            mv rep2$pid.dat rep1$pid.dat
        done
        echo ":::nat -q nat -j ':\$:' -x 'cfxrun :\$:' -i -r:" >> bat1$pid.dat
        mv bat1$pid.dat /u2/cfacs/binlive/cfxbatch.dat
        mv rep1$pid.dat /u2/cfacs/over_ride/rep_queues.dat
    fi
fi
;;
*)
echo "Unrecognised Options..." $opt
break
;;
esac
done
rm -f normals$pid.lis overs$pid.lis reps$pid.lis
