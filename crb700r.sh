#!/bin/ksh
#------------------------------------------------------------------------------
# Shellscript to execute Crib Reports in background. 
#------------------------------------------------------------------------------
#
# Author : A. Wilson
# Date   : 18th June 1992
# SCCS ID: %W%		(crb700r.sh)	%G%
#
# Parms  :  1. GIF Interface Name
#           2. Report Id. or 'SCREEN'
#    The Rest. Parameters to pass to 'runrep'
#
#------------------------------------------------------------------------------
# If the first parameter = 'PORT' or the second parameter is SCREEN
#   then the process MUST be run in foreground.
# Otherwise, the report can be run by crb701r.sh in background
#------------------------------------------------------------------------------
# Set up a few variables
#------------------------------------------------------------------------------
integer resp

spooldir='/u/cribbin/printspool/'
user=`whoami`
UID=`getuid`
dest=`cat /u/cribbin/mail.aliases | grep ${user} | cut -c10-`
integer dlen=${#dest}
pid=`echo $$`
alias nothing='sleep 0'
#------------------------------------------------------------------------------
# On-Screen Reports
#
# If the second (optional) parameter is SCREEN, then
#   Run the report in foreground,
#   Send to Destination (either PORT or GIFSEND)
#        If a bad response while gifsending, copy file into spool directory
#   Display the report using 'browse'
#------------------------------------------------------------------------------
case $2 in
SCREEN)
  iface_name=$1
  report_name=$3
  if test -f ${report_name}.lis
     then rm ${report_name}.lis
  fi
  shift 3
#
# Run Pace Report as executable, not sqlreportwriter
#
  if test -f /u/cribbin/${report_name}.rep
  then runrep term=/u/cribbin/srw_crib userid=/ desname=${report_name}.lis report=/u/cribbin/${report_name} "$@"
  else  /u/cribbin/${report_name} desname=${report_name}.lis "$@"
  fi

 resp=$? 
 if test ${resp} -ne 0
   then echo "Oracle Report Terminated with error code ${resp}...press Return"
        read -r
        rem_uid $UID
        exit 0
  fi
  case $iface_name in
   PORT*) echo "Transmitting report to PORT..."
          portprn ${report_name}.lis
          resp=$?
          if test ${resp} -ne 0
            then echo "Error code ${resp} during PORT printing   press Return..."
                 read -r
          fi;;
   SOFT*) nothing;;
       *) echo "Routing report to ${iface_name}"
          case ${iface_name} in
            C[RT]*) gifsend ${iface_name} ${report_name}.lis;;
                 *) lpr -r -P${iface_name} ${report_name}.lis;;
          esac
          resp=$?
          if test ${resp} -ne 0
            then echo "Error code $resp sending report to Interface ${iface_name}  press Return..."
              read -r
          fi;;
  esac
  browse ${report_name}.lis
  if test $? -ne 0
    then echo "Browse Terminated with error code $?"
  fi
  sleep 1
  rem_uid $UID
  exit 0;;
#------------------------------------------------------------------------------
# Normal type reports
# ie. those which don't have SCREEN as the second parameter
#------------------------------------------------------------------------------
*)
report_name=$2
if test -f ${report_name}.out
   then rm ${report_name}.out
fi
case $1 in
 MAIL*) if ((dlen > 0))
          then
     job="/u/cribbin/crb701r.sh $@ 1>>${report_name}.out 2>>${report_name}.out >>${report_name}.out" 
       nat -i -j "job$$" -x "$job"
        else
          echo "           MAILING TO ALL-IN-ONE Account"
          echo "           -----------------------------"
          echo
          echo "You have not been set up to mail CRIB Reports to All-in-one"
          echo "Please contact Helpdesk (6565) if you require this function"
        fi;;
 PORT*) shift 2
        if test -f /u/cribbin/${report_name}.rep
        then runrep term=/u/cribbin/srw_crib userid=/ desname=${report_name}.lis report=/u/cribbin/${report_name} "$@"
        else  /u/cribbin/${report_name} desname=${report_name}.lis "$@"
        fi
        resp=$?
        if test ${resp} -ne 0
           then echo "Oracle Report Terminated with error code ${resp}...press Return\c"
                read -r
  		rem_uid $UID
                exit 0
        fi
        echo "Transmitting report to PORT..."
        sleep 1
        portprn ${report_name}.lis
        resp=$?
        if test ${resp} -ne 0
          then echo "Error code ${resp} during PORT printing...press Return\c"
               read -r
        fi;;
     *)
     job="/u/cribbin/crb701r.sh $@ 1>>${report_name}.out 2>>${report_name}.out >>${report_name}.out" 
       nat -i -j "job$$" -x "$job"
  ;;
esac
rem_uid $UID
exit 0;;
esac
#------------------------------------------------------------------------------
# Process Ends.
#------------------------------------------------------------------------------
