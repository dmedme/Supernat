#!/bin/sh
#************************************************************
#* nataview.sh - program to read the system log for SUPERNAT stuff
#*
# Setting the SHELL environment variable disables shell escapes
# in pg
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems, 1990
SUPERNAT=${SUPERNAT:-/var/spool/nat}
natalog $0 $*
SHELL=/bin/true
export SHELL
exec pg $SUPERNAT/Activity
