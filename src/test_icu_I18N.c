/* $Id: test_icu_I18N.c,v 1.1 2007-04-27 14:31:15 marc Exp $
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

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <yaz/test.h>



#ifdef HAVE_ICU



void test_icu_I18N(int argc, char **argv)
{

  YAZ_CHECK(0 == 0);
  //YAZ_CHECK_EQ(0, 1);
}

#endif    

int main(int argc, char **argv)
{

    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG();

#ifdef HAVE_ICU

    test_icu_I18N(argc, argv); 
 
#else

    YAZ_CHECK(0 == 0);

#endif    
   
    YAZ_CHECK_TERM;
}




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
