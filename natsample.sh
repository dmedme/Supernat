:
#!/bin/sh5
# @(#) $Name$ $Id$
#Copyright (c) E2 Systems, 1991
#
# This file contains an example of how to use the forms handling capabilities
#
# To get immediate return of the form values, specify COMM_NO
#
trap '' 2 3 4 5 6 7 8 10 12 13 14 15
# Do not trap HANGUP! You can easily end up with an infinite loop, if called
# programs do not allow for reading end of file off a terminal (eg. ORACLE
# SQL*FORMS)
head=`date`
while true
do
    opt=`natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=SEARCH: Look for text in files    $head
PROMPT=Leave blank to exit
COMMAND=grep
SEL_NO/COMM_NO/SYSTEM
SCROLL
What do you want to look for
Case insensitive (Y or N)
Where do you want to look
EOF
`
#
# The COMMAND option must not be blank if SYSTEM is specified
#
    set -- $opt
    if [ $# -lt 4 ]
    then
        break
    else
        opt=$2
    fi
    if [ $3 = Y ]
    then
        flag="-i"
    else
        flag=
    fi
    shift
    shift
    shift
#
# Using natmenu as a browser that doesn't allow shell escapes
#
    (
        cat << EOF
HEAD=SEARCH: Output    $head
PROMPT=Press Return for another search
COMMAND=
SEL_NO/COMM_NO/MENU
SCROLL
EOF
        grep $flag $opt $*  
        echo End of Search
    ) | natmenu 3<&0 4>>/dev/null </dev/tty >/dev/tty
done
exit
