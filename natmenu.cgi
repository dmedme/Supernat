#!/bin/sh
# Copyright (c) E2 Systems Limited 1999
#
# natmenu.cgi - Master CGI Administrative Menu for supernat
#
# This is an exact translation of the existing natmenu.sh into a form that
# can be used with Apache.
#
# This is a prototype. There are no personalised access controls present, and
# the SUPERNAT documentation is not linked in, as it should be.
#
# The brackets round the whole thing allow the stderr (and hence, an execution
# trace) to be captured, for debugging purposes.
#
# Known Problems
# --------------
# Just the original weaknesses of natmenu. No validation of input
# variable format, no way of editing job attributes, etc. etc. 
#
# Untested sections
# -----------------
# The script components for stopping and starting SUPERNAT have not been
# tested, since the CGI script does not run as root.
#
{
trap 'echo "Abort Command Given"
exit 0' 1 2 3 15
# ****************************************************************************
# Standard Web Support Stuff
#
# Standard Header put on all the pages.
#
html_head="Content-type: text/html



<HTML>
<HEAD>
<TITLE>Supernat Web Administration Tool</TITLE>
</HEAD>
<BODY background=/snat/images/e2back.jpg bgcolor=\"#40f5a0\">
<CENTER>
<TABLE WIDTH=\"98%\" CELLSPACING=1 CELLPADDING=4 BORDER=1>
<TR><TD BGCOLOR=\"#50ffc0\">
<A HREF=\"/cgi-bin/natmenu.cgi\"><img src=\"/snat/images/e2button.gif\" alt=\"Supernat\"></A>
<HR>
<h3>Welcome to SUPERNAT!</h3>
<p>Choose an action or press the above button</p>"
#
# Footer
#
html_tail='</BODY>
</HTML>'
set -x
# **************************************************************************
# Set the Input Variables
# **************************************************************************
# The following turns the input on stdin into normal shell variables
# Embedded spaces in variable values are plus signs.
# This POST processing doesn't work any more, so we have switched to GET.
#eval `sed 's/&/ /g
#s=%2F=/=g
#s/%3A/:/g'`
if [ -n "$QUERY_STRING" ]
then
# ***********************************************************************
# Put the variables in the environment. The code should be proof against
# malicious insertion of shell meta-characters
    eval `echo $QUERY_STRING | sed 's=%2F=/=g
s/'\''/&\\&&/g
s/%3A/:/g
s/+/ /g' | nawk 'BEGIN {RS="&";FS="="}
$1 ~ /^[a-zA-Z][a-zA-Z0-9_]*$/ { printf "%s=%c%s%c\n",$1,39,$2,39}' `  
fi
# ************************************************************************
# Form Definition Function that replaces natmenu
#
# This generates three types of input form, corresponding to the three
# types of form natmenu handled:
# - A straightforward menu
# - Picking off a list
# - An input form
#
# Input parameters:
# 1 - The sub_menu variable, which selects the appropriate branch in
#     the main case statement when the data comes back
# 2 ... Any other context information needed to be able to continue the
#     processing after the next user submission
#
do_form() {
    sub_menu=$1
    shift
    extra_args=$*
    set +x
    echo "$html_head"
    echo '<FORM name="snatform" action="/cgi-bin/natmenu.cgi" method=get>
<input type="hidden" name="sub_menu" value="'$sub_menu'">
<p>'
if [ ! -z "$extra_args" ]
then
echo '<input type="hidden" name="extra_args" value="'$extra_args'">'
fi
nawk 'BEGIN {FS="="
ln = 1
}
#{ print "Input: " $0 }
/HEAD=/ { print "<p>"
print "<table>"
print "<tr><td></td></tr><tr><td><b><u>" $2 "</u></b></td></tr>"
next
}
/PARENT=/ { 
print "<input type=\"hidden\" name=\"parent\" value=\"" $2 "\">"
}
/PROMPT=/ { prompt = $2}
/COMMAND=/ {
print "<input type=\"hidden\" name=\"command\" value=\"" $2 "\">"
next
}
/SEL_NO/ { sel=0 }
/SEL_YES/ { sel=1 }
/COMM_NO/ { comm=0 }
/COMM_YES/ { comm=1 }
/SYSTEM/ {menu=0}
/MENU/ { menu= 1}
/SCROLL/ { FS = "/"
print "<input type=\"hidden\" name=\"sel\" value=\"" sel "\">"
print "<input type=\"hidden\" name=\"comm\" value=\"" comm "\">"
print "<input type=\"hidden\" name=\"menu\" value=\"" menu "\">"
next
}
FS == "/" {
if (sel == 0)
{
print "<tr><td>" $1 "&nbsp;&nbsp; </td><td><input type=text size=40 name=\"p" ln "\" value=\"" $2 "\"></td></tr>" 
}
else
if (sel == 1 && comm == 1)
{
print "<tr><td><input type=\"submit\" name=\"pick\" value=\"" $2 "\"></td><td>" $1 "</td></tr>"
}
else
if (sel == 1 && comm == 0)
{
print "<tr><td><input type=\"hidden\" name=\"p" ln "\" value=\"\"><input type=\"checkbox\" value=\"Pick\" onClick=\"snatform.p" ln ".value='\''" $2 "'\''\"></td><td>" $1 "</td></tr>"
}
ln++
}
END {
if (sel == 0 || comm == 0)
{
print "<tr><td><input type=\"submit\" name=\"Submit\" value=\"Submit\"></td></tr>"
}
print "<tr><td><input type=\"Reset\" name=\"Reset\" value=\"Reset\"></td></tr>" 
print "</TABLE>"
print "<input type=\"hidden\" name=\"ln_cnt\" value=\"" ln "\">"
print "</FORM>"
print prompt
}'
echo $html_tail
return
}
#
# Establish the local environment
# On SCO, you need a full path name. Normally, . will search the
# execution path.
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
# Function to start the scheduler
#
natrun_start_1 () {
if [ $ROOT_USER = N ]
then
    echo "Only root can start the scheduler"
    return
fi
natrun_shut
rm -f $BASE_DIR/supernat_fifo
echo 'HEAD=START:    Start the SUPERNAT Scheduler
PROMPT=Fill in the desired fields and then Execute
PARENT=$parent
COMMAND=START:
SEL_NO/COMM_NO/SYSTEM
SCROLL
Enter the simultaneous jobs maximum
Timestamped output logs (blank for no)' | do_form natrun_start_2
exit 0
}
natrun_start_2 () {
if [ "$p1" = "" ]
then
    simargs=""
else
    simargs=-s$p1
    if [ "$p2" = "" ]
    then
        :
    else
        simargs="$simargs -t"
    fi
fi
SIMARGS=$simargs
export SIMARGS
natstart.sh
get_natrun_pid
return
}
#
# Function to allow a user to select from a list of files
# Arguments:
# 1 - The File Selection Header
# 2 - The File Selection String
#
file_select_1() {
head=$1
sel=$2
{
echo "HEAD=$head
PROMPT=Select Files, and Press Return
COMMAND=$sel
PARENT=$parent
SEL_YES/COMM_NO/SYSTEM
SCROLL
All/All"
ls -lt $sel 2>&1 | sed '/\// s=\(.* \)\([^ ]*/\)\([^/]*\)$=\1\3/\2\3=
/\//!{
s=.* \([^ ]*\)$=&/\1=
}'
} | do_form file_select_2
exit 0
}
file_select_2() {
if [ "$p1" = "All" ]
then
    FILE_LIST=`ls $command 2>&1 `
else
i=2
FILE_LIST=""
while [ "$i" -lt "$ln_cnt" ]
do
    eval FILE_LIST=\"$FILE_LIST \$p$i\"
    i=`expr $i + 1`
done
fi
export FILE_LIST
return
}
#
# Function to allow a user to select from a list of jobs
# Arguments:
# 1 - The Header
# 2 - The Queue
# 3 ... The other queues.
#
job_select_1() {
head="$1"
queue=$2
shift
extra_args=$*
{
echo "HEAD=$head
PROMPT=Select Jobs, and Press Return
PARENT=$parent
COMMAND= 
SEL_YES/COMM_NO/SYSTEM
SCROLL
All/All"
natq -q $queue | sed '/\// s=/=.=g' | nawk '/^[RWHGSF]/ { print substr($0,1,78) "/" $8 }'
} | do_form job_select_2 $extra_args
exit
}
job_select_2() {
if [ "$p1" = "All" ]
then
    JOBS=`natq -q $queue | nawk '/^[RWHGSF]/ { print $8 }'`
else
i=2
JOBS=""
while [ "$i" -lt "$ln_cnt" ]
do
    eval JOBS=\"$JOBS \$p$i\"
    i=`expr $i + 1`
done
fi
export JOBS
return
}
#
# Function to handle output in the standard manner
# Arguments: 1 - Heading
# The others: Command to execute
#
output_dispose() {
echo "$html_head"
echo "<h3>$1</h3>"
shift
eval $* 2>&1 | sed 's=.*=<p>&</p>='
echo $html_tail
exit
}
# Function to allow a user to select the action to apply to selected files
#
# Arguments:
# 1 - The Action Selection Header
#
action_select_1() {
echo "HEAD=$1
PROMPT=Select Action, and Press Return
PARENT=$parent
SEL_YES/COMM_YES/MENU
SCROLL
BROWSE: View the Selected Items/BROWSE:
PRINT : Print the Selected Items/PRINT:
DELETE: Delete the Selected Items/DELETE:
EXIT  : Exit without doing anything/EXIT:" | do_form action_select_2
exit
}
#
# Function to allow browsing of files, with no SHELL escape
# Arguments:
# 1 - List of files to browse
#
file_browse() {
echo "$html_head"
for i in $BROWSE_FILES
do
echo "<h3>File: $1</h3>"
cat $i | sed 's=.*=<p>&</p>='
done
echo $html_tail
exit
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
queue_select_1() {
head=$1
all_queues
{
echo "HEAD=$head
PROMPT=Select Queues, and Press Return
PARENT=$pick/$parent
COMMAND= 
SEL_YES/COMM_NO/SYSTEM
SCROLL
All/All"
for i in $QUEUES
do
    echo $i/$i
done
} | do_form queue_select_2
exit
}
queue_select_2() {
if [ "$p1" = "All" ]
then
    all_queues
else
i=2
QUEUES=""
while [ "$i" -lt "$ln_cnt" ]
do
    eval QUEUES=\"$QUEUES \$p$i\"
    i=`expr $i + 1`
done
fi
export QUEUES
return
}
#
# Function to select a user from a list
#
user_select_1() {
head=$1
shift
{
echo "HEAD=$1 - Pick Users
PARENT=$parent
PROMPT=Select Users, and Press Return
SEL_YES/COMM_NO/SYSTEM
SCROLL
All/All"
natalist | nawk '{print  $0 "/\\\\" $0}'
} |do_form user_select_2 $*
exit
}
user_select_2() {
if [ "$p1" = "All" ]
then
    USERS=`natalist`
else
i=2
USERS=""
while [ "$i" -lt "$ln_cnt" ]
do
    eval USERS=\"$USERS \$p$i\"
    i=`expr $i + 1`
done
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
# ****************************************************************************
# Main control - process input options
#
while :
do
    case "$sub_menu" in
    "")
        echo "HEAD=MAIN: Queue Management Main Menu
PROMPT=Select Menu Option
PARENT=MAIN:
SEL_YES/COMM_YES/MENU
SCROLL
USER:      User Administration/USER:
SETUP:     Queue Set-up/SETUP:
GRANT:     Queue User Grant Maintenance/GRANT:
JOBS:      Queue Contents Maintenance/JOBS:
CONTROL:   Scheduler Monitoring and Control/CONTROL:
EXIT:      Exit/EXIT:" | do_form MAIN:
        exit
        ;;
    MAIN:)
        case "$pick" in
        USER:)
            echo "HEAD=USER: User Administration Menu
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
EXIT:      Return/EXIT:" | do_form USER:
            exit
            ;;
        SETUP:)
            echo "HEAD=SETUP: Queue Setup
PROMPT=Select Menu Option
PARENT=MAIN:
SEL_YES/COMM_YES/MENU
SCROLL
NATQHELP: Queue Setup Help/NATQHELP:
NATQADD:  Create a New Queue/NATQADD:
NATQDEL:  Delete a Queue/NATQDEL:
NATQCHG:  Change a Queue Definition/NATQCHG:
NATQLIST: List the Queue Definitions/natqlist
EXIT:      Return/EXIT:" | do_form SETUP:
            exit
            ;;
        JOBS:)
            echo "HEAD=JOBS: Management of Jobs in a Queue
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
EXIT:      Return/EXIT:" | do_form JOBS:
            exit
            ;;
        GRANT:)
            echo "HEAD=GRANT: Queue User Grants Maintenance
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
NATGHELP:  Queue User Grant Help/NATGHELP:
NATGALLOW: Allow Users Access to a Queue/NATGALLOW:
NATGDENY:  Deny Users Access to a Queue/NATGDENY:
NATGLIST:  List Users who can Access a Queue/NATGLIST:
NATGLISTL: List Users and their Access Rights/NATGLISTL:
EXIT:      Return/EXIT:" | do_form GRANT:
            exit
            ;;
#
# Scheduler Control Options
#
        CONTROL:)
            get_natrun_pid
            {
                echo "HEAD=CONTROL: Control SUPERNAT Scheduler Operations ($STATE)
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL"
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
                echo "LOG: Process Log Files/LOG:
EXIT: Return/EXIT:"
            } | do_form CONTROL:
            exit
            ;;
        *)
            sub_menu=""
            ;;
        esac
        ;;
    USER:)
        case "$pick" in
        NATAHELP:) 
            echo "HEAD=NATAHELP: User Maintenance Help
PARENT=USER:/MAIN:
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
        nataview.sh/NULL:" | do_form NATAHELP:
            exit
            ;;
        NATAALLOW:)
            echo "HEAD=NATAALLOW: Allow Users Access to the Queue Management System
PARENT=USER:/MAIN:
PROMPT=Fill in the desired fields and then Execute
COMMAND=nataperm
SEL_NO/COMM_YES/SYSTEM
SCROLL
Rights (use r,w,x and -, eg. rw-)
User to be granted access rights
User...
User..." | do_form NATAALLOW:
             exit
             ;;
        NATADENY:)
            echo "HEAD=NATADENY: Deny Users Access to the Queue Management System
PARENT=USER:
PROMPT=Fill in the desired fields and then Execute
COMMAND=nataperm ---
SEL_NO/COMM_YES/SYSTEM
SCROLL
User to be denied access
User...
User..." | do_form NATADENY:
            exit
            ;;
        natalist*)
            output_dispose "NATALIST: List Users who Can Access SUPERNAT" $p1
            exit
            ;;
        NATAVIEW:)
            output_dispose "NATAVIEW: View Activity Log" cat $BASE_DIR/Activity
            exit
            ;;
        EXIT:)
            sub_menu=
            ;;
        *)
            sub_menu="MAIN:"
            pick="USER:"
            ;;
        esac
        ;;
    SETUP:)
        case "$pick" in
        NATQHELP:) 
            echo "HEAD=NATQHELP: Queue Setup Help
PARENT=SETUP:/MAIN:
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
See 'man natrun' for more explanation of queue availability data/NULL:" | do_form NATQHELP:
            exit
            ;;
        NATQADD:)
            echo "HEAD=NATQADD: Define a New Queue
PARENT=SETUP:
PROMPT=Fill in the desired fields and then Execute
COMMAND=natqadd
SEL_NO/COMM_YES/SYSTEM
SCROLL
Queue to be created
zero or more (Priority,
Maximum simultaneous,
Start time,
End time)" | do_form NATQADD:
            exit
            ;;
        NATQDEL:)
            echo "HEAD=NATQDEL: Delete a Queue
PARENT=SETUP:
PROMPT=Fill in the desired fields and then Execute
COMMAND=natqdel
SEL_NO/COMM_YES/SYSTEM
SCROLL
Queue to be deleted" | do_form NATQDEL:
            exit
            ;;
        NATQCHG:)
            echo "HEAD=NATQCHG: Change Queue Characteristics
PARENT=SETUP:
PROMPT=Fill in the desired fields and then Execute
COMMAND=natqchg
SEL_NO/COMM_YES/SYSTEM
SCROLL
Queue to be Changed
zero or more (Priority
Maximum simultaneous
Start time
End time)" | do_form NATQCHG:
            exit
            ;;
        natqlist*)
            output_dispose "NATQLIST: List Queue Definitions" $pick
            exit
            ;;
        EXIT:)
            sub_menu=
            ;;
        *)
            sub_menu="MAIN:"
            pick="SETUP:"
            ;;
        esac
        ;;
    NATQADD:|NATQDEL:|NATQCHG:|NATAALLOW:|NATADENY:)
        args=""
        i=1
        while [ "$i" -lt "$ln_cnt" ]
        do
            eval args=\"$args \$p$i\"
            i=`expr $i + 1`
        done
        case "$command" in
        natqadd)
            output_dispose "NATQADD: Adding Queue Definition" $command $args
            exit
            ;;
        natqdel)
            output_dispose "NATQDEL: Removing Queue Definition" $command $args
            exit
            ;;
        natqchg)
            output_dispose "NATQCHG: Changing Queue Definition" $command $args
            exit
            ;;
        nataperm)
            output_dispose "NATAALLOW: Setting User Permissions" $command $args
            exit
            ;;
        nataperm*)
            output_dispose "NATADENY: Revoking User Permissions" $command "$args"
            exit
            ;;
        esac
        ;;
    JOBS:)
        case "$pick" in
        NATJHELP:) 
            echo "HEAD=NATJHELP: Queue Job Management Help
PARENT=JOBS:/MAIN:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
nat - Add jobs/NULL:
        nat <options> time job/NULL:
natrm - Remove job from a queue/NULL:
        natrm <options> job .../NULL:
natq - list queue jobs and status/NULL:
        natq <options>/NULL:
For an explanation of the options, see 'man nat', 'man natrm', 'man natq'/NULL:" | do_form NATJHELP
            exit
            ;;
        NATQ:)
            queue_select_1 "NATQ: Select Queues for Listing"
            exit
            ;;
        NATQL:)
            queue_select_1 "NATQL: Select Queues for Long Listing"
            exit
            ;;
        NATHOLD:|NATRELEASE:|NATEXPRESS:|NATRM:)
            queue_select_1 "$pick Select Queues to Act On"
            exit
            ;;
        EXIT:)
            sub_menu=
            ;;
        *)
            sub_menu="MAIN:"
            pick="JOBS:"
            ;;
        esac
        ;;
    GRANT:)
        case "$pick" in
        NATGHELP:) 
            echo "HEAD=NATGHELP: Queue User Grant Maintenance Help
PARENT=GRANT:/MAIN:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
natgallow.sh - allow user(s) to use the queue/NULL:
        natgallow.sh queue <rwx> user.../NULL:
natgdeny.sh - deny user(s) from using queue/NULL:
        natgdeny.sh queue user.../NULL:
natglist - list users who can use the queue/NULL:
        natglist [-l] queue/NULL:" | do_form NATGHELP:
            exit
            ;;
        NATGLISTL:)
            queue_select_1 "NATGLIST: Select Queues for Long User Listing"
            exit
            ;;
        NATGLIST:)
            queue_select_1 "NATGLIST: Select Queues for User Listing"
            exit
            ;;
        NATGALLOW:)
            queue_select_1 "NATGALLOW: Select Queues for User Access Grant"
            exit
            ;;
        NATGDENY:)
            queue_select_1 "NATGDENY: Select Queues for User Access Removal"
            exit
            ;;
        EXIT:)
            sub_menu=
            ;;
        *)
            sub_menu="MAIN:"
            pick="GRANT:"
            ;;
        esac
        ;;
    CONTROL:)
        case $pick in
        EXIT:)
            sub_menu=""
            ;;
        START:)
            output_dispose "START: Starting the Scheduler" natrun_start
            exit
            ;;
        STOP:)
            output_dispose "STOP: Stopping the Scheduler" natrun_stop
            exit
            ;;
        SIML:)
            echo "HEAD=SIML: Change the Maximum Simultaneous Processes
PROMPT=Entered Desired Value
SEL_NO/COMM_NO/SYSTEM
SCROLL
The New Maximum:" | do_form change_simul
            exit
            ;;
        STATUS:)
            output_dispose "STATUS: Querying the Scheduler Status" natrun_command status
            exit
            ;;
        LOG:)
            action_select_1 "LOG: Process Log Files"
            exit
            ;;
        natps.sh)
            output_dispose "CONTROL: Processing the Request" natps.sh
            ;;
        *)
            output_dispose "CONTROL: Processing the Request" $p1
            ;;
        esac
        sub_menu=""
        ;;
    change_simul)
        output_dispose "SIML: Setting the maximum simultaneous to $p1" natrun_command $p1
        exit
        ;;
    action_select_2)
        ACTION=$pick
        parent="$ACTION/$parent"
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
            file_select_1 "$head" "$BASE_DIR/log/*.log"
            exit
        fi
        sub_menu="MAIN:"
        pick="CONTROL:"
        ;;
    file_select_2)
        file_select_2
        if [ ! -z "$FILE_LIST" ]
        then
            case $parent in
            BROWSE:*)
               file_browse $FILE_LIST
               exit
               ;;
            PRINT:*)
               lpr $FILE_LIST
               ;;
            DELETE:*)
               rm -f $FILE_LIST
               ;;
            esac
        fi
        sub_menu="MAIN:"
        pick="CONTROL:"
        ;;
    queue_select_2)
        queue_select_2
        case "$parent" in
        NATQL:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    output_dispose "NATQL: Long Listing All Queues" natq -al
                else
                    echo "$html_head"
                    echo "<h3>NATQL: Long Listing for Selected Queues</h3>"
                    for i in $QUEUES
                    do
                        natq -l -q $i
                    done | sed 's=.*=<p>&</p>='
                    echo $html_tail
                    exit
                fi
            fi
            pick="JOBS:"
            sub_menu="MAIN:"
            ;;
        NATQ:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    output_dispose "NATQ: List All Queues" natq -a
                else
                    echo "$html_head"
                    echo "<h3>NATQ:Listing for Selected Queues</h3>"
                    for i in $QUEUES
                    do
                        natq -q $i
                    done |sed 's=.*=<p>&</p>='
                    echo $html_tail
                    exit
                fi
            fi
            pick="JOBS:"
            sub_menu="MAIN:"
            ;;
        NATHOLD:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                set $QUEUES
                job_select_1 "NATHOLD: Select Jobs for Holding in Queue $1" $*
                exit
            fi
            pick="JOBS:"
            sub_menu="MAIN:"
            ;;
        NATEXPRESS:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                set $QUEUES
                job_select_1 "NATEXPRESS: Select Jobs for Express treatment in Queue $1" $*
                exit
            fi
            pick="JOBS:"
            sub_menu="MAIN:"
            ;;
        NATRELEASE:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                set $QUEUES
                job_select_1 "NATRELEASE: Select Jobs for Release in Queue $1" $*
                exit
            fi
            pick="JOBS:"
            sub_menu="MAIN:"
            ;;
        NATRM:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                set $QUEUES
                job_select_1 "NATRM: Select Jobs for Removal in Queue $1" $*
                exit
            fi
            pick="JOBS:"
            sub_menu="MAIN:"
            ;;
        NATGLISTL:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                echo "$html_head"
                echo "<h3>NATGLISTL:Users and Permissions Listing for Selected Queues</h3>"
                for i in $QUEUES
                do
                    natglist -l $i
                done | sed 's=.*=<p>&</p>='
                echo $html_tail
                exit
            fi
            pick="GRANT:"
            sub_menu="MAIN:"
            ;;
        NATGLIST:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                echo "$html_head"
                echo "<h3>NATGLIST:Users and Permissions Listing for Selected Queues</h3>"
                for i in $QUEUES
                do
                    natglist $i
                done | sed 's=.*=<p>&</p>='
                echo $html_tail
                exit
            fi
            pick="GRANT:"
            sub_menu="MAIN:"
            ;;
        NATGALLOW:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                user_select_1 "NATGALLOW: Select Users for Access Grant" $QUEUES
            fi
            pick="GRANT:"
            sub_menu="MAIN:"
            ;;
        NATGDENY:*)
            if [ ! -z "$QUEUES" ]
            then
                if [ "$QUEUES" = All ]
                then
                    all_queues
                fi
                user_select_1 "NATGDENY: Select Users for Access Revoke" $QUEUES
            fi
            pick="GRANT:"
            sub_menu="MAIN:"
            ;;
        esac
        ;;
    user_select_2)
        user_select_2
        case $parent in
        NATGALLOW:*)
            if [ ! -z "$USERS" ]
            then
                QUEUES=`echo $extra_args | sed 's/+/ /g'`
                echo "HEAD=NATGALLOW: Allow Users Access to a queue
PARENT=GRANT:/MAIN:
PROMPT=Fill in the desired fields and then Execute
COMMAND= 
SEL_NO/COMM_NO/SYSTEM
SCROLL
Rights (use r,w,x and -, eg. rw-)" | do_form natgallow_2 $QUEUES USERS $USERS
                exit
            fi
            pick="GRANT:"
            sub_menu="MAIN:"
            ;;
        NATGDENY:*)
            if [ ! -z "$USERS" ]
            then
                QUEUES=`echo $extra_args | sed 's/+/ /g'`
                echo "$html_head"
                echo "<h3>NATGDENY:Revoking access for $* on $QUEUES</h3>"
                for i in $QUEUES
                do
                    eval natgperm $i --- $*
                done | sed 's=.*=<p>&</p>='
                echo $html_tail
                exit
            fi
            pick="GRANT:"
            sub_menu="MAIN:"
            ;;
        esac
        ;;
    natgallow_2)
        perms=$p1
        if [ ! -z "$perms" ]
        then 
            set `echo $extra_args | sed 's/+/ /g'`
            QUEUES=""
            until [ $1 = USERS ]
            do
                QUEUES="$QUEUES $1"
                shift
            done
            shift
            echo "$html_head"
            echo "<h3>NATGALLOW:Granting $perms to $* on $QUEUES</h3>"
            for i in $QUEUES
            do
                eval natgperm $i $perms $*
            done | sed 's=.*=<p>&</p>='
            echo $html_tail
            exit
        fi
        pick="GRANT:"
        sub_menu="MAIN:"
        ;;
    job_select_2)
        job_select_2
        QUEUES=`echo $extra_args | sed 's/+/ /g'`
        if [ ! -z "$QUEUES" ]
        then
            if [ "$QUEUES" = All ]
            then
                all_queues
            fi
            set $QUEUES
            echo "$html_head"
            case $parent in
            NATHOLD:*)
                echo "<h3>NATHOLD:Holding jobs $JOBS in queue $1</h3>"
                for j in $JOBS
                do
                     natcalc -w -j $j << EOF
HOLD($j);
EOF
                done 2>&1 | sed 's=.*=<p>&</p>='
                ;;
            NATRELEASE:*)
                echo "<h3>NATRELEASE:Releasing jobs $JOBS in queue $1</h3><p>"
                for j in $JOBS
                do
                    natcalc -w -j $j << EOF
RELEASE($j);
EOF
                done 2>&1 | sed 's=.*=<p>&</p>='
                ;;
            NATEXPRESS:*)
                echo "<h3>NATEXPRESS:Expressing jobs $JOBS in queue $1</h3><p>"
                for j in $JOBS
                do
natcalc -w -j $j << EOF
EXPRESS($j);
EOF
                done 2>&1 | sed 's=.*=<p>&</p>='
                ;;
            NATRM:*)
                echo "<h3>NATRM:Removing jobs $JOBS in queue $1</h3><p>"
                natrm -q $1 $JOBS 2>&1 | sed 's=.*=<p>&</p>='
                ;;
            esac
            shift
            if [ $# -gt 0 ]
            then
#
# Need to output continuation details
#
echo '<FORM name="snatform" action="/cgi-bin/natmenu.cgi" method=get>
<input type="hidden" name="sub_menu" value="job_select_1">
<input type="hidden" name="extra_args" value="'$*'">
<input type="hidden" name="parent" value="'$parent'">
<input type="submit" name="Continue" value="Continue">
</FORM>'
            fi
            echo "</p>"
            echo $html_tail
            exit
        fi
        menu="JOBS:"
        sub_menu="MAIN:"
        ;;
    job_select_1)
        set `echo $extra_args | sed 's/+/ /g'`
        case $parent in
        NATHOLD:*)
            job_select_1 "NATHOLD: Select Jobs for Holding in Queue $1" $*
            exit
            ;;
        NATEXPRESS:*)
            job_select_1 "NATEXPRESS: Select Jobs for Express treatment in Queue $1" $*
            exit
            ;;
        NATRELEASE:*)
            job_select_1 "NATRELEASE: Select Jobs for Release in Queue $1" $*
            exit
            ;;
        NATRM:*)
            job_select_1 "NATRM: Select Jobs for Removal in Queue $1" $*
            exit
            ;;
        esac
        pick="JOBS:"
        sub_menu="MAIN:"
        ;;
    *HELP)
        case $sub_menu in
        NATG*)
            pick=GRANT:
            ;;
        NATQ*)
            pick=SETUP:
            ;;
        NATA*)
            pick=USERS:
            ;;
        NATJ*)
            pick=JOBS:
            ;;
        esac
        sub_menu=MAIN:
        ;;
    *)
        output_dispose "SUPERNAT Management System" echo Unrecognised Options... $sub_menu $pick
        exit
        ;;
    esac
done
} 2>/tmp/junk$$.log
exit
