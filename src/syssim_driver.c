/*
 * DiskSim Storage Subsystem Simulation Environment (Version 4.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001-2008.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */

/*
 * A sample skeleton for a system simulator that calls DiskSim as
 * a slave.
 *
 * Contributed by Eran Gabber of Lucent Technologies - Bell Laboratories
 *
 * Usage:
 *  syssim <parameters file> <output file> <max. block number>
 * Example:
 *  syssim parv.seagate out 2676846
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/time.h>

#include "syssim_driver.h"
#include "disksim_interface.h"
#include "disksim_rand48.h"


#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"

#define BLOCK 4096
#define SECTOR  512
#define BLOCK2SECTOR  (BLOCK/SECTOR)

typedef struct  {
  int n;
  double sum;
  double sqr;
} Stat;


static SysTime now = 0;   /* current time */
static SysTime next_event = -1; /* next event */
static int completed = 0; /* last request was completed */
static Stat st;
static Stat wst;
static Stat rst;
static Stat wstp;
static Stat rstp;

int alln=0;
int MAXREQ=0;

struct timeval start,start1;
struct timeval end,end1;


void
panic(const char *s) 
{
  perror(s);
  exit(1);
}


void
add_statistics_page(Stat *s, int p, double x)
{
  s->n+=p;
  s->sum += x;
  s->sqr += x*x;
}

void
add_statistics(Stat *s, double x)
{
  s->n++;
  s->sum += x;
  s->sqr += x*x; 
}


void
print_statistics(Stat *s, Stat *ws, Stat *rs,Stat *wsp, Stat *rsp, const char *title)
{
  double avg, std;
  double wavg, wstd;
  double ravg, rstd;
  double wpavg, wpstd; 
  double rpavg, rpstd;

  avg = s->sum/s->n;
  std = sqrt((s->sqr - 2*avg*s->sum + s->n*avg*avg) / s->n);

  wavg = ws->sum/ws->n;
  wstd = sqrt((ws->sqr - 2*wavg*ws->sum + ws->n*wavg*wavg) / ws->n);

  wpavg = wsp->sum/wsp->n;
  wpstd = sqrt((wsp->sqr - 2*wpavg*wsp->sum + wsp->n*wpavg*wpavg) / wsp->n);

  ravg = rs->sum/rs->n;
  rstd = sqrt((rs->sqr - 2*ravg*rs->sum + rs->n*ravg*ravg) / rs->n);

  rpavg = rsp->sum/rsp->n;
  rpstd = sqrt((rsp->sqr - 2*rpavg*rsp->sum + rsp->n*rpavg*rpavg) / rsp->n);

  printf("\nall %s: n=%d average=%f std. deviation=%f\n", title, s->n, avg, std);

  printf("write %s: n=%d average=%f std. deviation=%f\n", title, ws->n, wavg, wstd);
  printf("write_page %s: n=%d average=%f std. deviation=%f\n", title, wsp->n, wpavg, wpstd);

  printf("read %s: n=%d average=%f std. deviation=%f\n", title, rs->n, ravg, rstd);
  printf("read_page %s: n=%d average=%f std. deviation=%f\n", title, rsp->n, rpavg, rpstd);
}



/*
 * Schedule next callback at time t.
 * Note that there is only *one* outstanding callback at any given time.
 * The callback is for the earliest event.
 */
void
syssim_schedule_callback(disksim_interface_callback_t fn, 
       SysTime t, 
       void *ctx)
{
  next_event = t;
}


/*
 * de-scehdule a callback.
 */
void
syssim_deschedule_callback(double t, void *ctx)
{
  next_event = -1;
}


void
syssim_report_completion(SysTime t, struct disksim_request *r, void *ctx)
{
  completed = 1;
  now = t;
  alln++;
  if(alln > (int)((double)MAXREQ*0.6))
  {
    //add_statistics_page(&st, r->bytecount/4096, t - r->start);
    add_statistics(&st, t - r->start);
    if(r->flags == 0)
    {
      add_statistics_page(&wstp, r->bytecount/4096, t - r->start);
      add_statistics(&wst, t - r->start);
    }
    else if(r->flags == 1)
    {
      add_statistics_page(&rstp, r->bytecount/4096, t - r->start);
      add_statistics(&rst, t - r->start);
    }
  }
}

int
main(int argc, char *argv[])
{
  int i;
  int nsectors;
  struct stat buf;
  //struct disksim_request r;
  struct disksim_interface *disksim;

  /*add*/
  //FILE *fread = fopen("/home/osnet/trace/Financial2forssd_cut.txt","r");
  outputfd = fopen("src/syssim_output1","w");
  lpb_ppn = fopen("/home/osnet/disksim_blas_lpb/lpb_ppn1.txt","w");
  lpb_lpn = fopen("/home/osnet/disksim_blas_lpb/lpb_lpn1.txt","w");
 /*add end*/

  if (argc != 6 || (nsectors = atoi(argv[3])) <= 0) {
    fprintf(stderr, "usage: %s <param file> <output file> <#sectors> <trace> <MAXREQ>\n",
      argv[0]);
    exit(1);
  }

  if (stat(argv[1], &buf) < 0)
   panic(argv[1]);
  FILE *fread = fopen(argv[4],"r");
  MAXREQ = atoi(argv[5]);
  disksim = disksim_interface_initialize(argv[1], 
           argv[2],
           syssim_report_completion,
           syssim_schedule_callback,
           syssim_deschedule_callback,
           0,
           0,
           0);

  /*add*/
  double time;
  int devno;
  int blnum=0,R_W=0,size;

 
  if (fread == NULL)
    perror ("Error opening file");
  else {
    int k=1;
    while(fscanf(fread,"%lf%ld%ld%ld%ld",&time,&devno,&blnum,&size,&R_W)!= EOF){ //接收新的request
      //printf("%d----接收新的request----\n",k);
      k++;
      //if(R_W == 0)
      //{
        struct disksim_request *r = malloc(sizeof(struct disksim_request));
        r->start = now;
        r->flags = R_W;
        r->devno = devno;
        r->blkno = blnum;
        r->bytecount = size * 512;  // ssd 4096
        completed = 0;
        unsigned long diff;
    gettimeofday(&start1, NULL);
        disksim_interface_request_arrive(disksim, now, r);

        /* Process events until this I/O is completed */
        while(next_event >= 0) {
          now = next_event;
          next_event = -1;
          disksim_interface_internal_event(disksim, now, 0);
        }
        gettimeofday(&end1, NULL);
    diff=1000000 * (end1.tv_sec-start1.tv_sec)+ end1.tv_usec-start1.tv_usec;
    fprintf(outputfd,"$$$$$ disksim_interface_request_arrive TIME = %ld\n",diff);
        if (!completed) {
          fprintf(stderr,
            "%s: internal error. Last event not completed %d\n",
            argv[0], i);
          exit(1);
        }
      //}
    }
  }

  /*add end*/ 
  printf(YELLOW"\n[LPB] Trace File=%s, max_req=%d\n"NONE, argv[4], atoi(argv[5]));
//

 
  /* NOTE: it is bad to use this internal disksim call from external... */
  DISKSIM_srand48(1); 

  /*for (i=0; i < 1000; i++) {
    r.start = now;
    r.flags = DISKSIM_READ;;
    r.devno = 0;*/

    /* NOTE: it is bad to use this internal disksim call from external... */
    /*r.blkno = BLOCK2SECTOR*(DISKSIM_lrand48()%(nsectors/BLOCK2SECTOR));
    r.bytecount = BLOCK;
    completed = 0;
    disksim_interface_request_arrive(disksim, now, &r);

    /* Process events until this I/O is completed */
    /*while(next_event >= 0) {
      now = next_event;
      next_event = -1;
      disksim_interface_internal_event(disksim, now, 0);
    }

    if (!completed) {
      fprintf(stderr,
        "%s: internal error. Last event not completed %d\n",
        argv[0], i);
      exit(1);
    }
  }*/

  disksim_interface_shutdown(disksim, now);

  print_statistics(&st, &wst, &rst, &wstp, &rstp, "response time");

  fclose(lpb_lpn);
  printf("lpb_lpn\n");
  fclose(lpb_ppn);
  printf("lpb_ppn\n");
  fclose(fread);
  printf("fread\n");


  exit(0);
}
