/*
 * Copyright (c) 1995-1999, Index Data
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: eventl.h,v $
 * Revision 1.3  2007-04-08 23:04:20  adam
 * Moved HTTP channel address from struct iochan to struct http_channel.
 * This fixes compilation on FreeBSD and reverts eventl.{c,h} to original
 * paraz state. This change, simple as it is, is untested.
 *
 * Revision 1.1  2006/12/20 20:47:16  quinn
 * Reorganized source tree
 *
 * Revision 1.3  2006/12/12 02:36:24  quinn
 * Implemented session timeout; ping command
 *
 * Revision 1.2  2006/11/18 05:00:38  quinn
 * Added record retrieval, etc.
 *
 * Revision 1.1.1.1  2006/11/14 20:44:38  quinn
 * PazPar2
 *
 * Revision 1.1.1.1  2000/02/23 14:40:18  heikki
 * Original import to cvs
 *
 * Revision 1.11  1999/04/20 09:56:48  adam
 * Added 'name' paramter to encoder/decoder routines (typedef Odr_fun).
 * Modified all encoders/decoders to reflect this change.
 *
 * Revision 1.10  1998/01/29 13:30:23  adam
 * Better event handle system for NT/Unix.
 *
 * Revision 1.9  1997/09/01 09:31:48  adam
 * Removed definition statserv_remove from statserv.h to eventl.h.
 *
 * Revision 1.8  1995/06/19 12:39:09  quinn
 * Fixed bug in timeout code. Added BER dumper.
 *
 * Revision 1.7  1995/06/16  10:31:34  quinn
 * Added session timeout.
 *
 * Revision 1.6  1995/05/16  08:51:02  quinn
 * License, documentation, and memory fixes
 *
 * Revision 1.5  1995/05/15  11:56:37  quinn
 * Asynchronous facilities. Restructuring of seshigh code.
 *
 * Revision 1.4  1995/03/27  08:34:23  quinn
 * Added dynamic server functionality.
 * Released bindings to session.c (is now redundant)
 *
 * Revision 1.3  1995/03/15  08:37:42  quinn
 * Now we're pretty much set for nonblocking I/O.
 *
 * Revision 1.2  1995/03/14  10:28:00  quinn
 * More work on demo server.
 *
 * Revision 1.1  1995/03/10  18:22:45  quinn
 * The rudiments of an asynchronous server.
 *
 */

#ifndef EVENTL_H
#define EVENTL_H

#include <time.h>

struct iochan;

typedef void (*IOC_CALLBACK)(struct iochan *i, int event);

typedef struct iochan
{
    int fd;
    int flags;
#define EVENT_INPUT     0x01
#define EVENT_OUTPUT    0x02
#define EVENT_EXCEPT    0x04
#define EVENT_TIMEOUT   0x08
#define EVENT_WORK      0x10
    int force_event;
    IOC_CALLBACK fun;
    void *data;
    int destroyed;
    time_t last_event;
    time_t max_idle;
    
    struct iochan *next;
} *IOCHAN;

#define iochan_destroy(i) (void)((i)->destroyed = 1)
#define iochan_getfd(i) ((i)->fd)
#define iochan_setfd(i, f) ((i)->fd = (f))
#define iochan_getdata(i) ((i)->data)
#define iochan_setdata(i, d) ((i)->data = d)
#define iochan_getflags(i) ((i)->flags)
#define iochan_setflags(i, d) ((i)->flags = d)
#define iochan_setflag(i, d) ((i)->flags |= d)
#define iochan_clearflag(i, d) ((i)->flags &= ~(d))
#define iochan_getflag(i, d) ((i)->flags & d ? 1 : 0)
#define iochan_getfun(i) ((i)->fun)
#define iochan_setfun(i, d) ((i)->fun = d)
#define iochan_setevent(i, e) ((i)->force_event = (e))
#define iochan_getnext(i) ((i)->next)
#define iochan_settimeout(i, t) ((i)->max_idle = (t), (i)->last_event = time(0))
#define iochan_activity(i) ((i)->last_event = time(0))

IOCHAN iochan_create(int fd, IOC_CALLBACK cb, int flags);
int event_loop(IOCHAN *iochans);

#endif
