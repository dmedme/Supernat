#!/bin/ksh
# Copyright (c) E2 Systems Limited 1999
#
# natuser.cgi - Web Access to Supernat queues
#
# This is an exact translation of the existing natuser.sh into a form that
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
<TITLE>Supernat Web Administration Tool (Version 3.3.7)</TITLE>
</HEAD>
<BODY background=/snat/images/e2back.jpg bgcolor=\"#40f5a0\">
<CENTER>
<TABLE WIDTH=\"98%\" CELLSPACING=1 CELLPADDING=4 BORDER=1>
<TR><TD BGCOLOR=\"#50ffc0\">
<A HREF=\"/cgi-bin/natuser.cgi\"><img src=\"/snat/images/e2button.gif\" alt=\"Supernat\"></A>
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
#
# The following turns the input on stdin into normal shell variables
# Embedded spaces in variable values are plus signs.
# **************************************************************************
# The POST processing below no longer works, so we use GET instead
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
    echo '<FORM name="snatform" action="/cgi-bin/natuser.cgi" method=get>
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
ROOT_USER=N
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
OVERVIEW:  List Currently Queued Jobs/natq -a
JOBS:      Job Management/JOBS:
PROC:      View Executing Supernat Processes/natps.sh
EXIT:      Exit/EXIT:" | do_form MAIN:
        exit
        ;;
    MAIN:)
        case "$pick" in
        JOBS:)
            echo "HEAD=JOBS: Management of Jobs in a Queue
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
NATQ:         List the Jobs in a Queue/NATQ:
NATRM:        Remove Jobs from a Queue/NATRM:
NATHOLD:      Put Jobs on Hold/NATHOLD:
NATRELEASE:   Release Jobs on Hold/NATRELEASE:
EXIT:      Return/EXIT:" | do_form JOBS:
            exit
            ;;
        natps.sh)
            output_dispose "PROC: View Executing Supernat Processes" natps.sh
        ;;
        "natq -a")
            output_dispose "OVERVIEW: List Currently Queued Jobs" natq -a
        ;;
        *)
            sub_menu=""
            ;;
        esac
        ;;
    JOBS:)
        case "$pick" in
        NATQ:)
            queue_select_1 "NATQ: Select Queues for Listing"
            exit
            ;;
        NATHOLD:|NATRELEASE:|NATRM:)
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
    queue_select_2)
        queue_select_2
        case "$parent" in
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
        esac
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
echo '<FORM name="snatform" action="/cgi-bin/natuser.cgi" method=get>
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
    *)
        output_dispose "SUPERNAT Management System" echo Unrecognised Options... $sub_menu $pick
        exit
        ;;
    esac
done
#} 2>/tmp/junk$$.log
} 2>/dev/null
exit
