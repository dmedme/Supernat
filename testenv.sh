:
#
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems 1993
# Local environment for SUPERNAT. Bourne Shell Version
#

NAT_HOME=/home/e2soft/supernat-3.3.8_amd64
ADMINNAME=root
ADMINGROUP=sys
SUPERNAT_FIFO=$NAT_HOME/var/spool/nat/supernat_fifo
SUPERNAT_LOCK=$NAT_HOME/var/spool/nat/supernat_lock
SUPERNAT=$NAT_HOME/var/spool/nat
SUPERNATDB=supernatdb
case $PATH in
*$NAT_HOME*)
    ;;
*)
    PATH=$NAT_HOME/usr/bin:$NAT_HOME/usr/sbin:$NAT_HOME/usr/lib/nat:$PATH
    export PATH
    ;;
esac
export ADMINNAME ADMINGROUP SUPERNAT SUPERNAT_FIFO SUPERNATDB SUPERNAT_LOCK NAT_HOME
