/*
 * natsym.c - routines to maintain the SUPERNAT V3 variable
 * database
 *
 * The variable database has three areas:
 * - A preamble, which contains:
 *   - Its size
 *   - A sequence generator, for unique job numbers (rather than having
 *     the rather tedious Inode number, which keeps changing)
 *   - The size and position of the hash table
 *   - The start and end points for the free space chain
 *   - The current limit of the space
 *   - The amount of space to extend the heap at a time
 * - The hash table itself. This is pre-sized in this implementation
 * - The heap, which contains all data, and free space records.
 *
 * The file starts off with:
 * - Preamble length 4096
 * - 1 as the next entry
 * - Zeros for the pointers
 * - A Hash table allocated to the size requested, and initialised to
 *   unused
 * - The extension value set to the value requested
 * - A null heap area
 *
 * Space is allocated and freed in a manner analogous to malloc() and free().
 * - Allocation
 *   - Search is first fit
 *   - Take it all if less than 16 bytes left
 *   - Chain forwards only
 *   - Not found, allocate a new region, add it to the en
 * - Free
 *   - Find position in list
 *   - Attempt to merge with neighbours
 *
 * Long integers are assumed to be the same size as (char *)!
 */ 
static char * sccs_id ="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1992"; 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ansi.h"
#include <setjmp.h>
#include "supernat.h"
#include "natcexe.h"
#ifdef AIX
#define ntohl(x) (x)
#endif

union free_preamble {
    char buf[2*sizeof(long)];
    struct free_header {
        long next_free;               /* Next free sector in the chain */
        int block_size;               /* Size of this block            */
    } free_header;
};
struct rec_handle  rec_dump[] = {
{IND_TYPE,ind_format},
{HASH_TYPE,hash_format},
{VAR_TYPE,var_format},
{JOB_ACC_TYPE,job_acc_format},
{OVERALL_JOB_TYPE,overall_job_format},
{ -1, 0}};
struct rec_handle rec_purge[] = {
{IND_TYPE,NULL},
{HASH_TYPE,NULL},
{VAR_TYPE,NULL},
{JOB_ACC_TYPE,job_acc_purge},
{OVERALL_JOB_TYPE,NULL},
{ -1, 0}};
   
/*
 * Create a new DB
 */
void fhcreate(file_name,hash_size,extent,last_job_no)
char * file_name;
int hash_size;            /* number of entries in the table */
int extent;               /* size of free space extents to allocate */
int last_job_no;
{
int i;
FILE * fp;
union hash_preamble hash_preamble;
union hash_entry hash_entry;
/*
 * Create the file
 */
    if ((fp = openfile(sg.supernat,file_name,"w+")) == (FILE *) NULL)
    {
        perror("DB CREATE FAILED");
        return;
    }
    for (i = 2; i < hash_size; i = (i << 1));
    hash_size = i;                 /* adjust the size to a power of 2 */
/*
 * Fill in the header
 */
    hash_preamble.hash_header.preamble_size = sizeof(hash_preamble);
                                        /*   - Its size */
    hash_preamble.hash_header.next_job = last_job_no + 1;
                                        /*   - A sequence generator,
                                         *     for unique job numbers
                                         *     (rather than having
                                         *     the rather tedious Inode number,
                                         *     which keeps changing) */
    hash_preamble.hash_header.last_backup = 0;
                                        /* Never been backed up */
    hash_preamble.hash_header.hash_size = hash_size;
                                        /*   - The size and position of the
                                         *     hash table */
    hash_preamble.hash_header.hash_offset = sizeof(hash_preamble);
    hash_preamble.hash_header.first_free = 0;
                                        /*   - The start and end points for the
                                         *     free space chain */
    hash_preamble.hash_header.last_free = 0;
    hash_preamble.hash_header.end_space = sizeof(hash_preamble) +
                                              sizeof(hash_entry) *hash_size ;
                                        /*   - The current limit of the space */
    hash_preamble.hash_header.extent = extent;
                                        /*   - The amount of space to extend
                                         *     the heap at a time */
/*
 * Write out the header
 */
    (void) fwrite((char *) &hash_preamble,
                  sizeof(char),sizeof(hash_preamble),fp);
/*
 * Set up an empty hash entry
 */
    hash_entry.hi.in_use = 0;
    hash_entry.hi.name = 0;
    hash_entry.hi.body = 0;
    hash_entry.hi.next = 0;
/*
 * Write out the empty hash entries
 */
    for (i = 0; i < hash_size; i++)
        (void) fwrite((char *) &hash_entry,sizeof(char),sizeof(hash_entry),fp);
    (void) fclose(fp);
    return;
}
/*
 * Open a DB
 */
DH_CON * fhopen(file_name,hash_to_use)
char * file_name;
HASH_FUNC hash_to_use;
{
DH_CON * dh_con;
char * fmode;

    if ((dh_con = (DH_CON *) malloc(sizeof(DH_CON))) == (DH_CON *) NULL)
    {
        perror("malloc() failed");
        return (DH_CON *) NULL;
    }
/*
 * Open the file
 */
    if (sg.read_write > 0)
        fmode = "r+";
    else
        fmode = "r";
    
    if ((dh_con->fp = openfile(sg.supernat,file_name,fmode)) == (FILE *) NULL)
    {
        perror("DB Open Failed");
        return (DH_CON *) NULL;
    }
    setbuf(dh_con->fp,dh_con->buf);
/*
 * Return the header
 */
    (void) fread((char *) &dh_con->hash_preamble,sizeof(char),
                    sizeof(dh_con->hash_preamble),dh_con->fp);
    dh_con->hashfunc = hash_to_use;
    return dh_con;
}
/*
 * Just to save on typing
 */
static void fh_head_write(dh_con)
DH_CON * dh_con;
{
    if (fseek(dh_con->fp,0,0) < 0)
        abort();
    if (fwrite((char *) &dh_con->hash_preamble,sizeof(char),
                                  sizeof(dh_con->hash_preamble),dh_con->fp) < 1)
        abort();
    (void) fflush(dh_con->fp);
    return;
}
/*
 * Close a DB
 */
int fhclose(dh_con)
DH_CON * dh_con;
{
FILE * fp;

    fp = dh_con->fp;
    if (sg.read_write > 0)
        fh_head_write(dh_con);
    free(dh_con);
    return fclose(fp);
}
/*
 * Function to allocate space in the file
 * - Allocation
 *   - Search is first fit
 *   - Take it all if less than 16 bytes left
 *   - Chain forwards only
 *   - Not found, allocate a new region, add it to the end
 *
 *  Allocate space if there is no room in the free space chain.
 */
static long fh_free_extend(dh_con,size)
DH_CON * dh_con;
int size;
{
union free_preamble x;
long ret_val;

    if (fseek(dh_con->fp,dh_con->hash_preamble.hash_header.end_space,0) < 0)
        return 0;
    x.free_header.block_size = size + sizeof(x);
    x.free_header.next_free = 0;                    /* The reference count */
    (void) fwrite((char *) &x, sizeof(char), sizeof(x),dh_con->fp);
    (void) fflush(dh_con->fp);
    ret_val = dh_con->hash_preamble.hash_header.end_space 
                       + sizeof(x);
    x.free_header.block_size = dh_con->hash_preamble.hash_header.extent +
                        sizeof(x);
    if (fseek(dh_con->fp,size,1) < 0)
        return 0;
    (void) fwrite((char *) &x, sizeof(char), sizeof(x),dh_con->fp);
    (void) fflush(dh_con->fp);
    if ( dh_con->hash_preamble.hash_header.first_free == 0)
    { 
    dh_con->hash_preamble.hash_header.first_free = 
              dh_con->hash_preamble.hash_header.end_space +
                 sizeof(x)
                 + size;
    dh_con->hash_preamble.hash_header.last_free = 
             dh_con->hash_preamble.hash_header.first_free;
    }
    else
    {
        if (fseek(dh_con->fp, dh_con->hash_preamble.hash_header.last_free,0)< 0)
            return 0;
        (void) fread((char *) &x,sizeof(char),sizeof(x),dh_con->fp);
        x.free_header.next_free = 
              dh_con->hash_preamble.hash_header.end_space +
                 sizeof(x)
                 + size;
        dh_con->hash_preamble.hash_header.last_free = x.free_header.next_free; 
        
        if (fseek(dh_con->fp, -sizeof(x), 1) < 0)
            return 0;
        (void) fwrite((char *) &x,sizeof(char),sizeof(x),dh_con->fp);
        (void) fflush(dh_con->fp);
    }
    dh_con->hash_preamble.hash_header.end_space = 
              dh_con->hash_preamble.hash_header.end_space +
                 sizeof(x) +  sizeof(x)
                 + size + dh_con->hash_preamble.hash_header.extent;
    fh_head_write(dh_con);
    if (fseek(dh_con->fp,ret_val,0) < 0)
        return 0;
    return ret_val;
}
/*********************************************************************
 * External entry point
 *
 * On exit, the file pointer is at the place where the data is to be
 * inserted.
 */
long fhmalloc(dh_con,size)
DH_CON * dh_con;                /* The file to allocate the space in */
int size;                       /* the space needed                  */
{
long ret_val;
long prev_val;
int head_changed;
union free_preamble x,prev_pre;
head_changed = 0;
/******* Uncomment the following line to validate the domain each write *******
    fhdomain_check(dh_con,1,rec_dump); */
/*
 * Search for a block that is big enough
 */

    for ( ret_val = dh_con->hash_preamble.hash_header.first_free,
          prev_val = 0;
                        ret_val > 0;)
    {                      /* Search down the chain */
        if (fseek(dh_con->fp,ret_val, 0) < 0)
            return 0;
        (void) fread((char *) &x,sizeof(char),sizeof(x),dh_con->fp);
        if (x.free_header.block_size > size + sizeof(x))
        {                 /* We have found some space */
            if (x.free_header.block_size > size + 4*sizeof(x))
            {                 /* Split the current block */
                union free_preamble alloc_block;
                alloc_block = x;
                x.free_header.block_size = x.free_header.block_size - 
                                           size - sizeof(x); 
                alloc_block.free_header.block_size = size +
                                                     sizeof(x);
                alloc_block.free_header.next_free = 0;
                if (prev_val == 0)
                {                 /* first item */
                    if (dh_con->hash_preamble.hash_header.first_free
                        == dh_con->hash_preamble.hash_header.last_free)
                        dh_con->hash_preamble.hash_header.last_free +=
                               alloc_block.free_header.block_size;
                    dh_con->hash_preamble.hash_header.first_free +=
                       alloc_block.free_header.block_size;
                    head_changed = 1;
                }
                else
                {                 /* advance the previous pointer */
                    prev_pre.free_header.next_free +=
                                             alloc_block.free_header.block_size;
                    if (fseek(dh_con->fp,prev_val,0) < 0)
                        return 0;
                    (void) fwrite((char *) &prev_pre,sizeof(char),
                                   sizeof(prev_pre), dh_con->fp);
                    (void) fflush(dh_con->fp);
                }
                if (fseek(dh_con->fp, ret_val,0) < 0)
                    return 0;
                (void) fwrite((char *) &alloc_block,sizeof(char),
                               sizeof(alloc_block), dh_con->fp);
                (void) fflush(dh_con->fp);
                if (fseek(dh_con->fp, ret_val +
                      alloc_block.free_header.block_size,0) < 0)
                    return 0;
                (void) fwrite((char *) &x,sizeof(char),sizeof(x), dh_con->fp);
                (void) fflush(dh_con->fp);
                if (x.free_header.next_free == 0)
                {                  /* this is the last */ 
                    dh_con->hash_preamble.hash_header.last_free =
                           ret_val + sizeof(x) + size;
                    head_changed = 1;
                }
            }
            else
            {              /* take the whole of the current block */
                if (prev_val == 0)
                {                 /* first item */
                    dh_con->hash_preamble.hash_header.first_free =
                                          x.free_header.next_free;
                    head_changed = 1;
                }
                else
                {                 /* advance the previous pointer */
                    prev_pre.free_header.next_free = x.free_header.next_free;
                    if (fseek(dh_con->fp,prev_val,0) < 0)
                        return 0;
                    (void) fwrite((char *) &prev_pre,sizeof(char),
                                  sizeof(prev_pre), dh_con->fp);
                    (void) fflush(dh_con->fp);
                }
                if (x.free_header.next_free == 0)
                {                  /* this is the last */ 
                    dh_con->hash_preamble.hash_header.last_free = prev_val;
                    head_changed = 1;
                }
                else               /* Need to zero the reference count */
                {
                    x.free_header.next_free = 0;
                    if (fseek(dh_con->fp,ret_val,0) < 0)
                        return 0;
                    (void) fwrite((char *) &x,sizeof(char),sizeof(x),
                                      dh_con->fp);
                    (void) fflush(dh_con->fp);
                }
            }
            ret_val += sizeof(x);
            break;
        }
        else        /* This block is not big enough */
        {
             prev_val = ret_val;
             prev_pre = x;
             ret_val = x.free_header.next_free;
        }
    }
    if (ret_val == 0)
                   /* failed to allocate so far */
        ret_val = fh_free_extend(dh_con,size);
    else
    { 
        if (head_changed)
            fh_head_write(dh_con);
        if (fseek(dh_con->fp,ret_val,0) < 0)
            return 0;
    }
    return ret_val;
}
/*
 * External entry point
 */
void fhfree(dh_con,where)
DH_CON * dh_con;                /* The file to allocate the space in */
long where;                     /* The place to be de-allocated */
{
    long ret_val;
long prev_val;
int head_changed;
union free_preamble x,prev_pre,this;
#define PREV_MERGE 1
int blocks_merged;

    blocks_merged = 0;
    head_changed = 0;
    where = where - sizeof(this);    /* Subtract the Header from the offset */

    if (fseek(dh_con->fp,where, 0) < 0)
        return;
    (void) fread((char *) &this,sizeof(char),sizeof(this),dh_con->fp);
              /* Read the details for the record being returned */
/*
 * Search for the place to put the space back into
 */
    for ( ret_val = dh_con->hash_preamble.hash_header.first_free,
          prev_val = 0;
                        ret_val < where && ret_val != 0;)
    {                      /* Search down the chain */
        if (fseek(dh_con->fp,ret_val, 0) < 0)
            return;
        (void) fread((char *) &x,sizeof(char),sizeof(x),dh_con->fp);
        prev_val = ret_val;
        prev_pre = x;
        ret_val = x.free_header.next_free;
    }
    
/*
 * There are four possibilities
 * - It can be merged with its predecessor
 * - It can be merged with its successor
 * - It can be merged with both
 * - It cannot be merged
 */
     if (prev_val == 0)                /* No previous record */
         blocks_merged = PREV_MERGE;
     else if ( where == prev_val + prev_pre.free_header.block_size)
     {             /* Merge with the previous item */
        blocks_merged = PREV_MERGE;
        where = prev_val;
        this.free_header.block_size +=   prev_pre.free_header.block_size;
        this.free_header.next_free =   prev_pre.free_header.next_free;
     }
     if ( where + this.free_header.block_size == ret_val)
     {              /* Merge with next item */
        if (fseek(dh_con->fp,ret_val, 0) < 0)
            return;
        (void) fread((char *) &x,sizeof(char),sizeof(x),dh_con->fp);
        this.free_header.next_free =   x.free_header.next_free;
        this.free_header.block_size +=   x.free_header.block_size;
        ret_val = this.free_header.next_free;
     }
     else this.free_header.next_free = ret_val;
     if (blocks_merged != PREV_MERGE)
     {              /* Need to write out the previous record, pointed at this */
        prev_pre.free_header.next_free = where;
        if (fseek(dh_con->fp, prev_val, 0) < 0)
            return;
        (void) fwrite((char *) &prev_pre,sizeof(char),
                       sizeof(prev_pre),dh_con->fp);
        (void) fflush(dh_con->fp);
     }
     if (fseek(dh_con->fp, where, 0) < 0)
         return;
     (void) fwrite((char *) &this,sizeof(char),sizeof(this),dh_con->fp);
     (void) fflush(dh_con->fp);
     if (prev_val == 0)
     {
         head_changed = 1;
         dh_con->hash_preamble.hash_header.first_free = where ;
     }
     if (ret_val == 0)
     {
         head_changed = 1;
         dh_con->hash_preamble.hash_header.last_free = where;
     }
     if (head_changed)
         fh_head_write(dh_con);
     return;
}
/*
 * Format an index entry
 */
void ind_format(len,ptr,offset)
int len;
char * ptr;
long offset;
{
    fprintf(sg.errout,"INDEX ENTRY: Length: %d Numeric Value: %d String Value: %.*s\n",
            len, *((int *) ptr), len, ptr);
    return;
}
void hash_format(len,hi,offset)
int len;
union hash_entry * hi;
long offset;
{
fprintf(sg.errout,"HASH ENTRY: In Use: %d Index Offset: %d Record Offset: %d Next:%d\n",
            hi->hi.in_use,
            hi->hi.name,
            hi->hi.body,
            hi->hi.next);
    return;

}
void var_format(len,ptr,offset)
int len;
struct symbol * ptr;
long offset;
{
    ptr->name_ptr = (char *) (ptr + 1);
    if (ptr->name_len < 0
        || ptr->name_len > 255
        || ptr->sym_value.tok_len < 0
        || ptr->sym_value.tok_len > 4096)
    fprintf(sg.errout,"VARIABLE CORRUPT: name length=%d, value length=%d\n",
            ptr->name_len,ptr->sym_value.tok_len);
    else
    {
        ptr->sym_value.tok_ptr = ptr->name_ptr + 1 + ptr->name_len;
        (void) fprintf(sg.errout,"VARIABLE: %s = %s\n",
            ptr->name_ptr,ptr->sym_value.tok_ptr);
    }
    return;
}
void res_format(used)
RESOURCE_PICTURE * used;
{
    fprintf(sg.errout,"  RESOURCE CONSUMPTION\n");

    fprintf(sg.errout,"    no_of_samples: %d\n",used->no_of_samples);
    if (used->low_time != 0)
    fprintf(sg.errout,"    low_time: %s\n",to_char("DD Month YYYY HH24:MI:SS",
              (double) used->low_time));            /* seconds since midnight */
    if (used->high_time != 0)
    fprintf(sg.errout,"    high_time: %s\n",to_char("DD Month YYYY HH24:MI:SS",
              (double) used->high_time));           /* seconds since midnight */
    fprintf(sg.errout,"    sigma_cpu: %f\n",used->sigma_cpu);
    fprintf(sg.errout,"    sigma_io: %f\n",used->sigma_io);
    fprintf(sg.errout,"    sigma_memory: %f\n",used->sigma_memory);
    fprintf(sg.errout,"    sigma_elapsed_time: %f\n",used->sigma_elapsed_time);
    fprintf(sg.errout,"    sigma_cpu_2: %f\n",used->sigma_cpu_2);
    fprintf(sg.errout,"    sigma_io_2: %f\n",used->sigma_io_2);
    fprintf(sg.errout,"    sigma_memory_2: %f\n",used->sigma_memory_2);
    fprintf(sg.errout,"    sigma_elapsed_time_2: %f\n",used->sigma_elapsed_time_2);
    return;
}
void job_acc_format(len,ptr,offset)
int len;
JOB_ACC * ptr;
long offset;
{
    fprintf(sg.errout,"JOB ACCOUNTING RECORD\n");
    fprintf(sg.errout,"job_no: %d\n",ptr->job_no);
    fprintf(sg.errout,"status: %d\n",ptr->status);
    fprintf(sg.errout,"execution_status: %d\n",ptr->execution_status);
    fprintf(sg.errout,"planned_time: %s\n",to_char("DD Month YYYY HH24:MI:SS",
            (double) ptr->planned_time));
    fprintf(sg.errout,"queue_name: %s\n",ptr->queue_name);
    fprintf(sg.errout,"user_name: %s\n",ptr->user_name);
    fprintf(sg.errout,"job_name: %s\n",ptr->job_name);
    if (ptr->used.no_of_samples > 0)
        res_format( &ptr->used);
    return;
}
void job_acc_purge(len,ptr,offset)
int len;
JOB_ACC * ptr;
long offset;
{
long cur_time;

    cur_time = time(0);
    if (ptr->used.no_of_samples > 0 &&
          (cur_time - 86400) > ptr->used.high_time)
        job_acc_hremove(offset,ptr);
    return;
}
void overall_job_format(len,ptr,offset)
int len;
OVERALL_JOB * ptr;
long offset;
{
    fprintf(sg.errout,"OVERALL JOB RECORD\n");
    fprintf(sg.errout,"job_name: %s\n",ptr->job_name);
    if (ptr->res_used.no_of_samples > 0)
        res_format( &ptr->res_used);
    return;
}
/*
 * Routine to validate the arena. If the dump_flag is non-zero, list
 * the state of the arena; what's free, what's in use, and what seems
 * to be present in the in use bits.
 */
void fhdomain_check(dh_con,dump_flag,rec_dispose)
DH_CON * dh_con;
int dump_flag;
struct rec_handle * rec_dispose;
{
union hash_entry hash_entry;
long   in_use, top_free, end_space;
union free_preamble pream;
FILE * fp;
/*
 * The start point is the end of the hash table
 */
    fp = dh_con->fp;
    in_use = (dh_con->hash_preamble.hash_header.hash_size *
             sizeof(union hash_entry)) +
             dh_con->hash_preamble.hash_header.hash_offset;
    end_space = dh_con->hash_preamble.hash_header.end_space;
    top_free = dh_con->hash_preamble.hash_header.first_free;
    if (dump_flag)
    (void) fprintf(stderr, 
"DOMAIN CHECK: First Data: %u End of File: %u\n\
First Free: %u Last Free: %u Next Job: %d\n",
        in_use,end_space,top_free,
               dh_con->hash_preamble.hash_header.last_free,
               dh_con->hash_preamble.hash_header.next_job);
    if (top_free == 0)
        top_free = end_space;
/*
 * Dump out the occupied hash entries
 */
    if (dump_flag)
    {
        int i;
        if (fseek (fp,
                      dh_con->hash_preamble.hash_header.hash_offset,0) < 0)
        {
            fprintf(sg.errout,"Cannot fseek() to HASH TABLE\n");
            return;
        }
        for (i = 0; i < dh_con->hash_preamble.hash_header.hash_size; i++)
        {
            (void) fread((char *) &hash_entry,sizeof(char),sizeof(hash_entry),
                fp);
            if ( hash_entry.hi.in_use != 0 || hash_entry.hi.next != 0)
            {
                fprintf(sg.errout,"OCCUPIED HASH SLOT No: %d\n",i);
                hash_format(sizeof(hash_entry),&hash_entry);
            } 
        } 
    }
/*
 * Loop - process all items, free and allocated, between the end of
 * the hash table and the end of the free space
 */
    while (in_use < end_space)
    {
/*
 * Loop - process all in use items up to the next free item.
 */
        while (in_use < top_free)
        {
            char * read_ptr;
            int read_len;
            int read_type;
            if ( fseek(fp,in_use ,0) < 0)
            {
                (void) fprintf(stderr, 
                     "Database seek to %u FAILED\n",
                     (long) in_use);
                     break;
            }
            if (  fread((char *) &pream,sizeof(union free_preamble),1,fp) < 1)
            {
                (void) fprintf(stderr, 
                     "Database read at %u FAILED\n",
                     (long) in_use);
                     break;
            }
            if (dump_flag)
                (void) fprintf(stderr, 
                     "Occupied Block at %u size %u reference count %u\n",
                     (long) in_use + sizeof(union free_preamble),
                     pream.free_header.block_size,
                     pream.free_header.next_free);
            if (fh_get_to_mem(dh_con,in_use + sizeof(union free_preamble),
                              &read_type,&read_ptr,&read_len))
            {
                struct rec_handle *r;
                for (r = rec_dispose;
                         r->rec_type != -1;
                            r++)
                {
                    if (read_type == r->rec_type)
                    {
                        if (r->rec_function != NULL)
                            (*r->rec_function)(read_len,read_ptr,
                                 (long) in_use + sizeof(union free_preamble));
                        break;
                    }
                } 
                if (r->rec_type == -1)
                    (void) fprintf(stderr, 
                         "Contents: Unknown Type %u Length %u String\n%.*s\n",
                         read_type,read_len,read_len,read_ptr);
                (void) free(read_ptr);
            }
            else
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x CANNOT BE READ\n",
                     (long) in_use + sizeof(union free_preamble),
                     pream.free_header.block_size,
                     pream.free_header.next_free);
            }
            if (pream.free_header.block_size == 0)
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x HAS ZERO SIZE\n",
                     (long) in_use + sizeof(union free_preamble),
                     pream.free_header.block_size,
                     pream.free_header.next_free);
                break;
            }
            else if (pream.free_header.block_size +in_use > end_space)
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x EXTENDS OUT OF FILE\n",
                     (long) in_use + sizeof(union free_preamble),
                     pream.free_header.block_size,
                     pream.free_header.next_free);
                break;
            }
            else if (pream.free_header.block_size < 0)
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x HAS NEGATIVE SIZE\n",
                     (long) in_use + sizeof(union free_preamble),
                     pream.free_header.block_size,
                     pream.free_header.next_free);
                break;
            }
            else
                in_use = in_use + pream.free_header.block_size ;
        }
        if (in_use > top_free)
        {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x OVERLAPS FREE\n",
                     (long) in_use - pream.free_header.block_size 
                     + sizeof(union free_preamble),
                     pream.free_header.block_size,
                     pream.free_header.next_free);
        }
        if (top_free != end_space)
        {
            if ( fseek(fp,top_free ,0) < 0)
            {
                (void) fprintf(stderr, 
                     "Database seek to %u FAILED\n",
                     (long) top_free);
                     break;
            }
            if (  fread((char *) &pream,sizeof(union free_preamble),1,fp) < 1)
            {
                (void) fprintf(stderr, 
                     "Database read at %u FAILED\n",
                     (long) top_free);
                     break;
            }
            in_use = top_free + pream.free_header.block_size;
            if (dump_flag)
                (void) fprintf(stderr, 
                     "Free Block at %u size %u Next Pointer %u\n",
                     (long) top_free, pream.free_header.block_size,
                     pream.free_header.next_free);
            if ( pream.free_header.next_free == 0 &&
                dh_con->hash_preamble.hash_header.last_free != top_free )
                (void) fprintf(stderr, 
"PREMATURE END OF FREE CHAIN BLOCK at %u size %u Next Pointer %u\n",
                     (long) top_free, pream.free_header.block_size,
                     pream.free_header.next_free);
            if ( pream.free_header.next_free == top_free)
            {
                (void) fprintf(stderr, 
"BLOCK CHAINED TO ITSELF at %u size %x Next Pointer %x\n",
                     (long) top_free, pream.free_header.block_size,
                     pream.free_header.next_free);
               break;
            }
            if ( pream.free_header.next_free < top_free &&
                 pream.free_header.next_free != 0)
            {
                (void) fprintf(stderr, 
"BLOCK POINTING BACKWARDS at %u size %x Next Pointer %x\n",
                     (long) top_free, pream.free_header.block_size,
                     pream.free_header.next_free);
               break;
            }
            if ( pream.free_header.block_size < sizeof(pream))
            {
                (void) fprintf(stderr, 
"ILLEGAL FREE BLOCK SIZE at %u size %x Next Pointer %x\n",
                     (long) top_free, pream.free_header.block_size,
                     pream.free_header.next_free);
               break;
            }
            if ( pream.free_header.next_free == 0)
                top_free = end_space;
            else
                top_free = pream.free_header.next_free;
        }
        else
            in_use = end_space;
    }
    return;
}
void fhdomain_check_swab(dh_con,dump_flag,rec_dispose)
DH_CON * dh_con;
int dump_flag;
struct rec_handle * rec_dispose;
{
union hash_entry hash_entry;
long   in_use, top_free, end_space;
union free_preamble pream;
FILE * fp;
/*
 * The start point is the end of the hash table
 */
    fp = dh_con->fp;
    in_use = (ntohl(dh_con->hash_preamble.hash_header.hash_size) *
             sizeof(union hash_entry)) +
             ntohl(dh_con->hash_preamble.hash_header.hash_offset);
    end_space = ntohl(dh_con->hash_preamble.hash_header.end_space);
    top_free = ntohl(dh_con->hash_preamble.hash_header.first_free);
    if (dump_flag)
    (void) fprintf(stderr, 
"DOMAIN CHECK: First Data: %u End of File: %u\n\
First Free: %u Last Free: %u Next Job: %d\n",
        in_use,end_space,top_free,
               ntohl(dh_con->hash_preamble.hash_header.last_free),
               ntohl(dh_con->hash_preamble.hash_header.next_job));
    if (top_free == 0)
        top_free = end_space;
/*
 * Dump out the occupied hash entries
 */
    if (dump_flag)
    {
        int i;
        if (fseek (fp,
                      ntohl(dh_con->hash_preamble.hash_header.hash_offset),0) < 0)
        {
            fprintf(sg.errout,"Cannot fseek() to HASH TABLE\n");
            return;
        }
        for (i = 0; i < ntohl(dh_con->hash_preamble.hash_header.hash_size); i++)
        {
            (void) fread((char *) &hash_entry,sizeof(char),sizeof(hash_entry),
                fp);
            if ( hash_entry.hi.in_use != 0 || hash_entry.hi.next != 0)
            {
                fprintf(sg.errout,"OCCUPIED HASH SLOT No: %d\n",i);
                hash_format(sizeof(hash_entry),&hash_entry);
            } 
        } 
    }
/*
 * Loop - process all items, free and allocated, between the end of
 * the hash table and the end of the free space
 */
    while (in_use < end_space)
    {
/*
 * Loop - process all in use items up to the next free item.
 */
        while (in_use < top_free)
        {
            char * read_ptr;
            int read_len;
            int read_type;
            if ( fseek(fp,in_use ,0) < 0)
            {
                (void) fprintf(stderr, 
                     "Database seek to %u FAILED\n",
                     (long) in_use);
                     break;
            }
            if (  fread((char *) &pream,sizeof(union free_preamble),1,fp) < 1)
            {
                (void) fprintf(stderr, 
                     "Database read at %u FAILED\n",
                     (long) in_use);
                     break;
            }
            if (dump_flag)
                (void) fprintf(stderr, 
                     "Occupied Block at %u size %u reference count %u\n",
                     (long) in_use + sizeof(union free_preamble),
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
            if (fh_get_to_mem_swab(dh_con,in_use + sizeof(union free_preamble),
                              &read_type,&read_ptr,&read_len))
            {
                 (void) fprintf(stderr, 
                         "Contents: Type %u Length %u String\n%.*s\n",
                         read_type,read_len,read_len,read_ptr);
                (void) free(read_ptr);
            }
            else
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x CANNOT BE READ\n",
                     (long) in_use + sizeof(union free_preamble),
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
            }
            if (pream.free_header.block_size == 0)
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x HAS ZERO SIZE\n",
                     (long) in_use + sizeof(union free_preamble),
                     pream.free_header.block_size,
                     pream.free_header.next_free);
                break;
            }
            else if (ntohl(pream.free_header.block_size) +in_use > end_space)
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x EXTENDS OUT OF FILE\n",
                     (long) in_use + sizeof(union free_preamble),
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
                break;
            }
            else if (ntohl(pream.free_header.block_size) < 0)
            {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x HAS NEGATIVE SIZE\n",
                     (long) in_use + sizeof(union free_preamble),
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
                break;
            }
            else
                in_use = in_use + ntohl(pream.free_header.block_size) ;
        }
        if (in_use > top_free)
        {
                (void) fprintf(stderr, 
"CORRUPT BLOCK at %u size %x reference count %x OVERLAPS FREE\n",
                     (long) in_use - ntohl(pream.free_header.block_size) 
                     + sizeof(union free_preamble),
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
        }
        if (top_free != end_space)
        {
            if ( fseek(fp,top_free ,0) < 0)
            {
                (void) fprintf(stderr, 
                     "Database seek to %u FAILED\n",
                     (long) top_free);
                     break;
            }
            if (  fread((char *) &pream,sizeof(union free_preamble),1,fp) < 1)
            {
                (void) fprintf(stderr, 
                     "Database read at %u FAILED\n",
                     (long) top_free);
                     break;
            }
            in_use = top_free + ntohl(pream.free_header.block_size);
            if (dump_flag)
                (void) fprintf(stderr, 
                     "Free Block at %u size %u Next Pointer %u\n",
                     (long) top_free,
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
            if ( pream.free_header.next_free == 0 &&
                ntohl(dh_con->hash_preamble.hash_header.last_free) != top_free )
                (void) fprintf(stderr, 
"PREMATURE END OF FREE CHAIN BLOCK at %u size %u Next Pointer %u\n",
                     (long) top_free,
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
            if ( ntohl(pream.free_header.next_free) == top_free)
            {
                (void) fprintf(stderr, 
"BLOCK POINTS TO ITSELF at %u size %x Next Pointer %x\n",
                     (long) top_free,
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
               break;
            }
            if ( ntohl(pream.free_header.next_free) < top_free &&
                 pream.free_header.next_free != 0)
            {
                (void) fprintf(stderr, 
"BLOCK POINTING BACKWARDS at %u size %x Next Pointer %x\n",
                     (long) top_free,
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
               break;
            }
            if ( ntohl(pream.free_header.block_size) < sizeof(pream))
            {
                (void) fprintf(stderr, 
"ILLEGAL FREE BLOCK SIZE at %u size %x Next Pointer %x\n",
                     (long) top_free,
                     ntohl(pream.free_header.block_size),
                     ntohl(pream.free_header.next_free));
               break;
            }
            if ( pream.free_header.next_free == 0)
                top_free = end_space;
            else
                top_free = ntohl(pream.free_header.next_free);
        }
        else
            in_use = end_space;
    }
    return;
}
/*
 * Routine to change the reference count for an allocated item, up or down
 * If the reference count goes to zero, free it.
 */
void fh_chg_ref(dh_con,where,delta)
DH_CON * dh_con;
long where;
int delta;
{
union free_preamble x;

    if (fseek(dh_con->fp,where - sizeof(x), 0)< 0)
        return;
    (void) fread((char *) &x,sizeof(x),sizeof(char),dh_con->fp);
    x.free_header.next_free += delta;
    if (x.free_header.next_free <= 0)
        fhfree(dh_con, where);                  /* Zero references, free */
    else
    {
        if (fseek(dh_con->fp, - sizeof(x), 1) < 0)
            return;
        (void) fwrite((char *) &x,sizeof(x),sizeof(char),dh_con->fp);
        (void) fflush(dh_con->fp);
    }
    return;
}
/*
 * Routine to get back the next job number
 */
int fh_next_job_id(dh_con)
DH_CON * dh_con;
{
int next_job;

    next_job = dh_con->hash_preamble.hash_header.next_job;
    dh_con->hash_preamble.hash_header.next_job++;
    fh_head_write(dh_con);
    return next_job;
}
/*
 * Routine to read an item and to allocate memory for it.
 * The item will have been written with fh_put_from_mem(), and will
 * have its real length stored ahead of it.
 */
int fh_get_to_mem(dh_con,offset,read_type,read_ptr,read_len)
DH_CON * dh_con;
long offset;
int * read_type;
char ** read_ptr;
int * read_len;
{
    if (fseek(dh_con->fp,offset,0) < 0)
        return 0;
    if (fread((char *) read_type,sizeof(*read_type),1,dh_con->fp) <= 0)
        return 0;
    if (fread((char *) read_len,sizeof(*read_len),1,dh_con->fp) <= 0)
        return 0;
    if ((*read_ptr = (char *) malloc(*read_len)) == (char *) NULL)
        return 0;
    if (fread(*read_ptr,sizeof(char),*read_len,dh_con->fp) <= 0)
        return 0;
    return 1;
}
int fh_get_to_mem_swab(dh_con,offset,read_type,read_ptr,read_len)
DH_CON * dh_con;
long offset;
int * read_type;
char ** read_ptr;
int * read_len;
{
    if (fseek(dh_con->fp,offset,0) < 0)
        return 0;
    if (fread((char *) read_type,sizeof(*read_type),1,dh_con->fp) <= 0)
        return 0;
    *read_type =ntohl(*read_type);
    if (fread((char *) read_len,sizeof(*read_len),1,dh_con->fp) <= 0)
        return 0;
    *read_len =ntohl(*read_len);
    if ((*read_ptr = (char *) malloc(*read_len)) == (char *) NULL)
        return 0;
    if (fread(*read_ptr,sizeof(char),*read_len,dh_con->fp) <= 0)
        return 0;
    return 1;
}
/*
 * Routine to write out an element preceded by its length
 * The seek position is assumed to be correct (since it has just been
 * allocated?)
 */
int fh_put_from_mem(dh_con,write_type,write_ptr,write_len)
DH_CON * dh_con;
int write_type;
char * write_ptr;
int  write_len;
{
    if (fwrite((char *) &write_type,sizeof(write_type),1,dh_con->fp) <= 0)
        return 0;
    if (fwrite((char *) &write_len,sizeof(write_len),1,dh_con->fp) <= 0)
        return 0;
    if (fwrite(write_ptr,sizeof(char),write_len,dh_con->fp) <= 0)
        return 0;
    (void) fflush(dh_con->fp);
    return 1;
}

/************************************************************************
 * Add an entry to the hash table
 * -  There are two cases:
 *    - The data has already been stored
 *    - The data has not been stored
 * -  If the data has already been stored
 *    - The data value is an offset within the file
 *    - The length value is zero
 * -  If the data has not been stored
 *    - The data value is a memory pointer
 *    - The length value is the length of memory to store
 * -  The same considerations apply to the item
 * -  The next_free pointer in the header is used to hold a reference count
 * -  Items with a reference count of zero can be returned to the free
 *    space.
 * -  The routine inserts regardless; it does not care if values
 *    already exist.
 */
long fhinsert(dh_con,item,item_len,data_type,data,data_len)
DH_CON * dh_con;
char * item;
int item_len;
int data_type;
char * data;
int data_len;
{
unsigned hv;              /* hash value */
union hash_entry hash_entry;
int read_len;
char * read_ptr;
int read_type;
long h_offset;
/*
 * Start by getting the item into memory, if it isn't there already
 */
    if (item_len == 0)
        (void) fh_get_to_mem(dh_con,(long) item,&read_type,
                                                &read_ptr,&read_len);
    else
    {
         read_ptr = item;
         read_len = item_len;
    }
    hv = (*(dh_con->hashfunc))(read_ptr,read_len,
             dh_con->hash_preamble.hash_header.hash_size);     /* get hash # */
    h_offset = dh_con->hash_preamble.hash_header.hash_offset +
                      hv * sizeof(hash_entry);
    if (fseek(dh_con->fp, h_offset,0) < 0)
    {
        if (item_len == 0)
            free(read_ptr);
        return 0;
    }
    (void) fread((char *) &hash_entry,sizeof(char),
                  sizeof(hash_entry),dh_con->fp);
    while (hash_entry.hi.in_use && hash_entry.hi.next)
    {               /* chain exists, follow it looking for a free entry */
        h_offset = hash_entry.hi.next;
        if (fseek(dh_con->fp, h_offset, 0) < 0)
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        (void) fread((char *) &hash_entry,
                     sizeof(char),sizeof(hash_entry),dh_con->fp);
    }
    if (hash_entry.hi.in_use)
    {                                /* Come to the end of the chain */
        hash_entry.hi.next = fhmalloc(dh_con,sizeof(hash_entry) + sizeof(int)
                                      + sizeof(int));
        if (hash_entry.hi.next == 0)
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        if (!fh_put_from_mem(dh_con,HASH_TYPE,(char *) &hash_entry,
                               sizeof(hash_entry))) /* Dummy write to get
                                                       the record marked as
                                                       being an index entry */
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        fh_chg_ref(dh_con,hash_entry.hi.next,1);
                                     /* ensure that it won't get purged */
        hash_entry.hi.next +=  sizeof(int) + sizeof(int);
        if (fseek(dh_con->fp, h_offset, 0) < 0)
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        (void) fwrite((char *) &hash_entry,sizeof(char),
                      sizeof(hash_entry),dh_con->fp);
        (void) fflush(dh_con->fp);
        h_offset = hash_entry.hi.next;
        if (fseek(dh_con->fp, h_offset, 0) < 0)
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        hash_entry.hi.next = 0;
    }
/*
 * At this point, h_offset is the position of our hash entry,
 * and we have just read it
 */
    hash_entry.hi.in_use = 1;
    if (item_len != 0)
    {                        /* Store the key value */
        hash_entry.hi.name = fhmalloc(dh_con,item_len + sizeof(item_len) +
                                      sizeof(int));
        if (hash_entry.hi.name == 0)
            return 0;
        if (!fh_put_from_mem(dh_con,IND_TYPE,item,item_len))
            return 0;
        fh_chg_ref(dh_con,hash_entry.hi.name,1);
    }
    else hash_entry.hi.name = (long) item;
    if (data_len != 0)
    {                        /* Store the data value */
        hash_entry.hi.body = fhmalloc(dh_con,data_len + sizeof(data_len) +
                                      sizeof(int));
        if (hash_entry.hi.body == 0)
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        if (!fh_put_from_mem(dh_con,data_type,data,data_len))
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        fh_chg_ref(dh_con,hash_entry.hi.body,1);
    }
    else hash_entry.hi.body = (long) data;
/*
 * Write out the hash entry
 */
    if (fseek(dh_con->fp, h_offset, 0) < 0)
    {
        if (item_len == 0)
            free(read_ptr);
        return 0;
    }
    (void) fwrite((char *) &hash_entry,sizeof(char),
                  sizeof(hash_entry),dh_con->fp);
    (void) fflush(dh_con->fp);
    if (item_len == 0)
        free(read_ptr);
    return h_offset;
}

/*
 * Function to find element
 * - Fills in the hash_entry union
 * - Returns the offset in the file
 * - If the hash_entry->hi.in_use flag is set, gets another from the
 *   chain
 * - Otherwise, computes the hash
 * - Returns zero if nothing found
 *
 * Function to find a further matching element
 * - Fills in the hash_entry union
 * - Returns the offset in the file
 *
 */

long fhlookup(dh_con,item,item_len,hash_entry)
DH_CON * dh_con;
char * item;
int item_len;
union hash_entry * hash_entry;
{
unsigned hv;
int read_type;
int read_len;
char * read_ptr;
long h_offset;
/*
 * Start by getting the item into memory, if it isn't there already
 */
    if (item_len == 0)
        (void) fh_get_to_mem(dh_con,(long) item,&read_type,&read_ptr,&read_len);
    else
    {
         read_ptr = item;
         read_len = item_len;
    }
    if (hash_entry->hi.in_use == 0)
    {
        hv = (*(dh_con->hashfunc))(read_ptr,read_len,
             dh_con->hash_preamble.hash_header.hash_size);     /* get hash # */
        h_offset = dh_con->hash_preamble.hash_header.hash_offset +
                      hv * sizeof(*hash_entry);
    }
    else h_offset = hash_entry->hi.next;
    while (h_offset != 0)
    {               /* chain exists, follow it looking for a used entry */
        if (fseek(dh_con->fp, h_offset, 0) < 0)
        {
            if (item_len == 0)
                free(read_ptr);
            return 0;
        }
        (void) fread((char *) hash_entry,
                     sizeof(char),sizeof(*hash_entry),dh_con->fp);
        if (hash_entry->hi.in_use != 0)
        {
            char * check_ptr;
            int check_len;
            int check_type;
            (void) fh_get_to_mem(dh_con,hash_entry->hi.name,
                                  &check_type,&check_ptr,&check_len);
            if (check_len == read_len && !memcmp(read_ptr,check_ptr,check_len))
            {                             /* We have found a match */
                if (item_len == 0)
                    free(read_ptr);
                free(check_ptr);
                return h_offset;
            }
            else free(check_ptr);
        }
        h_offset = hash_entry->hi.next;
    }
    if (item_len == 0)
        free(read_ptr);
    return 0;
}
/*
 * Function to delete element
 * - Deletes the first match that it finds
 * - Call repeatedly in order to ensure that everything has gone
 * - Leaves not in use hash entries in place, in the chain
 *   (since the head item isn't fhmalloc()'ed anyway)
 * - Manipulation of the hash_entry.hi.in_use flag controls
 *   whether the next entry, or the entry looked up by hash,
 *   is deleted (property of fhlookup).
 * - In order to delete an arbitrary item, after inspection
 *   - find it
 *   - set up the hash_entry with in_use set to 1, and
 *     next pointing to the entry to be zapped
 *   - call fhremove()
 * - In order to purge the body, it must be specifically de-referenced,
 *   since the count is only set when it is inserted?
 */

long fhremove(dh_con,item,item_len,hash_entry)
DH_CON * dh_con;
char * item;
int item_len;
union hash_entry * hash_entry;
{
long h_offset;

    if ((h_offset = fhlookup(dh_con,item,item_len,hash_entry)) == 0)
        return 0;                   /* No such value */
/*
 * At this point, h_offset is the position of our hash entry,
 * and we have just read it
 */
    hash_entry->hi.in_use = 0;
/*
 * Write out the hash entry
 */
    if (fseek(dh_con->fp, h_offset, 0) < 0)
        return 0;
    (void) fwrite((char *) hash_entry,sizeof(char),
                  sizeof(*hash_entry),dh_con->fp);
    (void) fflush(dh_con->fp);
/*
 * Update the references
 */
    fh_chg_ref(dh_con,hash_entry->hi.name,-1);
    return h_offset;
}
/***********************************************************
 * The Hash Functions
 */
static unsigned rtab[] = {
    7092,
    5393,
    1741,
    4217,
    8537,
    3043,
    5931,
    9660,
    5143,
    1161,
    3498,
    1563,
    4308,
    6730,
    2102,
    1535,
    7973,
    3797,
    6688,
    2813,
    9165,
    3116,
    9907,
    1224,
    5662,
    2614
};
/*
 * Hash function for string keys
 */

unsigned string_hash(w,len,modulo)
char *w;
int len;
int modulo;
{
    char *p;
    unsigned x,y = 0;

    x = len;
    for (p = w + x - 1; p >= w; --p)
    {
        y += rtab[(*p % 26)];    /* effect outcome as broadly as possible */
        y += x*(*p);            /* make it depend on order */
        --x;
    }
    x = y & (modulo-1);
    return(x);
}
/*
 * Hash function for long keys
 */
unsigned long_hash(w,len,modulo)
long *w;
int len;
int modulo;
{
    return((*w) & (modulo-1));
/*
 * Just use the lower bits?
    short *x;
    x = (short *) w;
 
    return(((*x & 65535)^(*(x+1) &65535)) & (modulo-1));
 */
}
