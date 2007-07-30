/* $Id: config.h,v 1.26 2007-07-30 11:52:08 quinn Exp $
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

#ifndef CONFIG_H
#define CONFIG_H

#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <yaz/nmem.h>

#ifdef HAVE_ICU
#include "icu_I18N.h"
#endif // HAVE_ICU


enum conf_metadata_type {
    Metadata_type_generic,    // Generic text field
    Metadata_type_number,     // A number
    Metadata_type_year        // A number
};

enum conf_metadata_merge {
    Metadata_merge_no,        // Don't merge
    Metadata_merge_unique,    // Include unique elements in merged block
    Metadata_merge_longest,   // Include the longest (strlen) value
    Metadata_merge_range,     // Store value as a range of lowest-highest
    Metadata_merge_all        // Just include all elements found
};

enum conf_sortkey_type {
    Metadata_sortkey_relevance,
    Metadata_sortkey_numeric,       // Standard numerical sorting
    Metadata_sortkey_skiparticle,   // Skip leading article when sorting
    Metadata_sortkey_string         // Flat string
};

// This controls the ability to insert 'static' values from settings into retrieval recs
enum conf_setting_type {
    Metadata_setting_no,
    Metadata_setting_postproc,      // Insert setting value into normalized record
    Metadata_setting_parameter      // Expose value to normalization stylesheets
};

// Describes known metadata elements and how they are to be manipulated
// An array of these structure provides a 'map' against which
// discovered metadata elements are matched. It also governs storage,
// to minimize number of cycles needed at various tages of processing
struct conf_metadata 
{
    char *name;  // The field name. Output by normalization stylesheet
    int brief;   // Is this element to be returned in the brief format?
    int termlist;// Is this field to be treated as a termlist for browsing?
    int rank;    // Rank factor. 0 means don't use this field for ranking, 
                 // 1 is default
                 // values >1  give additional significance to a field
    int sortkey_offset; // -1 if it's not a sortkey, otherwise index
                        // into service/record_cluster->sortkey array
    enum conf_metadata_type type;
    enum conf_metadata_merge merge;
    enum conf_setting_type setting; // Value is to be taken from session/db settings?
};



struct conf_metadata * conf_metadata_assign(NMEM nmem, 
                                            struct conf_metadata * metadata,
                                            const char *name,
                                            enum conf_metadata_type type,
                                            enum conf_metadata_merge merge,
                                            enum conf_setting_type setting,
                                            int brief,
                                            int termlist,
                                            int rank,
                                            int sortkey_offset);



// Controls sorting
struct conf_sortkey
{
    char *name;
    enum conf_sortkey_type type;
};

struct conf_sortkey * conf_sortkey_assign(NMEM nmem, 
                                            struct conf_sortkey * sortkey,
                                            const char *name,
                                            enum conf_sortkey_type type);



// It is conceivable that there will eventually be several 'services'
// offered from one server, with separate configuration -- possibly
// more than one services associated with the same port. For now,
// however, only a single service is possible.
struct conf_service
{
    int num_metadata;
    struct conf_metadata *metadata;
    int num_sortkeys;
    struct conf_sortkey *sortkeys;

};

struct conf_service * conf_service_create(NMEM nmem, 
                                          int num_metadata, int num_sortkeys);


struct conf_metadata* conf_service_add_metadata(NMEM nmem, 
                                                struct conf_service *service,
                                                int field_id,
                                                const char *name,
                                                enum conf_metadata_type type,
                                                enum conf_metadata_merge merge,
                                                enum conf_setting_type setting,
                                                int brief,
                                                int termlist,
                                                int rank,
                                                int sortkey_offset);

struct conf_sortkey * conf_service_add_sortkey(NMEM nmem,
                                               struct conf_service *service,
                                               int field_id,
                                               const char *name,
                                               enum conf_sortkey_type type);


int conf_service_metadata_field_id(struct conf_service *service, const char * name);

int conf_service_sortkey_field_id(struct conf_service *service, const char * name);


struct conf_server
{
    char *host;
    int port;
    char *proxy_host;
    int proxy_port;
    char *myurl;
    char *settings;

#ifdef HAVE_ICU
    struct icu_chain * icu_chn;
#endif // HAVE_ICU

    struct conf_service *service;
    struct conf_server *next;
};

struct conf_targetprofiles
{
    enum {
        Targetprofiles_local
    } type;
    char *src;
};

struct conf_config
{
    struct conf_server *servers;
    struct conf_targetprofiles *targetprofiles;
};

#ifndef CONFIG_NOEXTERNS

extern struct conf_config *config;

#endif

int read_config(const char *fname);
xsltStylesheet *conf_load_stylesheet(const char *fname);

#endif

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
