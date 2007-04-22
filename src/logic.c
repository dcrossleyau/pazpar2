/* $Id: logic.c,v 1.18 2007-04-22 16:41:42 adam Exp $
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

// This file contains the primary business logic. Several parts of it should
// Eventually be factored into separate modules.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>

#include <yaz/marcdisp.h>
#include <yaz/comstack.h>
#include <yaz/tcpip.h>
#include <yaz/proto.h>
#include <yaz/readconf.h>
#include <yaz/pquery.h>
#include <yaz/otherinfo.h>
#include <yaz/yaz-util.h>
#include <yaz/nmem.h>
#include <yaz/query-charset.h>
#include <yaz/querytowrbuf.h>
#if YAZ_VERSIONL >= 0x020163
#include <yaz/oid_db.h>
#endif

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <netinet/in.h>

#include "pazpar2.h"
#include "eventl.h"
#include "http.h"
#include "termlists.h"
#include "reclists.h"
#include "relevance.h"
#include "config.h"
#include "database.h"
#include "settings.h"

#define MAX_CHUNK 15

static void client_fatal(struct client *cl);
static void connection_destroy(struct connection *co);
static int client_prep_connection(struct client *cl);
static void ingest_records(struct client *cl, Z_Records *r);
void session_alert_watch(struct session *s, int what);

static struct connection *connection_freelist = 0;
static struct client *client_freelist = 0;

static char *client_states[] = {
    "Client_Connecting",
    "Client_Connected",
    "Client_Idle",
    "Client_Initializing",
    "Client_Searching",
    "Client_Presenting",
    "Client_Error",
    "Client_Failed",
    "Client_Disconnected",
    "Client_Stopped"
};

// Note: Some things in this structure will eventually move to configuration
struct parameters global_parameters = 
{
    "",
    "",
    "",
    "",
    0,
    0,
    30,
    "81",
    "Index Data PazPar2",
    VERSION,
    600, // 10 minutes
    60,
    100,
    MAX_CHUNK,
    0,
    0
};

static int send_apdu(struct client *c, Z_APDU *a)
{
    struct connection *co = c->connection;
    char *buf;
    int len, r;

    if (!z_APDU(global_parameters.odr_out, &a, 0, 0))
    {
        odr_perror(global_parameters.odr_out, "Encoding APDU");
	abort();
    }
    buf = odr_getbuf(global_parameters.odr_out, &len, 0);
    r = cs_put(co->link, buf, len);
    if (r < 0)
    {
        yaz_log(YLOG_WARN, "cs_put: %s", cs_errmsg(cs_errno(co->link)));
        return -1;
    }
    else if (r == 1)
    {
        fprintf(stderr, "cs_put incomplete (ParaZ does not handle that)\n");
        exit(1);
    }
    odr_reset(global_parameters.odr_out); /* release the APDU structure  */
    co->state = Conn_Waiting;
    return 0;
}

// Set authentication token in init if one is set for the client
// TODO: Extend this to handle other schemes than open (should be simple)
static void init_authentication(struct client *cl, Z_InitRequest *req)
{
    struct session_database *sdb = cl->database;
    char *auth = session_setting_oneval(sdb, PZ_AUTHENTICATION);

    if (auth)
    {
        Z_IdAuthentication *idAuth = odr_malloc(global_parameters.odr_out,
                sizeof(*idAuth));
        idAuth->which = Z_IdAuthentication_open;
        idAuth->u.open = auth;
        req->idAuthentication = idAuth;
    }
}

static void send_init(IOCHAN i)
{

    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_initRequest);

    a->u.initRequest->implementationId = global_parameters.implementationId;
    a->u.initRequest->implementationName = global_parameters.implementationName;
    a->u.initRequest->implementationVersion =
	global_parameters.implementationVersion;
    ODR_MASK_SET(a->u.initRequest->options, Z_Options_search);
    ODR_MASK_SET(a->u.initRequest->options, Z_Options_present);
    ODR_MASK_SET(a->u.initRequest->options, Z_Options_namedResultSets);

    ODR_MASK_SET(a->u.initRequest->protocolVersion, Z_ProtocolVersion_1);
    ODR_MASK_SET(a->u.initRequest->protocolVersion, Z_ProtocolVersion_2);
    ODR_MASK_SET(a->u.initRequest->protocolVersion, Z_ProtocolVersion_3);

    init_authentication(cl, a->u.initRequest);

    /* add virtual host if tunneling through Z39.50 proxy */
    
    if (0 < strlen(global_parameters.zproxy_override) 
        && 0 < strlen(cl->database->database->url))
    {
#if YAZ_VERSIONL >= 0x020163
        yaz_oi_set_string_oid(&a->u.initRequest->otherInfo,
                              global_parameters.odr_out,
                              yaz_oid_userinfo_proxy,
                              1, cl->database->database->url);
#else
        yaz_oi_set_string_oidval(&a->u.initRequest->otherInfo,
                                 global_parameters.odr_out, VAL_PROXY,
                                 1, cl->database->database->url);
#endif
    }

    if (send_apdu(cl, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	cl->state = Client_Initializing;
    }
    else
        cl->state = Client_Error;
    odr_reset(global_parameters.odr_out);
}

// Recursively traverse query structure to extract terms.
static void pull_terms(NMEM nmem, struct ccl_rpn_node *n, char **termlist, int *num)
{
    char **words;
    int numwords;
    int i;

    switch (n->kind)
    {
        case CCL_RPN_AND:
        case CCL_RPN_OR:
        case CCL_RPN_NOT:
        case CCL_RPN_PROX:
            pull_terms(nmem, n->u.p[0], termlist, num);
            pull_terms(nmem, n->u.p[1], termlist, num);
            break;
        case CCL_RPN_TERM:
            nmem_strsplit(nmem, " ", n->u.t.term, &words, &numwords);
            for (i = 0; i < numwords; i++)
                termlist[(*num)++] = words[i];
            break;
        default: // NOOP
            break;
    }
}

// Extract terms from query into null-terminated termlist
static void extract_terms(NMEM nmem, struct ccl_rpn_node *query, char **termlist)
{
    int num = 0;

    pull_terms(nmem, query, termlist, &num);
    termlist[num] = 0;
}

static void send_search(IOCHAN i)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client; 
    struct session *se = cl->session;
    struct session_database *sdb = cl->database;
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_searchRequest);
    int ndb;
    char **databaselist;
    Z_Query *zquery;
    int ssub = 0, lslb = 100000, mspn = 10;
    char *recsyn = 0;
    char *piggyback = 0;
    char *queryenc = 0;
    yaz_iconv_t iconv = 0;

    yaz_log(YLOG_DEBUG, "Sending search to %s", cl->database->database->url);

    // constructing RPN query
    a->u.searchRequest->query = zquery = odr_malloc(global_parameters.odr_out,
            sizeof(Z_Query));
    zquery->which = Z_Query_type_1;
    zquery->u.type_1 = p_query_rpn(global_parameters.odr_out, cl->pquery);

    // converting to target encoding
    if ((queryenc = session_setting_oneval(sdb, PZ_QUERYENCODING))){
        iconv = yaz_iconv_open(queryenc, "UTF-8");
        if (iconv){
            yaz_query_charset_convert_rpnquery(zquery->u.type_1, 
                                               global_parameters.odr_out, 
                                               iconv);
            yaz_iconv_close(iconv);
        } else
            yaz_log(YLOG_WARN, "Query encoding failed %s %s", 
                    cl->database->database->url, queryenc);
    }

    for (ndb = 0; sdb->database->databases[ndb]; ndb++)
	;
    databaselist = odr_malloc(global_parameters.odr_out, sizeof(char*) * ndb);
    for (ndb = 0; sdb->database->databases[ndb]; ndb++)
	databaselist[ndb] = sdb->database->databases[ndb];

    if (!(piggyback = session_setting_oneval(sdb, PZ_PIGGYBACK)) || *piggyback == '1')
    {
        if ((recsyn = session_setting_oneval(sdb, PZ_REQUESTSYNTAX)))
        {
#if YAZ_VERSIONL >= 0x020163
            a->u.searchRequest->preferredRecordSyntax =
                yaz_string_to_oid_odr(yaz_oid_std(),
                                      CLASS_RECSYN, recsyn,
                                      global_parameters.odr_out);
#else
            a->u.searchRequest->preferredRecordSyntax =
                yaz_str_to_z3950oid(global_parameters.odr_out,
                                    CLASS_RECSYN, recsyn);
#endif
        }
        a->u.searchRequest->smallSetUpperBound = &ssub;
        a->u.searchRequest->largeSetLowerBound = &lslb;
        a->u.searchRequest->mediumSetPresentNumber = &mspn;
    }
    a->u.searchRequest->resultSetName = "Default";
    a->u.searchRequest->databaseNames = databaselist;
    a->u.searchRequest->num_databaseNames = ndb;

    
    {  //scope for sending and logging queries 
        WRBUF wbquery = wrbuf_alloc();
        yaz_query_to_wrbuf(wbquery, zquery);


        if (send_apdu(cl, a) >= 0)
            {
                iochan_setflags(i, EVENT_INPUT);
                cl->state = Client_Searching;
                cl->requestid = se->requestid;
                yaz_log(YLOG_LOG, "SearchRequest %s %s %s", 
                         cl->database->database->url,
                        queryenc ? queryenc : "UTF-8",
                        wrbuf_cstr(wbquery));
            }
        else {
            cl->state = Client_Error;
                yaz_log(YLOG_WARN, "Failed SearchRequest %s  %s %s", 
                         cl->database->database->url, 
                        queryenc ? queryenc : "UTF-8",
                        wrbuf_cstr(wbquery));
        }
        
        wrbuf_destroy(wbquery);
    }    

    odr_reset(global_parameters.odr_out);
}

static void send_present(IOCHAN i)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client; 
    struct session_database *sdb = cl->database;
    Z_APDU *a = zget_APDU(global_parameters.odr_out, Z_APDU_presentRequest);
    int toget;
    int start = cl->records + 1;
    char *recsyn;

    toget = global_parameters.chunk;
    if (toget > global_parameters.toget - cl->records)
        toget = global_parameters.toget - cl->records;
    if (toget > cl->hits - cl->records)
	toget = cl->hits - cl->records;

    yaz_log(YLOG_DEBUG, "Trying to present %d records\n", toget);

    a->u.presentRequest->resultSetStartPoint = &start;
    a->u.presentRequest->numberOfRecordsRequested = &toget;

    a->u.presentRequest->resultSetId = "Default";

    if ((recsyn = session_setting_oneval(sdb, PZ_REQUESTSYNTAX)))
    {
#if YAZ_VERSIONL >= 0x020163
        a->u.presentRequest->preferredRecordSyntax =
            yaz_string_to_oid_odr(yaz_oid_std(),
                                  CLASS_RECSYN, recsyn,
                                  global_parameters.odr_out);
#else
        a->u.presentRequest->preferredRecordSyntax =
            yaz_str_to_z3950oid(global_parameters.odr_out,
                                CLASS_RECSYN, recsyn);
#endif
    }

    if (send_apdu(cl, a) >= 0)
    {
	iochan_setflags(i, EVENT_INPUT);
	cl->state = Client_Presenting;
    }
    else
        cl->state = Client_Error;
    odr_reset(global_parameters.odr_out);
}

static void do_initResponse(IOCHAN i, Z_APDU *a)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    Z_InitResponse *r = a->u.initResponse;

    yaz_log(YLOG_DEBUG, "Init response %s", cl->database->database->url);

    if (*r->result)
    {
	cl->state = Client_Idle;
    }
    else
        cl->state = Client_Failed; // FIXME need to do something to the connection
}

static void do_searchResponse(IOCHAN i, Z_APDU *a)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    struct session *se = cl->session;
    Z_SearchResponse *r = a->u.searchResponse;

    yaz_log(YLOG_DEBUG, "Search response %s (status=%d)", 
            cl->database->database->url, *r->searchStatus);

    if (*r->searchStatus)
    {
	cl->hits = *r->resultCount;
        se->total_hits += cl->hits;
        if (r->presentStatus && !*r->presentStatus && r->records)
        {
            yaz_log(YLOG_DEBUG, "Records in search response %s", 
                    cl->database->database->url);
            ingest_records(cl, r->records);
        }
        cl->state = Client_Idle;
    }
    else
    {          /*"FAILED"*/
	cl->hits = 0;
        cl->state = Client_Error;
        if (r->records) {
            Z_Records *recs = r->records;
            if (recs->which == Z_Records_NSD)
            {
                yaz_log(YLOG_WARN, 
                        "Search response: Non-surrogate diagnostic %s",
                        cl->database->database->url);
                cl->diagnostic = *recs->u.nonSurrogateDiagnostic->condition;
                cl->state = Client_Error;
            }
        }
    }
}

static void do_closeResponse(IOCHAN i, Z_APDU *a)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    /* Z_Close *r = a->u.close; */

    yaz_log(YLOG_WARN, "Close response %s", cl->database->database->url);

    cl->state = Client_Failed;
    connection_destroy(co);
}


char *normalize_mergekey(char *buf, int skiparticle)
{
    char *p = buf, *pout = buf;

    if (skiparticle)
    {
        char firstword[64];
        char articles[] = "the den der die des an a "; // must end in space

        while (*p && !isalnum(*p))
            p++;
        pout = firstword;
        while (*p && *p != ' ' && pout - firstword < 62)
            *(pout++) = tolower(*(p++));
        *(pout++) = ' ';
        *(pout++) = '\0';
        if (!strstr(articles, firstword))
            p = buf;
        pout = buf;
    }

    while (*p)
    {
        while (*p && !isalnum(*p))
            p++;
        while (isalnum(*p))
            *(pout++) = tolower(*(p++));
        if (*p)
            *(pout++) = ' ';
        while (*p && !isalnum(*p))
            p++;
    }
    if (buf != pout)
        do {
            *(pout--) = '\0';
        }
        while (pout > buf && *pout == ' ');

    return buf;
}

static void add_facet(struct session *s, const char *type, const char *value)
{
    int i;

    if (!*value)
        return;
    for (i = 0; i < s->num_termlists; i++)
        if (!strcmp(s->termlists[i].name, type))
            break;
    if (i == s->num_termlists)
    {
        if (i == SESSION_MAX_TERMLISTS)
        {
            yaz_log(YLOG_FATAL, "Too many termlists");
            exit(1);
        }
        s->termlists[i].name = nmem_strdup(s->nmem, type);
        s->termlists[i].termlist = termlist_create(s->nmem, s->expected_maxrecs, 15);
        s->num_termlists = i + 1;
    }
    termlist_insert(s->termlists[i].termlist, value);
}

static xmlDoc *normalize_record(struct client *cl, Z_External *rec)
{
    struct database_retrievalmap *m;
    struct session_database *sdb = cl->database;
    struct database *db = sdb->database;
    xmlNode *res;
    xmlDoc *rdoc;

    // First normalize to XML
    if (sdb->yaz_marc)
    {
        char *buf;
        int len;
        if (rec->which != Z_External_octet)
        {
            yaz_log(YLOG_WARN, "Unexpected external branch, probably BER %s",
                    db->url);
            return 0;
        }
        buf = (char*) rec->u.octet_aligned->buf;
        len = rec->u.octet_aligned->len;
        if (yaz_marc_read_iso2709(sdb->yaz_marc, buf, len) < 0)
        {
            yaz_log(YLOG_WARN, "Failed to decode MARC %s", db->url);
            return 0;
        }

        yaz_marc_write_using_libxml2(sdb->yaz_marc, 1);
        if (yaz_marc_write_xml(sdb->yaz_marc, &res,
                    "http://www.loc.gov/MARC21/slim", 0, 0) < 0)
        {
            yaz_log(YLOG_WARN, "Failed to encode as XML %s",
                    db->url);
            return 0;
        }
        rdoc = xmlNewDoc((xmlChar *) "1.0");
        xmlDocSetRootElement(rdoc, res);

    }
    else
    {
        yaz_log(YLOG_FATAL, 
                "Unknown native_syntax in normalize_record from %s",
                db->url);
        exit(1);
    }

    if (global_parameters.dump_records){
        fprintf(stderr, 
                "Input Record (normalized) from %s\n----------------\n",
                db->url);
#if LIBXML_VERSION >= 20600
        xmlDocFormatDump(stderr, rdoc, 1);
#else
        xmlDocDump(stderr, rdoc);
#endif
    }

    for (m = sdb->map; m; m = m->next){
        xmlDoc *new = 0;

#if 1
        {
            xmlNodePtr root = 0;
            new = xsltApplyStylesheet(m->stylesheet, rdoc, 0);
            root= xmlDocGetRootElement(new);
        if (!new || !root || !(root->children))
        {
            yaz_log(YLOG_WARN, "XSLT transformation failed from %s",
                    cl->database->database->url);
            xmlFreeDoc(new);
            xmlFreeDoc(rdoc);
            return 0;
        }
        }
#endif

#if 0
        // do it another way to detect transformation errors right now
        // but does not seem to work either!
        {
            xsltTransformContextPtr ctxt;
            ctxt = xsltNewTransformContext(m->stylesheet, rdoc);
            new = xsltApplyStylesheetUser(m->stylesheet, rdoc, 0, 0, 0, ctxt);
            if ((ctxt->state == XSLT_STATE_ERROR) ||
                (ctxt->state == XSLT_STATE_STOPPED)){
                yaz_log(YLOG_WARN, "XSLT transformation failed from %s",
                        cl->database->database->url);
                xmlFreeDoc(new);
                xmlFreeDoc(rdoc);
                return 0;
            }
        }
#endif      
   
        xmlFreeDoc(rdoc);
        rdoc = new;
    }
    if (global_parameters.dump_records)
    {
        fprintf(stderr, "Record from %s\n----------------\n", 
                cl->database->database->url);
#if LIBXML_VERSION >= 20600
        xmlDocFormatDump(stderr, rdoc, 1);
#else
        xmlDocDump(stderr, rdoc);
#endif
    }
    return rdoc;
}

// Extract what appears to be years from buf, storing highest and
// lowest values.
static int extract_years(const char *buf, int *first, int *last)
{
    *first = -1;
    *last = -1;
    while (*buf)
    {
        const char *e;
        int len;

        while (*buf && !isdigit(*buf))
            buf++;
        len = 0;
        for (e = buf; *e && isdigit(*e); e++)
            len++;
        if (len == 4)
        {
            int value = atoi(buf);
            if (*first < 0 || value < *first)
                *first = value;
            if (*last < 0 || value > *last)
                *last = value;
        }
        buf = e;
    }
    return *first;
}

static struct record *ingest_record(struct client *cl, Z_External *rec)
{
    xmlDoc *xdoc = normalize_record(cl, rec);
    xmlNode *root, *n;
    struct record *res;
    struct record_cluster *cluster;
    struct session *se = cl->session;
    xmlChar *mergekey, *mergekey_norm;
    xmlChar *type = 0;
    xmlChar *value = 0;
    struct conf_service *service = global_parameters.server->service;

    if (!xdoc)
        return 0;

    root = xmlDocGetRootElement(xdoc);
    if (!(mergekey = xmlGetProp(root, (xmlChar *) "mergekey")))
    {
        yaz_log(YLOG_WARN, "No mergekey found in record");
        xmlFreeDoc(xdoc);
        return 0;
    }

    res = nmem_malloc(se->nmem, sizeof(struct record));
    res->next = 0;
    res->client = cl;
    res->metadata = nmem_malloc(se->nmem,
            sizeof(struct record_metadata*) * service->num_metadata);
    memset(res->metadata, 0, sizeof(struct record_metadata*) * service->num_metadata);

    mergekey_norm = (xmlChar *) nmem_strdup(se->nmem, (char*) mergekey);
    xmlFree(mergekey);
    normalize_mergekey((char *) mergekey_norm, 0);

    cluster = reclist_insert(se->reclist, 
                             global_parameters.server->service, 
                             res, (char *) mergekey_norm, 
                             &se->total_merged);
    if (global_parameters.dump_records)
        yaz_log(YLOG_LOG, "Cluster id %d from %s (#%d)", cluster->recid,
                cl->database->database->url, cl->records);
    if (!cluster)
    {
        /* no room for record */
        xmlFreeDoc(xdoc);
        return 0;
    }
    relevance_newrec(se->relevance, cluster);

    for (n = root->children; n; n = n->next)
    {
        if (type)
            xmlFree(type);
        if (value)
            xmlFree(value);
        type = value = 0;

        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp((const char *) n->name, "metadata"))
        {
            struct conf_metadata *md = 0;
            struct conf_sortkey *sk = 0;
            struct record_metadata **wheretoput, *newm;
            int imeta;
            int first, last;

            type = xmlGetProp(n, (xmlChar *) "type");
            value = xmlNodeListGetString(xdoc, n->children, 0);

            if (!type || !value)
                continue;

            // First, find out what field we're looking at
            for (imeta = 0; imeta < service->num_metadata; imeta++)
                if (!strcmp((const char *) type, service->metadata[imeta].name))
                {
                    md = &service->metadata[imeta];
                    if (md->sortkey_offset >= 0)
                        sk = &service->sortkeys[md->sortkey_offset];
                    break;
                }
            if (!md)
            {
                yaz_log(YLOG_WARN, "Ignoring unknown metadata element: %s", type);
                continue;
            }

            // Find out where we are putting it
            if (md->merge == Metadata_merge_no)
                wheretoput = &res->metadata[imeta];
            else
                wheretoput = &cluster->metadata[imeta];
            
            // Put it there
            newm = nmem_malloc(se->nmem, sizeof(struct record_metadata));
            newm->next = 0;
            if (md->type == Metadata_type_generic)
            {
                char *p, *pe;
                for (p = (char *) value; *p && isspace(*p); p++)
                    ;
                for (pe = p + strlen(p) - 1;
                        pe > p && strchr(" ,/.:([", *pe); pe--)
                    *pe = '\0';
                newm->data.text = nmem_strdup(se->nmem, p);

            }
            else if (md->type == Metadata_type_year)
            {
                if (extract_years((char *) value, &first, &last) < 0)
                    continue;
            }
            else
            {
                yaz_log(YLOG_WARN, "Unknown type in metadata element %s", type);
                continue;
            }
            if (md->type == Metadata_type_year && md->merge != Metadata_merge_range)
            {
                yaz_log(YLOG_WARN, "Only range merging supported for years");
                continue;
            }
            if (md->merge == Metadata_merge_unique)
            {
                struct record_metadata *mnode;
                for (mnode = *wheretoput; mnode; mnode = mnode->next)
                    if (!strcmp((const char *) mnode->data.text, newm->data.text))
                        break;
                if (!mnode)
                {
                    newm->next = *wheretoput;
                    *wheretoput = newm;
                }
            }
            else if (md->merge == Metadata_merge_longest)
            {
                if (!*wheretoput ||
                        strlen(newm->data.text) > strlen((*wheretoput)->data.text))
                {
                    *wheretoput = newm;
                    if (sk)
                    {
                        char *s = nmem_strdup(se->nmem, newm->data.text);
                        if (!cluster->sortkeys[md->sortkey_offset])
                            cluster->sortkeys[md->sortkey_offset] = 
                                nmem_malloc(se->nmem, sizeof(union data_types));
                        normalize_mergekey(s,
                                (sk->type == Metadata_sortkey_skiparticle));
                        cluster->sortkeys[md->sortkey_offset]->text = s;
                    }
                }
            }
            else if (md->merge == Metadata_merge_all || md->merge == Metadata_merge_no)
            {
                newm->next = *wheretoput;
                *wheretoput = newm;
            }
            else if (md->merge == Metadata_merge_range)
            {
                assert(md->type == Metadata_type_year);
                if (!*wheretoput)
                {
                    *wheretoput = newm;
                    (*wheretoput)->data.number.min = first;
                    (*wheretoput)->data.number.max = last;
                    if (sk)
                        cluster->sortkeys[md->sortkey_offset] = &newm->data;
                }
                else
                {
                    if (first < (*wheretoput)->data.number.min)
                        (*wheretoput)->data.number.min = first;
                    if (last > (*wheretoput)->data.number.max)
                        (*wheretoput)->data.number.max = last;
                }
#ifdef GAGA
                if (sk)
                {
                    union data_types *sdata = cluster->sortkeys[md->sortkey_offset];
                    yaz_log(YLOG_LOG, "SK range: %d-%d", sdata->number.min, sdata->number.max);
                }
#endif
            }
            else
                yaz_log(YLOG_WARN, "Don't know how to merge on element name %s", md->name);

            if (md->rank)
                relevance_countwords(se->relevance, cluster, 
                                     (char *) value, md->rank);
            if (md->termlist)
            {
                if (md->type == Metadata_type_year)
                {
                    char year[64];
                    sprintf(year, "%d", last);
                    add_facet(se, (char *) type, year);
                    if (first != last)
                    {
                        sprintf(year, "%d", first);
                        add_facet(se, (char *) type, year);
                    }
                }
                else
                    add_facet(se, (char *) type, (char *) value);
            }
            xmlFree(type);
            xmlFree(value);
            type = value = 0;
        }
        else
            yaz_log(YLOG_WARN, "Unexpected element %s in internal record", n->name);
    }
    if (type)
        xmlFree(type);
    if (value)
        xmlFree(value);

    xmlFreeDoc(xdoc);

    relevance_donerecord(se->relevance, cluster);
    se->total_records++;

    return res;
}

// Retrieve first defined value for 'name' for given database.
// Will be extended to take into account user associated with session
char *session_setting_oneval(struct session_database *db, int offset)
{
    if (!db->settings[offset])
        return "";
    return db->settings[offset]->value;
}

static void ingest_records(struct client *cl, Z_Records *r)
{
#if USE_TIMING
    yaz_timing_t t = yaz_timing_create();
#endif
    struct record *rec;
    struct session *s = cl->session;
    Z_NamePlusRecordList *rlist;
    int i;

    if (r->which != Z_Records_DBOSD)
        return;
    rlist = r->u.databaseOrSurDiagnostics;
    for (i = 0; i < rlist->num_records; i++)
    {
        Z_NamePlusRecord *npr = rlist->records[i];

        cl->records++;
        if (npr->which != Z_NamePlusRecord_databaseRecord)
        {
            yaz_log(YLOG_WARN, 
                    "Unexpected record type, probably diagnostic %s",
                    cl->database->database->url);
            continue;
        }

        rec = ingest_record(cl, npr->u.databaseRecord);
        if (!rec)
            continue;
    }
    if (s->watchlist[SESSION_WATCH_RECORDS].fun && rlist->num_records)
        session_alert_watch(s, SESSION_WATCH_RECORDS);

#if USE_TIMING
    yaz_timing_stop(t);
    yaz_log(YLOG_LOG, "ingest_records %6.5f %3.2f %3.2f", 
            yaz_timing_get_real(t), yaz_timing_get_user(t),
            yaz_timing_get_sys(t));
    yaz_timing_destroy(&t);
#endif
}

static void do_presentResponse(IOCHAN i, Z_APDU *a)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    Z_PresentResponse *r = a->u.presentResponse;

    if (r->records) {
        Z_Records *recs = r->records;
        if (recs->which == Z_Records_NSD)
        {
            yaz_log(YLOG_WARN, "Non-surrogate diagnostic %s",
                    cl->database->database->url);
            cl->diagnostic = *recs->u.nonSurrogateDiagnostic->condition;
            cl->state = Client_Error;
        }
    }

    if (!*r->presentStatus && cl->state != Client_Error)
    {
        yaz_log(YLOG_DEBUG, "Good Present response %s",
                cl->database->database->url);
        ingest_records(cl, r->records);
        cl->state = Client_Idle;
    }
    else if (*r->presentStatus) 
    {
        yaz_log(YLOG_WARN, "Bad Present response %s",
                cl->database->database->url);
        cl->state = Client_Error;
    }
}

void connection_handler(IOCHAN i, int event)
{
    struct connection *co = iochan_getdata(i);
    struct client *cl = co->client;
    struct session *se = 0;

    if (cl)
        se = cl->session;
    else
    {
        yaz_log(YLOG_WARN, "Destroying orphan connection");
        connection_destroy(co);
        return;
    }

    if (co->state == Conn_Connecting && event & EVENT_OUTPUT)
    {
	int errcode;
        socklen_t errlen = sizeof(errcode);

	if (getsockopt(cs_fileno(co->link), SOL_SOCKET, SO_ERROR, &errcode,
	    &errlen) < 0 || errcode != 0)
	{
            client_fatal(cl);
	    return;
	}
	else
	{
            yaz_log(YLOG_DEBUG, "Connect OK");
	    co->state = Conn_Open;
            if (cl)
                cl->state = Client_Connected;
	}
    }

    else if (event & EVENT_INPUT)
    {
	int len = cs_get(co->link, &co->ibuf, &co->ibufsize);

	if (len < 0)
	{
            yaz_log(YLOG_WARN|YLOG_ERRNO, "Error reading from %s", 
                    cl->database->database->url);
            connection_destroy(co);
	    return;
	}
        else if (len == 0)
	{
            yaz_log(YLOG_WARN, "EOF reading from %s", cl->database->database->url);
            connection_destroy(co);
	    return;
	}
	else if (len > 1) // We discard input if we have no connection
	{
            co->state = Conn_Open;

            if (cl && (cl->requestid == se->requestid || cl->state == Client_Initializing))
            {
                Z_APDU *a;

                odr_reset(global_parameters.odr_in);
                odr_setbuf(global_parameters.odr_in, co->ibuf, len, 0);
                if (!z_APDU(global_parameters.odr_in, &a, 0, 0))
                {
                    client_fatal(cl);
                    return;
                }
                switch (a->which)
                {
                    case Z_APDU_initResponse:
                        do_initResponse(i, a);
                        break;
                    case Z_APDU_searchResponse:
                        do_searchResponse(i, a);
                        break;
                    case Z_APDU_presentResponse:
                        do_presentResponse(i, a);
                        break;
                    case Z_APDU_close:
                        do_closeResponse(i, a);
                        break;
                    default:
                        yaz_log(YLOG_WARN, 
                                "Unexpected Z39.50 response from %s",  
                                cl->database->database->url);
                        client_fatal(cl);
                        return;
                }
                // We aren't expecting staggered output from target
                // if (cs_more(t->link))
                //    iochan_setevent(i, EVENT_INPUT);
            }
            else  // we throw away response and go to idle mode
            {
                yaz_log(YLOG_DEBUG, "Ignoring result of expired operation");
                cl->state = Client_Idle;
            }
	}
	/* if len==1 we do nothing but wait for more input */
    }

    if (cl->state == Client_Connected) {
        send_init(i);
    }

    if (cl->state == Client_Idle)
    {
        if (cl->requestid != se->requestid && cl->pquery) {
            send_search(i);
        }
        else if (cl->hits > 0 && cl->records < global_parameters.toget &&
            cl->records < cl->hits) {
            send_present(i);
        }
    }
}

// Disassociate connection from client
static void connection_release(struct connection *co)
{
    struct client *cl = co->client;

    yaz_log(YLOG_DEBUG, "Connection release %s", co->host->hostport);
    if (!cl)
        return;
    cl->connection = 0;
    co->client = 0;
}

// Close connection and recycle structure
static void connection_destroy(struct connection *co)
{
    struct host *h = co->host;
    
    if (co->link)
    {
        cs_close(co->link);
        iochan_destroy(co->iochan);
    }

    yaz_log(YLOG_DEBUG, "Connection destroy %s", co->host->hostport);
    if (h->connections == co)
        h->connections = co->next;
    else
    {
        struct connection *pco;
        for (pco = h->connections; pco && pco->next != co; pco = pco->next)
            ;
        if (pco)
            pco->next = co->next;
        else
            abort();
    }
    if (co->client)
    {
        if (co->client->state != Client_Idle)
            co->client->state = Client_Disconnected;
        co->client->connection = 0;
    }
    co->next = connection_freelist;
    connection_freelist = co;
}

static int connection_connect(struct connection *con)
{
    COMSTACK link = 0;
    struct client *cl = con->client;
    struct host *host = con->host;
    void *addr;
    int res;

    assert(host->ipport);
    assert(cl);

    if (!(link = cs_create(tcpip_type, 0, PROTO_Z3950)))
    {
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "Failed to create comstack");
        exit(1);
    }
    
    if (0 == strlen(global_parameters.zproxy_override)){
        /* no Z39.50 proxy needed - direct connect */
        yaz_log(YLOG_DEBUG, "Connection create %s", cl->database->database->url);
        
        if (!(addr = cs_straddr(link, host->ipport)))
        {
            yaz_log(YLOG_WARN|YLOG_ERRNO, 
                    "Lookup of IP address %s failed", host->ipport);
            return -1;
        }
        
    } else {
        /* Z39.50 proxy connect */
        yaz_log(YLOG_DEBUG, "Connection create %s proxy %s", 
                cl->database->database->url, global_parameters.zproxy_override);
        
        if (!(addr = cs_straddr(link, global_parameters.zproxy_override)))
        {
            yaz_log(YLOG_WARN|YLOG_ERRNO, 
                    "Lookup of IP address %s failed", 
                    global_parameters.zproxy_override);
            return -1;
        }
    }
    
    res = cs_connect(link, addr);
    if (res < 0)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "cs_connect %s", cl->database->database->url);
        return -1;
    }
    con->link = link;
    con->state = Conn_Connecting;
    con->iochan = iochan_create(cs_fileno(link), connection_handler, 0);
    iochan_setdata(con->iochan, con);
    pazpar2_add_channel(con->iochan);

    /* this fragment is bad DRY: from client_prep_connection */
    cl->state = Client_Connecting;
    iochan_setflag(con->iochan, EVENT_OUTPUT);
    return 0;
}

void connect_resolver_host(struct host *host)
{
    struct connection *con = host->connections;

    while (con)
    {
        if (con->state == Conn_Resolving)
        {
            if (!host->ipport) /* unresolved */
            {
                connection_destroy(con);
                /* start all over .. at some point it will be NULL */
                con = host->connections;
            }
            else if (!con->client)
            {
                yaz_log(YLOG_WARN, "connect_unresolved_host : ophan client");
                connection_destroy(con);
                /* start all over .. at some point it will be NULL */
                con = host->connections;
            }
            else
            {
                connection_connect(con);
                con = con->next;
            }
        }
    }
}


// Creates a new connection for client, associated with the host of 
// client's database
static struct connection *connection_create(struct client *cl)
{
    struct connection *new;
    struct host *host = cl->database->database->host;

    if ((new = connection_freelist))
        connection_freelist = new->next;
    else
    {
        new = xmalloc(sizeof (struct connection));
        new->ibuf = 0;
        new->ibufsize = 0;
    }
    new->host = host;
    new->next = new->host->connections;
    new->host->connections = new;
    new->client = cl;
    cl->connection = new;
    new->link = 0;
    new->state = Conn_Resolving;
    if (host->ipport)
        connection_connect(new);
    return new;
}

// Close connection and set state to error
static void client_fatal(struct client *cl)
{
    yaz_log(YLOG_WARN, "Fatal error from %s", cl->database->database->url);
    connection_destroy(cl->connection);
    cl->state = Client_Error;
}

// Ensure that client has a connection associated
static int client_prep_connection(struct client *cl)
{
    struct connection *co;
    struct session *se = cl->session;
    struct host *host = cl->database->database->host;

    co = cl->connection;

    yaz_log(YLOG_DEBUG, "Client prep %s", cl->database->database->url);

    if (!co)
    {
        // See if someone else has an idle connection
        // We should look at timestamps here to select the longest-idle connection
        for (co = host->connections; co; co = co->next)
            if (co->state == Conn_Open && (!co->client || co->client->session != se))
                break;
        if (co)
        {
            connection_release(co);
            cl->connection = co;
            co->client = cl;
        }
        else
            co = connection_create(cl);
    }
    if (co)
    {
        if (co->state == Conn_Connecting)
        {
            cl->state = Client_Connecting;
            iochan_setflag(co->iochan, EVENT_OUTPUT);
        }
        else if (co->state == Conn_Open)
        {
            if (cl->state == Client_Error || cl->state == Client_Disconnected)
                cl->state = Client_Idle;
            iochan_setflag(co->iochan, EVENT_OUTPUT);
        }
        return 1;
    }
    else
        return 0;
}

// Initialize YAZ Map structures for MARC-based targets
static int prepare_yazmarc(struct session_database *sdb)
{
    char *s;

    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, "No settings for %s", sdb->database->url);
        return -1;
    }
    if ((s = session_setting_oneval(sdb, PZ_NATIVESYNTAX)) && !strncmp(s, "iso2709", 7))
    {
        char *encoding = "marc-8s", *e;
        yaz_iconv_t cm;

        // See if a native encoding is specified
        if ((e = strchr(s, ';')))
            encoding = e + 1;

        sdb->yaz_marc = yaz_marc_create();
        yaz_marc_subfield_str(sdb->yaz_marc, "\t");
        
        cm = yaz_iconv_open("utf-8", encoding);
        if (!cm)
        {
            yaz_log(YLOG_FATAL, 
                    "Unable to map from %s to UTF-8 for target %s", 
                    encoding, sdb->database->url);
            return -1;
        }
        yaz_marc_iconv(sdb->yaz_marc, cm);
    }
    return 0;
}

// Prepare XSLT stylesheets for record normalization
// Structures are allocated on the session_wide nmem to avoid having
// to recompute this for every search. This would lead
// to leaking if a single session was to repeatedly change the PZ_XSLT
// setting. However, this is not a realistic use scenario.
static int prepare_map(struct session *se, struct session_database *sdb)
{
   char *s;

    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, "No settings on %s", sdb->database->url);
        return -1;
    }
    if ((s = session_setting_oneval(sdb, PZ_XSLT)))
    {
        char **stylesheets;
        struct database_retrievalmap **m = &sdb->map;
        int num, i;

        nmem_strsplit(se->session_nmem, ",", s, &stylesheets, &num);
        for (i = 0; i < num; i++)
        {
            (*m) = nmem_malloc(se->session_nmem, sizeof(**m));
            (*m)->next = 0;
            if (!((*m)->stylesheet = conf_load_stylesheet(stylesheets[i])))
            {
                yaz_log(YLOG_FATAL, "Unable to load stylesheet: %s",
                        stylesheets[i]);
                return -1;
            }
            m = &(*m)->next;
        }
    }
    if (!sdb->map)
        yaz_log(YLOG_WARN, "No Normalization stylesheet for target %s",
                sdb->database->url);
    return 0;
}

// This analyzes settings and recomputes any supporting data structures
// if necessary.
static int prepare_session_database(struct session *se, struct session_database *sdb)
{
    if (!sdb->settings)
    {
        yaz_log(YLOG_WARN, "No settings associated with %s", sdb->database->url);
        return -1;
    }
    if (sdb->settings[PZ_NATIVESYNTAX] && !sdb->yaz_marc)
    {
        if (prepare_yazmarc(sdb) < 0)
            return -1;
    }
    if (sdb->settings[PZ_XSLT] && !sdb->map)
    {
        if (prepare_map(se, sdb) < 0)
            return -1;
    }
    return 0;
}

// Initialize CCL map for a target
static CCL_bibset prepare_cclmap(struct client *cl)
{
    struct session_database *sdb = cl->database;
    struct setting *s;
    CCL_bibset res;

    if (!sdb->settings)
        return 0;
    res = ccl_qual_mk();
    for (s = sdb->settings[PZ_CCLMAP]; s; s = s->next)
    {
        char *p = strchr(s->name + 3, ':');
        if (!p)
        {
            yaz_log(YLOG_WARN, "Malformed cclmap name: %s", s->name);
            ccl_qual_rm(&res);
            return 0;
        }
        p++;
        ccl_qual_fitem(res, s->value, p);
    }
    return res;
}

// Parse the query given the settings specific to this client
static int client_parse_query(struct client *cl, const char *query)
{
    struct session *se = cl->session;
    struct ccl_rpn_node *cn;
    int cerror, cpos;
    CCL_bibset ccl_map = prepare_cclmap(cl);

    if (!ccl_map)
        return -1;
    cn = ccl_find_str(ccl_map, query, &cerror, &cpos);
    ccl_qual_rm(&ccl_map);
    if (!cn)
    {
        cl->state = Client_Error;
        yaz_log(YLOG_WARN, "Failed to parse query for %s",
                         cl->database->database->url);
        return -1;
    }
    wrbuf_rewind(se->wrbuf);
    ccl_pquery(se->wrbuf, cn);
    wrbuf_putc(se->wrbuf, '\0');
    if (cl->pquery)
        xfree(cl->pquery);
    cl->pquery = xstrdup(wrbuf_buf(se->wrbuf));

    if (!se->relevance)
    {
        // Initialize relevance structure with query terms
        char *p[512];
        extract_terms(se->nmem, cn, p);
        se->relevance = relevance_create(se->nmem, (const char **) p,
                se->expected_maxrecs);
    }

    ccl_rpn_delete(cn);
    return 0;
}

static struct client *client_create(void)
{
    struct client *r;
    if (client_freelist)
    {
        r = client_freelist;
        client_freelist = client_freelist->next;
    }
    else
        r = xmalloc(sizeof(struct client));
    r->pquery = 0;
    r->database = 0;
    r->connection = 0;
    r->session = 0;
    r->hits = 0;
    r->records = 0;
    r->setno = 0;
    r->requestid = -1;
    r->diagnostic = 0;
    r->state = Client_Disconnected;
    r->next = 0;
    return r;
}

void client_destroy(struct client *c)
{
    struct session *se = c->session;
    if (c == se->clients)
        se->clients = c->next;
    else
    {
        struct client *cc;
        for (cc = se->clients; cc && cc->next != c; cc = cc->next)
            ;
        if (cc)
            cc->next = c->next;
    }
    if (c->connection)
        connection_release(c->connection);
    c->next = client_freelist;
    client_freelist = c;
}

void session_set_watch(struct session *s, int what, session_watchfun fun, void *data)
{
    s->watchlist[what].fun = fun;
    s->watchlist[what].data = data;
}

void session_alert_watch(struct session *s, int what)
{
    if (!s->watchlist[what].fun)
        return;
    (*s->watchlist[what].fun)(s->watchlist[what].data);
    s->watchlist[what].fun = 0;
    s->watchlist[what].data = 0;
}

//callback for grep_databases
static void select_targets_callback(void *context, struct session_database *db)
{
    struct session *se = (struct session*) context;
    struct client *cl = client_create();
    cl->database = db;
    cl->session = se;
    cl->next = se->clients;
    se->clients = cl;
}

// Associates a set of clients with a session;
// Note: Session-databases represent databases with per-session setting overrides
int select_targets(struct session *se, struct database_criterion *crit)
{
    while (se->clients)
        client_destroy(se->clients);

    return session_grep_databases(se, crit, select_targets_callback);
}

int session_active_clients(struct session *s)
{
    struct client *c;
    int res = 0;

    for (c = s->clients; c; c = c->next)
        if (c->connection && (c->state == Client_Connecting ||
                    c->state == Client_Initializing ||
                    c->state == Client_Searching ||
                    c->state == Client_Presenting))
            res++;

    return res;
}

// parses crit1=val1,crit2=val2|val3,...
static struct database_criterion *parse_filter(NMEM m, const char *buf)
{
    struct database_criterion *res = 0;
    char **values;
    int num;
    int i;

    if (!buf || !*buf)
        return 0;
    nmem_strsplit(m, ",", buf,  &values, &num);
    for (i = 0; i < num; i++)
    {
        char **subvalues;
        int subnum;
        int subi;
        struct database_criterion *new = nmem_malloc(m, sizeof(*new));
        char *eq = strchr(values[i], '=');
        if (!eq)
        {
            yaz_log(YLOG_WARN, "Missing equal-sign in filter");
            return 0;
        }
        *(eq++) = '\0';
        new->name = values[i];
        nmem_strsplit(m, "|", eq, &subvalues, &subnum);
        new->values = 0;
        for (subi = 0; subi < subnum; subi++)
        {
            struct database_criterion_value *newv = nmem_malloc(m, sizeof(*newv));
            newv->value = subvalues[subi];
            newv->next = new->values;
            new->values = newv;
        }
        new->next = res;
        res = new;
    }
    return res;
}

char *search(struct session *se, char *query, char *filter)
{
    int live_channels = 0;
    struct client *cl;
    struct database_criterion *criteria;

    yaz_log(YLOG_DEBUG, "Search");

    nmem_reset(se->nmem);
    criteria = parse_filter(se->nmem, filter);
    se->requestid++;
    live_channels = select_targets(se, criteria);
    if (live_channels)
    {
        int maxrecs = live_channels * global_parameters.toget;
        se->num_termlists = 0;
        se->reclist = reclist_create(se->nmem, maxrecs);
        // This will be initialized in send_search()
        se->total_records = se->total_hits = se->total_merged = 0;
        se->expected_maxrecs = maxrecs;
    }
    else
        return "NOTARGETS";

    se->relevance = 0;

    for (cl = se->clients; cl; cl = cl->next)
    {
        if (prepare_session_database(se, cl->database) < 0)
            return "CONFIG_ERROR";
        if (client_parse_query(cl, query) < 0)  // Query must parse for all targets
            return "QUERY";
    }

    for (cl = se->clients; cl; cl = cl->next)
    {
        client_prep_connection(cl);
    }

    return 0;
}

// Creates a new session_database object for a database
static void session_init_databases_fun(void *context, struct database *db)
{
    struct session *se = (struct session *) context;
    struct session_database *new = nmem_malloc(se->session_nmem, sizeof(*new));
    int num = settings_num();
    int i;

    new->database = db;
    new->yaz_marc = 0;
    new->map = 0;
    new->settings = nmem_malloc(se->session_nmem, sizeof(struct settings *) * num);
    memset(new->settings, 0, sizeof(struct settings*) * num);
    if (db->settings)
    {
        for (i = 0; i < num; i++)
            new->settings[i] = db->settings[i];
    }
    new->next = se->databases;
    se->databases = new;
}

// Doesn't free memory associated with sdb -- nmem takes care of that
static void session_database_destroy(struct session_database *sdb)
{
    struct database_retrievalmap *m;

    for (m = sdb->map; m; m = m->next)
        xsltFreeStylesheet(m->stylesheet);
    if (sdb->yaz_marc)
        yaz_marc_destroy(sdb->yaz_marc);
}

// Initialize session_database list -- this represents this session's view
// of the database list -- subject to modification by the settings ws command
void session_init_databases(struct session *se)
{
    se->databases = 0;
    grep_databases(se, 0, session_init_databases_fun);
}

// Probably session_init_databases_fun should be refactored instead of
// called here.
static struct session_database *load_session_database(struct session *se, char *id)
{
    struct database *db = find_database(id, 0);

    session_init_databases_fun((void*) se, db);
    // New sdb is head of se->databases list
    return se->databases;
}

// Find an existing session database. If not found, load it
static struct session_database *find_session_database(struct session *se, char *id)
{
    struct session_database *sdb;

    for (sdb = se->databases; sdb; sdb = sdb->next)
        if (!strcmp(sdb->database->url, id))
            return sdb;
    return load_session_database(se, id);
}

// Apply a session override to a database
void session_apply_setting(struct session *se, char *dbname, char *setting, char *value)
{
    struct session_database *sdb = find_session_database(se, dbname);
    struct setting *new = nmem_malloc(se->session_nmem, sizeof(*new));
    int offset = settings_offset(setting);

    if (offset < 0)
    {
        yaz_log(YLOG_WARN, "Unknown setting %s", setting);
        return;
    }
    new->precedence = 0;
    new->target = dbname;
    new->name = setting;
    new->value = value;
    new->next = sdb->settings[offset];
    sdb->settings[offset] = new;

    // Force later recompute of settings-driven data structures
    // (happens when a search starts and client connections are prepared)
    switch (offset)
    {
        case PZ_NATIVESYNTAX:
            if (sdb->yaz_marc)
            {
                yaz_marc_destroy(sdb->yaz_marc);
                sdb->yaz_marc = 0;
            }
            break;
        case PZ_XSLT:
            if (sdb->map)
            {
                struct database_retrievalmap *m;
                // We don't worry about the map structure -- it's in nmem
                for (m = sdb->map; m; m = m->next)
                    xsltFreeStylesheet(m->stylesheet);
                sdb->map = 0;
            }
            break;
    }
}

void destroy_session(struct session *s)
{
    struct session_database *sdb;

    yaz_log(YLOG_LOG, "Destroying session");
    while (s->clients)
        client_destroy(s->clients);
    for (sdb = s->databases; sdb; sdb = sdb->next)
        session_database_destroy(sdb);
    nmem_destroy(s->nmem);
    wrbuf_destroy(s->wrbuf);
}

struct session *new_session(NMEM nmem) 
{
    int i;
    struct session *session = nmem_malloc(nmem, sizeof(*session));

    yaz_log(YLOG_DEBUG, "New Pazpar2 session");
    
    session->total_hits = 0;
    session->total_records = 0;
    session->num_termlists = 0;
    session->reclist = 0;
    session->requestid = -1;
    session->clients = 0;
    session->expected_maxrecs = 0;
    session->session_nmem = nmem;
    session->nmem = nmem_create();
    session->wrbuf = wrbuf_alloc();
    session_init_databases(session);
    for (i = 0; i <= SESSION_WATCH_MAX; i++)
    {
        session->watchlist[i].data = 0;
        session->watchlist[i].fun = 0;
    }

    return session;
}

struct hitsbytarget *hitsbytarget(struct session *se, int *count)
{
    static struct hitsbytarget res[1000]; // FIXME MM
    struct client *cl;

    *count = 0;
    for (cl = se->clients; cl; cl = cl->next)
    {
        char *name = session_setting_oneval(cl->database, PZ_NAME);

        res[*count].id = cl->database->database->url;
        res[*count].name = *name ? name : "Unknown";
        res[*count].hits = cl->hits;
        res[*count].records = cl->records;
        res[*count].diagnostic = cl->diagnostic;
        res[*count].state = client_states[cl->state];
        res[*count].connected  = cl->connection ? 1 : 0;
        (*count)++;
    }

    return res;
}

struct termlist_score **termlist(struct session *s, const char *name, int *num)
{
    int i;

    for (i = 0; i < s->num_termlists; i++)
        if (!strcmp((const char *) s->termlists[i].name, name))
            return termlist_highscore(s->termlists[i].termlist, num);
    return 0;
}

#ifdef MISSING_HEADERS
void report_nmem_stats(void)
{
    size_t in_use, is_free;

    nmem_get_memory_in_use(&in_use);
    nmem_get_memory_free(&is_free);

    yaz_log(YLOG_LOG, "nmem stat: use=%ld free=%ld", 
            (long) in_use, (long) is_free);
}
#endif

struct record_cluster *show_single(struct session *s, int id)
{
    struct record_cluster *r;

    reclist_rewind(s->reclist);
    while ((r = reclist_read_record(s->reclist)))
        if (r->recid == id)
            return r;
    return 0;
}

struct record_cluster **show(struct session *s, struct reclist_sortparms *sp, int start,
        int *num, int *total, int *sumhits, NMEM nmem_show)
{
    struct record_cluster **recs = nmem_malloc(nmem_show, *num 
                                       * sizeof(struct record_cluster *));
    struct reclist_sortparms *spp;
    int i;
#if USE_TIMING    
    yaz_timing_t t = yaz_timing_create();
#endif

    for (spp = sp; spp; spp = spp->next)
        if (spp->type == Metadata_sortkey_relevance)
        {
            relevance_prepare_read(s->relevance, s->reclist);
            break;
        }
    reclist_sort(s->reclist, sp);

    *total = s->reclist->num_records;
    *sumhits = s->total_hits;

    for (i = 0; i < start; i++)
        if (!reclist_read_record(s->reclist))
        {
            *num = 0;
            recs = 0;
            break;
        }

    for (i = 0; i < *num; i++)
    {
        struct record_cluster *r = reclist_read_record(s->reclist);
        if (!r)
        {
            *num = i;
            break;
        }
        recs[i] = r;
    }
#if USE_TIMING
    yaz_timing_stop(t);
    yaz_log(YLOG_LOG, "show %6.5f %3.2f %3.2f", 
            yaz_timing_get_real(t), yaz_timing_get_user(t),
            yaz_timing_get_sys(t));
    yaz_timing_destroy(&t);
#endif
    return recs;
}

void statistics(struct session *se, struct statistics *stat)
{
    struct client *cl;
    int count = 0;

    memset(stat, 0, sizeof(*stat));
    for (cl = se->clients; cl; cl = cl->next)
    {
        if (!cl->connection)
            stat->num_no_connection++;
        switch (cl->state)
        {
            case Client_Connecting: stat->num_connecting++; break;
            case Client_Initializing: stat->num_initializing++; break;
            case Client_Searching: stat->num_searching++; break;
            case Client_Presenting: stat->num_presenting++; break;
            case Client_Idle: stat->num_idle++; break;
            case Client_Failed: stat->num_failed++; break;
            case Client_Error: stat->num_error++; break;
            default: break;
        }
        count++;
    }
    stat->num_hits = se->total_hits;
    stat->num_records = se->total_records;

    stat->num_clients = count;
}

void start_http_listener(void)
{
    char hp[128] = "";
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.listener_override)
        strcpy(hp, global_parameters.listener_override);
    else
    {
        strcpy(hp, ser->host ? ser->host : "");
        if (ser->port)
        {
            if (*hp)
                strcat(hp, ":");
            sprintf(hp + strlen(hp), "%d", ser->port);
        }
    }
    http_init(hp);
}

void start_proxy(void)
{
    char hp[128] = "";
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.proxy_override)
        strcpy(hp, global_parameters.proxy_override);
    else if (ser->proxy_host || ser->proxy_port)
    {
        strcpy(hp, ser->proxy_host ? ser->proxy_host : "");
        if (ser->proxy_port)
        {
            if (*hp)
                strcat(hp, ":");
            sprintf(hp + strlen(hp), "%d", ser->proxy_port);
        }
    }
    else
        return;

    http_set_proxyaddr(hp, ser->myurl ? ser->myurl : "");
}

void start_zproxy(void)
{
    struct conf_server *ser = global_parameters.server;

    if (*global_parameters.zproxy_override){
        yaz_log(YLOG_LOG, "Z39.50 proxy  %s", 
                global_parameters.zproxy_override);
        return;
    }

    else if (ser->zproxy_host || ser->zproxy_port)
    {
        char hp[128] = "";

        strcpy(hp, ser->zproxy_host ? ser->zproxy_host : "");
        if (ser->zproxy_port)
        {
            if (*hp)
                strcat(hp, ":");
            else
                strcat(hp, "@:");

            sprintf(hp + strlen(hp), "%d", ser->zproxy_port);
        }
        strcpy(global_parameters.zproxy_override, hp);
        yaz_log(YLOG_LOG, "Z39.50 proxy  %s", 
                global_parameters.zproxy_override);

    }
    else
        return;
}

// Master list of connections we're handling events to
static IOCHAN channel_list = 0; 
void pazpar2_add_channel(IOCHAN chan)
{
    chan->next = channel_list;
    channel_list = chan;
}

void pazpar2_event_loop()
{
    event_loop(&channel_list);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
