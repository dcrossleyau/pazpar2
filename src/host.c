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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <yaz/log.h>
#include <yaz/nmem.h>
#include <yaz/matchstr.h>
#include <yaz/unix.h>

#include "ppmutex.h"
#include "session.h"
#include "host.h"

#include <sys/types.h>

struct database_hosts {
    struct host *hosts;
    YAZ_MUTEX mutex;
};

// Create a new host structure for hostport
static struct host *create_host(const char *proxy,
                                const char *tproxy,
                                CS_TYPE cs_type,
                                iochan_man_t iochan_man)
{
    struct host *host;

    host = xmalloc(sizeof(struct host));
    host->proxy = 0;
    host->tproxy = 0;
    if (proxy && *proxy)
        host->proxy = xstrdup(proxy);
    else
    {
        assert(tproxy);
        host->tproxy = xstrdup(tproxy);
    }
    host->connections = 0;
    host->ipport = 0;
    host->mutex = 0;

    if (cs_type == unix_type)
        host->ipport = xstrdup(host->tproxy);
    else
    {
        if (host_getaddrinfo(host, iochan_man))
        {
            xfree(host->ipport);
            xfree(host->tproxy);
            xfree(host->proxy);
            xfree(host);
            return 0;
        }
    }
    pazpar2_mutex_create(&host->mutex, "host");

    yaz_cond_create(&host->cond_ready);

    return host;
}

struct host *find_host(database_hosts_t hosts, const char *url,
                       const char *proxy, iochan_man_t iochan_man)
{
    struct host *p;
    char *tproxy = 0;
    const char *host = 0;
    CS_TYPE t;
    enum oid_proto proto;
    char *connect_host = 0;
    if (!cs_parse_host(url, &host, &t, &proto, &connect_host))
        return 0;
    else
    {
        if (t == unix_type)
            tproxy = xstrdup(url);
        else
        {
            if (proxy && *proxy)
            {
                /* plain proxy host already given, but we get CS_TYPE t  */
                xfree(connect_host);
            }
            else
            {
                if (connect_host)
                {
                    tproxy = connect_host;
                }
                else
                {
                    char *cp;
                    tproxy = xstrdup(host);
                    for (cp = tproxy; *cp; cp++)
                        if (strchr("/?#~", *cp))
                        {
                            *cp = '\0';
                            break;
                        }
                }
            }
        }
    }
    yaz_mutex_enter(hosts->mutex);
    for (p = hosts->hosts; p; p = p->next)
    {
        if (!yaz_strcmp_null(p->tproxy, tproxy) &&
            !yaz_strcmp_null(p->proxy, proxy))
        {
            break;
        }
    }
    if (!p)
    {
        p = create_host(proxy, tproxy, t, iochan_man);
        if (p)
        {
            p->next = hosts->hosts;
            hosts->hosts = p;
        }
    }
    yaz_mutex_leave(hosts->mutex);
    xfree(tproxy);
    return p;
}

database_hosts_t database_hosts_create(void)
{
    database_hosts_t p = xmalloc(sizeof(*p));
    p->hosts = 0;
    p->mutex = 0;
    pazpar2_mutex_create(&p->mutex, "database");
    return p;
}

void database_hosts_destroy(database_hosts_t *pp)
{
    if (*pp)
    {
        struct host *p = (*pp)->hosts;
        while (p)
        {
            struct host *p_next = p->next;
            yaz_mutex_destroy(&p->mutex);
            yaz_cond_destroy(&p->cond_ready);
            xfree(p->ipport);
            xfree(p);
            p = p_next;
        }
        yaz_mutex_destroy(&(*pp)->mutex);
        xfree(*pp);
    }
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

