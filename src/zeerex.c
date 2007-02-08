/* $Id: zeerex.c,v 1.3 2007-02-08 19:26:33 adam Exp $ */

#include <string.h>

#include <yaz/yaz-util.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "zeerex.h"

// Replace this with something that will take a callback
static void fail(const char *s, xmlNode *n)
{
    yaz_log(YLOG_WARN, "Zeerex Err '%s' in elem '%s/%s'", s, n->parent->name, n->name);
}

// returns an nmem-allocated string if attr is present, or null
static char *attrtostr(NMEM m, xmlNode *n, const char *name)
{
    char *s = xmlGetProp(n, name);
    if (s)
    {
        char *r = nmem_strdup(m, s);
        xmlFree(s);
        return r;
    }
    else
        return 0;
}

static int attrtoint(xmlNode *n, const char *name)
{
    char *s = xmlGetProp(n, name);
    if (s)
    {
        int val = atoi(s);
        xmlFree(s);
        return val;
    }
    else
        return 0;
}

static Zr_bool attrtobool(xmlNode *node, const char *name)
{
    char *v = xmlGetProp(node, name);
    if (v)
    {
        Zr_bool res;
        if (!strcmp(v, "true"))
            res = Zr_bool_true;
        else if (!strcmp(v, "false"))
            res = Zr_bool_false;
        else
            res = Zr_bool_unknown;
        xmlFree(v);
        return res;
    }
    else
        return Zr_bool_unknown;
}

static char *valuetostr(NMEM m, xmlNode *n)
{
    char *val = xmlNodeGetContent(n);
    if (val)
    {
        char *res = nmem_strdup(m, val);
        xmlFree(val);
        return res;
    }
    else
        return 0;
}

static int valuetoint(xmlNode *n)
{
    char *s = xmlNodeGetContent(n);
    if (s)
    {
        int res = atoi(s);
        xmlFree(s);
        return res;
    }
    else
        return 0;
}

static Zr_langstr *findlangstr(NMEM m, xmlNode *node, const char *name)
{
    xmlNode *n;
    Zr_langstr *res = 0;
    for (n = node->children; n; n = n->next)
    {
        if (n->type == XML_ELEMENT_NODE && !strcmp(n->name, name))
        {
            Zr_langstr *new = nmem_malloc(m, sizeof(*new));
            memset(new, 0, sizeof(*new));
            new->primary = attrtobool(n, "primary");
            new->lang = attrtostr(m, n, "lang");
            new->str = valuetostr(m, n);
            new->next = res;
            res = new;
        }
    }
    return res;
}

static struct zr_authentication *authentication(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_authentication *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    r->type = attrtostr(m, node, "type");
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "open"))
            r->open = valuetostr(m, n);
        else if (!strcmp(n->name, "user"))
            r->user = valuetostr(m, n);
        else if (!strcmp(n->name, "group"))
            r->group = valuetostr(m, n);
        else if (!strcmp(n->name, "password"))
            r->password = valuetostr(m, n);
        else
        {
            fail("Unexpected element", n);
            return 0;
        }
    }
    return r;
}


static struct zr_serverInfo *serverInfo(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_serverInfo *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));

    r->protocol = attrtostr(m, n, "protocol");
    r->version = attrtostr(m, n, "version");
    r->transport = attrtostr(m, n, "transport");
    r->method = attrtostr(m, n, "method");
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "host"))
            r->host = valuetostr(m, n);
        else if (!strcmp(n->name, "port"))
            r->port = valuetoint(n);
        else if (!strcmp(n->name, "database"))
            r->database = valuetostr(m, n);
        else if (!strcmp(n->name, "authentication") && !(r->authentication =
                        authentication(m, n)))
            return 0;
        else
        {
            fail("Unexpected element", n);
            return 0;
        }
    }
    return r;
}

static struct zr_agent *agent(NMEM m, xmlNode *node)
{
    struct zr_agent *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    r->type = attrtostr(m, node, "type");
    r->identifier = attrtostr(m, node, "identifier");
    r->value = valuetostr(m, node);
    return r;
}

static struct zr_implementation *implementation(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_implementation *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    r->identifier = attrtostr(m, node, "identifier");
    r->version = attrtostr(m, node, "version");
    r->title = findlangstr(m, node, "title");
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "agent"))
        {
            struct zr_agent *ag = agent(m, node);
            if (!ag)
                return 0;
            ag->next = r->agents;
            r->agents = ag;
        }
    }
    return r;
}

struct zr_databaseInfo *databaseInfo(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_databaseInfo *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));

    r->title = findlangstr(m, node, "title");
    r->description = findlangstr(m, node, "description");
    r->history = findlangstr(m, node, "history");
    r->extent = findlangstr(m, node, "extent");
    r->restrictions = findlangstr(m, node, "restrictions");
    r->langUsage = findlangstr(m, node, "langUsage");

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "agents"))
        {
            xmlNode *n2;
            for (n2 = n->children; n2; n2 = n2->next)
            {
                if (n2->type != XML_ELEMENT_NODE)
                    continue;
                if (strcmp(n2->name, "agent"))
                    continue;
                else
                {
                    struct zr_agent *ag = agent(m, n2);
                    if (!ag)
                        return 0;
                    ag->next = r->agents;
                    r->agents = ag;
                }
            }
        }
        else if (!strcmp(n->name, "implementation") &&
                !(r->implementation = implementation(m, n)))
            return 0;
        else if (!strcmp(n->name, "links"))
        {
            xmlNode *n2;
            for (n2 = n->children; n2; n2 = n2->next)
            {
                if (!n2->type != XML_ELEMENT_NODE)
                    continue;
                if (!strcmp(n2->name, "link"))
                    continue;
                else
                {
                    struct zr_link *li = nmem_malloc(m, sizeof(*li));
                    memset(li, 0, sizeof(*li));
                    li->type = attrtostr(m, n2, "type");
                    li->value = valuetostr(m, n2);
                    li->next = r->links;
                    r->links = li;
                }
            }
        }
        else if (!strcmp(n->name, "history") && !r->lastUpdate)
            r->lastUpdate = attrtostr(m, n, "lastUpdate");
        else if (!strcmp(n->name, "extent") && !r->numberOfRecords)
            r->numberOfRecords = attrtoint(n, "numberOfRecords");
        else if (!strcmp(n->name, "langUsage") && !r->codes)
            r->codes = attrtostr(m, n, "codes");
    }
    return r;
}

struct zr_metaInfo *metaInfo(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_metaInfo *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));

    for (n = node->children; n; n = n->next)
    {
        if (!n->type == XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "dateModified"))
            r->dateModified = valuetostr(m, n);
        else if (!strcmp(n->name, "dateAggregated"))
            r->dateAggregated = valuetostr(m, n);
        else if (!strcmp(n->name, "aggregatedFrom"))
            r->aggregatedFrom = valuetostr(m, n);
        else
        {
            fail("Unexpected element", n);
            return 0;
        }
    }
    return r;
}

struct zr_set *set(NMEM m, xmlNode *node)
{
    struct zr_set *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    r->name = attrtostr(m, node, "name");
    r->identifier = attrtostr(m, node, "identifier");
    r->title = findlangstr(m, node, "title");
    return r;
}

struct zr_attr *attr(NMEM m, xmlNode *node)
{
    struct zr_attr *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    r->type = attrtoint(node, "type");
    r->set = attrtostr(m, node, "set");
    return r;
}

static struct zr_map *map(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_map *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));

    r->lang = attrtostr(m, node, "lang");
    r->primary = attrtobool(node, "primary");
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "name"))
        {
            r->set = attrtostr(m, n, "set");
            r->name = valuetostr(m, n);
        }
        else if (!strcmp(n->name, "attr"))
        {
            struct zr_attr *new = attr(m, n);
            if (!new)
                return 0;
            new->next = r->attrs;
            r->attrs = new;
        }
        else
        {
            fail("Unexpected element", n);
            return 0;
        }
    }
    return r;
}

static Zr_setting *findsetting(NMEM m, xmlNode *node, char *name)
{
    static Zr_setting *r = 0;
    xmlNode *n;
    for (n = node->children; n; n = n->next)
    {
        if (node->type == XML_ELEMENT_NODE && !strcmp(n->name, name))
        {
            xmlNode *n2;
            struct zr_setting *new = nmem_malloc(m, sizeof(*new));
            memset(new, 0, sizeof(*new));
            new->type = attrtostr(m, n, "type");
            for (n2 = n->children; n2; n2 = n2->next)
            {
                if (n2->type == XML_ELEMENT_NODE && !strcmp(n2->name, "map"))
                {
                    new->map = map(m, n2);
                    if (!new)
                        return 0;
                    break;
                }
            }
            if (!new->map)
                new->value = xmlNodeGetContent(n);
            new->next = r;
            r = new;
        }
    }
    return r;
}

static struct zr_configInfo *configInfo(NMEM m, xmlNode *node)
{
    struct zr_configInfo *r = nmem_malloc(m, sizeof(*r));

    r->defaultv = findsetting(m, node, "default");
    r->setting = findsetting(m, node, "setting");
    r->supports = findsetting(m, node, "supports");
    return r;
}

static struct zr_index *parse_index(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_index *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));

    r->search = attrtobool(node, "search");
    r->scan = attrtobool(node, "scan");
    r->sort = attrtobool(node, "sort");
    r->id = attrtostr(m, node, "id");
    r->title = findlangstr(m, node, "title");

    for (n = node->children; n; n = n->next)
    {
        if (!strcmp(n->name, "map"))
        {
            struct zr_map *new = map(m, n);
            if (!new)
                return 0;
            new->next = r->maps;
            r->maps = new;
        }
        else if (!strcmp(n->name, "configInfo") && !(r->configInfo = configInfo(m, n)))
            return 0;
        else if (strcmp(n->name, "title"))
        {
            fail("Unknown child element", n);
            return 0;
        }
    }
    return r;
}

static struct zr_sortKeyword *sortKeyword(NMEM m, xmlNode *node)
{
    struct zr_sortKeyword *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    r->value = valuetostr(m, node);
    return r;
}

static struct zr_indexInfo *indexInfo(NMEM m , xmlNode *node)
{
    xmlNode *n;
    struct zr_indexInfo *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "set"))
        {
            struct zr_set *new = set(m, n);
            if (!new)
                return 0;
            new->next = r->sets;
            r->sets = new;
        }
        else if (!strcmp(n->name, "index"))
        {
            struct zr_index *new = parse_index(m, n);
            if (!new)
                return 0;
            new->next = r->indexes;
            r->indexes = new;
        }
        else if (!strcmp(n->name, "sortKeyword"))
        {
            struct zr_sortKeyword *new = sortKeyword(m, n);
            if (!new)
                return 0;
            new->next = r->sortKeywords;
            r->sortKeywords = new;
        }
        else if (!strcmp(n->name, "sortKeyword") && !(r->configInfo = configInfo(m, n)))
            return 0;
        else
        {
            fail("Unknown child element", n);
            return 0;
        }
    }
    return r;
}

static struct zr_elementSet *elementSet(NMEM m, xmlNode *node)
{
    struct zr_elementSet *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    r->name = attrtostr(m, node, "name");
    r->identifier = attrtostr(m, node, "identifier");
    r->title = findlangstr(m, node, "title");
    return r;
}

static struct zr_recordSyntax *recordSyntax(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_recordSyntax *r = nmem_malloc(m, sizeof(*r));
    struct zr_elementSet **elementp = &r->elementSets;

    memset(r, 0, sizeof(*r));
    r->name = attrtostr(m, node, "name");
    r->identifier = attrtostr(m, node, "identifier");
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "elementSet"))
        {
            if (!(*elementp = elementSet(m, n)))
                return 0;
            elementp = &(*elementp)->next;
        }
        else
        {
            fail("Unknown child element", n);
            return 0;
        }
    }
    return r;
}

static struct zr_recordInfo *recordInfo(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_recordInfo *r = nmem_malloc(m, sizeof(*r));
    struct zr_recordSyntax **syntaxp = &r->recordSyntaxes;

    memset(r, 0, sizeof(*r));
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "recordSyntax"))
        {
            if (!(*syntaxp = recordSyntax(m, n)))
                return 0;
            syntaxp = &(*syntaxp)->next;
        }
        else
        {
            fail("Unknown child element", n);
            return 0;
        }
    }
    return r;
}


static struct zr_schema *schema(NMEM m, xmlNode *node)
{
    struct zr_schema *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));
    
    r->name = attrtostr(m, node, "name");
    r->identifier = attrtostr(m, node, "identifier");
    r->retrieve = attrtobool(node, "retrieve");
    r->sort = attrtobool(node, "sort");
    r->location = attrtostr(m, node, "location");
    r->title = findlangstr(m, node, "title");
    return r;
}

static struct zr_schemaInfo *schemaInfo(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_schemaInfo *r = nmem_malloc(m, sizeof(*r));
    struct zr_schema **schemap = &r->schemas;

    memset(r, 0, sizeof(*r));
    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "schema"))
        {
            if (!(*schemap = schema(m, n)))
                return 0;
            schemap = &(*schemap)->next;
        }
        else
        {
            fail("Unknown child element", n);
            return 0;
        }
    }
    return r;
}

static struct zr_explain *explain(NMEM m, xmlNode *node)
{
    xmlNode *n;
    struct zr_explain *r = nmem_malloc(m, sizeof(*r));
    memset(r, 0, sizeof(*r));

    for (n = node->children; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE)
            continue;
        if (!strcmp(n->name, "serverInfo") && !(r->serverInfo = serverInfo(m, n)))
            return 0;
        else if (!strcmp(n->name, "databaseInfo") && !(r->databaseInfo = databaseInfo(m, n)))
            return 0;
        else if (!strcmp(n->name, "metaInfo") && !(r->metaInfo = metaInfo(m, n)))
            return 0;
        else if (!strcmp(n->name, "indexInfo") && !(r->indexInfo = indexInfo(m, n)))
            return 0;
        else if (!strcmp(n->name, "recordInfo") && !(r->recordInfo = recordInfo(m, n)))
            return 0;
        else if (!strcmp(n->name, "schemaInfo") && !(r->schemaInfo = schemaInfo(m, n)))
            return 0;
        else if (!strcmp(n->name, "configInfo") && !(r->configInfo = configInfo(m, n)))
           return 0;
        else
        {
            fail("Unknown child element", n);
            return 0;
        }
    }
    return r;
}

struct zr_explain *zr_read_xml(NMEM m, xmlNode *n)
{
    return explain(m, n);
}

struct zr_explain *zr_read_file(NMEM m, const char *fn)
{
    xmlDoc *doc = xmlParseFile(fn);
    struct zr_explain *r;
    if (!doc)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "Unable to open %s", fn);
        return 0;
    }
    r = explain(m, xmlDocGetRootElement(doc));
    xmlFree(doc);
    return r;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
