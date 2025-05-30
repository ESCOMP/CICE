/*
** This file was downloaded and modified from https://github.com/jmrosinski/GPTL
** with the following Copyright permission,
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the “Software”), to deal
** in the Software for any noncommercial purposes without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to permit
** persons to whom the Software is furnished to do so, subject to the following
** conditions: The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.  Any
** commercial use (including sale) of the software, and derivative development
** towards commercial use, requires written permission of the copyright
** holder. THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO
** EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
** OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
**
*/

/*
** $Id: get_memusage.c,v 1.10 2010-11-09 19:08:53 rosinski Exp $
**
** Author: Jim Rosinski
**   Credit to Chuck Bardeen for MACOS section (__APPLE__ ifdef)
**
** get_memusage:
**
**   Designed to be called from Fortran, returns information about memory
**   usage in each of 5 input int* args.  On Linux read from the /proc
**   filesystem because getrusage() returns placebos (zeros).  Return -1 for
**   values which are unavailable or ambiguous on a particular architecture.
**
**   Return value: 0  = success
**                 -1 = failure
*/

#define _NO_CHANGE      0
#define _UPPER_CASE     1
#define _ADD_UNDERSCORE 2
#define _ADD_TWO_UNDERSCORES 3

#ifdef FORTRANUNDERSCORE
#define NAMING _ADD_UNDERSCORE
#endif

#ifdef FORTRANDOUBLEUNDERSCORE
#define NAMING _ADD_TWO_UNDERSCORES
#endif

#ifdef FORTRANCAPS
#define NAMING _UPPER_CASE
#endif

#ifndef NAMING
#define NAMING _NO_CHANGE
#endif

#if (NAMING == _ADD_UNDERSCORE)
#define ice_memusage_gptl ice_memusage_gptl_
#endif

#if (NAMING == _ADD_TWO_UNDERSCORES)
#define ice_memusage_gptl ice_memusage_gptl__
#endif

#if (NAMING == _UPPER_CASE)
#define ice_memusage_gptl ICE_MEMUSAGE_GPTL
#endif


#include <sys/resource.h>

/*#include "gptl.h" */   /* additional cpp defs and function prototypes */
/* extern int ice_memusage_gptl (int *, int *, int *, int *, int *);  */

/* _AIX is automatically defined when using the AIX C compilers */
#ifdef _AIX
#include <sys/times.h>
#endif

#ifdef IRIX64
#include <sys/time.h>
#endif

#ifdef HAVE_SLASHPROC

#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

#elif (defined __APPLE__)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#endif

#ifdef BGP

#include <spi/kernel_interface.h>
#include <common/bgp_personality.h>
#include <common/bgp_personality_inlines.h>
#include <malloc.h>
#define   Personality                    _BGP_Personality_t

#endif

#ifdef BGQ

#include <malloc.h>
#include <spi/include/kernel/memory.h>

#endif

int ice_memusage_gptl (int *size, int *rss, int *share, int *text, int *datastack)
{
#if defined (BGP) || defined(BGQ)

  long long alloc;
  struct mallinfo m;
#if defined (BGP)
  Personality pers;
#endif
#if defined (BGQ)
  uint64_t shared_mem_count;
#endif
  long long total;
  int node_config;

 /* memory available */
#if defined(BGP)
  Kernel_GetPersonality(&pers, sizeof(pers));
  total = BGP_Personality_DDRSizeMB(&pers);

  node_config  = BGP_Personality_processConfig(&pers);
  if (node_config == _BGP_PERS_PROCESSCONFIG_VNM) total /= 4;
  else if (node_config == _BGP_PERS_PROCESSCONFIG_2x2) total /= 2;
  total *= 1024*1024;

  *size = total;
#endif

#if defined(BGQ)
  Kernel_GetMemorySize(KERNEL_MEMSIZE_SHARED, &shared_mem_count);

  shared_mem_count *= 1024*1024;
  *size = shared_mem_count;

#endif
  /* total memory used  - heap only (not static memory)*/

  m = mallinfo();
  alloc = m.hblkhd + m.uordblks;

  *rss = alloc;
  *share     = -1;
  *text     = -1;
  *datastack = -1;


#elif (defined HAVE_SLASHPROC)
  FILE *fd;                       /* file descriptor for fopen */
  int pid;                        /* process id */
  static char *head = "/proc/";   /* part of path */
  static char *tail = "/statm";   /* part of path */
  char file[19];                  /* full path to file in /proc */
  int dum;                        /* placeholder for unused return arguments */
  int ret;                        /* function return value */

  /*
  ** The file we want to open is /proc/<pid>/statm
  */

  pid = (int) getpid ();
  /* tcraig, not needed and can be problematic
  **  if (pid > 999999) {
  **    fprintf (stderr, "get_memusage: pid %d is too large\n", pid);
  **    return -1;
  **  }
  */

  sprintf (file, "%s%d%s", head, pid, tail);
  if ((fd = fopen (file, "r")) < 0) {
    fprintf (stderr, "get_memusage: bad attempt to open %s\n", file);
    return -1;
  }

  /*
  ** Read the desired data from the /proc filesystem directly into the output
  ** arguments, close the file and return.
  */

  ret = fscanf (fd, "%d %d %d %d %d %d %d",
                size, rss, share, text, datastack, &dum, &dum);
  ret = fclose (fd);
  return 0;

#elif (defined __APPLE__)

  FILE *fd;
  char cmd[60];
  int pid = (int) getpid ();

  sprintf (cmd, "ps -o vsz -o rss -o tsiz -p %d | grep -v RSS", pid);
  fd = popen (cmd, "r");

  if (fd) {
    fscanf (fd, "%d %d %d", size, rss, text);
    *share     = -1;
    *datastack = -1;
    (void) pclose (fd);
  }

  return 0;

#else

  struct rusage usage;         /* structure filled in by getrusage */

  if (getrusage (RUSAGE_SELF, &usage) < 0)
    return -1;

  *size      = -1;
  *rss       = usage.ru_maxrss;
  *share     = -1;
  *text      = -1;
  *datastack = -1;
#ifdef IRIX64
  *datastack = usage.ru_idrss + usage.ru_isrss;
#endif
  return 0;

#endif
}
