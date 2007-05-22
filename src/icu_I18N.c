/* $Id: icu_I18N.c,v 1.19 2007-05-22 07:51:45 adam Exp $
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


#ifdef HAVE_ICU
#include "icu_I18N.h"

#include <yaz/log.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unicode/ustring.h>  /* some more string fcns*/
#include <unicode/uchar.h>    /* char names           */


//#include <unicode/ustdio.h>
//#include <unicode/utypes.h>   /* Basic ICU data types */
#include <unicode/ucol.h> 
//#include <unicode/ucnv.h>     /* C   Converter API    */
//#include <unicode/uloc.h>
//#include <unicode/ubrk.h>
/* #include <unicode/unistr.h> */




int icu_check_status (UErrorCode status)
{
    if(U_FAILURE(status)){
        yaz_log(YLOG_WARN, 
                "ICU: %d %s\n", status, u_errorName(status));
        return 0;   
    }
    return 1;
    
}



struct icu_buf_utf16 * icu_buf_utf16_create(size_t capacity)
{
    struct icu_buf_utf16 * buf16 
        = (struct icu_buf_utf16 *) malloc(sizeof(struct icu_buf_utf16));

    buf16->utf16 = 0;
    buf16->utf16_len = 0;
    buf16->utf16_cap = 0;

    if (capacity > 0){
        buf16->utf16 = (UChar *) malloc(sizeof(UChar) * capacity);
        buf16->utf16[0] = (UChar) 0;
        buf16->utf16_cap = capacity;
    }
    return buf16;
};


struct icu_buf_utf16 * icu_buf_utf16_resize(struct icu_buf_utf16 * buf16,
                                            size_t capacity)
{
    if (buf16){
        if (capacity >  0){
            if (0 == buf16->utf16)
                buf16->utf16 = (UChar *) malloc(sizeof(UChar) * capacity);
            else
                buf16->utf16 
                    = (UChar *) realloc(buf16->utf16, sizeof(UChar) * capacity);
            buf16->utf16[0] = (UChar) 0;
            buf16->utf16_len = 0;
            buf16->utf16_cap = capacity;
        } 
        else { 
            if (buf16->utf16)
                free(buf16->utf16);
            buf16->utf16 = 0;
            buf16->utf16_len = 0;
            buf16->utf16_cap = 0;
        }
    }

    return buf16;
};


struct icu_buf_utf16 * icu_buf_utf16_copy(struct icu_buf_utf16 * dest16,
                                          struct icu_buf_utf16 * src16)
{
    if(!dest16 || !src16
       || dest16 == src16)
        return 0;

    if (dest16->utf16_cap < src16->utf16_len)
        icu_buf_utf16_resize(dest16, src16->utf16_len * 2);

    u_strncpy(dest16->utf16, src16->utf16, src16->utf16_len);
    dest16->utf16_len = src16->utf16_len;

    return dest16;
};


void icu_buf_utf16_destroy(struct icu_buf_utf16 * buf16)
{
    if (buf16){
        if (buf16->utf16)
            free(buf16->utf16);
        free(buf16);
    }
};






struct icu_buf_utf8 * icu_buf_utf8_create(size_t capacity)
{
    struct icu_buf_utf8 * buf8 
        = (struct icu_buf_utf8 *) malloc(sizeof(struct icu_buf_utf8));

    buf8->utf8 = 0;
    buf8->utf8_len = 0;
    buf8->utf8_cap = 0;

    if (capacity > 0){
        buf8->utf8 = (uint8_t *) malloc(sizeof(uint8_t) * capacity);
        buf8->utf8[0] = (uint8_t) 0;
        buf8->utf8_cap = capacity;
    }
    return buf8;
};



struct icu_buf_utf8 * icu_buf_utf8_resize(struct icu_buf_utf8 * buf8,
                                          size_t capacity)
{
    if (buf8){
        if (capacity >  0){
            if (0 == buf8->utf8)
                buf8->utf8 = (uint8_t *) malloc(sizeof(uint8_t) * capacity);
            else
                buf8->utf8 
                    = (uint8_t *) realloc(buf8->utf8, 
                                          sizeof(uint8_t) * capacity);
            buf8->utf8[0] = (uint8_t) 0;
            buf8->utf8_len = 0;
            buf8->utf8_cap = capacity;
        } 
        else { 
            if (buf8->utf8)
                free(buf8->utf8);
            buf8->utf8 = 0;
            buf8->utf8_len = 0;
            buf8->utf8_cap = 0;
        }
    }

    return buf8;
};


struct icu_buf_utf8 * icu_buf_utf8_copy(struct icu_buf_utf8 * dest8,
                                          struct icu_buf_utf8 * src8)
{
    if(!dest8 || !src8
       || dest8 == src8)
        return 0;
    

    if (dest8->utf8_cap < src8->utf8_len)
        icu_buf_utf8_resize(dest8, src8->utf8_len * 2);

    strncpy((char*) dest8->utf8, (char*) src8->utf8, src8->utf8_len);

    return dest8;
};



void icu_buf_utf8_destroy(struct icu_buf_utf8 * buf8)
{
    if (buf8){
        if (buf8->utf8)
            free(buf8->utf8);
        free(buf8);
    }
};



UErrorCode icu_utf16_from_utf8(struct icu_buf_utf16 * dest16,
                               struct icu_buf_utf8 * src8,
                               UErrorCode * status)
{
    int32_t utf16_len = 0;
  
    u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                  &utf16_len,
                  (const char *) src8->utf8, src8->utf8_len, status);
  
    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        //|| dest16->utf16_len > dest16->utf16_cap
        ){
        icu_buf_utf16_resize(dest16, utf16_len * 2);
        *status = U_ZERO_ERROR;
        u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                      &utf16_len,
                      (const char *) src8->utf8, src8->utf8_len, status);
    }

    //if (*status != U_BUFFER_OVERFLOW_ERROR
    if (U_SUCCESS(*status)  
        && utf16_len < dest16->utf16_cap)
        dest16->utf16_len = utf16_len;
    else {
        dest16->utf16[0] = (UChar) 0;
        dest16->utf16_len = 0;
    }
  
    return *status;
};

 

UErrorCode icu_utf16_from_utf8_cstr(struct icu_buf_utf16 * dest16,
                                    const char * src8cstr,
                                    UErrorCode * status)
{
    size_t src8cstr_len = 0;
    int32_t utf16_len = 0;

    src8cstr_len = strlen(src8cstr);
  
    u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                  &utf16_len,
                  src8cstr, src8cstr_len, status);
  
    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        //|| dest16->utf16_len > dest16->utf16_cap
        ){
        icu_buf_utf16_resize(dest16, utf16_len * 2);
        *status = U_ZERO_ERROR;
        u_strFromUTF8(dest16->utf16, dest16->utf16_cap,
                      &utf16_len,
                      src8cstr, src8cstr_len, status);
    }

    //  if (*status != U_BUFFER_OVERFLOW_ERROR
    if (U_SUCCESS(*status)  
        && utf16_len < dest16->utf16_cap)
        dest16->utf16_len = utf16_len;
    else {
        dest16->utf16[0] = (UChar) 0;
        dest16->utf16_len = 0;
    }
  
    return *status;
};




UErrorCode icu_utf16_to_utf8(struct icu_buf_utf8 * dest8,
                             struct icu_buf_utf16 * src16,
                             UErrorCode * status)
{
    int32_t utf8_len = 0;
  
    u_strToUTF8((char *) dest8->utf8, dest8->utf8_cap,
                &utf8_len,
                src16->utf16, src16->utf16_len, status);
  
    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        //|| dest8->utf8_len > dest8->utf8_cap
        ){
        icu_buf_utf8_resize(dest8, utf8_len * 2);
        *status = U_ZERO_ERROR;
        u_strToUTF8((char *) dest8->utf8, dest8->utf8_cap,
                    &utf8_len,
                    src16->utf16, src16->utf16_len, status);

    }

    //if (*status != U_BUFFER_OVERFLOW_ERROR
    if (U_SUCCESS(*status)  
        && utf8_len < dest8->utf8_cap)
        dest8->utf8_len = utf8_len;
    else {
        dest8->utf8[0] = (uint8_t) 0;
        dest8->utf8_len = 0;
    }
  
    return *status;
};



struct icu_casemap * icu_casemap_create(const char *locale, char action,
                                        UErrorCode *status)
{    
    struct icu_casemap * casemap
        = (struct icu_casemap *) malloc(sizeof(struct icu_casemap));
    strcpy(casemap->locale, locale);
    casemap->action = action;

    switch(casemap->action) {    
    case 'l':   
        break;
    case 'u':   
        break;
    case 't':  
        break;
    case 'f':  
        break;
    default:
        icu_casemap_destroy(casemap);
        return 0;
    }

    return casemap;
};

void icu_casemap_destroy(struct icu_casemap * casemap)
{
    if (casemap) 
        free(casemap);
};


int icu_casemap_casemap(struct icu_casemap * casemap,
                        struct icu_buf_utf16 * dest16,
                        struct icu_buf_utf16 * src16,
                        UErrorCode *status)
{
    if(!casemap)
        return 0;
    
    return icu_utf16_casemap(dest16, src16,
                             casemap->locale, casemap->action, status);
};


int icu_utf16_casemap(struct icu_buf_utf16 * dest16,
                      struct icu_buf_utf16 * src16,
                      const char *locale, char action,
                      UErrorCode *status)
{
    int32_t dest16_len = 0;
    
    switch(action) {    
    case 'l':    
        dest16_len = u_strToLower(dest16->utf16, dest16->utf16_cap,
                                  src16->utf16, src16->utf16_len, 
                                  locale, status);
        break;
    case 'u':    
        dest16_len = u_strToUpper(dest16->utf16, dest16->utf16_cap,
                                  src16->utf16, src16->utf16_len, 
                                  locale, status);
        break;
    case 't':    
        dest16_len = u_strToTitle(dest16->utf16, dest16->utf16_cap,
                                  src16->utf16, src16->utf16_len,
                                  0, locale, status);
        break;
    case 'f':    
        dest16_len = u_strFoldCase(dest16->utf16, dest16->utf16_cap,
                                   src16->utf16, src16->utf16_len,
                                   U_FOLD_CASE_DEFAULT, status);
        break;
        
    default:
        return U_UNSUPPORTED_ERROR;
        break;
    }

    // check for buffer overflow, resize and retry
    if (*status == U_BUFFER_OVERFLOW_ERROR
        && dest16 != src16        // do not resize if in-place conversion 
        //|| dest16_len > dest16->utf16_cap
        ){
        icu_buf_utf16_resize(dest16, dest16_len * 2);
        *status = U_ZERO_ERROR;

    
        switch(action) {    
        case 'l':    
            dest16_len = u_strToLower(dest16->utf16, dest16->utf16_cap,
                                      src16->utf16, src16->utf16_len, 
                                      locale, status);
            break;
        case 'u':    
            dest16_len = u_strToUpper(dest16->utf16, dest16->utf16_cap,
                                      src16->utf16, src16->utf16_len, 
                                      locale, status);
            break;
        case 't':    
            dest16_len = u_strToTitle(dest16->utf16, dest16->utf16_cap,
                                      src16->utf16, src16->utf16_len,
                                      0, locale, status);
            break;
        case 'f':    
            dest16_len = u_strFoldCase(dest16->utf16, dest16->utf16_cap,
                                       src16->utf16, src16->utf16_len,
                                       U_FOLD_CASE_DEFAULT, status);
            break;
        
        default:
            return U_UNSUPPORTED_ERROR;
            break;
        }
    }
    
    if (U_SUCCESS(*status)
        && dest16_len < dest16->utf16_cap)
        dest16->utf16_len = dest16_len;
    else {
        dest16->utf16[0] = (UChar) 0;
        dest16->utf16_len = 0;
    }
  
    return *status;
};



UErrorCode icu_sortkey8_from_utf16(UCollator *coll,
                                   struct icu_buf_utf8 * dest8, 
                                   struct icu_buf_utf16 * src16,
                                   UErrorCode * status)
{ 
  
    int32_t sortkey_len = 0;

    sortkey_len = ucol_getSortKey(coll, src16->utf16, src16->utf16_len,
                                  dest8->utf8, dest8->utf8_cap);

    // check for buffer overflow, resize and retry
    if (sortkey_len > dest8->utf8_cap) {
        icu_buf_utf8_resize(dest8, sortkey_len * 2);
        sortkey_len = ucol_getSortKey(coll, src16->utf16, src16->utf16_len,
                                      dest8->utf8, dest8->utf8_cap);
    }

    if (U_SUCCESS(*status)
        && sortkey_len > 0)
        dest8->utf8_len = sortkey_len;
    else {
        dest8->utf8[0] = (UChar) 0;
        dest8->utf8_len = 0;
    }

    return sortkey_len;
};



struct icu_tokenizer * icu_tokenizer_create(const char *locale, char action,
                                            UErrorCode *status)
{
    struct icu_tokenizer * tokenizer
        = (struct icu_tokenizer *) malloc(sizeof(struct icu_tokenizer));

    strcpy(tokenizer->locale, locale);
    tokenizer->action = action;
    tokenizer->bi = 0;
    tokenizer->buf16 = 0;
    tokenizer->token_count = 0;
    tokenizer->token_id = 0;
    tokenizer->token_start = 0;
    tokenizer->token_end = 0;


    switch(tokenizer->action) {    
    case 'l':
        tokenizer->bi
            = ubrk_open(UBRK_LINE, tokenizer->locale,
                        0, 0, status);
        break;
    case 's':
        tokenizer->bi
            = ubrk_open(UBRK_SENTENCE, tokenizer->locale,
                        0, 0, status);
        break;
    case 'w':
        tokenizer->bi 
            = ubrk_open(UBRK_WORD, tokenizer->locale,
                        0, 0, status);
        break;
    case 'c':
        tokenizer->bi 
            = ubrk_open(UBRK_CHARACTER, tokenizer->locale,
                        0, 0, status);
        break;
    case 't':
        tokenizer->bi 
            = ubrk_open(UBRK_TITLE, tokenizer->locale,
                        0, 0, status);
        break;
    default:
        *status = U_UNSUPPORTED_ERROR;
        return 0;
        break;
    }
    
    // ICU error stuff is a very  funny business
    if (U_SUCCESS(*status))
        return tokenizer;

    // freeing if failed
    icu_tokenizer_destroy(tokenizer);
    return 0;
};

void icu_tokenizer_destroy(struct icu_tokenizer * tokenizer)
{
    if (tokenizer) {
        if (tokenizer->bi)
            ubrk_close(tokenizer->bi);
        free(tokenizer);
    }
};

int icu_tokenizer_attach(struct icu_tokenizer * tokenizer, 
                         struct icu_buf_utf16 * src16, 
                         UErrorCode *status)
{
    if (!tokenizer || !tokenizer->bi || !src16)
        return 0;


    tokenizer->buf16 = src16;
    tokenizer->token_count = 0;
    tokenizer->token_id = 0;
    tokenizer->token_start = 0;
    tokenizer->token_end = 0;

    ubrk_setText(tokenizer->bi, src16->utf16, src16->utf16_len, status);
    
 
    if (U_FAILURE(*status))
        return 0;

    return 1;
};

int32_t icu_tokenizer_next_token(struct icu_tokenizer * tokenizer, 
                         struct icu_buf_utf16 * tkn16, 
                         UErrorCode *status)
{
    int32_t tkn_start = 0;
    int32_t tkn_end = 0;
    int32_t tkn_len = 0;
    

    if (!tokenizer || !tokenizer->bi
        || !tokenizer->buf16 || !tokenizer->buf16->utf16_len)
        return 0;

    // never change tokenizer->buf16 and keep always invariant
    // 0 <= tokenizer->token_start 
    //   <= tokenizer->token_end 
    //   <= tokenizer->buf16->utf16_len
    // returns length of token

    if (0 == tokenizer->token_end) // first call
        tkn_start = ubrk_first(tokenizer->bi);
    else //successive calls
        tkn_start = tokenizer->token_end;

    // get next position
    tkn_end = ubrk_next(tokenizer->bi);

    // repairing invariant at end of ubrk, which is UBRK_DONE = -1 
    if (UBRK_DONE == tkn_end)
        tkn_end = tokenizer->buf16->utf16_len;

    // copy out if everything is well
    if(U_FAILURE(*status))
        return 0;        
    
    // everything OK, now update internal state
    tkn_len = tkn_end - tkn_start;

    if (0 < tkn_len){
        tokenizer->token_count++;
        tokenizer->token_id++;
    } else {
        tokenizer->token_id = 0;    
    }
    tokenizer->token_start = tkn_start;
    tokenizer->token_end = tkn_end;
    

    // copying into token buffer if it exists 
    if (tkn16){
        if (tkn16->utf16_cap < tkn_len)
            icu_buf_utf16_resize(tkn16, (size_t) tkn_len * 2);

        u_strncpy(tkn16->utf16, &(tokenizer->buf16->utf16)[tkn_start], 
                  tkn_len);

        tkn16->utf16_len = tkn_len;
    }

    return tkn_len;
}


int32_t icu_tokenizer_token_id(struct icu_tokenizer * tokenizer)
{
    return tokenizer->token_id;
};

int32_t icu_tokenizer_token_start(struct icu_tokenizer * tokenizer)
{
    return tokenizer->token_start;
};

int32_t icu_tokenizer_token_end(struct icu_tokenizer * tokenizer)
{
    return tokenizer->token_end;
};

int32_t icu_tokenizer_token_length(struct icu_tokenizer * tokenizer)
{
    return (tokenizer->token_end - tokenizer->token_start);
};

int32_t icu_tokenizer_token_count(struct icu_tokenizer * tokenizer)
{
    return tokenizer->token_count;
};



struct icu_normalizer * icu_normalizer_create(const char *rules, char action,
                                              UErrorCode *status)
{

    struct icu_normalizer * normalizer
        = (struct icu_normalizer *) malloc(sizeof(struct icu_normalizer));

    normalizer->action = action;
    normalizer->trans = 0;
    normalizer->rules16 =  icu_buf_utf16_create(0);
    icu_utf16_from_utf8_cstr(normalizer->rules16, rules, status);
     
    switch(normalizer->action) {    
    case 'f':
        normalizer->trans
            = utrans_openU(normalizer->rules16->utf16, 
                           normalizer->rules16->utf16_len,
                           UTRANS_FORWARD,
                           0, 0, 
                           normalizer->parse_error, status);
        // yaz_log(YLOG_LOG, "utrans_open %p", normalizer->trans);
        break;
    case 'r':
        normalizer->trans
            = utrans_openU(normalizer->rules16->utf16,
                           normalizer->rules16->utf16_len,
                           UTRANS_REVERSE ,
                           0, 0,
                           normalizer->parse_error, status);
        // yaz_log(YLOG_LOG, "utrans_open %p", normalizer->trans);
        break;
    default:
        *status = U_UNSUPPORTED_ERROR;
        return 0;
        break;
    }
    
    if (U_SUCCESS(*status))
        return normalizer;

    // freeing if failed
    icu_normalizer_destroy(normalizer);
    return 0;
};


void icu_normalizer_destroy(struct icu_normalizer * normalizer){
    if (normalizer) {
        if (normalizer->rules16) 
            icu_buf_utf16_destroy(normalizer->rules16);
        if (normalizer->trans)
        {
            // yaz_log(YLOG_LOG, "utrans_close %p", normalizer->trans);
            utrans_close(normalizer->trans);
        }
        free(normalizer);
    }
};



int icu_normalizer_normalize(struct icu_normalizer * normalizer,
                             struct icu_buf_utf16 * dest16,
                             struct icu_buf_utf16 * src16,
                             UErrorCode *status)
{
    if (!normalizer || !normalizer->trans || !src16 || !dest16)
        return 0;

    if (!icu_buf_utf16_copy(dest16, src16))
        return 0;

    utrans_transUChars (normalizer->trans, 
                        dest16->utf16, &(dest16->utf16_len),
                        dest16->utf16_cap,
                        0, &(src16->utf16_len), status);

    if (U_FAILURE(*status)){
        dest16->utf16[0] = (UChar) 0;
        dest16->utf16_len = 0;
    }
    
    return dest16->utf16_len;
}




struct icu_chain_step * icu_chain_step_create(struct icu_chain * chain,
                                              enum icu_chain_step_type type,
                                              const uint8_t * rule,
                                              struct icu_buf_utf16 * buf16,
                                              UErrorCode *status)
{
    struct icu_chain_step * step = 0;
    
    if(!chain || !type || !rule)
        return 0;

    step = (struct icu_chain_step *) malloc(sizeof(struct icu_chain_step));

    step->type = type;
    step->more_tokens = 0;
    step->need_new_token = 1;

    if (buf16)
        step->buf16 = buf16;
    else
        step->buf16 = 0;

    // create auxilary objects
    switch(step->type) {
    case ICU_chain_step_type_display:
        break;
    case ICU_chain_step_type_norm:
        break;
    case ICU_chain_step_type_sort:
        break;
    case ICU_chain_step_type_casemap:
        step->u.casemap = icu_casemap_create((char *) chain->locale, 
                                             (char) rule[0], status);
        break;
    case ICU_chain_step_type_normalize:
        step->u.normalizer = icu_normalizer_create((char *) rule, 'f', status);
        break;
    case ICU_chain_step_type_tokenize:
        step->u.tokenizer = icu_tokenizer_create((char *) chain->locale, 
                                                 (char) rule[0], status);
        break;
    default:
        break;
    }

    return step;
};


void icu_chain_step_destroy(struct icu_chain_step * step){
    
    if (!step)
        return;

    icu_chain_step_destroy(step->previous);

    switch(step->type) {
    case ICU_chain_step_type_display:
        break;
    case ICU_chain_step_type_norm:
        break;
    case ICU_chain_step_type_sort:
        break;
    case ICU_chain_step_type_casemap:
        icu_casemap_destroy(step->u.casemap);
        icu_buf_utf16_destroy(step->buf16);
        break;
    case ICU_chain_step_type_normalize:
        icu_normalizer_destroy(step->u.normalizer);
        icu_buf_utf16_destroy(step->buf16);
        break;
    case ICU_chain_step_type_tokenize:
        icu_tokenizer_destroy(step->u.tokenizer);
        icu_buf_utf16_destroy(step->buf16);
        break;
    default:
        break;
    }
    free(step);
};



struct icu_chain * icu_chain_create(const uint8_t * identifier,
                                    const uint8_t * locale)
{

    struct icu_chain * chain 
        = (struct icu_chain *) malloc(sizeof(struct icu_chain));

    strncpy((char *) chain->identifier, (const char *) identifier, 128);
    chain->identifier[128 - 1] = '\0';
    strncpy((char *) chain->locale, (const char *) locale, 16);    
    chain->locale[16 - 1] = '\0';

    chain->token_count = 0;

    chain->display8 = icu_buf_utf8_create(0);
    chain->norm8 = icu_buf_utf8_create(0);
    chain->sort8 = icu_buf_utf8_create(0);

    chain->src16 = icu_buf_utf16_create(0);

    chain->steps = 0;

    return chain;
};


void icu_chain_destroy(struct icu_chain * chain)
{
    if (chain){
        icu_buf_utf8_destroy(chain->display8);
        icu_buf_utf8_destroy(chain->norm8);
        icu_buf_utf8_destroy(chain->sort8);
        
        icu_buf_utf16_destroy(chain->src16);
    
        icu_chain_step_destroy(chain->steps);
        free(chain);
    }
};



struct icu_chain * icu_chain_xml_config(xmlNode *xml_node, 
                                        UErrorCode * status){

    xmlNode *node = 0;
    struct icu_chain * chain = 0;
   
    if (!xml_node 
        ||xml_node->type != XML_ELEMENT_NODE 
        || strcmp((const char *) xml_node->name, "icu_chain"))

        return 0;
    
    xmlChar *xml_id = xmlGetProp(xml_node, (xmlChar *) "id");
    xmlChar *xml_locale = xmlGetProp(xml_node, (xmlChar *) "locale");

    if (!xml_id || !strlen((const char *) xml_id) 
        || !xml_locale || !strlen((const char *) xml_locale))
        return 0;

    chain = icu_chain_create((const uint8_t *) xml_id, 
                             (const uint8_t *) xml_locale);
    
    xmlFree(xml_id);
    xmlFree(xml_locale);
    if (!chain)
        return 0;
        
    for (node = xml_node->children; node; node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE)
            continue;

        xmlChar *xml_rule = xmlGetProp(node, (xmlChar *) "rule");
        struct icu_chain_step * step = 0;

        if (!strcmp((const char *) node->name, 
                    (const char *) "casemap")){
            step = icu_chain_insert_step(chain, ICU_chain_step_type_casemap, 
                                         (const uint8_t *) xml_rule, status);
        }
        else if (!strcmp((const char *) node->name,
                         (const char *) "normalize")){
            step = icu_chain_insert_step(chain, ICU_chain_step_type_normalize, 
                                         (const uint8_t *) xml_rule, status);
        }
        else if (!strcmp((const char *) node->name,
                         (const char *) "tokenize")){
            step = icu_chain_insert_step(chain, ICU_chain_step_type_tokenize, 
                                         (const uint8_t *) xml_rule, status);
        }
        else if (!strcmp((const char *) node->name,
                         (const char *) "display")){
            step = icu_chain_insert_step(chain, ICU_chain_step_type_display, 
                                         (const uint8_t *) "", status);
        }
        else if (!strcmp((const char *) node->name,
                         (const char *) "normal")){
            step = icu_chain_insert_step(chain, ICU_chain_step_type_norm, 
                                         (const uint8_t *) "", status);
        }
        else if (!strcmp((const char *) node->name,
                         (const char *) "sort")){
            step = icu_chain_insert_step(chain, ICU_chain_step_type_sort, 
                                         (const uint8_t *) "", status);
        }

        xmlFree(xml_rule);
        if (!step || U_FAILURE(*status)){
            icu_chain_destroy(chain);
            return 0;
        }
        

    }

    return chain;
};



struct icu_chain_step * icu_chain_insert_step(struct icu_chain * chain,
                                              enum icu_chain_step_type type,
                                              const uint8_t * rule,
                                              UErrorCode *status)
{    
    struct icu_chain_step * step = 0;
    struct icu_buf_utf16 * src16 = 0;
    struct icu_buf_utf16 * buf16 = 0;

    if (!chain || !type || !rule)
        return 0;

    // assign utf16 src buffers as needed 
    if (chain->steps && chain->steps->buf16)
        src16 = chain->steps->buf16;
    else if (chain->src16)
        src16 = chain->src16;
    else
        return 0;

    
    // create utf16 destination buffers as needed, or
    switch(type) {
    case ICU_chain_step_type_display:
        buf16 = src16;
        break;
    case ICU_chain_step_type_norm:
        buf16 = src16;
        break;
    case ICU_chain_step_type_sort:
        buf16 = src16;
        break;
    case ICU_chain_step_type_casemap:
        buf16 = icu_buf_utf16_create(0);
        break;
    case ICU_chain_step_type_normalize:
        buf16 = icu_buf_utf16_create(0);
        break;
    case ICU_chain_step_type_tokenize:
        buf16 = icu_buf_utf16_create(0);
        break;
    default:
        break;
    }

    // create actual chain step with this buffer
    step = icu_chain_step_create(chain, type, rule, buf16, status);

    step->previous = chain->steps;
    chain->steps = step;

    return step;
};


int icu_chain_step_next_token(struct icu_chain * chain,
                              struct icu_chain_step * step,
                              UErrorCode *status)
{
    struct icu_buf_utf16 * src16 = 0;
    
    //printf("icu_chain_step_next_token %d\n", (int) step);

    if (!chain || !chain->src16 || !step || !step->more_tokens)
        return 0;

    // assign utf16 src buffers as neeed, advance in previous steps
    // tokens until non-zero token met, and setting stop condition
    if (step->previous){
        src16 = step->previous->buf16;
        if (step->need_new_token)
            //while (step->more_tokens &&  !src16->utf16_len)
                step->more_tokens 
                    = icu_chain_step_next_token(chain, step->previous, status);
    }
    else { // first step can only work once on chain->src16 input buffer
        src16 = chain->src16;
        step->more_tokens = 1;
    }

    // stop if nothing to process 
    // i.e new token source was not properly assigned
    if (!step->more_tokens || !src16) // || !src16->utf16_len 
        return 0;

    //printf("icu_chain_step_next_token %d working\n", (int) step);


    // perform the work, eventually put this steps output in 
    // step->buf16 or the chains UTF8 output buffers 
    switch(step->type) {
    case ICU_chain_step_type_display:
        icu_utf16_to_utf8(chain->display8, src16, status);
        break;
    case ICU_chain_step_type_norm:
        icu_utf16_to_utf8(chain->norm8, src16, status);
        break;
    case ICU_chain_step_type_sort:
        icu_utf16_to_utf8(chain->sort8, src16, status);
        break;
    case ICU_chain_step_type_casemap:
        icu_casemap_casemap(step->u.casemap,
                            step->buf16, src16, status);
        break;
    case ICU_chain_step_type_normalize:
        icu_normalizer_normalize(step->u.normalizer,
                                 step->buf16, src16, status);
        break;
    case ICU_chain_step_type_tokenize:
        // attach to new src16 token only first time during splitting
        if (step->need_new_token){
            icu_tokenizer_attach(step->u.tokenizer, src16, status);
            step->need_new_token = 0;
        }
        // splitting one src16 token into multiple buf16 tokens
        step->more_tokens
            = icu_tokenizer_next_token(step->u.tokenizer,
                                       step->buf16, status);
        // make sure to get new previous token if this one had been used up
        if (step->previous && !step->more_tokens){
            if (icu_chain_step_next_token(chain, step->previous, status)){
                icu_tokenizer_attach(step->u.tokenizer, src16, status);
                step->need_new_token = 0;   
                step->more_tokens
                    = icu_tokenizer_next_token(step->u.tokenizer,
                                               step->buf16, status);
            }
        }
        if (0 == step->more_tokens)
            return 0;
        break;
    default:
        return 0;
        break;
    }



    // stop further token processing if last step and 
    // new tokens are needed from previous (non-existing) step
    if (!step->previous && step->need_new_token)
        step->more_tokens = 0;

    //printf("%d %d %d\n", 
    //       step->more_tokens, src16->utf16_len, step->buf16->utf16_len);


    if (U_FAILURE(*status))
        return 0;

    return 1;
};



int icu_chain_assign_cstr(struct icu_chain * chain,
                          const char * src8cstr, 
                          UErrorCode *status)
{
    struct icu_chain_step * stp = 0; 

    if (!chain || !src8cstr)
        return 0;

    stp = chain->steps;
    
    // clear token count
    chain->token_count = 0;

    // clear all steps stop states

    while (stp){
        stp->more_tokens = 1;
        stp = stp->previous;
    }
    
    // finally convert UTF8 to UTF16 string
    icu_utf16_from_utf8_cstr(chain->src16, src8cstr, status);
            
    if (U_FAILURE(*status))
        return 0;

    return 1;
};



int icu_chain_next_token(struct icu_chain * chain,
                         UErrorCode *status)
{
    int success = 0;
    
    if (!chain || !chain->steps)
        return 0;

    success = icu_chain_step_next_token(chain, chain->steps, status);
    
    if (success){
        chain->token_count++;
        return chain->token_count;
    }

    return 0;
};

int icu_chain_get_token_count(struct icu_chain * chain)
{
    if (!chain)
        return 0;
    
    return chain->token_count;
};



const char * icu_chain_get_display(struct icu_chain * chain)
{
    if (chain->display8)
        return (const char *) chain->display8->utf8;
    
    return 0;
};

const char * icu_chain_get_norm(struct icu_chain * chain)
{
    if (chain->norm8)
        return (const char *) chain->norm8->utf8;
    
    return 0;
};

const char * icu_chain_get_sort(struct icu_chain * chain)
{
    if (chain->sort8)
        return (const char *) chain->sort8->utf8;
    
    return 0;
};




#endif // HAVE_ICU    




/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
