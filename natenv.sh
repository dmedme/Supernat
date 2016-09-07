:
#
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems 1993
# Local environment for SUPERNAT. Bourne Shell Version
#
NAT_HOME=/usr/lib/nat
SUPERNAT_VERSION=3.3.8
ADMINNAME=root
ADMINGROUP=sys
SUPERNAT_FIFO=/var/spool/nat/supernat_fifo
SUPERNAT_LOCK=/var/spool/nat/supernat_lock
SUPERNAT=/var/spool/nat
SUPERNATDB=supernatdb
case $PATH in
*$NAT_HOME*)
    ;;
*)
    PATH=$NAT_HOME:$PATH
    export PATH
    ;;
esac
export ADMINNAME ADMINGROUP SUPERNAT SUPERNAT_FIFO SUPERNATDB SUPERNAT_LOCK NAT_HOME SUPERNAT_VERSION
