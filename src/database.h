/* $Id: database.h,v 1.6 2007-04-11 18:42:25 quinn Exp $
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

#ifndef DATABASE_H
#define DATABASE_H

void load_simpletargets(const char *fn);
void prepare_databases(void);
struct database *find_database(const char *id, int new);
int database_match_criteria(struct session_database *db, struct database_criterion *cl);
int session_grep_databases(struct session *se, struct database_criterion *cl,
        void (*fun)(void *context, struct session_database *db));
int grep_databases(void *context, struct database_criterion *cl,
        void (*fun)(void *context, struct database *db));
int match_zurl(const char *zurl, const char *pattern);

#endif
