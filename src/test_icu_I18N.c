/* $Id: test_icu_I18N.c,v 1.25 2007-05-24 10:35:21 adam Exp $
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

// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8
 

#if HAVE_CONFIG_H
#include "cconfig.h"
#endif

#define USE_TIMING 0
#if USE_TIMING
#include <yaz/timing.h>
#endif

#include <yaz/test.h>



#ifdef HAVE_ICU
#include "icu_I18N.h"

#include <string.h>
#include <stdlib.h>

//#include <unicode/ustring.h>  
// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8


#define MAX_KEY_SIZE 256
struct icu_termmap
{
    uint8_t sort_key[MAX_KEY_SIZE]; // standard C string '\0' terminated
    char disp_term[MAX_KEY_SIZE];  // standard C utf-8 string
};



int icu_termmap_cmp(const void *vp1, const void *vp2)
{
    struct icu_termmap *itmp1 = *(struct icu_termmap **) vp1;
    struct icu_termmap *itmp2 = *(struct icu_termmap **) vp2;

    int cmp = 0;
    
    cmp = strcmp((const char *)itmp1->sort_key, 
                 (const char *)itmp2->sort_key);
    return cmp;
};




int test_icu_casemap(const char * locale, char action,
                     const char * src8cstr, const char * chk8cstr)
{
    int success = 0;
    UErrorCode status = U_ZERO_ERROR;

    struct icu_buf_utf8 * src8 = icu_buf_utf8_create(0);
    struct icu_buf_utf8 * dest8 = icu_buf_utf8_create(0);
    struct icu_buf_utf16 * src16 = icu_buf_utf16_create(0);
    struct icu_buf_utf16 * dest16 = icu_buf_utf16_create(0);


    int src8cstr_len = strlen(src8cstr);
    int chk8cstr_len = strlen(chk8cstr);

    // converting to UTF16
    icu_utf16_from_utf8_cstr(src16, src8cstr, &status);

    // perform case mapping
    icu_utf16_casemap(dest16, src16, locale, action, &status);
  
    // converting to UTF8
    icu_utf16_to_utf8(dest8, dest16, &status);
      

  
    // determine success
    if (dest8->utf8 
        && (dest8->utf8_len == strlen(chk8cstr))
        && !strcmp(chk8cstr, (const char *) dest8->utf8))
        success = 1;
    else
        success = 0;

    // report failures
    if (!success){
        printf("\nERROR\n");
        printf("original string:   '%s' (%d)\n", src8cstr, src8cstr_len);
        printf("icu_casemap '%s:%c' '%s' (%d)\n", 
               locale, action, dest8->utf8, dest8->utf8_len);
        printf("expected string:   '%s' (%d)\n", chk8cstr, chk8cstr_len);
    }
  
    // clean the buffers  
    icu_buf_utf8_destroy(src8);
    icu_buf_utf8_destroy(dest8);
    icu_buf_utf16_destroy(src16);
    icu_buf_utf16_destroy(dest16);
  
  
    return success;
}



// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

void test_icu_I18N_casemap(int argc, char **argv)
{

    // Locale 'en'

    // sucessful tests
    YAZ_CHECK(test_icu_casemap("en", 'l',
                               "A ReD fOx hunTS sQUirriLs", 
                               "a red fox hunts squirrils"));
    
    YAZ_CHECK(test_icu_casemap("en", 'u',
                               "A ReD fOx hunTS sQUirriLs", 
                               "A RED FOX HUNTS SQUIRRILS"));
    
    YAZ_CHECK(test_icu_casemap("en", 'f',
                               "A ReD fOx hunTS sQUirriLs", 
                               "a red fox hunts squirrils"));
    
    YAZ_CHECK(test_icu_casemap("en", 't',
                               "A ReD fOx hunTS sQUirriLs", 
                               "A Red Fox Hunts Squirrils"));
    

    // Locale 'da'

    // sucess expected    
    YAZ_CHECK(test_icu_casemap("da", 'l',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "åh æble, øs fløde i åen efter blåbærgrøden"));

    YAZ_CHECK(test_icu_casemap("da", 'u',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "ÅH ÆBLE, ØS FLØDE I ÅEN EFTER BLÅBÆRGRØDEN"));

    YAZ_CHECK(test_icu_casemap("da", 'f',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "åh æble, øs fløde i åen efter blåbærgrøden"));

    YAZ_CHECK(test_icu_casemap("da", 't',
                               "åh ÆbLE, øs fLØde i Åen efter bLåBærGRødeN", 
                               "Åh Æble, Øs Fløde I Åen Efter Blåbærgrøden"));

    // Locale 'de'

    // sucess expected    
    YAZ_CHECK(test_icu_casemap("de", 'l',
                               "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                               "zwölf ärgerliche würste rollen über die straße"));

    YAZ_CHECK(test_icu_casemap("de", 'u',
                               "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                               "ZWÖLF ÄRGERLICHE WÜRSTE ROLLEN ÜBER DIE STRASSE"));

    YAZ_CHECK(test_icu_casemap("de", 'f',
                               "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                               "zwölf ärgerliche würste rollen über die strasse"));

    YAZ_CHECK(test_icu_casemap("de", 't',
                               "zWÖlf ärgerliche Würste rollen ÜBer die StRAße",
                               "Zwölf Ärgerliche Würste Rollen Über Die Straße"));

}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

int test_icu_sortmap(const char * locale, int src_list_len,
                     const char ** src_list, const char ** chk_list)
{
    int success = 1;

    UErrorCode status = U_ZERO_ERROR;

    struct icu_buf_utf8 * buf8 = icu_buf_utf8_create(0);
    struct icu_buf_utf16 * buf16 = icu_buf_utf16_create(0);

    int i;

    struct icu_termmap * list[src_list_len];

    UCollator *coll = ucol_open(locale, &status); 
    icu_check_status(status);

    if(U_FAILURE(status))
        return 0;

    // assigning display terms and sort keys using buf 8 and buf16
    for( i = 0; i < src_list_len; i++) 
        {

            list[i] = (struct icu_termmap *) malloc(sizeof(struct icu_termmap));

            // copy display term
            strcpy(list[i]->disp_term, src_list[i]);    

            // transforming to UTF16
            icu_utf16_from_utf8_cstr(buf16, list[i]->disp_term, &status);
            icu_check_status(status);

            // computing sortkeys
            icu_sortkey8_from_utf16(coll, buf8, buf16, &status);
            icu_check_status(status);
    
            // assigning sortkeys
            memcpy(list[i]->sort_key, buf8->utf8, buf8->utf8_len);    
            //strncpy(list[i]->sort_key, buf8->utf8, buf8->utf8_len);    
            //strcpy((char *) list[i]->sort_key, (const char *) buf8->utf8);
        } 


    // do the sorting
    qsort(list, src_list_len, 
          sizeof(struct icu_termmap *), icu_termmap_cmp);

    // checking correct sorting
    for (i = 0; i < src_list_len; i++){
        if (0 != strcmp(list[i]->disp_term, chk_list[i])){
            success = 0;
        }
    }

    if(!success){
        printf("\nERROR\n"); 
        printf("Input str: '%s' : ", locale); 
        for (i = 0; i < src_list_len; i++) {
            printf(" '%s'", list[i]->disp_term); 
        }
        printf("\n");
        printf("ICU sort:  '%s' : ", locale); 
        for (i = 0; i < src_list_len; i++) {
            printf(" '%s'", list[i]->disp_term); 
            //printf("(%d|%d)", list[i]->sort_key[0],list[i]->sort_key[1]); 
        }
        printf("\n"); 
        printf("Expected:  '%s' : ", locale); 
        for (i = 0; i < src_list_len; i++) {
            printf(" '%s'", chk_list[i]); 
        }
        printf("\n"); 
    }
  


    for( i = 0; i < src_list_len; i++)
        free(list[i]);
        
    
    ucol_close(coll);

    icu_buf_utf8_destroy(buf8);
    icu_buf_utf16_destroy(buf16);

    return success;  
}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

void test_icu_I18N_sortmap(int argc, char **argv)
{

    // sucessful tests
    size_t en_1_len = 6;
    const char * en_1_src[6] = {"z", "K", "a", "A", "Z", "k"};
    const char * en_1_cck[6] = {"a", "A", "k", "K", "z", "Z"};
    YAZ_CHECK(test_icu_sortmap("en", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(test_icu_sortmap("en_AU", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(test_icu_sortmap("en_CA", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(test_icu_sortmap("en_GB", en_1_len, en_1_src, en_1_cck));
    YAZ_CHECK(test_icu_sortmap("en_US", en_1_len, en_1_src, en_1_cck));
    
    // sucessful tests 
    size_t da_1_len = 6;
    const char * da_1_src[6] = {"z", "å", "o", "æ", "a", "ø"};
    const char * da_1_cck[6] = {"a", "o", "z", "æ", "ø", "å"};
    YAZ_CHECK(test_icu_sortmap("da", da_1_len, da_1_src, da_1_cck));
    YAZ_CHECK(test_icu_sortmap("da_DK", da_1_len, da_1_src, da_1_cck));
    
    // sucessful tests
    size_t de_1_len = 9;
    const char * de_1_src[9] = {"u", "ä", "o", "t", "s", "ß", "ü", "ö", "a"};
    const char * de_1_cck[9] = {"a","ä", "o", "ö", "s", "ß", "t", "u", "ü"};
    YAZ_CHECK(test_icu_sortmap("de", de_1_len, de_1_src, de_1_cck));
    YAZ_CHECK(test_icu_sortmap("de_AT", de_1_len, de_1_src, de_1_cck));
    YAZ_CHECK(test_icu_sortmap("de_DE", de_1_len, de_1_src, de_1_cck));
    
}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8




int test_icu_normalizer(const char * rules8cstr,
                            const char * src8cstr,
                            const char * chk8cstr)
{
    int success = 0;
    
    UErrorCode status = U_ZERO_ERROR;

    struct icu_buf_utf16 * src16 = icu_buf_utf16_create(0);
    struct icu_buf_utf16 * dest16 = icu_buf_utf16_create(0);
    struct icu_buf_utf8 * dest8 = icu_buf_utf8_create(0);
    struct icu_normalizer * normalizer
        = icu_normalizer_create(rules8cstr, 'f', &status);
    icu_check_status(status);
    
    icu_utf16_from_utf8_cstr(src16, src8cstr, &status);
    icu_check_status(status);

    icu_normalizer_normalize(normalizer, dest16, src16, &status);
    icu_check_status(status);

    icu_utf16_to_utf8(dest8, dest16, &status);
    icu_check_status(status);


    if(!strcmp((const char *) dest8->utf8, 
               (const char *) chk8cstr))
        success = 1;
    else {
        success = 0;
        printf("Normalization\n");
        printf("Rules:      '%s'\n", rules8cstr);
        printf("Input:      '%s'\n", src8cstr);
        printf("Normalized: '%s'\n", dest8->utf8);
        printf("Expected:   '%s'\n", chk8cstr);
    }
    

    icu_normalizer_destroy(normalizer);
    icu_buf_utf16_destroy(src16);
    icu_buf_utf16_destroy(dest16);
    icu_buf_utf8_destroy(dest8);

    return success;
};


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

void test_icu_I18N_normalizer(int argc, char **argv)
{

    YAZ_CHECK(test_icu_normalizer("[:Punctuation:] Any-Remove",
                                  "Don't shoot!",
                                  "Dont shoot"));
    
    YAZ_CHECK(test_icu_normalizer("[:Control:] Any-Remove",
                                  "Don't\n shoot!",
                                  "Don't shoot!"));

    YAZ_CHECK(test_icu_normalizer("[:Decimal_Number:] Any-Remove",
                                  "This is 4 you!",
                                  "This is  you!"));

    YAZ_CHECK(test_icu_normalizer("Lower; [:^Letter:] Remove",
                                  "Don't shoot!",
                                  "dontshoot"));
    
    YAZ_CHECK(test_icu_normalizer("[:^Number:] Remove",
                                  "Monday 15th of April",
                                  "15"));

    YAZ_CHECK(test_icu_normalizer("Lower;"
                                  "[[:WhiteSpace:][:Punctuation:]] Remove",
                                  " word4you? ",
                                  "word4you"));


    YAZ_CHECK(test_icu_normalizer("NFD; [:Nonspacing Mark:] Remove; NFC",
                                  "à côté de l'alcôve ovoïde",
                                  "a cote de l'alcove ovoide"));

}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

int test_icu_tokenizer(const char * locale, char action,
                     const char * src8cstr, int count)
{
    int success = 1;

    UErrorCode status = U_ZERO_ERROR;
    struct icu_buf_utf16 * src16 = icu_buf_utf16_create(0);
    struct icu_buf_utf16 * tkn16 = icu_buf_utf16_create(0);
    struct icu_buf_utf8 * tkn8 = icu_buf_utf8_create(0);

    //printf("Input:  '%s'\n", src8cstr);

    // transforming to UTF16
    icu_utf16_from_utf8_cstr(src16, src8cstr, &status);
    icu_check_status(status);

    // set up tokenizer
    struct icu_tokenizer * tokenizer 
        = icu_tokenizer_create(locale, action, &status);
    icu_check_status(status);
    YAZ_CHECK(tokenizer);

    // attach text buffer to tokenizer
    icu_tokenizer_attach(tokenizer, src16, &status);    
    icu_check_status(status);
    YAZ_CHECK(tokenizer->bi);

    // perform work on tokens
    //printf("Tokens: ");
    while(icu_tokenizer_next_token(tokenizer, tkn16, &status)){
        icu_check_status(status);

        // converting to UTF8
        icu_utf16_to_utf8(tkn8, tkn16, &status);

        //printf("token %d %d %d %d '%s'\n",
        //       
        //       icu_tokenizer_token_start(tokenizer),
        //       icu_tokenizer_token_end(tokenizer),
        //       icu_tokenizer_token_length(tokenizer),
        //       tkn8->utf8);
    }

    if (count != icu_tokenizer_token_count(tokenizer)){
        success = 0;
        printf("\nTokenizer '%s:%c' Error: \n", locale, action);
        printf("Input:  '%s'\n", src8cstr);
        printf("Tokens: %d", icu_tokenizer_token_count(tokenizer));
        printf(", expected: %d\n", count);
    }

    icu_tokenizer_destroy(tokenizer);
    icu_buf_utf16_destroy(src16);
    icu_buf_utf16_destroy(tkn16);
    icu_buf_utf8_destroy(tkn8);
        
    return success;
}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

void test_icu_I18N_tokenizer(int argc, char **argv)
{


    const char * en_str 
        = "O Romeo, Romeo! wherefore art thou Romeo?";
    
    YAZ_CHECK(test_icu_tokenizer("en", 's', en_str, 2));
    YAZ_CHECK(test_icu_tokenizer("en", 'l', en_str, 7));
    YAZ_CHECK(test_icu_tokenizer("en", 'w', en_str, 16));
    YAZ_CHECK(test_icu_tokenizer("en", 'c', en_str, 41));



    const char * da_str 
        = "Blåbærtærte. Denne kage stammer fra Finland. "
        "Den er med blåbær, men alle sommerens forskellige bær kan bruges.";
    
    YAZ_CHECK(test_icu_tokenizer("da", 's', da_str, 3));
    YAZ_CHECK(test_icu_tokenizer("dar", 'l', da_str, 17));
    YAZ_CHECK(test_icu_tokenizer("da", 'w', da_str, 37));
    YAZ_CHECK(test_icu_tokenizer("da", 'c', da_str, 110));

}


void test_icu_I18N_chain(int argc, char **argv)
{
    const char * en_str 
        = "O Romeo, Romeo! wherefore art thou\t Romeo?";

    printf("ICU chain:\ninput: '%s'\n", en_str);

    UErrorCode status = U_ZERO_ERROR;
    //struct icu_chain_step * step = 0;
    struct icu_chain * chain = 0;
    

    const char * xml_str = "<icu_chain id=\"en:word\" locale=\"en\">"
        "<normalize rule=\"[:Control:] Any-Remove\"/>"
        "<tokenize rule=\"l\"/>"
        "<normalize rule=\"[[:WhiteSpace:][:Punctuation:]] Remove\"/>"
        "<display/>"
        "<casemap rule=\"l\"/>"
        "<normal/>"
        "<sort/>"
        "</icu_chain>";

    
    xmlDoc *doc = xmlParseMemory(xml_str, strlen(xml_str));
    xmlNode *xml_node = xmlDocGetRootElement(doc);
    YAZ_CHECK(xml_node);


    chain = icu_chain_xml_config(xml_node, &status);

#if 0
    chain  = icu_chain_create((uint8_t *) "en:word", (uint8_t *) "en");
    step = icu_chain_insert_step(chain, ICU_chain_step_type_normalize,
                                 (const uint8_t *) "[:Control:] Any-Remove",
                                 &status);
    step = icu_chain_insert_step(chain, ICU_chain_step_type_tokenize,
                                 (const uint8_t *) "s",
                                 &status);
    step = icu_chain_insert_step(chain, ICU_chain_step_type_tokenize,
                                 (const uint8_t *) "l",
                                 &status);
    step = icu_chain_insert_step(chain, ICU_chain_step_type_normalize,
                                 (const uint8_t *)
                                 "[[:WhiteSpace:][:Punctuation:]] Any-Remove",
                                 &status);
    step = icu_chain_insert_step(chain, ICU_chain_step_type_display,
                                 (const uint8_t *)"",
                                 &status);
/*     step = icu_chain_insert_step(chain, ICU_chain_step_type_normalize, */
/*                                  (const uint8_t *) "Lower", */
/*                                  &status); */
    step = icu_chain_insert_step(chain, ICU_chain_step_type_casemap,
                                 (const uint8_t *) "l",
                                 &status);
    step = icu_chain_insert_step(chain, ICU_chain_step_type_norm,
                                 (const uint8_t *)"",
                                 &status);
/*     step = icu_chain_insert_step(chain, ICU_chain_step_type_sort, */
/*                                  (const uint8_t *)"", */
/*                                  &status); */
    
#endif

    xmlFreeDoc(doc);
    YAZ_CHECK(chain);

    YAZ_CHECK(icu_chain_assign_cstr(chain, en_str, &status));

    while (icu_chain_next_token(chain, &status)){
        printf("%d '%s' '%s'\n",
               icu_chain_get_token_count(chain),
               icu_chain_get_norm(chain),
               icu_chain_get_display(chain));
    }

    YAZ_CHECK_EQ(icu_chain_get_token_count(chain), 7);


    YAZ_CHECK(icu_chain_assign_cstr(chain, "what is this?", &status));

    while (icu_chain_next_token(chain, &status)){
        printf("%d '%s' '%s'\n",
               icu_chain_get_token_count(chain),
               icu_chain_get_norm(chain),
               icu_chain_get_display(chain));
    }


    YAZ_CHECK_EQ(icu_chain_get_token_count(chain), 3);

    icu_chain_destroy(chain);
}


void test_bug_1140(void)
{
    const char * en_str 
        = "O Romeo, Romeo! wherefore art thou\t Romeo?";

    printf("ICU chain:\ninput: '%s'\n", en_str);

    UErrorCode status = U_ZERO_ERROR;
    //struct icu_chain_step * step = 0;
    struct icu_chain * chain = 0;
    
    const char * xml_str = "<icu_chain id=\"en:word\" locale=\"en\">"

        /* if the first rule is normalize instead. Then it works */
#if 0
        "<normalize rule=\"[:Control:] Any-Remove\"/>"
#endif
        "<tokenize rule=\"l\"/>"
        "<normalize rule=\"[[:WhiteSpace:][:Punctuation:]] Remove\"/>"
        "<display/>"
        "<casemap rule=\"l\"/>"
        "<normal/>"
        "<sort/>"
        "</icu_chain>";

    
    xmlDoc *doc = xmlParseMemory(xml_str, strlen(xml_str));
    xmlNode *xml_node = xmlDocGetRootElement(doc);
    YAZ_CHECK(xml_node);

    chain = icu_chain_xml_config(xml_node, &status);

    xmlFreeDoc(doc);
    YAZ_CHECK(chain);
    
    YAZ_CHECK(icu_chain_assign_cstr(
                  chain,  "O Romeo, Romeo! wherefore art thou\t Romeo?",
                  &status));

    while (icu_chain_next_token(chain, &status))
        ;

    YAZ_CHECK_EQ(icu_chain_get_token_count(chain), 7);

    YAZ_CHECK(icu_chain_assign_cstr(chain, "what is this?", &status));

    while (icu_chain_next_token(chain, &status)){
        printf("%d '%s' '%s'\n",
               icu_chain_get_token_count(chain),
               icu_chain_get_norm(chain),
               icu_chain_get_display(chain));
    }

    /* we expect 'what' 'is' 'this', i.e. 3 tokens */
    YAZ_CHECK_EQ(icu_chain_get_token_count(chain), 3);

    icu_chain_destroy(chain);
}

#endif // HAVE_ICU

// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8

int main(int argc, char **argv)
{

    YAZ_CHECK_INIT(argc, argv); 
    YAZ_CHECK_LOG();

#ifdef HAVE_ICU

    //test_icu_I18N_casemap_failures(argc, argv);
    test_icu_I18N_casemap(argc, argv);
    test_icu_I18N_sortmap(argc, argv);
    test_icu_I18N_normalizer(argc, argv);
    test_icu_I18N_tokenizer(argc, argv);
    test_icu_I18N_chain(argc, argv);
#if 0
    /* currently fails */
    test_bug_1140();
#endif

#else // HAVE_ICU

    printf("ICU unit tests omitted.\n"
           "Please install libicu36-dev and icu-doc or similar\n");
    YAZ_CHECK(0 == 0);

#endif // HAVE_ICU
   
    YAZ_CHECK_TERM;
}


// DO NOT EDIT THIS FILE IF YOUR EDITOR DOES NOT SUPPORT UTF-8



/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
