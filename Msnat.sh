:
# Msnat.sh - compile the SUPERNAT executables
#  @(#) $Name$ $Id$
# Copyright (c) 1993 E2 Systems
# SUPERNAT
# =======
#
if [ "$#" -lt 1 ]
then
echo Must indicate an Operating System
exit
fi
OS=$1
shift
YEXFLAGS=
HFILE=y.tab.h
TFILE=y.tab.c
YACC=yacc
export YACC HFILE TFILE
case "$OS" in
NT4)
#
# Microsoft NT Version 4
#
LIBDIR=/gnuwin32/b18/H-i386-cygwin32/i386-cygwin32/lib
export LIBDIR
INCS="-I/gnuwin32/b18/H-i386-cygwin32/lib/gcc-lib/i386-cygwin32/cygnus-2.7.2-970404/include -I/gnuwin32/b18/H-i386-cygwin32/i386-cygwin32/include -I/gnuwin32/X11r6.3/include -I/gnuwin32/b18/include -I/gnuwin32/pdcurses-2.3b-win32"
LIBS="../e2common/comlib.a -luser32 -lkernel32 -lm -lcygwin"
CFLAGS="-DADMINNAME=\\\"dme\\\" -Deveryone=\\\"sys\\\" -g -I../e2common -DAT -DSINGLE_IMAGE -DPOSIX -g -I. $INCS -DAT -DNT4 -L$LIBDIR -L/gnuwin32/b18/H-i386-cygwin32/lib/gcc-lib/i386-cygwin32/cygnus-2.7.2-970404 -L/gnuwin32/pdcurses-2.3b-win32/lib"
LFLAGS=$LIBS
VCC=gcc
XPGCC=gcc
RANLIB=ranlib
AR="ar rv"
OWNHOME=/e2soft/snat
OWNUSER=dme
OWNGRP=everyone
MEMGRP=system
DEFSUPERNAT=/hlthchk/glasgow
HFILE=NATCPARS.H
TFILE=NATCPARS.C
YACC=byacc
export YACC HFILE TFILE
;;
AIX)
#
# AIX Version 4.1 and later.
#
CFLAGS="-DADMINNAME=\\\"root\\\" -DADMINGROUP=\\\"sys\\\" -g -DPOSIX -DAIX -I. -I../e2common -DAT -DSINGLE_IMAGE"
VCC=cc
XPGCC=cc
LIBS="../e2common/comlib.a -lm -lc -lbsd"
RANLIB=ranlib
AR="ar rv"
OWNHOME=/usr/natman
OWNUSER=root
OWNGRP=sys
MEMGRP=system
DEFSUPERNAT=/usr/spool/nat
;;
LINUX)
#
# LINUX RedHat 5.1/6
#
CFLAGS="-DADMINNAME=\\\"root\\\" -DADMINGROUP=\\\"sys\\\" -g2 -DPOSIX -DAIX -I. -I../e2common -DAT -DSINGLE_IMAGE -DLINUX -DDEFSUPERNAT=\\\"/var/spool/nat\\\""
#LFLAGS=-pg
VCC=cc
XPGCC=cc
LIBS="../e2common/comlib.a -lm -lc"
RANLIB=ranlib
AR="ar rv"
OWNHOME=/usr/natman
OWNUSER=root
OWNGRP=sys
MEMGRP=system
DEFSUPERNAT=/var/spool/nat
YEXFLAGS=-y
YACC=bison
;;
SOL8GCC)
#
# Solaris 8 with GCC
#
CFLAGS="-fwritable-strings -DADMINNAME=\\\"root\\\" -DADMINGROUP=\\\"daemon\\\" -g -DPOSIX -DSCO -DV4 -DICL -DSOLAR -I. -I../e2common -DAT -DSINGLE_IMAGE"
CC=gcc
export CC
VCC=gcc
XPGCC=gcc
LIBS="../e2common/comlib.a -lsocket -lnsl -lcurses -ltermlib -lm -lc /usr/ucblib/libucb.a"
RANLIB="ar ts"
AR="ar rv"
OWNHOME=/usr/local/natman
OWNUSER=root
OWNGRP=daemon
MEMGRP=sys
DEFSUPERNAT=/var/spool/nat
NATSOCK=natsock.o
export NATSOCK
;;
OSF)
#
# Digital UNIX 3.2C and 4.
#
#CFLAGS="-DADMINNAME=\\\"root\\\" -DADMINGROUP=\\\"daemon\\\" -g -O0 -std1 -DPOSIX -DOSF -I. -I../e2common -DAT -DSINGLE_IMAGE"
CFLAGS="-DADMINNAME=\\\"root\\\" -DADMINGROUP=\\\"daemon\\\" -g -O0 -DPOSIX -DOSF -I. -I../e2common -DAT -DSINGLE_IMAGE"
VCC=cc
XPGCC=cc
LIBS="../e2common/comlib.a -lm -lc -lbsd"
RANLIB=ranlib
AR="ar rv"
OWNHOME=/usr/opt/snat
OWNUSER=root
OWNGRP=daemon
MEMGRP=mem
DEFSUPERNAT=/var/spool/nat
;;
#
# SUNOS 4 or higher. SUNOS 3 is more like Ultrix, and the code
# probably doesn't work any more..
#
SUNOS)
#OWNHOME=/usr/natman
#DEFSUPERNAT=/usr/spool/nat
OWNUSER=natman
OWNHOME=/usr/testenv/snat
OWNGRP=daemon
DEFSUPERNAT=/usr/testenv/snat/data
LIBS="../e2common/comlib.a /usr/lib/libm.a /usr/lib/libc.a /usr/xpg2lib/libxpg.a /usr/5lib/libc.a /usr/lib/libkvm.a"
CFLAGS="-I. -I../e2common -I/usr/5include -g -DAT -DSINGLE_IMAGE -DSUN"
XPGCC=/usr/xpg2bin/cc
VCC=/usr/5bin/cc
LD=ld
XPGCC=cc
RANLIB=ranlib
AR="ar rv"
MEMGRP=kmem
;;
#
# Pyramid OSx 5.1
#
PYR)
OWNHOME=/usr/natman
OWNUSER=natman
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
CFLAGS="-gx -DPOSIX -DPYR -DATT -I. -I../e2common -I/usr/.attinclude -I/usr/.ucbinclude"
XPGCC="att cc"
RANLIB=ranlib
AR="ar rv"
LIBS="../e2common/comlib.a -lm /.attlib/libc.a /usr/lib/libgen.a /.ucblib/libc.a"
MEMGRP=sys
;;
#
# Ultrix
#
ULTRIX)
OWNUSER=natman
OWNHOME=/usr/natman
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
LD=ld
XPGCC=cc
VCC="cc -Y"
LIBS="../e2common/comlib.a -lm"
RANLIB=ranlib
AR="ar rv"
# Pick up Ultrix funnies (particularly sh5)
CFLAGS="-DULTRIX -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
MEMGRP=kmem
;;
# 
# SCO  (System V.3)
#
# The Microsoft C compiler does not generate correct code when
# unions or pointers to unions are passed to functions. We are
# therefore using the AT&T compiler, which generates larger programs,
# but which gets the symbol information correct for sdb, and which
# generates correct coff.
SCO)
#OWNHOME=/usr/natman
#DEFSUPERNAT=/usr/spool/nat
OWNUSER=path
OWNHOME=/users/home/path
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
LIBS="../e2common/comlib.a -lsocket -lcurses -ltermlib -lm"
CFLAGS="-DSCO -DPOSIX -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar -ts"
VCC=cc
XPGCC=cc
AR="ar rv"
MEMGRP=mem
YEXFLAGS=-Sm10400 
;;
#
# System V4
#
DGUX)
OWNUSER=natman
OWNHOME=/usr/natman
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
LIBS="../e2common/comlib.a -lnsl -lcurses -lm -lc"
CFLAGS="-DSCO -DPOSIX  -DV4 -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar ts"
VCC=cc
XPGCC=cc
AR="ar rv"
MEMGRP=sys
;;
#
# System V4
#
V4)
OWNUSER=natman
OWNHOME=/usr/natman
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
LIBS="../e2common/comlib.a -lsocket -lnsl -lcurses -ltermlib -lm -lc /usr/ucblib/libucb.a"
CFLAGS="-DSCO -DPOSIX  -DV4 -DICL -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar ts"
VCC=cc
XPGCC=cc
AR="ar rv"
MEMGRP=sys
;;
#
# Pyramid OSXDC
#
OSXDC)
# OWNUSER=e2
OWNUSER=path
# OWNHOME=/u1/e2
OWNHOME=/users/home3/path
# OWNGRP=other
OWNGRP=daemon
# DEFSUPERNAT=/u1/e2/nat
DEFSUPERNAT=/var/spool/nat
LIBS="../e2common/comlib.a -lsocket -lnsl -lcurses -ltermlib -lm -lc /usr/ucblib/libucb.a"
CFLAGS="-DSCO -DPOSIX  -DV4 -DICL -g -I. -I../e2common -DAT -DSINGLE_IMAGE -DOSXDC"
RANLIB="ar ts"
VCC=/usr/ccs/bin/cc
XPGCC=/usr/ccs/bin/cc
AR="ar rv"
MEMGRP=sys
;;
SOLAR)
OWNUSER=natman
#OWNHOME=/usr/natman
OWNHOME=/export/home/natman
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
#OWNUSER=root
#OWNHOME=`pwd`
#DEFSUPERNAT=`pwd`/data
LIBS="../e2common/comlib.a -lsocket -lnsl -lcurses -ltermlib -lm -lc /usr/ucblib/libucb.a"
CFLAGS="-DSCO -DPOSIX  -DV4 -DICL -DSOLAR -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar ts"
VCC=cc
XPGCC=cc
AR="ar rv"
MEMGRP=sys
NATSOCK=natsock.o
export NATSOCK
;;
#
# DYNIX PTX
#
PTX)
OWNUSER=e2
OWNHOME=/portspace1/May/e2
OWNGRP=sys
DEFSUPERNAT=/usr/spool/nat
LIBS="../e2common/comlib.a -lsocket -lcurses -ltermlib -lm -lseq"
CFLAGS="-DPTX -DPOSIX -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar -ts"
VCC=cc
XPGCC=cc
AR="ar -rv"
MEMGRP=sys
;;
#
# DYNIX PTX
#
PTX4)
OWNUSER=root
OWNHOME=/usr/local/natman
OWNGRP=natman
DEFSUPERNAT=/usr/spool/nat
LIBS="../e2common/comlib.a -lsocket -lnsl -lcurses -ltermlib -lm -lseq"
CFLAGS="-Wc,+bsd-socket -DPTX4 -DSOLAR -DSCO -DICL -DV4  -DPOSIX -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar -ts"
VCC=cc
XPGCC=cc
AR="ar -rv"
MEMGRP=sys
NATSOCK=natsock.o
export NATSOCK
;;
HP7)
#
# HP-UX 9 for 700
#
OWNUSER=root
OWNHOME=/users/natman
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
LIBS="../e2common/comlib.a -lm -lC -lV3 -lBSD"
CFLAGS="-DPOSIX -DHP7 -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar ts"
VCC=cc
XPGCC=cc
AR="ar rv"
MEMGRP=sys
;;
HP8)
#
# HP-UX 9 for 800
#
OWNUSER=root
OWNHOME=/users/natman
OWNGRP=daemon
DEFSUPERNAT=/usr/spool/nat
LIBS="../e2common/comlib.a -lm -lc -lV3 -lBSD"
CFLAGS="-DPOSIX -DHP7 -g -I. -I../e2common -DAT -DSINGLE_IMAGE"
RANLIB="ar ts"
VCC=cc
XPGCC=cc
AR="ar rv"
MEMGRP=sys
;;
*)
   echo Unknown Operating System
   exit 1
;;
esac
export CFLAGS VCC XPGCC LIBS RANLIB AR OWNHOME OWNUSER OWNGRP DEFSUPERNAT MEMGRP YEXFLAGS
make -e -f M.snat $*
