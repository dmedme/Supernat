#!/bin/ksh
#!/bin/sh5
# (Select the starting string dependent on environment)
# natuser.sh - User Menu for SUPERNAT
# @(#) $Name$ $Id$
#Copyright (c) E2 Systems, 1990
#
# This file contains definitions of Queue Management Menus
#
# The order of items is important 
# - execution always starts with the first item
# - forward references to return menus are not allowed
#
#
trap 'echo "Abort Command Given"
exit 0' 1 2 3
#
# Establish the local environment
# 
. /usr/local/natman/bin/natenv.sh
BASE_DIR=$SUPERNAT
export BASE_DIR
 
if echo $PATH | grep $NAT_HOME >/dev/null 2>&1
then
    :
else
    PATH=$NAT_HOME:$PATH
    export PATH
fi
#
# Function to find the root natrun process
#
get_natrun_pid () {
NATRUN_PID=`ps -ef | nawk 'BEGIN {nat_cnt = 0}
/natrun/ { pid[$2] = 1
this_pid[nat_cnt] = $2
par_pid[nat_cnt] = $3
nat_cnt++
}
END { for (i = 0; i < nat_cnt; i++)
    {
       parp = par_pid[i]
       if (pid[parp] == 0)
       {
           print this_pid[i]
           exit
       }
    }
} '`
export NATRUN_PID
if [ -z "$NATRUN_PID" ]
then
    STATE="DOWN"
else
    STATE="UP"
fi
export STATE
return
}
#
# Function to allow a user to select from a list of files
# Arguments:
# 1 - The File Selection Header
# 2 - The File Selection String
#
file_select () {
head=$1
sel=$2
FILE_LIST=`(
echo HEAD=$head
echo PROMPT=Select Files, and Press Return
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
echo All/All
ls -lt $sel 2>&1 | sed '/\// s=\(.* \)\([^ ]*/\)\([^/]*\)$=\1\3/\2\3=
/\//!{
s=.* \([^ ]*\)$=&/\1=
}'
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
if [ "$FILE_LIST" = " " -o "$FILE_LIST" = "EXIT:" ]
then
    FILE_LIST=""
elif [ "$FILE_LIST" = " All " ]
then
    FILE_LIST=`ls $sel 2>&1 `
fi
export FILE_LIST
return
}
#
# Function to allow a user to select from a list of jobs
# Arguments:
# 1 - The Header
# 2 - The Queue
#
job_select () {
head="$1"
queue=$2
JOBS=`(
echo HEAD=$head
echo PROMPT=Select Jobs, and Press Return
echo COMMAND= 
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
echo All/All
natq -q $queue | sed '/\// s=/=.=g' | nawk '/^[RWHGSF]/ { print substr($0,1,78) "/" $8 }'
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
if [ "$JOBS" = " " -o "$JOBS" = "EXIT:" ]
then
    JOBS=""
elif [ "$JOBS" = " All " ]
then
JOBS=`natq -q $queue | nawk '/^[RWHGSF]/ { print $8 }'`
fi
export JOBS
return
}
#
# Function to handle output in the standard manner
# Arguments: 1 - Heading
# The others: Command to execute
#
output_dispose () {
head=$1
shift
args=$*
JUNK=`(
echo HEAD=$head
echo PROMPT=Press Return to Continue
echo COMMAND= 
echo SEL_YES/COMM_NO/MENU
echo SCROLL
$args | sed 's.[=/]. .g
:top
/.....................................................................[^ ]* /{
h
s=\(.....................................................................[^ ]* \).*=\1/NULL:=
p
g
s/.....................................................................[^ ]* /->                       /
b top
}
/............................................................................./{
h
s=\(.............................................................................\).*=\1/NULL:=
p
g
s/............................................................................./->                       /
b top
}
s=.*=& /NULL:='
echo " /NULL:"
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
return
}
# Function to allow a user to select the action to apply to selected files
#
# Arguments:
# 1 - The Action Selection Header
#
action_select () {
ACTION=` natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=$1
PROMPT=Select Action, and Press Return
SEL_YES/COMM_NO/MENU
SCROLL
BROWSE: View the Selected Items/BROWSE:
PRINT : Print the Selected Items/PRINT:
DELETE: Delete the Selected Items/DELETE:
EXIT  : Exit without doing anything/EXIT:
EOF
`
export ACTION
return
}
#
# Function to allow browsing of files, with no SHELL escape
# Arguments:
# 1 - List of files to browse
#
file_browse () {
BROWSE_FILES=$1
export BROWSE_FILES
(
SHELL=/bin/true
export SHELL
echo Browse Selected Files with pg | pg -p '(Screen %d (h for help))' -s - $BROWSE_FILES
)
return
}
#
# Function to put the names of the queues in a list
#
all_queues() {
QUEUES=`natqlist | nawk -F= '/^DEFINE=/ {
    if ($2 != "GLOBAL")
        print $2
}'`
export QUEUES
return
}
#
# Function to select a queue from a list
#
queue_select () {
head=$1
all_queues
QUEUES=`(
echo HEAD=$head
echo PROMPT=Select Queues, and Press Return
echo COMMAND= 
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
echo All/All
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
# Function to select a user from a list
#
user_select () {
USERS=`(
echo HEAD=The Available Users
echo PROMPT=Select Users, and Press Return
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
natalist | nawk '{print  $0 "/\\\\" $0}'
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty `
if [  "$USERS" = "EXIT:" -o "$USERS" = " " ]
then
    USERS=
fi
export USERS
return
}
#
# Function to check that the FIFO exists
#
check_supernat_run () {
if [ ! -p $BASE_DIR/supernat_fifo  ]
then
    return 1
else
    return 0
fi
}
#
# Function to submit a command to supernat
#
natrun_command () {
sched_comm="$*"
if check_supernat_run
then
#
# Acquire the right to submit
#
trap 'echo "Abort Command Given"
return 1' 1 2 3
echo "Issuing SUPERNAT Command......" $sched_comm
#
# Issue the request
#
if natcmd $sched_comm
then
    return 0
else
    return 1
fi
fi
}
#
# Function to shut down natrun, if it seems to be running
#
# *****************************************************************************
# Initialise
# Note whether or not we are super-user
#
if id | grep '^uid=0' >> /dev/null 2>& 1
then
    ROOT_USER=Y
else
    ROOT_USER=N
fi
export ROOT_USER
get_natrun_pid
# ****************************************************************************
# Main control - process options until exit requested
#
while :
do

opt=`natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=MAIN: Queue Management Main Menu
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
QUEUE:     List the Queues/natq -a
PROC:      View Executing Supernat Processes/natps.sh
SPOOL:     List Print Jobs/lpq
JOBS:      Job Management/JOBS:
EXIT:      Exit/EXIT:

HEAD=JOBS: Management of Jobs in a Queue
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
NATQ:         List the Jobs in a Queue/NATQ:
NATRM:        Remove Jobs from a Queue/NATRM:
NATHOLD:      Put Jobs on Hold/NATHOLD:
NATRELEASE:   Release Jobs on Hold/NATRELEASE:
EXIT:         Return/EXIT:


EOF
`
set -- $opt
case $1 in
EXIT:)
break
;;
NATQL:)
queue_select "NATQL: Select Queues for Long Listing"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        output_dispose "NATQL: Long Listing All Queues" natq -al
    else
        for i in $QUEUES
        do
            output_dispose "NATQL: Long Listing Queue $i" natq -l -q $i
        done
    fi
fi
;;
NATQ:)
queue_select "NATQ: Select Queues for Listing"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        output_dispose "NATQ: List All Queues" natq -a
    else
        for i in $QUEUES
        do
            output_dispose "NATQ: Jobs in Queue $i" natq -q $i
        done
    fi
fi
;;
NATHOLD:)
queue_select "NATHOLD: Select Queues to Act On"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    for i in $QUEUES
    do
        job_select "NATHOLD: Select Jobs for Holding in Queue $i" $i
        if [ ! -z "$JOBS" ]
        then
            for j in $JOBS
            do
natcalc -w -j $j << EOF
HOLD($j);
EOF
            done
        fi
    done
fi
;;
NATRELEASE:)
queue_select "NATRELEASE: Select Queues to Act On"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    for i in $QUEUES
    do
        job_select "NATRELEASE: Select Jobs for Release From Queue $i" $i
        if [ ! -z "$JOBS" ]
        then
            for j in $JOBS
            do
natcalc -w -j $j << EOF
RELEASE($j);
EOF
            done
        fi
    done
fi
;;
NATRM:)
queue_select "NATRM: Select Queues to Act On"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    for i in $QUEUES
    do
        job_select "NATRM: Select Jobs for Removal From Queue $i" $i
        if [ ! -z "$JOBS" ]
        then
            natrm -q $i $JOBS
        fi
    done
fi
;;
*)
echo "Unrecognised Options..." $opt
sleep 5
exit
;;
esac
done
exit
