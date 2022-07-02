

/*
 Copyright (c) 2009 Dave Gamble
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

/* cJSON */
/* JSON parser in C. */
#include <math.h>

#include "cJSON.h"
#include "smartusd_cJSON.h"
#include "cJSON.c"
//#include <util.h>

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131E-16
#endif

#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)

static const char *ep;

long _stripwhite(char *buf,int accept)
{
    int32_t i,j,c;
    if ( buf == 0 || buf[0] == 0 )
        return(0);
    for (i=j=0; buf[i]!=0; i++)
    {
        buf[j] = c = buf[i];
        if ( c == accept || (c != ' ' && c != '\n' && c != '\r' && c != '\t' && c != '\b') )
            j++;
    }
    buf[j] = 0;
    return(j);
}

char *clonestr(char *str)
{
    char *clone;
    if ( str == 0 || str[0] == 0 )
    {
        //LogPrintf("warning cloning nullstr.%p\n",str);
       //#ifdef __APPLE__
       //        while ( 1 ) sleep(1);
       //#endif
        str = (char *)"<nullstr>";
    }
    clone = (char *)malloc(strlen(str)+16);
    strcpy(clone,str);
    return(clone);
}

int32_t _unhex(char c)
{
    if ( c >= '0' && c <= '9' )
        return(c - '0');
    else if ( c >= 'a' && c <= 'f' )
        return(c - 'a' + 10);
    else if ( c >= 'A' && c <= 'F' )
        return(c - 'A' + 10);
    return(-1);
}

int32_t is_hexstr(char *str,int32_t n)
{
    int32_t i;
    if ( str == 0 || str[0] == 0 )
        return(0);
    for (i=0; str[i]!=0; i++)
    {
        if ( n > 0 && i >= n )
            break;
        if ( _unhex(str[i]) < 0 )
            break;
    }
    if ( n == 0 )
        return(i);
    return(i == n);
}

int32_t unhex(char c)
{
    int32_t hex;
    if ( (hex= _unhex(c)) < 0 )
    {
        //LogPrintf("unhex: illegal hexchar.(%c)\n",c);
    }
    return(hex);
}

unsigned char _decode_hex(char *hex) { return((unhex(hex[0])<<4) | unhex(hex[1])); }

int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex)
{
    int32_t adjust,i = 0;
    //LogPrintf("decode.(%s)\n",hex);
    if ( is_hexstr(hex,n) <= 0 )
    {
        memset(bytes,0,n);
        return(n);
    }
    if ( hex[n-1] == '\n' || hex[n-1] == '\r' )
        hex[--n] = 0;
    if ( n == 0 || (hex[n*2+1] == 0 && hex[n*2] != 0) )
    {
        if ( n > 0 )
        {
            bytes[0] = unhex(hex[0]);
            //LogPrintf("decode_hex n.%d hex[0] (%c) -> %d hex.(%s) [n*2+1: %d] [n*2: %d %c] len.%ld\n",n,hex[0],bytes[0],hex,hex[n*2+1],hex[n*2],hex[n*2],(long)strlen(hex));
        }
        bytes++;
        hex++;
        adjust = 1;
    } else adjust = 0;
    if ( n > 0 )
    {
        for (i=0; i<n; i++)
            bytes[i] = _decode_hex(&hex[i*2]);
    }
    //bytes[i] = 0;
    return(n + adjust);
}

char hexbyte(int32_t c)
{
    c &= 0xf;
    if ( c < 10 )
        return('0'+c);
    else if ( c < 16 )
        return('a'+c-10);
    else return(0);
}

int32_t init_hexbytes_noT(char *hexbytes,unsigned char *message,long len)
{
    int32_t i;
    if ( len <= 0 )
    {
        hexbytes[0] = 0;
        return(1);
    }
    for (i=0; i<len; i++)
    {
        hexbytes[i*2] = hexbyte((message[i]>>4) & 0xf);
        hexbytes[i*2 + 1] = hexbyte(message[i] & 0xf);
        //LogPrintf("i.%d (%02x) [%c%c]\n",i,message[i],hexbytes[i*2],hexbytes[i*2+1]);
    }
    hexbytes[len*2] = 0;
    //LogPrintf("len.%ld\n",len*2+1);
    return((int32_t)len*2+1);
}

char *bits256_str(char hexstr[65],bits256 x)
{
    init_hexbytes_noT(hexstr,x.bytes,sizeof(x));
    return(hexstr);
}

long stripquotes(char *str)
{
    long len,offset;
    if ( str == 0 )
        return(0);
    len = strlen(str);
    if ( str[0] == '"' && str[len-1] == '"' )
        str[len-1] = 0, offset = 1;
    else offset = 0;
    return(offset);
}

static int32_t cJSON_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower((int32_t)(*s1)) == tolower((int32_t)(*s2)); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower((int32_t)(*(const unsigned char *)s1)) - tolower((int32_t)(*(const unsigned char *)s2));
}

// the following written by jl777
/******************************************************************************
 * Copyright © 2014-2020 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *
 * propagated or distributed except according to the terms contained in the   *
 * LICENSE file.                                                              *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

void copy_cJSON(struct destbuf *dest,cJSON *obj)
{
    char *str;
    int i;
    long offset;
    dest->buf[0] = 0;
    if ( obj != 0 )
    {
        str = cJSON_Print(obj);
        if ( str != 0 )
        {
            offset = stripquotes(str);
            //strcpy(dest,str+offset);
            for (i=0; i<MAX_JSON_FIELD-1; i++)
                if ( (dest->buf[i]= str[offset+i]) == 0 )
                    break;
            dest->buf[i] = 0;
            free(str);
        }
    }
}

void copy_cJSON2(char *dest,int32_t maxlen,cJSON *obj)
{
    struct destbuf tmp;
    maxlen--;
    dest[0] = 0;
    if ( maxlen > sizeof(tmp.buf) )
        maxlen = sizeof(tmp.buf);
    copy_cJSON(&tmp,obj);
    if ( strlen(tmp.buf) < maxlen )
        strcpy(dest,tmp.buf);
    else dest[0] = 0;
}

int64_t _get_cJSON_int(cJSON *json)
{
    struct destbuf tmp;
    if ( json != 0 )
    {
        copy_cJSON(&tmp,json);
        if ( tmp.buf[0] != 0 )
            return(calc_nxt64bits(tmp.buf));
    }
    return(0);
}

int64_t get_cJSON_int(cJSON *json,char *field)
{
    cJSON *numjson;
    if ( json != 0 )
    {
        numjson = cJSON_GetObjectItem(json,field);
        if ( numjson != 0 )
            return(_get_cJSON_int(numjson));
    }
    return(0);
}

int64_t conv_floatstr(char *numstr)
{
    double val,corr;
    val = atof(numstr);
    corr = (val < 0.) ? -0.50000000001 : 0.50000000001;
    return((int64_t)(val * SATOSHIDEN + corr));
}

int64_t _conv_cJSON_float(cJSON *json)
{
    int64_t conv_floatstr(char *);
    struct destbuf tmp;
    if ( json != 0 )
    {
        copy_cJSON(&tmp,json);
        return(conv_floatstr(tmp.buf));
    }
    return(0);
}

int64_t conv_cJSON_float(cJSON *json,char *field)
{
    if ( json != 0 )
        return(_conv_cJSON_float(cJSON_GetObjectItem(json,field)));
    return(0);
}

int32_t safecopy(char *dest,char *src,long len)
{
    int32_t i = -1;
    if ( src != 0 && dest != 0 && src != dest )
    {
        if ( dest != 0 )
            memset(dest,0,len);
        for (i=0; i<len&&src[i]!=0; i++)
            dest[i] = src[i];
        if ( i == len )
        {
            printf("safecopy: %s too long %ld\n",src,len);
#ifdef __APPLE__
            //getchar();
#endif
            return(-1);
        }
        dest[i] = 0;
    }
    return(i);
}

int32_t extract_cJSON_str(char *dest,int32_t max,cJSON *json,char *field)
{
    int32_t safecopy(char *dest,char *src,long len);
    char *str;
    cJSON *obj;
    int32_t len;
    long offset;
    dest[0] = 0;
    obj = cJSON_GetObjectItem(json,field);
    if ( obj != 0 )
    {
        str = cJSON_Print(obj);
        offset = stripquotes(str);
        len = safecopy(dest,str+offset,max);
        free(str);
        return(len);
    }
    return(0);
}

cJSON *gen_list_json(char **list)
{
    cJSON *array,*item;
    array = cJSON_CreateArray();
    while ( list != 0 && *list != 0 && *list[0] != 0 )
    {
        item = cJSON_CreateString(*list++);
        cJSON_AddItemToArray(array,item);
    }
    return(array);
}

uint64_t get_API_nxt64bits(cJSON *obj)
{
    uint64_t nxt64bits = 0;
    struct destbuf tmp;
    if ( obj != 0 )
    {
        if ( cJSON_IsNumber(obj) != 0 )
            return((uint64_t)obj->valuedouble);
        copy_cJSON(&tmp,obj);
        nxt64bits = calc_nxt64bits(tmp.buf);
    }
    return(nxt64bits);
}
uint64_t j64bits(cJSON *json,char *field) { if ( field == 0 ) return(get_API_nxt64bits(json)); return(get_API_nxt64bits(cJSON_GetObjectItem(json,field))); }
uint64_t j64bitsi(cJSON *json,int32_t i) { return(get_API_nxt64bits(cJSON_GetArrayItem(json,i))); }

uint64_t get_satoshi_obj(cJSON *json,char *field)
{
    int32_t i,n;
    uint64_t prev,satoshis,mult = 1;
    struct destbuf numstr,checkstr;
    cJSON *numjson;
    numjson = cJSON_GetObjectItem(json,field);
    copy_cJSON(&numstr,numjson);
    satoshis = prev = 0; mult = 1; n = (int32_t)strlen(numstr.buf);
    for (i=n-1; i>=0; i--,mult*=10)
    {
        satoshis += (mult * (numstr.buf[i] - '0'));
        //if ( satoshis < prev )
        //    LogPrintf("get_satoshi_obj numstr.(%s) i.%d prev.%llu vs satoshis.%llu\n",numstr.buf,i,(unsigned long long)prev,(unsigned long long)satoshis);
        prev = satoshis;
    }
    sprintf(checkstr.buf,"%llu",(long long)satoshis);
    if ( strcmp(checkstr.buf,numstr.buf) != 0 )
    {
        //LogPrintf("SATOSHI GREMLIN?? numstr.(%s) -> %.8f -> (%s)\n",numstr.buf,dstr(satoshis),checkstr.buf);
    }
    return(satoshis);
}

void add_satoshis_json(cJSON *json,char *field,uint64_t satoshis)
{
    cJSON *obj;
    char numstr[64];
    sprintf(numstr,"%lld",(long long)satoshis);
    obj = cJSON_CreateString(numstr);
    cJSON_AddItemToObject(json,field,obj);
    //if ( satoshis != get_satoshi_obj(json,field) )
        //LogPrintf("error adding satoshi obj %ld -> %ld\n",(unsigned long)satoshis,(unsigned long)get_satoshi_obj(json,field));
}

char *cJSON_str(cJSON *json)
{
    if ( json != 0 && cJSON_IsString(json) != 0 )
        return(json->valuestring);
    return(0);
}

void jadd(cJSON *json,char *field,cJSON *item) { if ( json != 0 )cJSON_AddItemToObject(json,field,item); }
void jaddstr(cJSON *json,char *field,char *str) { if ( json != 0 && str != 0 ) cJSON_AddItemToObject(json,field,cJSON_CreateString(str)); }
void jaddnum(cJSON *json,char *field,double num) { if ( json != 0 )cJSON_AddItemToObject(json,field,cJSON_CreateNumber(num)); }
void jadd64bits(cJSON *json,char *field,uint64_t nxt64bits) { char numstr[64]; sprintf(numstr,"%llu",(long long)nxt64bits), jaddstr(json,field,numstr); }
void jaddi(cJSON *json,cJSON *item) { if ( json != 0 ) cJSON_AddItemToArray(json,item); }
void jaddistr(cJSON *json,char *str) { if ( json != 0 ) cJSON_AddItemToArray(json,cJSON_CreateString(str)); }
void jaddinum(cJSON *json,double num) { if ( json != 0 ) cJSON_AddItemToArray(json,cJSON_CreateNumber(num)); }
void jaddi64bits(cJSON *json,uint64_t nxt64bits) { char numstr[64]; sprintf(numstr,"%llu",(long long)nxt64bits), jaddistr(json,numstr); }
char *jstr(cJSON *json,char *field) { if ( json == 0 ) return(0); if ( field == 0 ) return(cJSON_str(json)); return(cJSON_str(cJSON_GetObjectItem(json,field))); }

char *jstri(cJSON *json,int32_t i) { return(cJSON_str(cJSON_GetArrayItem(json,i))); }
char *jprint(cJSON *json,int32_t freeflag)
{
    char *str;
    /*static portable_mutex_t mutex; static int32_t initflag;
    if ( initflag == 0 )
    {
        portable_mutex_init(&mutex);
        initflag = 1;
    }*/
    if ( json == 0 )
        return(clonestr((char *)"{}"));
    //portable_mutex_lock(&mutex);
    //usleep(5000);
    str = cJSON_Print(json), _stripwhite(str,' ');
    if ( freeflag != 0 )
        free_json(json);
    //portable_mutex_unlock(&mutex);
    return(str);
}

bits256 get_API_bits256(cJSON *obj)
{
    bits256 hash; char *str;
    memset(hash.bytes,0,sizeof(hash));
    if ( obj != 0 )
    {
        if ( cJSON_IsString(obj) != 0 && (str= obj->valuestring) != 0 && strlen(str) == 64 )
            decode_hex(hash.bytes,sizeof(hash),str);
    }