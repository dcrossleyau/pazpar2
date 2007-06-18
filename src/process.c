/* $Id: process.c,v 1.3 2007-06-18 11:10:20 adam Exp $
   Copyright (c) 2006-2007, Index Data.

This file is part of Pazpar2.

Pazpar2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Pazpar2 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Pazpar2; see the file LICENSE.  If not, write to the
Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
 */

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>

#include <yaz/log.h>

#include "pazpar2.h"

static void write_pidfile(const char *pidfile)
{
    if (pidfile)
    {
        FILE *f = fopen(pidfile, "w");
        if (!f)
        {
            yaz_log(YLOG_ERRNO|YLOG_FATAL, "Couldn't create %s", pidfile);
            exit(0);
        }
        fprintf(f, "%ld", (long) getpid());
        fclose(f);
    }
}

pid_t child_pid = 0;
void kill_child_handler(int num)
{
    if (child_pid)
        kill(child_pid, num);
}

int pazpar2_process(int debug, int daemon,
                    void (*work)(void *data), void *data,
                    const char *pidfile, const char *uid /* not yet used */)
{
    int run = 1;
    int cont = 1;
    void (*old_sighup)(int);
    void (*old_sigterm)(int);

    if (debug)
    {
        /* in debug mode.. it's quite simple */
        write_pidfile(pidfile);
        work(data);
        exit(0);
    }
    
    write_pidfile(pidfile);
    /* running in production mode. */
    if (uid)
    {
        /* OK to use the non-thread version here */
        struct passwd *pw = getpwnam(uid);
        if (!pw)
        {
            yaz_log(YLOG_FATAL, "%s: Unknown user", uid);
            exit(1);
        }
        if (setuid(pw->pw_uid) < 0)
        {
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "setuid");
            exit(1);
        }
    }

    if (daemon)
    {
        /* create pipe so that parent waits until child has created
           PID (or failed) */
        static int hand[2]; /* hand shake for child */
        if (pipe(hand) < 0)
        {
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "pipe");
            return 1;
        }
        switch (fork())
        {
        case 0: 
            break;
        case -1:
            return 1;
        default:
            close(hand[1]);
            while(1)
            {
                char dummy[1];
                int res = read(hand[0], dummy, 1);
                if (res < 0 && errno != EINTR)
                {
                    yaz_log(YLOG_FATAL|YLOG_ERRNO, "read fork handshake");
                    break;
                }
                else if (res >= 0)
                    break;
            }
            close(hand[0]);
            _exit(0);
        }
        /* child */
        close(hand[0]);
        if (setsid() < 0)
            return 1;
        
        close(0);
        close(1);
        close(2);
        open("/dev/null", O_RDWR);
        dup(0); dup(0);
        close(hand[1]);
    }
    /* keep signals in their original state and make sure that some signals
       to parent process also gets sent to the child.. 
    */
    old_sighup = signal(SIGHUP, kill_child_handler);
    old_sigterm = signal(SIGTERM, kill_child_handler);
    while (cont)
    {
        pid_t p = fork();
        pid_t p1;
        int status;
        if (p == (pid_t) (-1))
        {
            
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "fork");
            exit(1);
        }
        else if (p == 0)
        {
            /* child */
            signal(SIGHUP, old_sighup);  /* restore */
            signal(SIGTERM, old_sigterm);/* restore */

            work(data);
            exit(0);
        }

        /* enable signalling in kill_child_handler */
        child_pid = p;
        
        p1 = wait(&status);

        /* disable signalling in kill_child_handler */
        child_pid = 0;

        if (p1 != p)
        {
            yaz_log(YLOG_FATAL, "p1=%d != p=%d", p1, p);
            exit(1);
        }
        
        if (WIFSIGNALED(status))
        {
            /*  keep the child alive in case of errors, but _log_ */
            switch(WTERMSIG(status)) {
            case SIGILL:
                yaz_log(YLOG_WARN, "Received SIGILL from child %ld", (long) p);
                cont = 1;
                break;
            case SIGABRT:
                yaz_log(YLOG_WARN, "Received SIGABRT from child %ld", (long) p);
                cont = 1;
                break ;
            case SIGSEGV:
                yaz_log(YLOG_WARN, "Received SIGSEGV from child %ld", (long) p);
                cont = 1;
                break;
            case SIGBUS:        
                yaz_log(YLOG_WARN, "Received SIGBUS from child %ld", (long) p);
                cont = 1;
                break;
            case SIGTERM:
                yaz_log(YLOG_LOG, "Received SIGTERM from child %ld",
                        (long) p);
                cont = 0;
                break;
            default:
                yaz_log(YLOG_WARN, "Received SIG %d from child %ld",
                        WTERMSIG(status), (long) p);
                cont = 0;
            }
        }
        else if (status == 0)
            cont = 0; /* child exited normally */
        else
        {   /* child exited with error */
            yaz_log(YLOG_LOG, "Exit %d from child %ld", status, (long) p);
            cont = 0;
        }
        if (cont) /* respawn slower as we get more errors */
            sleep(1 + run/5);
        run++;
    }
    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
