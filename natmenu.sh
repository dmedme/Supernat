#!/bin/sh
#!/bin/ksh
#!/bin/sh5
# (Select the starting string dependent on environment)
# natmenu.sh - master administrative menu for supernat
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
# Establish your local environment
# On SCO, you need a full path name. Normally, . will search the
# execution path.
. /usr/lib/nat/natenv.sh
#. /usr/opt/snat/bin/natenv.sh
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
NATRUN_PID=`ps -ef | awk 'BEGIN {nat_cnt = 0}
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
# Function to start the scheduler
#
natrun_start () {
if [ $ROOT_USER = N ]
then
    echo "Only root can start the scheduler"
    sleep 5
    return
fi
natrun_shut
options=`natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=START:    Start the SUPERNAT Scheduler
PROMPT=Fill in the desired fields and then Execute
COMMAND=START:
SEL_NO/COMM_NO/SYSTEM
SCROLL
Enter the simultaneous jobs maximum
Timestamped output logs (blank for no)

EOF
`
set -- $options
if [ $# = 0 ]
then
    return
else
    shift
    if [ $# = 0 ]
    then
        simargs=""
    else
        simargs=-s$1
        if [ $# = 1 ]
        then
            :
        else
            simargs="$simargs -t"
        fi
    fi
    SIMARGS=$simargs
    export SIMARGS
    natstart.sh
fi
get_natrun_pid
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
natq -q $queue | sed '/\// s=/=.=g' | awk '/^[RWHGSF]/ { print substr($0,1,78) "/" $8 }'
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
if [ "$JOBS" = " " -o "$JOBS" = "EXIT:" ]
then
    JOBS=""
elif [ "$JOBS" = " All " ]
then
JOBS=`natq -q $queue | awk '/^[RWHGSF]/ { print $8 }'`
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
) | tee fred.log| natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
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
QUEUES=`natqlist | awk -F= '/^DEFINE=/ {
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
natalist | awk '{print  $0 "/\\\\" $0}'
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
natrun_shut() {
if [ ! -z "$NATRUN_PID" ]
then
    natrun_command halt
    NATRUN_PID=
    export NATRUN_PID
    STATE=DOWN
    export STATE
fi
}
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
HEAD=MAIN: Queue Management Main Menu (Supernat Version $SUPERNAT_VERSION)
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
USER:      User Administration/USER:
SETUP:     Queue Set-up/SETUP:
GRANT:     Queue User Grant Maintenance/GRANT:
JOBS:      Queue Contents Maintenance/JOBS:
CONTROL:   Scheduler Monitoring and Control/CONTROL:
EXIT:      Exit/EXIT:

HEAD=USER: User Administration Menu
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
NATAHELP:  User Administration Help/NATAHELP:
NATAALLOW: Allow Users Access/NATAALLOW:
NATADENY:  Deny Users Access/NATADENY:
NATALIST:  List Users who can Access/natalist
NATALISTL: List Users and their Access Rights/natalist -l
NATAVIEW:  View the SUPERNAT Activity Log/NATAVIEW:
EXIT:      Return/EXIT:

HEAD=SETUP: Queue Setup
PROMPT=Select Menu Option
PARENT=MAIN:
SEL_YES/COMM_YES/MENU
SCROLL
NATQHELP: Queue Setup Help/NATQHELP:
NATQADD:  Create a New Queue/NATQADD:
NATQDEL:  Delete a Queue/NATQDEL:
NATQCHG:  Change a Queue Definition/NATQCHG:
NATQLIST: List the Queue Definitions/natqlist
EXIT:     Return/EXIT:

HEAD=JOBS: Management of Jobs in a Queue
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
NATJHELP:     Job Management Help/NATJHELP:
NATQ:         List the Jobs in a Queue/NATQ:
NATQL:        Long List of the Jobs in a Queue/NATQL:
NATRM:        Remove Jobs from a Queue/NATRM:
NATHOLD:      Put Jobs on Hold/NATHOLD:
NATRELEASE:   Release Jobs on Hold/NATRELEASE:
NATEXPRESS:   Express Jobs/NATEXPRESS:
EXIT:         Return/EXIT:

HEAD=GRANT: Queue User Grants Maintenance
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
NATGHELP:  Queue User Grant Help/NATGHELP:
NATGALLOW: Allow Users Access to a Queue/NATGALLOW:
NATGDENY:  Deny Users Access to a Queue/NATGDENY:
NATGLIST:  List Users who can Access a Queue/NATGLIST:
NATGLISTL: List Users and their Access Rights/NATGLISTL:
EXIT:      Return/EXIT:

HEAD=NATAHELP: User Maintenance Help
PARENT=USER:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
nataallow.sh - allow user(s) to use Supernat/NULL:
        nataallow.sh <rwx> user.../NULL:
natadeny.sh - deny user(s) from using Supernat/NULL:
        natadeny.sh user.../NULL:
natalist - list users who can use Supernat/NULL:
        natalist [-l]/NULL:
nataview.sh - read activity log/NULL:
        nataview.sh/NULL:

HEAD=NATQHELP: Queue Setup Help
PARENT=SETUP:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
natqadd - create a new queue/NULL:
        natqadd queue  [[priority [max simul [start [end]]]] ...]/NULL:
natqdel - delete a queue/NULL:
        natqdel queue ... /NULL:
natqchg - change queue availability data (must replace all values)/NULL:
        natqchg queue [[priority [max simul [start [end]]]] ...]/NULL:
natqlist - list queues in the archive/NULL:
        natqlist/NULL:
See 'man natrun' for more explanation of queue availability data/NULL:

HEAD=NATJHELP: Queue Job Management Help
PARENT=JOBS:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
nat - Add jobs/NULL:
        nat <options> time job/NULL:
natrm - Remove job from a queue/NULL:
        natrm <options> job .../NULL:
natq - list queue jobs and status/NULL:
        natq <options>/NULL:
For an explanation of the options, see 'man nat', 'man natrm', 'man natq'/NULL:

HEAD=NATGHELP: Queue User Grant Maintenance Help
PARENT=GRANT:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
natgallow.sh - allow user(s) to use the queue/NULL:
        natgallow.sh queue <rwx> user.../NULL:
natgdeny.sh - deny user(s) from using queue/NULL:
        natgdeny.sh queue user.../NULL:
natglist - list users who can use the queue/NULL:
        natglist [-l] queue/NULL:

HEAD=NATAALLOW: Allow Users to Access the Queue Management System
PARENT=USER:
PROMPT=Fill in the desired fields and then Execute
COMMAND=nataperm
SEL_NO/COMM_YES/SYSTEM
SCROLL
Rights (use r,w,x and -, eg. rw-)
User to be granted access rights
User...
User...

HEAD=NATADENY: Deny Users Access to the Queue Management System
PARENT=USER:
PROMPT=Fill in the desired fields and then Execute
COMMAND=nataperm ---
SEL_NO/COMM_YES/SYSTEM
SCROLL
User to be denied access
User...
User...

HEAD=NATQADD: Define a New Queue
PARENT=SETUP:
PROMPT=Fill in the desired fields and then Execute
COMMAND=natqadd
SEL_NO/COMM_YES/SYSTEM
SCROLL
Queue to be created
zero or more (Priority,
Maximum simultaneous,
Start time,
End time)

HEAD=NATQDEL: Delete a Queue
PARENT=SETUP:
PROMPT=Fill in the desired fields and then Execute
COMMAND=natqdel
SEL_NO/COMM_YES/SYSTEM
SCROLL
Queue to be deleted

HEAD=NATQCHG: Change Queue Characteristics
PARENT=SETUP:
PROMPT=Fill in the desired fields and then Execute
COMMAND=natqchg                                                                                
SEL_NO/COMM_YES/SYSTEM
SCROLL
Queue to be Changed
zero or more (Priority
Maximum simultaneous
Start time
End time)

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
NATGLISTL:)
queue_select "NATGLIST: Select Queues for Long User Listing"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    for i in $QUEUES
    do
output_dispose "NATGLIST: Users and Permissions for Queue $i" natglist -l $i
    done
fi
;;
NATAVIEW:)
output_dispose "NATAVIEW: View Activity Log" cat $BASE_DIR/Activity
;;
NATGLIST:)
queue_select "NATGLIST: Select Queues for User Listing"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    for i in $QUEUES
    do
output_dispose "NATGLIST: Users for Queue $i" natglist $i
    done
fi
;;
NATGALLOW:)
queue_select "NATGALLOW: Select Queues for User Access Grant"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    user_select "NATGALLOW: Select Users for Access Grant"
    if [ ! -z "$USERS" ]
    then
perms=`natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=NATGALLOW: Allow Users Access to a queue
PARENT=GRANT:
PROMPT=Fill in the desired fields and then Execute
COMMAND= 
SEL_NO/COMM_NO/SYSTEM
SCROLL
Rights (use r,w,x and -, eg. rw-)

EOF
`
        if [ ! -z "$perms" ]
        then 
            for i in $QUEUES
            do
                eval natgperm $i $perms $USERS
            done
        fi
    fi
fi
;;
NATGDENY:)
queue_select "NATGDENY: Select Queues for User Access Removal"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    user_select "NATGDENY: Select Users for Access Removal"
    if [ ! -z "$USERS" ]
    then
        for i in $QUEUES
        do
            natgperm $i --- $USERS
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
NATEXPRESS:)
queue_select "NATEXPRESS: Select Queues to Act On"
if [ ! -z "$QUEUES" ]
then
    if [ "$QUEUES" = All ]
    then
        all_queues
    fi
    for i in $QUEUES
    do
        job_select "NATEXPRESS: Select Jobs for Express From Queue $i" $i
        if [ ! -z "$JOBS" ]
        then
            for j in $JOBS
            do
natcalc -w -j $j << EOF
EXPRESS($j);
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
#
# Scheduler Control Options
#
CONTROL:)
while :
do
opt=`(
echo HEAD="CONTROL: Control SUPERNAT Scheduler Operations ($STATE)"
echo PROMPT=Select Menu Option
echo SEL_YES/COMM_YES/MENU
echo SCROLL
if [ $ROOT_USER = Y -a  -z "$NATRUN_PID"  ]
then
echo START: Start the Scheduler/START:
fi
if [ ! -z "$NATRUN_PID" ]
then
echo PROC: View the Executing Processes/natps.sh
if [ $ROOT_USER = Y ]
then
echo SIML:  Change the Maximum Simultaneous Processes/SIML:
echo STATUS:  Display Scheduler Internal Status/STATUS:
echo STOP:  Shut down the Scheduler/STOP:
fi
fi
echo LOG: Process Log Files/LOG:
echo EXIT: Return/EXIT:
echo 
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
set -- $opt
case $1 in
EXIT:)
    break
    ;;
START:)
    natrun_start
    ;;
STOP:)
    natrun_shut
    ;;
SIML:)
    shift
sim=`natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=SIML: Change the Maximum Simultaneous Processes
PROMPT=Entered Desired Value
SEL_NO/COMM_NO/SYSTEM
SCROLL
The New Maximum:

EOF
`
    if [ ! -z "$sim" ]
    then
        natrun_command $sim
    fi
    ;;
STATUS:)
    natrun_command status 2>&1 | (
SHELL=/bin/true
export SHELL
pg -p '(Screen %d (h for help))' -s -
)
    ;;
LOG:)
    action_select "LOG: Process Log Files"
    case $ACTION in
    BROWSE:)
       head="LOG: Browse Selected Log Files"
       ;;
    PRINT:)
       head="LOG: Print Selected Log Files"
       ;;
    DELETE:)
       head="LOG: Delete Selected Log Files"
       ;;
    esac
    if [ "$ACTION" != "EXIT:" ]
    then
        file_select "$head" "$BASE_DIR/log/*.log"
        if [ ! -z "$FILE_LIST" ]
        then
            case $ACTION in
            BROWSE:)
               file_browse $FILE_LIST
               ;;
            PRINT:)
               lpr $FILE_LIST
               ;;
            DELETE:)
               rm -f $FILE_LIST
               ;;
            esac
        fi
    fi
    ;;
*)
    break
    ;;
esac
done
;;
*)
echo "Unrecognised Options..." $opt
sleep 5
exit
;;
esac
done
exit
