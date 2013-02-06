/* This file is part of Pazpar2.
   Copyright (C) 2006-2013 Index Data

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

/** \file
    \brief control MUTEX debugging
*/

#ifndef PAZPAR2_PPMUTEX_H
#define PAZPAR2_PPMUTEX_H

#include <yaz/mutex.h>

#if YAZ_POSIX_THREADS
#include <pthread.h>
#endif

void pazpar2_mutex_init(void);

void pazpar2_mutex_create(YAZ_MUTEX *p, const char *name);

typedef struct {
    int readers_reading;
    int writers_writing;
#if YAZ_POSIX_THREADS
    pthread_mutex_t mutex;
    pthread_cond_t lock_free;
#endif
} Pazpar2_lock_rdwr;

void pazpar2_lock_rdwr_init(Pazpar2_lock_rdwr *p);
void pazpar2_lock_rdwr_destroy(Pazpar2_lock_rdwr *p);
void pazpar2_lock_rdwr_rlock(Pazpar2_lock_rdwr *p);
void pazpar2_lock_rdwr_wlock(Pazpar2_lock_rdwr *p);
void pazpar2_lock_rdwr_runlock(Pazpar2_lock_rdwr *p);
void pazpar2_lock_rdwr_wunlock(Pazpar2_lock_rdwr *p);
void pazpar2_lock_rdwr_upgrade(Pazpar2_lock_rdwr *p);
void pazpar2_lock_rdwr_downgrade(Pazpar2_lock_rdwr *p);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

