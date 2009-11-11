/*
 * Revision Control Information
 *
 * $Id: prtime.c,v 1.4 2009/04/11 02:04:46 fabio Exp $
 *
 */
/* LINTLIBRARY */

#include <stdio.h>
#include "util.h"


/*
 *  util_print_time -- massage a long which represents a time interval in
 *  milliseconds, into a string suitable for output
 *
 *  Hack for IBM/PC -- avoids using floating point
 */

char *
util_print_time(long t)
{
    static char s[40];

    (void) sprintf(s, "%ld.%02ld sec", t/1000, (t%1000)/10);
    return s;
}
