/************************************************************
 * natdump.c - program to dump out the supernat database
 *
 */
static char * sccs_id="@(#) $Name$ $Id$\n Copyright (c) E2 Systems 1990";
/*
 * Copyright (c) E2 Systems 1990. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "supernat.h"
void ind_format();
void hash_format();
void var_format();
void job_acc_format();
void overall_job_format();
#define ISLOT 0
#define HSLOT 1
#define VSLOT 2
#define JSLOT 3
#define OSLOT 4
struct rec_handle  rec_out[] = {
{IND_TYPE,NULL},
{HASH_TYPE,NULL},
{VAR_TYPE,NULL},
{JOB_ACC_TYPE,NULL},
{OVERALL_JOB_TYPE,NULL},
{ -1, 0}};

/*
 *    Usage: natdump
 */
int natdump_main(argc, argv, envp)
int    argc;
char    **argv;
char ** envp;
{
int c;
int verbose_flag = 0;
int swab_flag = 0;

    natinit(argc, argv);
    checkperm(U_READ|U_WRITE, (QUEUE *)NULL);
    if (argc == 1)
    {
        verbose_flag = 1;
        rec_out[ISLOT].rec_function = ind_format;
        rec_out[HSLOT].rec_function = hash_format;
        rec_out[VSLOT].rec_function = var_format;
        rec_out[JSLOT].rec_function = job_acc_format;
        rec_out[OSLOT].rec_function = overall_job_format;
    }
    else
    while ( ( c = getopt ( argc, argv, "aihvjos" ) ) != EOF )
    {
        switch ( c )
        {
/*
 * Dump everything
 */
        case 'a' :
            verbose_flag = 1;
            rec_out[ISLOT].rec_function = ind_format;
            rec_out[HSLOT].rec_function = hash_format;
            rec_out[VSLOT].rec_function = var_format;
            rec_out[JSLOT].rec_function = job_acc_format;
            rec_out[OSLOT].rec_function = overall_job_format;
            break;
/*
 * Dump a database file from a SPARC on an Intel Platform
 */
        case 's' :
            swab_flag = 1;
            break;
/*
 * Dump Index Entries
 */
        case 'i' :
            rec_out[ISLOT].rec_function = ind_format;
            break;
/*
 * Dump Hash Entries
 */
        case 'h' :
            verbose_flag = 1;
            rec_out[HSLOT].rec_function = hash_format;
            break;
/*
 * Dump Variables
 */
        case 'v' :
            rec_out[VSLOT].rec_function = var_format;
            break;
/*
 * Dump Job Accounting Records
 */
        case 'j' :
            rec_out[JSLOT].rec_function = job_acc_format;
            break;
/*
 * Dump Overall Job Records
 */
        case 'o' :
            rec_out[OSLOT].rec_function = overall_job_format;
            break;
        case '?' : /* Default - invalid opt.*/
        default:
               (void) fprintf(stderr,
                  "Invalid argument;  valid are -a, -i, -h, -v, -j, -o\n");
               exit(1);
        break;    
        }
    }
    sg.cur_db = dh_open();
    if (swab_flag)
        fhdomain_check_swab(sg.cur_db,verbose_flag,rec_out);
    else
        fhdomain_check(sg.cur_db,verbose_flag,rec_out);
    (void) fhclose(sg.cur_db);
    exit(0);
}
