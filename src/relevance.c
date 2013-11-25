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
#include <math.h>
#include <stdlib.h>

#include "pazpar2_config.h"
#include "relevance.h"
#include "session.h"
#include "client.h"

#ifdef WIN32
#define log2(x) (log(x)/log(2))
#endif

struct relevance
{
    int *doc_frequency_vec;
    int *term_frequency_vec_tmp;
    int *term_pos;
    int vec_len;
    struct word_entry *entries;
    pp2_charset_token_t prt;
    int rank_cluster;
    double follow_factor;
    double lead_decay;
    int length_divide;
    NMEM nmem;
};

struct word_entry {
    const char *norm_str;
    const char *display_str;
    int termno;
    char *ccl_field;
    struct word_entry *next;
};

static struct word_entry *word_entry_match(struct relevance *r,
                                           const char *norm_str,
                                           const char *rank, int *weight)
{
    int i = 1;
    struct word_entry *entries = r->entries;
    for (; entries; entries = entries->next, i++)
    {
        if (*norm_str && !strcmp(norm_str, entries->norm_str))
        {
            const char *cp = 0;
            int no_read = 0;
            sscanf(rank, "%d%n", weight, &no_read);
            rank += no_read;
            while (*rank == ' ')
                rank++;
            if (no_read > 0 && (cp = strchr(rank, ' ')))
            {
                if ((cp - rank) == strlen(entries->ccl_field) &&
                    memcmp(entries->ccl_field, rank, cp - rank) == 0)
                    *weight = atoi(cp + 1);
            }
            return entries;
        }
    }
    return 0;
}

int relevance_snippet(struct relevance *r,
                      const char *words, const char *name,
                      WRBUF w_snippet)
{
    int no = 0;
    const char *norm_str;
    int highlight = 0;

    pp2_charset_token_first(r->prt, words, 0);
    while ((norm_str = pp2_charset_token_next(r->prt)))
    {
        size_t org_start, org_len;
        struct word_entry *entries = r->entries;
        int i;

        pp2_get_org(r->prt, &org_start, &org_len);
        for (; entries; entries = entries->next, i++)
        {
            if (*norm_str && !strcmp(norm_str, entries->norm_str))
                break;
        }
        if (entries)
        {
            if (!highlight)
            {
                highlight = 1;
                wrbuf_puts(w_snippet, "<match>");
                no++;
            }
        }
        else
        {
            if (highlight)
            {
                highlight = 0;
                wrbuf_puts(w_snippet, "</match>");
            }
        }
        wrbuf_xmlputs_n(w_snippet, words + org_start, org_len);
    }
    if (highlight)
        wrbuf_puts(w_snippet, "</match>");
    if (no)
    {
        yaz_log(YLOG_DEBUG, "SNIPPET match: %s", wrbuf_cstr(w_snippet));
    }
    return no;
}

void relevance_countwords(struct relevance *r, struct record_cluster *cluster,
                          const char *words, const char *rank,
                          const char *name)
{
    int *w = r->term_frequency_vec_tmp;
    const char *norm_str;
    int i, length = 0;
    double lead_decay = r->lead_decay;
    struct word_entry *e;
    WRBUF wr = cluster->relevance_explain1;
    int printed_about_field = 0;

    pp2_charset_token_first(r->prt, words, 0);
    for (e = r->entries, i = 1; i < r->vec_len; i++, e = e->next)
    {
        w[i] = 0;
        r->term_pos[i] = 0;
    }

    assert(rank);
    while ((norm_str = pp2_charset_token_next(r->prt)))
    {
        int local_weight = 0;
        e = word_entry_match(r, norm_str, rank, &local_weight);
        if (e)
        {
            int res = e->termno;
            int j;

            if (!printed_about_field)
            {
                printed_about_field = 1;
                wrbuf_printf(wr, "field=%s content=", name);
                if (strlen(words) > 50)
                {
                    wrbuf_xmlputs_n(wr, words, 49);
                    wrbuf_puts(wr, " ...");
                }
                else
                    wrbuf_xmlputs(wr, words);
                wrbuf_puts(wr, ";\n");
            }
            assert(res < r->vec_len);
            w[res] += local_weight / (1 + log2(1 + lead_decay * length));
            wrbuf_printf(wr, "%s: w[%d] += w(%d) / "
                         "(1+log2(1+lead_decay(%f) * length(%d)));\n",
                         e->display_str, res, local_weight, lead_decay, length);
            j = res - 1;
            if (j > 0 && r->term_pos[j])
            {
                int d = length + 1 - r->term_pos[j];
                wrbuf_printf(wr, "%s: w[%d] += w[%d](%d) * follow(%f) / "
                             "(1+log2(d(%d));\n",
                             e->display_str, res, res, w[res],
                             r->follow_factor, d);
                w[res] += w[res] * r->follow_factor / (1 + log2(d));
            }
            for (j = 0; j < r->vec_len; j++)
                r->term_pos[j] = j < res ? 0 : length + 1;
        }
        length++;
    }

    for (e = r->entries, i = 1; i < r->vec_len; i++, e = e->next)
    {
        if (length == 0 || w[i] == 0)
            continue;
        wrbuf_printf(wr, "%s: tf[%d] += w[%d](%d)", e->display_str, i, i, w[i]);
        switch (r->length_divide)
        {
        case 0:
            cluster->term_frequency_vecf[i] += (double) w[i];
            break;
        case 1:
            wrbuf_printf(wr, " / log2(1+length(%d))", length);
            cluster->term_frequency_vecf[i] +=
                (double) w[i] / log2(1 + length);
            break;
        case 2:
            wrbuf_printf(wr, " / length(%d)", length);
            cluster->term_frequency_vecf[i] += (double) w[i] / length;
        }
        cluster->term_frequency_vec[i] += w[i];
        wrbuf_printf(wr, " (%f);\n", cluster->term_frequency_vecf[i]);
    }

    cluster->term_frequency_vec[0] += length;
}

static void pull_terms(struct relevance *res, struct ccl_rpn_node *n)
{
    char **words;
    int numwords;
    char *ccl_field;
    int i;

    switch (n->kind)
    {
    case CCL_RPN_AND:
    case CCL_RPN_OR:
    case CCL_RPN_NOT:
    case CCL_RPN_PROX:
        pull_terms(res, n->u.p[0]);
        pull_terms(res, n->u.p[1]);
        break;
    case CCL_RPN_TERM:
        nmem_strsplit(res->nmem, " ", n->u.t.term, &words, &numwords);
        for (i = 0; i < numwords; i++)
        {
            const char *norm_str;

            ccl_field = nmem_strdup_null(res->nmem, n->u.t.qual);

            pp2_charset_token_first(res->prt, words[i], 0);
            while ((norm_str = pp2_charset_token_next(res->prt)))
            {
                struct word_entry **e = &res->entries;
                while (*e)
                    e = &(*e)->next;
                *e = nmem_malloc(res->nmem, sizeof(**e));
                (*e)->norm_str = nmem_strdup(res->nmem, norm_str);
                (*e)->ccl_field = ccl_field;
                (*e)->termno = res->vec_len++;
                (*e)->display_str = nmem_strdup(res->nmem, words[i]);
                (*e)->next = 0;
            }
        }
        break;
    default:
        break;
    }
}
void relevance_clear(struct relevance *r)
{
    if (r)
    {
        int i;
        for (i = 0; i < r->vec_len; i++)
            r->doc_frequency_vec[i] = 0;
    }
}

struct relevance *relevance_create_ccl(pp2_charset_fact_t pft,
                                       struct ccl_rpn_node *query,
                                       int rank_cluster,
                                       double follow_factor, double lead_decay,
                                       int length_divide)
{
    NMEM nmem = nmem_create();
    struct relevance *res = nmem_malloc(nmem, sizeof(*res));

    res->nmem = nmem;
    res->entries = 0;
    res->vec_len = 1;
    res->rank_cluster = rank_cluster;
    res->follow_factor = follow_factor;
    res->lead_decay = lead_decay;
    res->length_divide = length_divide;
    res->prt = pp2_charset_token_create(pft, "relevance");

    pull_terms(res, query);

    res->doc_frequency_vec = nmem_malloc(nmem, res->vec_len * sizeof(int));

    // worker array
    res->term_frequency_vec_tmp =
        nmem_malloc(res->nmem,
                    res->vec_len * sizeof(*res->term_frequency_vec_tmp));

    res->term_pos =
        nmem_malloc(res->nmem, res->vec_len * sizeof(*res->term_pos));

    relevance_clear(res);
    return res;
}

void relevance_destroy(struct relevance **rp)
{
    if (*rp)
    {
        pp2_charset_token_destroy((*rp)->prt);
        nmem_destroy((*rp)->nmem);
        *rp = 0;
    }
}

void relevance_newrec(struct relevance *r, struct record_cluster *rec)
{
    if (!rec->term_frequency_vec)
    {
        int i;

        // term frequency [1,..] . [0] is total length of all fields
        rec->term_frequency_vec =
            nmem_malloc(r->nmem,
                        r->vec_len * sizeof(*rec->term_frequency_vec));
        for (i = 0; i < r->vec_len; i++)
            rec->term_frequency_vec[i] = 0;

        // term frequency divided by length of field [1,...]
        rec->term_frequency_vecf =
            nmem_malloc(r->nmem,
                        r->vec_len * sizeof(*rec->term_frequency_vecf));
        for (i = 0; i < r->vec_len; i++)
            rec->term_frequency_vecf[i] = 0.0;
    }
}

void relevance_donerecord(struct relevance *r, struct record_cluster *cluster)
{
    int i;

    for (i = 1; i < r->vec_len; i++)
        if (cluster->term_frequency_vec[i] > 0)
            r->doc_frequency_vec[i]++;

    r->doc_frequency_vec[0]++;
}

// Prepare for a relevance-sorted read
void relevance_prepare_read(struct relevance *rel, struct reclist *reclist,
                            enum conf_sortkey_type type)
{
    int i;
    float *idfvec = xmalloc(rel->vec_len * sizeof(float));
    int n_clients = clients_count();
    struct client * clients[n_clients];
    yaz_log(YLOG_LOG,"round-robin: have %d clients", n_clients);
    for (i = 0; i < n_clients; i++)
        clients[i] = 0;


    reclist_enter(reclist);
    // Calculate document frequency vector for each term.
    for (i = 1; i < rel->vec_len; i++)
    {
        if (!rel->doc_frequency_vec[i])
            idfvec[i] = 0;
        else
        {
            /* add one to nominator idf(t,D) to ensure a value > 0 */
            idfvec[i] = log((float) (1 + rel->doc_frequency_vec[0]) /
                            rel->doc_frequency_vec[i]);
        }
    }
    // Calculate relevance for each document
    while (1)
    {
        int relevance = 0;
        WRBUF w;
        struct word_entry *e = rel->entries;
        struct record_cluster *rec = reclist_read_record(reclist);
        if (!rec)
            break;
        w = rec->relevance_explain2;
        wrbuf_rewind(w);
        wrbuf_puts(w, "relevance = 0;\n");
        for (i = 1; i < rel->vec_len; i++)
        {
            float termfreq = (float) rec->term_frequency_vecf[i];
            int add = 100000 * termfreq * idfvec[i];

            wrbuf_printf(w, "idf[%d] = log(((1 + total(%d))/termoccur(%d));\n",
                         i, rel->doc_frequency_vec[0],
                         rel->doc_frequency_vec[i]);
            wrbuf_printf(w, "%s: relevance += 100000 * tf[%d](%f) * "
                         "idf[%d](%f) (%d);\n",
                         e->display_str, i, termfreq, i, idfvec[i], add);
            relevance += add;
            e = e->next;
        }
        if (!rel->rank_cluster)
        {
            struct record *record;
            int cluster_size = 0;

            for (record = rec->records; record; record = record->next)
                cluster_size++;

            wrbuf_printf(w, "score = relevance(%d)/cluster_size(%d);\n",
                         relevance, cluster_size);
            relevance /= cluster_size;
        }
        else
        {
            wrbuf_printf(w, "score = relevance(%d);\n", relevance);
        }
        // Experimental round-robin
        // Overwrites the score calculated above, but I keep it there to
        // get the log entries
        if (type == Metadata_sortkey_relevance_h) {
            struct record *record;
            int thisclient = 0;
            struct record *bestrecord = 0;
            int nclust = 0;
            for (record = rec->records; record; record = record->next) {
                if ( bestrecord == 0 || bestrecord->position < record->position )
                    bestrecord = record;
                nclust++;
            }
            while ( clients[thisclient] != 0
                    && clients[thisclient] != bestrecord->client )
                thisclient++;
            if ( clients[thisclient] == 0 )
            {
                yaz_log(YLOG_LOG,"round-robin: found new client at %d: p=%p\n", thisclient, bestrecord->client);
                clients[thisclient] = bestrecord->client;
            }
            int tfrel = relevance;
            relevance = -(bestrecord->position * n_clients + thisclient) ;
            wrbuf_printf(w,"round-robin score: pos=%d client=%d ncl=%d tfscore=%d score=%d\n",
                         bestrecord->position, thisclient, nclust, tfrel, relevance );
            yaz_log(YLOG_LOG,"round-robin score: pos=%d client=%d ncl=%d score=%d",
                         bestrecord->position, thisclient, nclust, relevance );
        }
        rec->relevance_score = relevance;
    }
    reclist_leave(reclist);
    xfree(idfvec);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

