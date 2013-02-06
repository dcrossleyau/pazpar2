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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <yaz/log.h>
#include "ppmutex.h"

static int ppmutex_level = 0;

void pazpar2_mutex_init(void)
{
    ppmutex_level = yaz_log_module_level("mutex");
}

void pazpar2_mutex_create(YAZ_MUTEX *p, const char *name)
{
    assert(p);
    yaz_mutex_create(p);
    yaz_mutex_set_name(*p, ppmutex_level, name);
}

void pazpar2_lock_rdwr_init(Pazpar2_lock_rdwr *p)
{
    p->readers_reading = 0;
    p->writers_writing = 0;
#if YAZ_POSIX_THREADS
    pthread_mutex_init(&p->mutex, 0);
    pthread_cond_init(&p->lock_free, 0);
#endif
}

void pazpar2_lock_rdwr_destroy(Pazpar2_lock_rdwr *p)
{
    assert (p->readers_reading == 0);
    assert (p->writers_writing == 0);
#if YAZ_POSIX_THREADS
    pthread_mutex_destroy(&p->mutex);
    pthread_cond_destroy(&p->lock_free);
#endif
}

void pazpar2_lock_rdwr_rlock(Pazpar2_lock_rdwr *p)
{
#if YAZ_POSIX_THREADS
    pthread_mutex_lock(& p->mutex);
    while (p->writers_writing)
	pthread_cond_wait(&p->lock_free, &p->mutex);
    p->readers_reading++;
    pthread_mutex_unlock(&p->mutex);
#endif
}

void pazpar2_lock_rdwr_wlock(Pazpar2_lock_rdwr *p)
{
#if YAZ_POSIX_THREADS
    pthread_mutex_lock(&p->mutex);
    while (p->writers_writing || p->readers_reading)
	pthread_cond_wait(&p->lock_free, &p->mutex);
    p->writers_writing++;
    pthread_mutex_unlock(&p->mutex);
#endif
}

void pazpar2_lock_rdwr_upgrade(Pazpar2_lock_rdwr *p)
{
#if YAZ_POSIX_THREADS
    assert(p->readers_reading > 0);
    pthread_mutex_lock(&p->mutex);
    --p->readers_reading;
    while (p->writers_writing || p->readers_reading)
	pthread_cond_wait(&p->lock_free, &p->mutex);
    p->writers_writing++;
    pthread_mutex_unlock(&p->mutex);
#endif
}

void pazpar2_lock_rdwr_downgrade(Pazpar2_lock_rdwr *p)
{
#if YAZ_POSIX_THREADS
    assert(p->writers_writing == 1);
    pthread_mutex_lock(&p->mutex);
    p->writers_writing--;
    p->readers_reading++;
    pthread_cond_broadcast(&p->lock_free);
    pthread_mutex_unlock(&p->mutex);
#endif
}

void pazpar2_lock_rdwr_runlock(Pazpar2_lock_rdwr *p)
{
#if YAZ_POSIX_THREADS
    assert(p->readers_reading > 0);
    pthread_mutex_lock(&p->mutex);
    p->readers_reading--;
    if (p->readers_reading == 0)
        pthread_cond_signal(&p->lock_free);
    pthread_mutex_unlock(&p->mutex);
#endif
}

void pazpar2_lock_rdwr_wunlock(Pazpar2_lock_rdwr *p)
{
#if YAZ_POSIX_THREADS
    assert(p->writers_writing == 1);
    pthread_mutex_lock(&p->mutex);
    p->writers_writing--;
    pthread_cond_broadcast(&p->lock_free);
    pthread_mutex_unlock(&p->mutex);
#endif
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

