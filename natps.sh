#!/bin/sh
#!/bin/sh5
# (Select the starting string dependent on environment)
# natps.sh - Function to list out all the processes which are
# related to natrun
#
# @(#) $Name$ $Id$
#Copyright (c) E2 Systems, 1998
#
# Script to list the natrun processes and their descendents.
#
nawk 'BEGIN {
    comm="ps -ef|cut -c 1-132"
    ps_cnt = 0
#
# Read in the process lines
#
    while((comm|getline)>0)
    {
        ps_ln[ps_cnt] = $0
        pid_ln[$2] = ps_cnt
        pid[ps_cnt] = $2
        par_pid[ps_cnt] = $3
        if ($0 ~ "natrun")
            nat_flag[ps_cnt] = 1
        else
            nat_flag[ps_cnt] = 0
#        print ps_cnt " " nat_flag[ps_cnt] " " $0
        ps_cnt++
    }
#
# Build up sort keys for those process lines which are descendents of
# a natrun process.
#
#    print "ps_cnt: " ps_cnt
    srt_cnt = 0
    for (i = 0; i < ps_cnt; i++)
    {
        par = par_pid[i]
        srt_key =  pid[i] "|0|" i
        want_this = nat_flag[i]
#
# Create a sort key of all the ancestor processes
#
        while (par != 1 && par != 0)
        {
             srt_key = par "|" srt_key
             nxt = pid_ln[par]
             if (nat_flag[nxt] == 1)
                 want_this = 1 
             par = par_pid[nxt]
        }
#        print "want_this: " want_this "  srt_key: " srt_key
        if (want_this == 1)
        {
            srt_list[srt_cnt] = srt_key
            srt_cnt++
        }
    } 
#    print "srt_cnt: " srt_cnt
#
# Noddysort - re-order the keys
#
    for (i = 0; i < (srt_cnt -2); i++)
        for (j = srt_cnt - 1; j > i ; j--)
             if (srt_list[i] > srt_list[j])
             {
                 x = srt_list[j]
                 srt_list[j] = srt_list[i]
                 srt_list[i] = x
             }
#
# Output the lines in the correct order
#
    for (i = 0; i < srt_cnt; i++)
    {
#        print srt_list[i]
        nf = split(srt_list[i],arr,"|")
        j = arr[nf] 
        print ps_ln[j]
    }
    exit
}' /dev/null
