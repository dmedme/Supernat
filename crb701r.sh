#!/bin/ksh
#------------------------------------------------------------------------------
# Shellscript to execute Crib Reports in background. 
#------------------------------------------------------------------------------
#
# Author : A. Wilson
# Date   : 18th June 1992
# SCCS ID: %W%	(crb701r.sh)	%G%
#
# Parms  : 1. GIF Interface Name
#          2. Report Id.
#   The Rest. Parameters to pass to the report
#
#------------------------------------------------------------------------------
# Save the Interface and report names before shifting
#
spooldir='/u/cribbin/printspool/'
user=`whoami`
interface_name=$1
report_name=$2
pid=`echo $$`
#
# Run the report
#
shift 2
#
# Run pace report as executable, not sqlreportwriter
#
  if test -f /u/cribbin/${report_name}.rep
  then runrep term=/u/cribbin/srw_crib userid=/ desname=${report_name}.${pid}.lis report=/u/cribbin/${report_name} batch=yes "$@"
  else  /u/cribbin/${report_name} desname=${report_name}.${pid}.lis "$@"
  fi

case $? in
  0) sleep 0;;
  *) echo Oracle report terminated with response $? 
     exit 0;;
esac
#
# Either
# mail the report to the Unix user..
# or
# Send it via the specified GIF Interface
# If the process fails, move the file into the printspool directory
 
case $interface_name in 
  MAIL*) dest=`cat /u/cribbin/mail.aliases | grep ${user} | cut -c10-`
         mail -s "CRIB Mail" $dest < ${report_name}.${pid}.lis;;
 C[RT]*) gifsend ${interface_name} ${report_name}.${pid}.lis;;
      *) lpr -r -P${interface_name} ${report_name}.${pid}.lis
         exit 0;;
esac
case $? in
  0) rm -f ${report_name}.${pid}.lis;;
  *) mv ${report_name}.${pid}.lis ${spooldir}${interface_name}.${user}.${pid};;
esac
exit 0
#------------------------------------------------------------------------------
# Process Ends.
