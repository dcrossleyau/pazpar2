/*
 * $Id: http.c,v 1.2 2006-11-24 20:29:07 quinn Exp $
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#include <yaz/yaz-util.h>
#include <yaz/comstack.h>
#include <netdb.h>

#include "command.h"
#include "util.h"
#include "eventl.h"
#include "pazpar2.h"
#include "http.h"
#include "http_command.h"

static void proxy_io(IOCHAN i, int event);

extern IOCHAN channel_list;

static struct sockaddr_in *proxy_addr = 0; // If this is set, we proxy normal HTTP requests
static char proxy_url[256] = "";
static struct http_buf *http_buf_freelist = 0;

static struct http_buf *http_buf_create()
{
    struct http_buf *r;

    if (http_buf_freelist)
    {
        r = http_buf_freelist;
        http_buf_freelist = http_buf_freelist->next;
    }
    else
        r = xmalloc(sizeof(struct http_buf));
    r->offset = 0;
    r->len = 0;
    r->next = 0;
    return r;
}

static void http_buf_destroy(struct http_buf *b)
{
    b->next = http_buf_freelist;
    http_buf_freelist = b;
}

static void http_buf_destroy_queue(struct http_buf *b)
{
    struct http_buf *p;
    while (b)
    {
        p = b->next;
        http_buf_destroy(b);
        b = p;
    }
}

#ifdef GAGA
// Calculate length of chain
static int http_buf_len(struct http_buf *b)
{
    int sum = 0;
    for (; b; b = b->next)
        sum += b->len;
    return sum;
}
#endif

static struct http_buf *http_buf_bybuf(char *b, int len)
{
    struct http_buf *res = 0;
    struct http_buf **p = &res;

    while (len)
    {
        *p = http_buf_create();
        int tocopy = len;
        if (tocopy > HTTP_BUF_SIZE)
            tocopy = HTTP_BUF_SIZE;
        memcpy((*p)->buf, b, tocopy);
        (*p)->len = tocopy;
        len -= tocopy;
        b += tocopy;
        p = &(*p)->next;
    }
    return res;
}

// Add a (chain of) buffers to the end of an existing queue.
static void http_buf_enqueue(struct http_buf **queue, struct http_buf *b)
{
    while (*queue)
        queue = &(*queue)->next;
    *queue = b;
}

static struct http_buf *http_buf_bywrbuf(WRBUF wrbuf)
{
    return http_buf_bybuf(wrbuf_buf(wrbuf), wrbuf_len(wrbuf));
}

// Non-destructively collapse chain of buffers into a string (max *len)
// Return
static int http_buf_peek(struct http_buf *b, char *buf, int len)
{
    int rd = 0;
    while (b && rd < len)
    {
        int toread = len - rd;
        if (toread > b->len)
            toread = b->len;
        memcpy(buf + rd, b->buf + b->offset, toread);
        rd += toread;
        b = b->next;
    }
    buf[rd] = '\0';
    return rd;
}

// Ddestructively munch up to len  from head of queue.
static int http_buf_read(struct http_buf **b, char *buf, int len)
{
    int rd = 0;
    while ((*b) && rd < len)
    {
        int toread = len - rd;
        if (toread > (*b)->len)
            toread = (*b)->len;
        memcpy(buf + rd, (*b)->buf + (*b)->offset, toread);
        rd += toread;
        if (toread < (*b)->len)
        {
            (*b)->len -= toread;
            (*b)->offset += toread;
            break;
        }
        else
        {
            struct http_buf *n = (*b)->next;
            http_buf_destroy(*b);
            *b = n;
        }
    }
    buf[rd] = '\0';
    return rd;
}

void http_addheader(struct http_response *r, const char *name, const char *value)
{
    struct http_channel *c = r->channel;
    struct http_header *h = nmem_malloc(c->nmem, sizeof *h);
    h->name = nmem_strdup(c->nmem, name);
    h->value = nmem_strdup(c->nmem, value);
    h->next = r->headers;
    r->headers = h;
}

char *http_argbyname(struct http_request *r, char *name)
{
    struct http_argument *p;
    if (!name)
        return 0;
    for (p = r->arguments; p; p = p->next)
        if (!strcmp(p->name, name))
            return p->value;
    return 0;
}

char *http_headerbyname(struct http_request *r, char *name)
{
    struct http_header *p;
    for (p = r->headers; p; p = p->next)
        if (!strcmp(p->name, name))
            return p->value;
    return 0;
}

struct http_response *http_create_response(struct http_channel *c)
{
    struct http_response *r = nmem_malloc(c->nmem, sizeof(*r));
    strcpy(r->code, "200");
    r->msg = "OK";
    r->channel = c;
    r->headers = 0;
    r->payload = 0;
    return r;
}

// Check if we have a complete request. Return 0 or length (including trailing newline)
// FIXME: Does not deal gracefully with requests carrying payload
// but this is kind of OK since we will reject anything other than an empty GET
static int request_check(struct http_buf *queue)
{
    char tmp[4096];
    int len = 0;
    char *buf = tmp;

    http_buf_peek(queue, tmp, 4096);
    while (*buf) // Check if we have a sequence of lines terminated by an empty line
    {
        char *b = strstr(buf, "\r\n");

        if (!b)
            return 0;

        len += (b - buf) + 2;
        if (b == buf)
            return len;
        buf = b + 2;
    }
    return 0;
}

struct http_request *http_parse_request(struct http_channel *c, struct http_buf **queue,
        int len)
{
    struct http_request *r = nmem_malloc(c->nmem, sizeof(*r));
    char *p, *p2;
    char tmp[4096];
    char *buf = tmp;

    if (len > 4096)
        return 0;
    if (http_buf_read(queue, buf, len) < len)
        return 0;

    r->channel = c;
    r->arguments = 0;
    r->headers = 0;
    // Parse first line
    for (p = buf, p2 = r->method; *p && *p != ' ' && p - buf < 19; p++)
        *(p2++) = *p;
    if (*p != ' ')
    {
        yaz_log(YLOG_WARN, "Unexpected HTTP method in request");
        return 0;
    }
    *p2 = '\0';

    if (!(buf = strchr(buf, ' ')))
    {
        yaz_log(YLOG_WARN, "Syntax error in request (1)");
        return 0;
    }
    buf++;
    if (!(p = strchr(buf, ' ')))
    {
        yaz_log(YLOG_WARN, "Syntax error in request (2)");
        return 0;
    }
    *(p++) = '\0';
    if ((p2 = strchr(buf, '?'))) // Do we have arguments?
        *(p2++) = '\0';
    r->path = nmem_strdup(c->nmem, buf);
    if (p2)
    {
        // Parse Arguments
        while (*p2)
        {
            struct http_argument *a;
            char *equal = strchr(p2, '=');
            char *eoa = strchr(p2, '&');
            if (!equal)
            {
                yaz_log(YLOG_WARN, "Expected '=' in argument");
                return 0;
            }
            if (!eoa)
                eoa = equal + strlen(equal); // last argument
            else
                *(eoa++) = '\0';
            a = nmem_malloc(c->nmem, sizeof(struct http_argument));
            *(equal++) = '\0';
            a->name = nmem_strdup(c->nmem, p2);
            a->value = nmem_strdup(c->nmem, equal);
            a->next = r->arguments;
            r->arguments = a;
            p2 = eoa;
        }
    }
    buf = p;

    if (strncmp(buf, "HTTP/", 5))
        strcpy(r->http_version, "1.0");
    else
    {
        buf += 5;
        if (!(p = strstr(buf, "\r\n")))
            return 0;
        *(p++) = '\0';
        p++;
        strcpy(r->http_version, buf);
        buf = p;
    }
    strcpy(c->version, r->http_version);

    r->headers = 0;
    while (*buf)
    {
        if (!(p = strstr(buf, "\r\n")))
            return 0;
        if (p == buf)
            break;
        else
        {
            struct http_header *h = nmem_malloc(c->nmem, sizeof(*h));
            if (!(p2 = strchr(buf, ':')))
                return 0;
            *(p2++) = '\0';
            h->name = nmem_strdup(c->nmem, buf);
            while (isspace(*p2))
                p2++;
            if (p2 >= p) // Empty header?
            {
                buf = p + 2;
                continue;
            }
            *p = '\0';
            h->value = nmem_strdup(c->nmem, p2);
            h->next = r->headers;
            r->headers = h;
            buf = p + 2;
        }
    }

    return r;
}


static struct http_buf *http_serialize_response(struct http_channel *c,
        struct http_response *r)
{
    wrbuf_rewind(c->wrbuf);
    struct http_header *h;

    wrbuf_printf(c->wrbuf, "HTTP/1.1 %s %s\r\n", r->code, r->msg);
    for (h = r->headers; h; h = h->next)
        wrbuf_printf(c->wrbuf, "%s: %s\r\n", h->name, h->value);
    wrbuf_printf(c->wrbuf, "Content-length: %d\r\n", r->payload ? strlen(r->payload) : 0);
    wrbuf_printf(c->wrbuf, "Content-type: text/xml\r\n");
    wrbuf_puts(c->wrbuf, "\r\n");

    if (r->payload)
        wrbuf_puts(c->wrbuf, r->payload);

    return http_buf_bywrbuf(c->wrbuf);
}

// Serialize a HTTP request
static struct http_buf *http_serialize_request(struct http_request *r)
{
    struct http_channel *c = r->channel;
    wrbuf_rewind(c->wrbuf);
    struct http_header *h;
    struct http_argument *a;

    wrbuf_printf(c->wrbuf, "%s %s", r->method, r->path);

    if (r->arguments)
    {
        wrbuf_putc(c->wrbuf, '?');
        for (a = r->arguments; a; a = a->next) {
            if (a != r->arguments)
                wrbuf_putc(c->wrbuf, '&');
            wrbuf_printf(c->wrbuf, "%s=%s", a->name, a->value);
        }
    }

    wrbuf_printf(c->wrbuf, " HTTP/%s\r\n", r->http_version);

    for (h = r->headers; h; h = h->next)
        wrbuf_printf(c->wrbuf, "%s: %s\r\n", h->name, h->value);

    wrbuf_puts(c->wrbuf, "\r\n");
    
    return http_buf_bywrbuf(c->wrbuf);
}


// Cleanup
static void http_destroy(IOCHAN i)
{
    struct http_channel *s = iochan_getdata(i);

    yaz_log(YLOG_DEBUG, "Destroying http channel");
    if (s->proxy)
    {
        yaz_log(YLOG_DEBUG, "Destroying Proxy channel");
        if (s->proxy->iochan)
        {
            close(iochan_getfd(s->proxy->iochan));
            iochan_destroy(s->proxy->iochan);
        }
        http_buf_destroy_queue(s->proxy->oqueue);
        xfree(s->proxy);
    }
    http_buf_destroy_queue(s->iqueue);
    http_buf_destroy_queue(s->oqueue);
    nmem_destroy(s->nmem);
    wrbuf_free(s->wrbuf, 1);
    xfree(s);
    close(iochan_getfd(i));
    iochan_destroy(i);
}

static int http_weshouldproxy(struct http_request *rq)
{
    if (proxy_addr && !strstr(rq->path, "search.pz2"))
        return 1;
    return 0;
}

static int http_proxy(struct http_request *rq)
{
    struct http_channel *c = rq->channel;
    struct http_proxy *p = c->proxy;
    struct http_header *hp;
    struct http_buf *requestbuf;

    yaz_log(YLOG_DEBUG, "Proxy request");

    if (!p) // This is a new connection. Create a proxy channel
    {
        int sock;
        struct protoent *pe;
        int one = 1;
        int flags;

        yaz_log(YLOG_DEBUG, "Creating a new proxy channel");
        if (!(pe = getprotobyname("tcp"))) {
            abort();
        }
        if ((sock = socket(PF_INET, SOCK_STREAM, pe->p_proto)) < 0)
        {
            yaz_log(YLOG_WARN|YLOG_ERRNO, "socket");
            return -1;
        }
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)
                        &one, sizeof(one)) < 0)
            abort();
        if ((flags = fcntl(sock, F_GETFL, 0)) < 0) 
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl");
        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
            yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl2");
        if (connect(sock, (struct sockaddr *) proxy_addr, sizeof(*proxy_addr)) < 0)
            if (errno != EINPROGRESS)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "Proxy connect");
                return -1;
            }

        p = xmalloc(sizeof(struct http_proxy));
        p->oqueue = 0;
        p->channel = c;
        c->proxy = p;
        // We will add EVENT_OUTPUT below
        p->iochan = iochan_create(sock, proxy_io, EVENT_INPUT);
        iochan_setdata(p->iochan, p);
        p->iochan->next = channel_list;
        channel_list = p->iochan;
    }

    // Modify Host: header
    for (hp = rq->headers; hp; hp = hp->next)
        if (!strcmp(hp->name, "Host"))
            break;
    if (!hp)
    {
        yaz_log(YLOG_WARN, "Failed to find Host header in proxy");
        return -1;
    }
    hp->value = nmem_strdup(c->nmem, proxy_url);
    requestbuf = http_serialize_request(rq);
    http_buf_enqueue(&p->oqueue, requestbuf);
    iochan_setflag(p->iochan, EVENT_OUTPUT);
    return 0;
}

static void http_io(IOCHAN i, int event)
{
    struct http_channel *hc = iochan_getdata(i);
    struct http_request *request;
    struct http_response *response;

    switch (event)
    {
        int res, reqlen;
        struct http_buf *htbuf;

        case EVENT_INPUT:
            yaz_log(YLOG_DEBUG, "HTTP Input event");

            htbuf = http_buf_create();
            res = read(iochan_getfd(i), htbuf->buf, HTTP_BUF_SIZE -1);
            if (res <= 0 && errno != EAGAIN)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "HTTP read");
                http_buf_destroy(htbuf);
                http_destroy(i);
                return;
            }
            if (res > 0)
            {
                htbuf->buf[res] = '\0';
                htbuf->len = res;
                http_buf_enqueue(&hc->iqueue, htbuf);
            }

            if ((reqlen = request_check(hc->iqueue)) <= 2)
            {
                yaz_log(YLOG_DEBUG, "We don't have a complete HTTP request yet");
                return;
            }
            yaz_log(YLOG_DEBUG, "We think we have a complete HTTP request (len %d)", reqlen);

            nmem_reset(hc->nmem);
            if (!(request = http_parse_request(hc, &hc->iqueue, reqlen)))
            {
                yaz_log(YLOG_WARN, "Failed to parse request");
                http_destroy(i);
                return;
            }
            yaz_log(YLOG_LOG, "Request: %s %s v %s", request->method,  request->path,
                    request->http_version);
            if (http_weshouldproxy(request))
                http_proxy(request);
            else
            {
                struct http_buf *hb;
                // Execute our business logic!
                response = http_command(request);
                if (!response)
                {
                    http_destroy(i);
                    return;
                }
                if (!(hb =  http_serialize_response(hc, response)))
                {
                    http_destroy(i);
                    return;
                }
                http_buf_enqueue(&hc->oqueue, hb);
                yaz_log(YLOG_DEBUG, "Response ready");
                iochan_setflags(i, EVENT_OUTPUT); // Turns off input selecting
            }
            if (hc->iqueue)
            {
                yaz_log(YLOG_DEBUG, "We think we have more input to read. Forcing event");
                iochan_setevent(i, EVENT_INPUT);
            }

            break;

        case EVENT_OUTPUT:
            yaz_log(YLOG_DEBUG, "HTTP output event");
            if (hc->oqueue)
            {
                struct http_buf *wb = hc->oqueue;
                res = write(iochan_getfd(hc->iochan), wb->buf + wb->offset, wb->len);
                if (res <= 0)
                {
                    yaz_log(YLOG_WARN|YLOG_ERRNO, "write");
                    http_destroy(i);
                    return;
                }
                yaz_log(YLOG_DEBUG, "HTTP Wrote %d octets", res);
                if (res == wb->len)
                {
                    hc->oqueue = hc->oqueue->next;
                    http_buf_destroy(wb);
                }
                else
                {
                    wb->len -= res;
                    wb->offset += res;
                }
                if (!hc->oqueue) {
                    yaz_log(YLOG_DEBUG, "Writing finished");
                    if (!strcmp(hc->version, "1.0"))
                    {
                        yaz_log(YLOG_DEBUG, "Closing 1.0 connection");
                        http_destroy(i);
                        return;
                    }
                    else
                        iochan_setflags(i, EVENT_INPUT); // Turns off output flag
                }
            }

            if (!hc->oqueue && hc->proxy && !hc->proxy->iochan) 
                http_destroy(i); // Server closed; we're done
            break;
        default:
            yaz_log(YLOG_WARN, "Unexpected event on connection");
            http_destroy(i);
    }
}

// Handles I/O on a client connection to a backend web server (proxy mode)
static void proxy_io(IOCHAN pi, int event)
{
    struct http_proxy *pc = iochan_getdata(pi);
    struct http_channel *hc = pc->channel;

    switch (event)
    {
        int res;
        struct http_buf *htbuf;

        case EVENT_INPUT:
            yaz_log(YLOG_DEBUG, "Proxy input event");
            htbuf = http_buf_create();
            res = read(iochan_getfd(pi), htbuf->buf, HTTP_BUF_SIZE -1);
            yaz_log(YLOG_DEBUG, "Proxy read %d bytes.", res);
            if (res == 0 || (res < 0 && errno != EINPROGRESS))
            {
                if (hc->oqueue)
                {
                    yaz_log(YLOG_WARN, "Proxy read came up short");
                    // Close channel and alert client HTTP channel that we're gone
                    http_buf_destroy(htbuf);
                    close(iochan_getfd(pi));
                    iochan_destroy(pi);
                    pc->iochan = 0;
                }
                else
                {
                    http_destroy(hc->iochan);
                    return;
                }
            }
            else
            {
                htbuf->buf[res] = '\0';
                htbuf->len = res;
                http_buf_enqueue(&hc->oqueue, htbuf);
            }
            iochan_setflag(hc->iochan, EVENT_OUTPUT);
            break;
        case EVENT_OUTPUT:
            yaz_log(YLOG_DEBUG, "Proxy output event");
            if (!(htbuf = pc->oqueue))
            {
                iochan_clearflag(pi, EVENT_OUTPUT);
                return;
            }
            res = write(iochan_getfd(pi), htbuf->buf + htbuf->offset, htbuf->len);
            if (res <= 0)
            {
                yaz_log(YLOG_WARN|YLOG_ERRNO, "write");
                http_destroy(hc->iochan);
                return;
            }
            if (res == htbuf->len)
            {
                struct http_buf *np = htbuf->next;
                http_buf_destroy(htbuf);
                pc->oqueue = np;
            }
            else
            {
                htbuf->len -= res;
                htbuf->offset += res;
            }

            if (!pc->oqueue) {
                iochan_setflags(pi, EVENT_INPUT); // Turns off output flag
            }
            break;
        default:
            yaz_log(YLOG_WARN, "Unexpected event on connection");
            http_destroy(hc->iochan);
    }
}

/* Accept a new command connection */
static void http_accept(IOCHAN i, int event)
{
    struct sockaddr_in addr;
    int fd = iochan_getfd(i);
    socklen_t len;
    int s;
    IOCHAN c;
    int flags;
    struct http_channel *ch;

    len = sizeof addr;
    if ((s = accept(fd, (struct sockaddr *) &addr, &len)) < 0)
    {
        yaz_log(YLOG_WARN|YLOG_ERRNO, "accept");
        return;
    }
    if ((flags = fcntl(s, F_GETFL, 0)) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl");
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "fcntl2");

    yaz_log(YLOG_LOG, "New command connection");
    c = iochan_create(s, http_io, EVENT_INPUT | EVENT_EXCEPT);

    ch = xmalloc(sizeof(*ch));
    ch->proxy = 0;
    ch->nmem = nmem_create();
    ch->wrbuf = wrbuf_alloc();
    ch->iochan = c;
    ch->iqueue = ch->oqueue = 0;
    iochan_setdata(c, ch);

    c->next = channel_list;
    channel_list = c;
}

/* Create a http-channel listener */
void http_init(int port)
{
    IOCHAN c;
    int l;
    struct protoent *p;
    struct sockaddr_in myaddr;
    int one = 1;

    yaz_log(YLOG_LOG, "HTTP port is %d", port);
    if (!(p = getprotobyname("tcp"))) {
        abort();
    }
    if ((l = socket(PF_INET, SOCK_STREAM, p->p_proto)) < 0)
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "socket");
    if (setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (char*)
                    &one, sizeof(one)) < 0)
        abort();

    bzero(&myaddr, sizeof myaddr);
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(port);
    if (bind(l, (struct sockaddr *) &myaddr, sizeof myaddr) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "bind");
    if (listen(l, SOMAXCONN) < 0) 
        yaz_log(YLOG_FATAL|YLOG_ERRNO, "listen");

    c = iochan_create(l, http_accept, EVENT_INPUT | EVENT_EXCEPT);
    c->next = channel_list;
    channel_list = c;
}

void http_set_proxyaddr(char *host)
{
    char *p;
    int port;
    struct hostent *he;

    strcpy(proxy_url, host);
    p = strchr(host, ':');
    yaz_log(YLOG_DEBUG, "Proxying for %s", host);
    if (p) {
        port = atoi(p + 1);
        *p = '\0';
    }
    else
        port = 80;
    if (!(he = gethostbyname(host))) 
    {
        fprintf(stderr, "Failed to lookup '%s'\n", host);
        exit(1);
    }
    proxy_addr = xmalloc(sizeof(struct sockaddr_in));
    proxy_addr->sin_family = he->h_addrtype;
    memcpy(&proxy_addr->sin_addr.s_addr, he->h_addr_list[0], he->h_length);
    proxy_addr->sin_port = htons(port);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
