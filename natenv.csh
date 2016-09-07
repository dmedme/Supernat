#
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems 1993
# Local environment for SUPERNAT. C Shell Version
#
setenv NAT_HOME /usr/testenv/snat/bin
setenv ADMINNAME natman
if ( $?NAT_OS ) then
    if ( $NAT_OS == AIX ) then
        setenv ADMINGROUP staff
    else
        setenv ADMINGROUP daemon
    endif
else
    setenv ADMINGROUP daemon
endif
setenv SUPERNAT_FIFO /usr/spool/nat/supernat_fifo
# setenv SUPERNAT_FIFO /usr/spool/nat/supernat_fifo
setenv SUPERNAT_LOCK /usr/spool/nat/supernat_lock
# setenv SUPERNAT_LOCK /usr/spool/nat/supernat_lock
# setenv SUPERNAT /usr/testenv/snat/data
setenv SUPERNAT /usr/spool/nat
setenv SUPERNATDB supernatdb
