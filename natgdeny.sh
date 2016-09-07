#!/bin/sh
#************************************************************
#* natgdeny.sh - program to deny users access to a particular configuration
#*
# @(#) $Name$ $Id$
#Copyright (c) E2 Systems, 1991
dir=$1
shift
case "$@" in
    *\**)
    while [ $# -gt 0 ]
    do
    case $# in
        1)
        exec natgperm $dir --- "$1"
        shift
        ;;
        2)
        exec natgperm $dir --- "$1" "$2"
        shift 2
        ;;
        3)
        exec natgperm $dir --- "$1" "$2" "$3"
        shift 3
        ;;
        4)
        exec natgperm $dir --- "$1" "$2" "$3" "$4"
        shift 4
        ;;
        5)
        exec natgperm $dir --- "$1" "$2" "$3" "$4" "$5"
        shift 5
        ;;
        6)
        exec natgperm $dir --- "$1" "$2" "$3" "$4" "$5" "$6"
        shift 6
        ;;
        7)
        exec natgperm $dir --- "$1" "$2" "$3" "$4" "$5" "$6" "$7"
        shift 7
        ;;
        8)
        exec natgperm $dir --- "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8"
        shift 8
        ;;
        *)
        exec natgperm $dir --- "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
        shift 9
        ;;
        esac
    done
    ;;
    *)
    exec natgperm $dir --- $*
    ;;
esac
