#!/bin/sh
# natstart.sh - Start up the scheduler, as it would be from natmenu.sh
# ***************************************************************
### BEGIN INIT INFO
# Provides:          Supernat
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Supernat - The Job Scheduler For UNIX
### END INIT INFO
# If any parameters are provided, do not start up the scheduler;
# clean up.
#
# Set up the environment
#
. /usr/lib/nat/natenv.sh
# ***************************************************************
# Begin - kill off any schedulers that may be running.
#
ps -e | awk '/natrun/ {print "kill -15 " $1}' | sh
#
# Clean up in case the system crashed.
#
rm -f $SUPERNAT/tmp* $SUPERNAT_FIFO $SUPERNAT_LOCK
if [ "$#" -gt 0 -a "$1" = stop ]
then
    exit
fi
#
# Start up; lock out job submission whilst we are sorting out the
# database
#
until semlock -s $SUPERNAT_LOCK
do
    echo "Waiting to lock SUPERNAT sub-system......"
    sleep 10
done
#
# Check for first ever run
#
if [ ! -f $SUPERNAT/Queuelist ]
then
echo rwxroot > $SUPERNAT/Userlist
nataperm rw- \*
cat << EOF > $SUPERNAT/Queuelist
DEFINE=GLOBAL
END
EOF
natcalc -n $SUPERNAT/supernatdb -c < /dev/null
fi
#
# Tidy up the jobs history; get rid of log files over 7 days old.
# 
find $SUPERNAT/log -mtime +7 -exec rm -f \{\} \;
#
# Export the variable values, if there are any.
#
natdump -v 2> $SUPERNAT/vars.sav
#
# Clean out the accumulated job history
#
cp $SUPERNAT/$SUPERNATDB $SUPERNAT/$SUPERNATDB.old 
natrebuild
#
# Re-insert the variables
#
if [ -s $SUPERNAT/vars.sav ]
then
sed 's/VARIABLE:\(.*\)/\1;/' $SUPERNAT/vars.sav |natcalc -w
fi
#
# Save the old log file
#
mv $SUPERNAT/natrun.log $SUPERNAT/natrun.log.old 
#
# Start the scheduler with a global ceiling of 7 jobs, and time-stamped
# stdout and stderr in the log files.
#
if [ -z "$SIMARGS" ]
then
    /usr/sbin/natrun $SIMARGS >$SUPERNAT/natrun.log 2>&1 & 
else
    /usr/sbin/natrun -s7 -t >$SUPERNAT/natrun.log 2>&1 & 
fi
until [ -p $SUPERNAT_FIFO ]
do
    echo "Waiting for SUPERNAT sub-system to initialise......"
    sleep 10
done
#
# Release the lock
#
semlock -u $SUPERNAT_LOCK
echo Supernat Scheduler Started
exit
