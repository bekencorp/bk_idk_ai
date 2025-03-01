/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-05-05     Bernard      the first version
 * 2013-06-10     Bernard      fix the slow speed issue when download file.
 * 2015-11-14     aozima       add content_length_remainder.
 * 2017-12-23     aozima       update gethostbyname to getaddrinfo.
 * 2018-01-04     aozima       add ipv6 address support.
 * 2018-07-26     chenyong     modify log information
 * 2018-08-07     chenyong     modify header processing
 */
#include "components/webclient.h"

#if CONFIG_WIFI_ENABLE

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <os/mem.h>
#include <components/netif.h>
#include <components/event.h>
#include "lwip/errno.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <os/str.h>
#include <os/mem.h>
#include <os/mem.h>
#define RT_NULL NULL
#define RT_TRUE BK_TRUE

#define os_calloc(nmemb,size)   ((size) && (nmemb) > (~( unsigned int) 0)/(size))?0:os_zalloc((nmemb)*(size))

#define TAG  "http"

/* default receive or send timeout */
#define WEBCLIENT_DEFAULT_TIMEO        30
#define POST_DATA_LEN 2*1024

static void bk_webclient_hex_dump(const char *s, int length)
{
		BK_RAW_LOGI(NULL, "begin print data:");
		for (int i = 0; i < length; i++)
			BK_RAW_LOGI(NULL, "%c", *(u8 *)(s+i));
		BK_RAW_LOGI(NULL, "\r\n");
}

extern long int strtol(const char *nptr, char **endptr, int base);

static int webclient_strncasecmp(const char *a, const char *b, size_t n)
{
    uint8_t c1, c2;
    if (n <= 0)
        return 0;
    do {
        c1 = tolower(*a++);
        c2 = tolower(*b++);
    } while (--n && c1 && c1 == c2);
    return c1 - c2;
}

static const char *webclient_strstri(const char* str, const char* subStr)
{
    int len = strlen(subStr);

    if(len == 0)
    {
        return RT_NULL;
    }

    while(*str)
    {
        if(webclient_strncasecmp(str, subStr, len) == 0)
        {
            return str;
        }
        ++str;
    }
    return RT_NULL;
}

static int webclient_send(struct webclient_session* session, const void *buffer, size_t len, int flag)
{
#ifdef WEBCLIENT_USING_MBED_TLS
    if (session->tls_session)
    {
        return mbedtls_client_write(session->tls_session, buffer, len);
    }
#endif

    return send(session->socket, buffer, len, flag);
}

static int webclient_recv(struct webclient_session* session, void *buffer, size_t len, int flag)
{
#ifdef WEBCLIENT_USING_MBED_TLS
    if (session->tls_session)
    {
        return mbedtls_client_read(session->tls_session, buffer, len);
    }
#endif
    int ret = recv(session->socket, buffer, len, flag);
    return ret;
}

static int webclient_read_line(struct webclient_session *session, char *buffer, int size)
{
    int rc, count = 0;
    char ch = 0, last_ch = 0;

    BK_ASSERT(session);
    BK_ASSERT(buffer);

    /* Keep reading until we fill the buffer. */
    while (count < size)
    {
        rc = webclient_recv(session, (unsigned char *) &ch, 1, 0);
#if defined(WEBCLIENT_USING_MBED_TLS) || defined(WEBCLIENT_USING_SAL_TLS)
        if (session->is_tls && (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE))
        {
            continue;
        }
#endif
        if (rc <= 0)
            return rc;

        if (ch == '\n' && last_ch == '\r')
            break;

        buffer[count++] = ch;

        last_ch = ch;
    }

    if (count > size)
    {
        BK_LOGE(TAG,"read line failed. The line data length is out of buffer size(%d)!\r\n", count);
        return -WEBCLIENT_ERROR;
    }

    return count;
}

/**
 * resolve server address
 *
 * @param session http session
 * @param res the server address information
 * @param url the input server URI address
 * @param request the pointer to point the request url, for example, /index.html
 *
 * @return 0 on resolve server address OK, others failed
 *
 * URL example:
 * http://www.rt-thread.org
 * http://www.rt-thread.org:80
 * https://www.rt-thread.org/
 * http://192.168.1.1:80/index.htm
 * http://[fe80::1]
 * http://[fe80::1]/
 * http://[fe80::1]/index.html
 * http://[fe80::1]:80/index.html
 */
int webclient_resolve_address(struct webclient_session *session, struct addrinfo **res,
                                     const char *url, const char **request)
{
    int rc = WEBCLIENT_OK;
    char *ptr;
    char port_str[6] = "80"; /* default port of 80(http) */
    const char *port_ptr;
    const char *path_ptr;

    const char *host_addr = 0;
    int url_len, host_addr_len = 0;

    BK_ASSERT(res);
    BK_ASSERT(request);

    url_len = strlen(url);

    /* strip protocol(http or https) */
    if (strncmp(url, "http://", 7) == 0)
    {
        host_addr = url + 7;
    }
    else if (strncmp(url, "https://", 8) == 0)
    {
        strncpy(port_str, "443", 4);
        host_addr = url + 8;
    }
    else
    {
        rc = -WEBCLIENT_ERROR;
        goto __exit;
    }

    /* ipv6 address */
    if (host_addr[0] == '[')
    {
        host_addr += 1;
        ptr = strstr(host_addr, "]");
        if (!ptr)
        {
            rc = -WEBCLIENT_ERROR;
            goto __exit;
        }
        host_addr_len = ptr - host_addr;
    }

    path_ptr = strstr(host_addr, "/");
    *request = path_ptr ? path_ptr : "/";

    /* resolve port */
    port_ptr = strstr(host_addr + host_addr_len, ":");
    if (port_ptr && path_ptr && (port_ptr < path_ptr))
    {
        int port_len = path_ptr - port_ptr - 1;

        strncpy(port_str, port_ptr + 1, port_len);
        port_str[port_len] = '\0';
    }

    if (port_ptr && (!path_ptr))
    {
        strcpy(port_str, port_ptr + 1);
    }

    /* ipv4 or domain. */
    if (!host_addr_len)
    {

        // if (((port_ptr) && (path_ptr) && (port_ptr < path_ptr)) || (port_ptr && (!path_ptr)))
        if ((port_ptr && path_ptr && (port_ptr < path_ptr) )|| (port_ptr && (!path_ptr)))
        {
            host_addr_len = port_ptr - host_addr;
        }
        else if (path_ptr)
        {
            host_addr_len = path_ptr - host_addr;
        }
        else
        {
            host_addr_len = strlen(host_addr);
        }

    }

    if ((host_addr_len < 1) || (host_addr_len > url_len))
    {
        rc = -WEBCLIENT_ERROR;
        goto __exit;
    }

    /* get host address ok. */
    {
        char *host_addr_new = web_malloc(host_addr_len + 1);

        if (!host_addr_new)
        {
            rc = -WEBCLIENT_ERROR;
            goto __exit;
        }

        memcpy(host_addr_new, host_addr, host_addr_len);
        host_addr_new[host_addr_len] = '\0';
        session->host = host_addr_new;
    }

    // BK_LOGI(TAG,"host address: %s , port: %s\r\n", session->host, port_str);

#ifdef WEBCLIENT_USING_MBED_TLS
    if (session->tls_session)
    {
        session->tls_session->port = web_strdup(port_str);
        session->tls_session->host = web_strdup(session->host);
        if (session->tls_session->port == RT_NULL || session->tls_session->host == RT_NULL)
        {
            return -WEBCLIENT_NOMEM;
        }

        return rc;
    }
#endif

    /* resolve the host name. */
    {
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
        int ret;
    // struct addrinfo *ress;

        // memset(&hint, 0, sizeof(hint));
        ret = getaddrinfo(session->host, port_str, &hints,res);
        // ret = getaddrinfo("baidu.com", "80", &hint, res);
        if (ret != 0)
        {
            BK_LOGE(TAG,"getaddrinfo err: %d '%s'.\r\n", ret, session->host);
            rc = -WEBCLIENT_ERROR;
            goto __exit;
        }
    }

__exit:
    if (rc != WEBCLIENT_OK)
    {
        if (session->host)
        {
            web_free(session->host);
            session->host = RT_NULL;
        }

        if (*res)
        {
            lwip_freeaddrinfo(*res);
            *res = RT_NULL;
        }
    }

    return rc;
}

#ifdef WEBCLIENT_USING_MBED_TLS
/**
 * create and initialize https session.
 *
 * @param session webclient session
 * @param URI input server URI address
 *
 * @return <0: create failed, no memory or other errors
 *         =0: success
 */
static int webclient_open_tls(struct webclient_session *session, const char *URI)
{
    int tls_ret = 0;
    const char *pers = "webclient";

    BK_ASSERT(session);

    session->tls_session = (MbedTLSSession *) web_calloc(1, sizeof(MbedTLSSession));
    if (session->tls_session == RT_NULL)
    {
        return -WEBCLIENT_NOMEM;
    }

    session->tls_session->buffer_len = WEBCLIENT_RESPONSE_BUFSZ;
    session->tls_session->buffer = web_malloc(session->tls_session->buffer_len);
    if(session->tls_session->buffer == RT_NULL)
    {
        BK_LOGE(TAG,"no memory for tls_session buffer!\r\n");
        return -WEBCLIENT_ERROR;
    }

    if((tls_ret = mbedtls_client_init(session->tls_session, (void *)pers, strlen(pers))) < 0)
    {
        BK_LOGE(TAG,"initialize https client failed return: -0x%x.\r\n", -tls_ret);
        return -WEBCLIENT_ERROR;
    }

    return WEBCLIENT_OK;
}
#endif

/**
 * connect to http server.
 *
 * @param session webclient session
 * @param URI the input server URI address
 *
 * @return <0: connect failed or other error
 *         =0: connect success
 */
int webclient_connect(struct webclient_session *session, const char *URI)
{
    int rc = WEBCLIENT_OK;
    int socket_handle;
    struct timeval timeout;
    struct addrinfo *res = RT_NULL;
    const char *req_url;

    BK_ASSERT(session);
    BK_ASSERT(URI);

    timeout.tv_sec = WEBCLIENT_DEFAULT_TIMEO;
    timeout.tv_usec = 0;

    if (strncmp(URI, "https://", 8) == 0)
    {
#if defined(WEBCLIENT_USING_SAL_TLS)
        session->is_tls = RT_TRUE;
#elif defined(WEBCLIENT_USING_MBED_TLS)
        if(webclient_open_tls(session, URI) < 0)
        {
            BK_LOGE(TAG,"connect failed, https client open URI(%s) failed!\r\n", URI);
            return -WEBCLIENT_ERROR;
        }
        session->is_tls = RT_TRUE;
#else
        BK_LOGE(TAG,"not support https connect, please enable webclient https configure!\r\n");
        rc = -WEBCLIENT_ERROR;
        goto __exit;
#endif
    }

    /* Check valid IP address and URL */
    rc = webclient_resolve_address(session, &res, URI, &req_url);
    if (rc != WEBCLIENT_OK)
    {
        BK_LOGE(TAG,"connect failed, resolve address error(%d).\r\n", rc);
        goto __exit;
    }

    /* Not use 'getaddrinfo()' for https connection */
    if (session->is_tls == 0 && res == RT_NULL)
    {
        rc = -WEBCLIENT_ERROR;
        goto __exit;
    }

    /* copy host address */
    if (req_url)
    {
        session->req_url = web_strdup(req_url);
    }
    else
    {
        BK_LOGE(TAG,"connect failed, resolve request address error.\r\n");
        rc = -WEBCLIENT_ERROR;
        goto __exit;
    }

#ifdef WEBCLIENT_USING_MBED_TLS
    if (session->tls_session)
    {
        int tls_ret = 0;

        if ((tls_ret = mbedtls_client_context(session->tls_session)) < 0)
        {
            BK_LOGE(TAG,"connect failed, https client context return: -0x%x\r\n", -tls_ret);
            return -WEBCLIENT_ERROR;
        }

        if ((tls_ret = mbedtls_client_connect(session->tls_session)) < 0)
        {
            BK_LOGE(TAG,"connect failed, https client connect return: -0x%x\r\n", -tls_ret);
            return -WEBCLIENT_CONNECT_FAILED;
        }

        socket_handle = session->tls_session->server_fd.fd;

        /* set recv timeout option */
        setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, (void*) &timeout,
                sizeof(timeout));
        setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, (void*) &timeout,
                sizeof(timeout));

        session->socket = socket_handle;

        return WEBCLIENT_OK;
    }
#endif

    {
#ifdef WEBCLIENT_USING_SAL_TLS
        if (session->is_tls)
        {
            socket_handle = socket(res->ai_family, SOCK_STREAM, PROTOCOL_TLS);
        }
        else
        {
            socket_handle = socket(res->ai_family, SOCK_STREAM, IPPROTO_TCP);
        }
#else
        socket_handle = socket(res->ai_family, SOCK_STREAM, IPPROTO_TCP);
#endif

        if (socket_handle < 0)
        {
            BK_LOGE(TAG,"connect failed, create socket(%d) error.\r\n", socket_handle);
            rc = -WEBCLIENT_NOSOCKET;
            goto __exit;
        }
        // int ret_socke = SOC_bindSocketDev_TCP(socket_handle);
        // if(ret_socke != -1)
        //     socket_handle = ret_socke;

        /* set receive and send timeout option */
        setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout,
                   sizeof(timeout));
        setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, (void *) &timeout,
                   sizeof(timeout));

        if (connect(socket_handle, res->ai_addr, res->ai_addrlen) != 0)
        {
            /* connect failed, close socket */
            BK_LOGE(TAG,"connect failed, connect socket(%d) error.\r\n", socket_handle);
            closesocket(socket_handle);
            rc = -WEBCLIENT_CONNECT_FAILED;
            goto __exit;
        }

        session->socket = socket_handle;
    }

__exit:
    if (res)
    {
        freeaddrinfo(res);
    }

    return rc;
}

/**
 * add fields data to request header data.
 *
 * @param session webclient session
 * @param fmt fields format
 *
 * @return >0: data length of successfully added
 *         <0: not enough header buffer size
 */
int webclient_header_fields_add(struct webclient_session *session, const char *fmt, ...)
{
    int32_t length;
    va_list args;

    BK_ASSERT(session);
    BK_ASSERT(session->header->buffer);

    va_start(args, fmt);
    length = vsnprintf(session->header->buffer + session->header->length,
            session->header->size - session->header->length, fmt, args);
    if (length < 0)
    {
        BK_LOGE(TAG,"add fields header data failed, return length(%d) error.\r\n", length);
        return -WEBCLIENT_ERROR;
    }
    va_end(args);

    session->header->length += length;

    /* check header size */
    if (session->header->length >= session->header->size)
    {
        BK_LOGE(TAG,"not enough header buffer size(%d)!\r\n", session->header->size);
        return -WEBCLIENT_ERROR;
    }

    return length;
}

/**
 * get fields information from request/response header data.
 *
 * @param session webclient session
 * @param fields fields keyword
 *
 * @return = NULL: get fields data failed
 *        != NULL: success get fields data
 */
const char *webclient_header_fields_get(struct webclient_session *session, const char *fields)
{
    char *resp_buf = RT_NULL;
    size_t resp_buf_len = 0;

    BK_ASSERT(session);
    BK_ASSERT(session->header->buffer);

    resp_buf = session->header->buffer;
    while (resp_buf_len < session->header->length)
    {
        if (webclient_strstri(resp_buf, fields))
        {
            char *mime_ptr = RT_NULL;

            /* jump space */
            mime_ptr = strstr(resp_buf, ":");
            if (mime_ptr != NULL)
            {
                mime_ptr += 1;

                while (*mime_ptr && (*mime_ptr == ' ' || *mime_ptr == '\t'))
                    mime_ptr++;

                return mime_ptr;
            }
        }

        if (*resp_buf == '\0')
            break;

        resp_buf += strlen(resp_buf) + 1;
        resp_buf_len += strlen(resp_buf) + 1;
    }

    return RT_NULL;
}

/**
 * get http response status code.
 *
 * @param session webclient session
 *
 * @return response status code
 */
int webclient_resp_status_get(struct webclient_session *session)
{
    BK_ASSERT(session);

    return session->resp_status;
}

/**
 * get http response data content length.
 *
 * @param session webclient session
 *
 * @return response content length
 */
int webclient_content_length_get(struct webclient_session *session)
{
    BK_ASSERT(session);

    return session->content_length;
}

int webclient_send_header(struct webclient_session *session, int method)
{
    int rc = WEBCLIENT_OK;
    char *header = RT_NULL;

    BK_ASSERT(session);

    header = session->header->buffer;

    if (session->header->length == 0)
    {
        /* use default header data */
        if (webclient_header_fields_add(session, "GET %s HTTP/1.1\r\n", session->req_url) < 0)
            return -WEBCLIENT_NOMEM;
        if (webclient_header_fields_add(session, "Host: %s\r\n", session->host) < 0)
            return -WEBCLIENT_NOMEM;
        if (webclient_header_fields_add(session, "User-Agent: BEKEN HTTP Agent\r\n\r\n") < 0)
            return -WEBCLIENT_NOMEM;

        webclient_write(session, (unsigned char *) session->header->buffer, session->header->length);
    }
    else
    {
        if (method != WEBCLIENT_USER_METHOD)
        {
            /* check and add fields header data */
            if (memcmp(header, "HTTP/1.", strlen("HTTP/1.")))
            {
                char *header_buffer = RT_NULL;
                int length = 0;

                header_buffer = web_strdup(session->header->buffer);
                if (header_buffer == RT_NULL)
                {
                    BK_LOGE(TAG,"no memory for header buffer!\r\n");
                    rc = -WEBCLIENT_NOMEM;
                    goto __exit;
                }

                /* splice http request header data */
                if (method == WEBCLIENT_GET)
                    length = snprintf(session->header->buffer, session->header->size, "GET %s HTTP/1.1\r\n%s",
                            session->req_url ? session->req_url : "/", header_buffer);
                else if (method == WEBCLIENT_POST)
                    length = snprintf(session->header->buffer, session->header->size, "POST %s HTTP/1.1\r\n%s",
                            session->req_url ? session->req_url : "/", header_buffer);
                session->header->length = length;

                web_free(header_buffer);

            }

            if (strstr(header, "Host:") == RT_NULL)
            {
                if (webclient_header_fields_add(session, "Host: %s\r\n", session->host) < 0)
                    return -WEBCLIENT_NOMEM;
            }

            if (strstr(header, "User-Agent:") == RT_NULL)
            {
                if (webclient_header_fields_add(session, "User-Agent: RT-Thread HTTP Agent\r\n") < 0)
                    return -WEBCLIENT_NOMEM;
            }

            if (strstr(header, "Accept:") == RT_NULL)
            {
                if (webclient_header_fields_add(session, "Accept: */*\r\n") < 0)
                    return -WEBCLIENT_NOMEM;
            }

            /* header data end */
            snprintf(session->header->buffer + session->header->length, session->header->size, "\r\n");
            session->header->length += 2;

            /* check header size */
            if (session->header->length > session->header->size)
            {
                BK_LOGE(TAG,"send header failed, not enough header buffer size(%d)!\r\n", session->header->size);
                rc = -WEBCLIENT_NOBUFFER;
                goto __exit;
            }

            webclient_write(session, (unsigned char *) session->header->buffer, session->header->length);
        }
        else
        {
            webclient_write(session, (unsigned char *) session->header->buffer, session->header->length);
        }
    }

    /* get and echo request header data */
    {
        char *header_str, *header_ptr;
        int header_line_len;
        BK_LOGI(TAG,"request header:\r\n");

        for(header_str = session->header->buffer; (header_ptr = strstr(header_str, "\r\n")) != RT_NULL; )
        {
            header_line_len = header_ptr - header_str;

            if (header_line_len > 0)
            {
                BK_LOGI(TAG,"%.*s\r\n", header_line_len, header_str);
            }
            header_str = header_ptr + strlen("\r\n");
        }
#ifdef WEBCLIENT_DEBUG
        LOG_RAW("\n");
#endif
    }

__exit:
    return rc;
}

/**
 * resolve server response data.
 *
 * @param session webclient session
 *
 * @return <0: resolve response data failed
 *         =0: success
 */
int webclient_handle_response(struct webclient_session *session)
{
    int rc = WEBCLIENT_OK;
    char *mime_buffer = RT_NULL;
    char *mime_ptr = RT_NULL;
    char *Cookie= RT_NULL;
    const char *transfer_encoding;
    int i;

    BK_ASSERT(session);

    /* clean header buffer and size */
    memset(session->header->buffer, 0x00, session->header->size);
    session->header->length = 0;

    BK_LOGI(TAG,"response header:\r\n");
    /* We now need to read the header information */
    while (1)
    {
        mime_buffer = session->header->buffer + session->header->length;

        /* read a line from the header information. */
        rc = webclient_read_line(session, mime_buffer, session->header->size - session->header->length);
        if (rc < 0)
            break;
        /* End of headers is a blank line.  exit. */
        if (rc == 0)
            break;
        if ((rc == 1) && (mime_buffer[0] == '\r'))
        {
            mime_buffer[0] = '\0';
            break;
        }

        /* set terminal charater */
        mime_buffer[rc - 1] = '\0';

        /* echo response header data */
        BK_LOGI(TAG,"%s\r\n", mime_buffer);

        session->header->length += rc;

        if (session->header->length >= session->header->size)
        {
            BK_LOGE(TAG,"not enough header buffer size(%d)!\r\n", session->header->size);
            return -WEBCLIENT_NOMEM;
        }
    }

    /* get HTTP status code */
    mime_ptr = web_strdup(session->header->buffer);
    if (mime_ptr == RT_NULL)
    {
        BK_LOGE(TAG,"no memory for get http status code buffer!\r\n");
        return -WEBCLIENT_NOMEM;
    }

    if (strstr(mime_ptr, "HTTP/1."))
    {
        char *ptr = mime_ptr;

        ptr += strlen("HTTP/1.x");

        while (*ptr && (*ptr == ' ' || *ptr == '\t'))
            ptr++;

        /* Terminate string after status code */
        for (i = 0; ((ptr[i] != ' ') && (ptr[i] != '\t')); i++);
        ptr[i] = '\0';

        session->resp_status = (int) strtol(ptr, RT_NULL, 10);
    }

    Cookie = (char *)webclient_header_fields_get(session, "Set-Cookie:");
    if (Cookie != RT_NULL)
    {   
        session->Cookie = web_strdup(Cookie);
    }
    /* get content length */
    if (webclient_header_fields_get(session, "Content-Length:") != RT_NULL)
    {
        session->content_length = atoi(webclient_header_fields_get(session, "Content-Length:"));
    }
    session->content_remainder = session->content_length ? (size_t) session->content_length : 0xFFFFFFFF;

    transfer_encoding = webclient_header_fields_get(session, "Transfer-Encoding:");
    if (transfer_encoding && strcmp(transfer_encoding, "chunked") == 0)
    {
        char line[16];

        /* chunk mode, we should get the first chunk size */
        webclient_read_line(session, line, session->header->size);
        session->chunk_sz = strtol(line, RT_NULL, 16);
        session->chunk_offset = 0;
    }

    if (mime_ptr)
    {
        web_free(mime_ptr);
    }


    if (rc < 0)
    {
        return rc;
    }

    return session->resp_status;
}

/**
 * create webclient session, set maximum header and response size
 *
 * @param header_sz maximum send header size
 * @param resp_sz maximum response data size
 *
 * @return  webclient session structure
 */
struct webclient_session *webclient_session_create(size_t header_sz)
{
    struct webclient_session *session;

    /* create session */
    session = (struct webclient_session *) web_calloc(1, sizeof(struct webclient_session));
    if (session == RT_NULL)
    {
        BK_LOGE(TAG,"webclient create failed, no memory for webclient session!\r\n");
        return RT_NULL;
    }

    /* initialize the socket of session */
    session->socket = -1;
    session->content_length = -1;

    session->header = (struct webclient_header *) web_calloc(1, sizeof(struct webclient_header));
    if (session->header == RT_NULL)
    {
        BK_LOGE(TAG,"webclient create failed, no memory for session header!\r\n");
        web_free(session);
        session = RT_NULL;
        return RT_NULL;
    }

    session->header->size = header_sz;
    session->header->buffer = (char *) web_calloc(1, header_sz);
    if (session->header->buffer == RT_NULL)
    {
        BK_LOGE(TAG,"webclient create failed, no memory for session header buffer!\r\n");
        web_free(session->header);
        web_free(session);
        session = RT_NULL;
        return RT_NULL;
    }

    return session;
}

static int webclient_clean(struct webclient_session *session);

/**
 *  send GET request to http server and get response header.
 *
 * @param session webclient session
 * @param URI input server URI address
 * @param header GET request header
 *             = NULL: use default header data
 *            != NULL: use custom header data
 *
 * @return <0: send GET request failed
 *         >0: response http status code
 */
int webclient_get(struct webclient_session *session, const char *URI)
{
    int rc = WEBCLIENT_OK;
    int resp_status = 0;

    BK_ASSERT(session);
    BK_ASSERT(URI);

    rc = webclient_connect(session, URI);
    if (rc != WEBCLIENT_OK)
    {
        /* connect to webclient server failed. */
        return rc;
    }

    rc = webclient_send_header(session, WEBCLIENT_GET);
    if (rc != WEBCLIENT_OK)
    {
        /* send header to webclient server failed. */
       return rc;
    }

    /* handle the response header of webclient server */
    resp_status = webclient_handle_response(session);

    BK_LOGI(TAG,"get position handle response(%d).\r\n", resp_status);

    if (resp_status > 0)
    {
        const char *location = webclient_header_fields_get(session, "Location:");
        /* relocation */
        if ((resp_status == 302 || resp_status == 301) && location)
        {
            char *new_url;

            new_url = web_strdup(location);
            if (new_url == RT_NULL)
            {
                return -WEBCLIENT_NOMEM;
            }

            /* clean webclient session */
            webclient_clean(session);
            /* clean webclient session header */
            session->header->length = 0;
            memset(session->header->buffer, 0, session->header->size);
            rc = webclient_get(session, new_url);
            web_free(new_url);
            return rc;
        }
    }

    return resp_status;
}

/**
 *  http breakpoint resume.
 *
 * @param session webclient session
 * @param URI input server URI address
 * @param position last downloaded position
 *
 * @return <0: send GET request failed
 *         >0: response http status code
 */
int webclient_get_position(struct webclient_session *session, const char *URI, int position)
{
    int rc = WEBCLIENT_OK;
    int resp_status = 0;

    BK_ASSERT(session);
    BK_ASSERT(URI);

    rc = webclient_connect(session, URI);
    if (rc != WEBCLIENT_OK)
    {
        return rc;
    }

    /* splice header*/
    if (webclient_header_fields_add(session, "Range: bytes=%d-\r\n", position) <= 0)
    {
        rc = -WEBCLIENT_ERROR;
        return rc;
    }

    rc = webclient_send_header(session, WEBCLIENT_GET);
    if (rc != WEBCLIENT_OK)
    {
        return rc;
    }

    /* handle the response header of webclient server */
    resp_status = webclient_handle_response(session);

    BK_LOGI(TAG,"get position handle response(%d).\r\n", resp_status);

    if (resp_status > 0)
    {
        const char *location = webclient_header_fields_get(session, "Location:");
        /* relocation */
        if ((resp_status == 302 || resp_status == 301) && location)
        {
            char *new_url;
            new_url = web_strdup(location);
            if (new_url == RT_NULL)
            {
                return -WEBCLIENT_NOMEM;
            }

            /* clean webclient session */
            webclient_clean(session);
            /* clean webclient session header */
            session->header->length = 0;
            memset(session->header->buffer, 0, session->header->size);
            rc = webclient_get_position(session, new_url, position);
            web_free(new_url);
            return rc;
        }
    }

    return resp_status;
}

/**
 * send POST request to server and get response header data.
 *
 * @param session webclient session
 * @param URI input server URI address
 * @param header POST request header, can't be empty
 * @param post_data data send to the server
 *                = NULL: just connect server and send header
 *               != NULL: send header and body data, resolve response data
 * @param data_len the length of send data
 *
 * @return <0: send POST request failed
 *         =0: send POST header success
 *         >0: response http status code
 */
int webclient_post(struct webclient_session *session, const char *URI, const void *post_data, size_t data_len)
{
    int rc = WEBCLIENT_OK;
    int resp_status = 0;

    BK_ASSERT(session);
    BK_ASSERT(URI);

    if ((post_data != RT_NULL) && (data_len == 0))
    {
        BK_LOGE(TAG,"input post data length failed.\r\n");
        return -WEBCLIENT_ERROR;
    }

    rc = webclient_connect(session, URI);
    if (rc != WEBCLIENT_OK)
    {
        /* connect to webclient server failed. */
        return rc;
    }

    rc = webclient_send_header(session, WEBCLIENT_POST);
    if (rc != WEBCLIENT_OK)
    {
        /* send header to webclient server failed. */
        return rc;
    }

    if (post_data && (data_len > 0))
    {
        webclient_write(session, post_data, data_len);
    }

    /* resolve response data, get http status code */
    resp_status = webclient_handle_response(session);
    BK_LOGI(TAG,"post handle response(%d).\r\n", resp_status);

    return resp_status;
}

#ifdef XIAOYA_OS
/*to solve non char array problem.*/
int webclient_post_extern(struct webclient_session *session, const char *URI, const void *post_data,size_t len)
{
    int rc = WEBCLIENT_OK;
    int resp_status = 0;

    BK_ASSERT(session);
    BK_ASSERT(URI);

    rc = webclient_connect(session, URI);
    if (rc != WEBCLIENT_OK)
    {
        /* connect to webclient server failed. */
        return rc;
    }

    rc = webclient_send_header(session, WEBCLIENT_POST);
    if (rc != WEBCLIENT_OK)
    {
        /* send header to webclient server failed. */
        return rc;
    }

    if (post_data)
    {
        webclient_write(session, (unsigned char *) post_data, len);

        /* resolve response data, get http status code */
        resp_status = webclient_handle_response(session);
        BK_LOGI(TAG,"post handle response(%d).\r\n", resp_status);
    }

    return resp_status;
}
#endif

/**
 * set receive and send data timeout.
 *
 * @param session http session
 * @param millisecond timeout millisecond
 *
 * @return 0: set timeout success
 */
int webclient_set_timeout(struct webclient_session *session, int millisecond)
{
    struct timeval timeout;
    int second = (millisecond) / 1000;

    BK_ASSERT(session);

    timeout.tv_sec = second;
    timeout.tv_usec = 0;

    /* set recv timeout option */
    setsockopt(session->socket, SOL_SOCKET, SO_RCVTIMEO,
               (void *) &timeout, sizeof(timeout));
    setsockopt(session->socket, SOL_SOCKET, SO_SNDTIMEO,
               (void *) &timeout, sizeof(timeout));

    return 0;
}

static int webclient_next_chunk(struct webclient_session *session)
{
    char line[64];
    int length;

    BK_ASSERT(session);

    memset(line, 0x00, sizeof(line));
    length = webclient_read_line(session, line, sizeof(line));
    if (length > 0)
    {
        if (strcmp(line, "\r") == 0)
        {
            length = webclient_read_line(session, line, sizeof(line));
            if (length <= 0)
            {
                closesocket(session->socket);
                session->socket = -1;
                return length;
            }
        }
    }
    else
    {
        closesocket(session->socket);
        session->socket = -1;

        return length;
    }

    session->chunk_sz = strtol(line, RT_NULL, 16);
    session->chunk_offset = 0;

    if (session->chunk_sz == 0)
    {
        /* end of chunks */
        closesocket(session->socket);
        session->socket = -1;
        session->chunk_sz = -1;
    }

    return session->chunk_sz;
}

/**
 *  read data from http server.
 *
 * @param session http session
 * @param buffer read buffer
 * @param length the maximum of read buffer size
 *
 * @return <0: read data error
 *         =0: http server disconnect
 *         >0: successfully read data length
 */
int webclient_read(struct webclient_session *session, void *buffer, size_t length)
{
    int bytes_read = 0;
    int total_read = 0;
    int left;

    BK_ASSERT(session);

    /* get next chunk size is zero, client is already closed, return zero */
    if (session->chunk_sz < 0)
    {
        return 0;
    }

    if (session->socket < 0)
    {
        return -WEBCLIENT_DISCONNECT;
    }

    if (length == 0)
    {
        return 0;
    }

    /* which is transfered as chunk mode */
    if (session->chunk_sz)
    {
        if ((int) length > (session->chunk_sz - session->chunk_offset))
        {
            length = session->chunk_sz - session->chunk_offset;
        }

        bytes_read = webclient_recv(session, buffer, length, 0);
        if (bytes_read <= 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                /* recv timeout */
                return -WEBCLIENT_TIMEOUT;
            }
            else
            {
                closesocket(session->socket);
                session->socket = -1;
                return 0;
            }
        }

        session->chunk_offset += bytes_read;
        if (session->chunk_offset >= session->chunk_sz)
        {
            webclient_next_chunk(session);
        }

        return bytes_read;
    }

    if (session->content_length > 0)
    {
        if (length > session->content_remainder)
        {
            length = session->content_remainder;
        }

        if (length == 0)
        {
            return 0;
        }
    }

    /*
     * Read until: there is an error, we've read "size" bytes or the remote
     * side has closed the connection.
     */
    left = length;
    do
    {
        bytes_read = webclient_recv(session, (void *)((char *)buffer + total_read), left, 0);
        if (bytes_read <= 0)
        {
#if defined(WEBCLIENT_USING_SAL_TLS) || defined(WEBCLIENT_USING_MBED_TLS)
            if(session->is_tls &&
                (bytes_read == MBEDTLS_ERR_SSL_WANT_READ || bytes_read == MBEDTLS_ERR_SSL_WANT_WRITE))
            {
                continue;
            }

#endif
            BK_LOGI(TAG,"receive data error(%d).\r\n", bytes_read);

            if (total_read)
            {
                break;
            }
            else
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN)
                {
                    /* recv timeout */
                    BK_LOGI(TAG,"receive data timeout.\r\n");
                    return -WEBCLIENT_TIMEOUT;
                }
                else
                {
                    closesocket(session->socket);
                    session->socket = -1;
                    return 0;
                }
            }
        }

        left -= bytes_read;
        total_read += bytes_read;
    }
    while (left);

    if (session->content_length > 0)
    {
        session->content_remainder -= total_read;
    }

    return total_read;
}

/**
 *  write data to http server.
 *
 * @param session http session
 * @param buffer write buffer
 * @param length write buffer size
 *
 * @return <0: write data error
 *         =0: http server disconnect
 *         >0: successfully write data length
 */
int webclient_write(struct webclient_session *session, const void *buffer, size_t length)
{
    int bytes_write = 0;
    int total_write = 0;
    int left = length;

    BK_ASSERT(session);

    if (session->socket < 0)
    {
        return -WEBCLIENT_DISCONNECT;
    }

    if (length == 0)
    {
        return 0;
    }

    /* send all of data on the buffer. */
    do
    {
        bytes_write = webclient_send(session, (void *)((char *)buffer + total_write), left, 0);
        if (bytes_write <= 0)
        {
#if defined(WEBCLIENT_USING_SAL_TLS) || defined(WEBCLIENT_USING_MBED_TLS)
            if(session->is_tls &&
                (bytes_write == MBEDTLS_ERR_SSL_WANT_READ || bytes_write == MBEDTLS_ERR_SSL_WANT_WRITE))
            {
                continue;
            }
#endif
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                /* send timeout */
                if (total_write)
                {
                    return total_write;
                }
                continue;
                /* TODO: whether return the TIMEOUT
                 * return -WEBCLIENT_TIMEOUT; */
            }
            else
            {
                closesocket(session->socket);
                session->socket = -1;

                if (total_write == 0)
                {
                    return -WEBCLIENT_DISCONNECT;
                }
                break;
            }
        }

        left -= bytes_write;
        total_write += bytes_write;
    }
    while (left);

    return total_write;
}

/* close session socket, free host and request url */
static int webclient_clean(struct webclient_session *session)
{
#ifdef WEBCLIENT_USING_MBED_TLS
    if (session->tls_session)
    {
        mbedtls_client_close(session->tls_session);
    }
    else
    {
        if (session->socket >= 0)
        {
            closesocket(session->socket);
            session->socket = -1;
        }
    }
#else
    if (session->socket >= 0)
    {
        closesocket(session->socket);
        session->socket = -1;
    }
#endif

    if(session->Cookie)
    {
        web_free(session->Cookie);
        session->Cookie=NULL;
    }

    if (session->host)
    {
        web_free(session->host);
        session->host=NULL;
    }

    if (session->req_url)
    {
        web_free(session->req_url);
        session->req_url=NULL;
    }

    session->content_length = -1;

    return 0;
}

/**
 * close a webclient client session.
 *
 * @param session http client session
 *
 * @return 0: close success
 */
int webclient_close(struct webclient_session *session)
{
    BK_ASSERT(session);

    webclient_clean(session);

    if (session->header && session->header->buffer)
    {
        web_free(session->header->buffer);
        session->header->buffer=NULL;
    }

    if (session->header)
    {
        web_free(session->header);
        session->header=NULL;
    }

    if (session)
    {
        web_free(session);
        session = RT_NULL;
    }

    return 0;
}

/**
 * get wenclient request response data.
 *
 * @param session wenclient session
 * @param response response buffer address
 * @param resp_len response buffer length
 *
 * @return response data size
 */
int webclient_response(struct webclient_session *session, void **response, size_t *resp_len)
{
    unsigned char *buf_ptr;
    unsigned char *response_buf = 0;
    int length, total_read = 0;

    BK_ASSERT(session);
    BK_ASSERT(response);

    /* initialize response */
    *response = RT_NULL;

    /* not content length field kind */
    if (session->content_length < 0)
    {
        size_t result_sz;

        total_read = 0;
        while (1)
        {
            unsigned char *new_resp = RT_NULL;

            result_sz = total_read + WEBCLIENT_RESPONSE_BUFSZ;
            new_resp = web_realloc(response_buf, result_sz + 1);
            if (new_resp == RT_NULL)
            {
                BK_LOGE(TAG,"no memory for realloc new response buffer!\r\n");
                break;
            }

            response_buf = new_resp;
            buf_ptr = (unsigned char *) response_buf + total_read;

            /* read result */
            length = webclient_read(session, buf_ptr, result_sz - total_read);
            if (length <= 0)
                break;

            total_read += length;
        }
    }
    else
    {
        int result_sz;

        result_sz = session->content_length;
        response_buf = web_calloc(1, result_sz + 1);
        if (response_buf == RT_NULL)
        {
            return -WEBCLIENT_NOMEM;
        }

        buf_ptr = (unsigned char *) response_buf;
        for (total_read = 0; total_read < result_sz;)
        {
            length = webclient_read(session, buf_ptr, result_sz - total_read);
            if (length <= 0)
                break;

            buf_ptr += length;
            total_read += length;
        }
    }

    if ((total_read == 0) && (response_buf != 0))
    {
        web_free(response_buf);
        response_buf = RT_NULL;
    }

    if (response_buf)
    {
        *response = (void *)response_buf;
        *(response_buf + total_read) = '\0';
        *resp_len = total_read;
    }

    return total_read;
}

/**
 * add request(GET/POST) header data.
 *
 * @param request_header add request buffer address
 * @param fmt fields format
 *
 * @return <=0: add header failed
 *          >0: add header data size
 */

int webclient_request_header_add(char **request_header, const char *fmt, ...)
{
    int32_t length, header_length;
    char *header;
    va_list args;

    BK_ASSERT(request_header);

    if (*request_header == RT_NULL)
    {
        header = os_calloc(1, WEBCLIENT_HEADER_BUFSZ);
        if (header == RT_NULL)
        {
            BK_LOGE(TAG,"No memory for webclient request header add.\r\n");
            return -1;
        }
        *request_header = header;
    }
    else
    {
        header = *request_header;
    }

    va_start(args, fmt);
    header_length = strlen(header);
    length = vsnprintf(header + header_length, WEBCLIENT_HEADER_BUFSZ - header_length, fmt, args);
    if (length < 0)
    {
        BK_LOGE(TAG,"add request header data failed, return length(%d) error.\r\n", length);
        return -WEBCLIENT_ERROR;
    }
    va_end(args);

    /* check header size */
    if (strlen(header) >= WEBCLIENT_HEADER_BUFSZ)
    {
        BK_LOGE(TAG,"not enough request header data size(%d)!\r\n", WEBCLIENT_HEADER_BUFSZ);
        return -WEBCLIENT_ERROR;
    }

    return length;
}

/**
 *  send request(GET/POST) to server and get response data.
 *
 * @param URI input server address
 * @param header send header data
 *             = NULL: use default header data, must be GET request
 *            != NULL: user custom header data, GET or POST request
 * @param post_data data sent to the server
 *             = NULL: it is GET request
 *            != NULL: it is POST request
 * @param data_len send data length
 * @param response response buffer address
 * @param resp_len response buffer length
 *
 * @return <0: request failed
 *        >=0: response buffer size
 */
int webclient_request(const char *URI, const char *header, const void *post_data, size_t data_len, void **response, size_t *resp_len)
{
    struct webclient_session *session = RT_NULL;
    int rc = WEBCLIENT_OK;
    int totle_length = 0;

    BK_ASSERT(URI);

    if (post_data == RT_NULL && response == RT_NULL)
    {
        BK_LOGE(TAG,"request get failed, get response data cannot be empty.\r\n");
        return -WEBCLIENT_ERROR;
    }

    if ((post_data != RT_NULL) && (data_len == 0))
    {
        BK_LOGE(TAG,"input post data length failed.\r\n");
        return -WEBCLIENT_ERROR;
    }

    if ((response != RT_NULL && resp_len == RT_NULL) || 
        (response == RT_NULL && resp_len != RT_NULL))
    {
        BK_LOGE(TAG,"input response data or length failed.\r\n");
        return -WEBCLIENT_ERROR;
    }

    if (post_data == RT_NULL)
    {
        /* send get request */
        session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ);
        if (session == RT_NULL)
        {
            rc = -WEBCLIENT_NOMEM;
            goto __exit;
        }

        if (header != RT_NULL)
        {
            char *header_str, *header_ptr;
            int header_line_length;

            for(header_str = (char *)header; (header_ptr = strstr(header_str, "\r\n")) != RT_NULL; )
            {
                header_line_length = header_ptr + strlen("\r\n") - header_str;
                webclient_header_fields_add(session, "%.*s", header_line_length, header_str);
                header_str += header_line_length;
            }
        }

        if (webclient_get(session, URI) != 200)
        {
            rc = -WEBCLIENT_ERROR;
            goto __exit;
        }

        totle_length = webclient_response(session, response, resp_len);
        if (totle_length <= 0)
        {
            rc = -WEBCLIENT_ERROR;
            goto __exit;
        }
    }
    else
    {
        /* send post request */
        session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ);
        if (session == RT_NULL)
        {
            rc = -WEBCLIENT_NOMEM;
            goto __exit;
        }

        if (header != RT_NULL)
        {
            char *header_str, *header_ptr;
            int header_line_length;

            for(header_str = (char *)header; (header_ptr = strstr(header_str, "\r\n")) != RT_NULL; )
            {
                header_line_length = header_ptr + strlen("\r\n") - header_str;
                webclient_header_fields_add(session, "%.*s", header_line_length, header_str);
                header_str += header_line_length;
            }
        }

        if (strstr(session->header->buffer, "Content-Length") == RT_NULL)
        {
            webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
        }

        if (strstr(session->header->buffer, "Content-Type") == RT_NULL)
        {
            webclient_header_fields_add(session, "Content-Type: application/octet-stream\r\n");
        }

        if (webclient_post(session, URI, post_data, data_len) != 200)
        {
            rc = -WEBCLIENT_ERROR;
            goto __exit;
        }

        totle_length = webclient_response(session, response, resp_len);
        if (totle_length <= 0)
        {
            rc = -WEBCLIENT_ERROR;
            goto __exit;
        }
    }

__exit:
    if (session)
    {
        webclient_close(session);
        session = RT_NULL;
    }

    if (rc < 0)
    {
        return rc;
    }

    return totle_length;
}

static bk_err_t bk_webclient_dispatch_event(bk_webclient_input_t *client, bk_webclient_event_id_t event_id, void *data, int len)
{
	bk_webclient_event_t *event = &client->event;

	if (client->event_handler) {
		event->event_id = event_id;
		event->data = data;
		event->data_len = len;
		return client->event_handler(event);
	}
	return BK_OK;
}

/**************************HTTP GET/POST INTERFACE***************************************/
int bk_webclient_get(bk_webclient_input_t *input) {
    int ret = 0;
	struct webclient_session* session = RT_NULL;
	unsigned char *buffer = RT_NULL;
	int bytes_read, resp_status;
	int content_length = -1;
	char *url = RT_NULL;

	url = web_strdup(input->url);
	if(url == RT_NULL)
	{
		BK_LOGE(TAG,"no memory for create get request uri buffer.\n");
		return BK_ERR_NO_MEM;
	}

	buffer = (unsigned char *) web_malloc(input->rx_buffer_size);
	if (buffer == RT_NULL)
	{
		BK_LOGE(TAG,"no memory for receive buffer.\n");
		ret = BK_ERR_NO_MEM;
		goto __exit;

	}

	/* create webclient session and set header response size */
	session = webclient_session_create(input->header_size);
	if (session == RT_NULL)
	{
		ret = BK_ERR_NO_MEM;
		goto __exit;
	}

	/* send GET request by default header */
	if ((resp_status = webclient_get(session, url)) != 200)
	{
		BK_LOGE(TAG,"webclient GET request failed, response(%d) error.\n", resp_status);
		ret = BK_ERR_STATE;
		goto __exit;
	}

	BK_LOGI(TAG,"webclient get response data: \n");

	content_length = webclient_content_length_get(session);
	if (content_length < 0)
	{
		BK_LOGI(TAG,"webclient GET request type is chunked.\n");
		do
		{
			bytes_read = webclient_read(session, (void *)buffer, input->rx_buffer_size);
			if (bytes_read <= 0)
			{
				break;
			}

		} while (1);

		BK_LOGI(TAG,"\n");
	}
	else
	{
		int content_pos = 0;

		do
		{
			bytes_read = webclient_read(session, (void *)buffer,
					content_length - content_pos > input->rx_buffer_size ?
							input->rx_buffer_size : content_length - content_pos);
			if (bytes_read <= 0)
			{
				break;
			}
			content_pos += bytes_read;
			BK_LOGI(TAG,"%s bytes_read:%d content_pos:%d\n", __func__, bytes_read, content_pos);
		} while (content_pos < content_length);

		if (content_pos != content_length) {
			BK_LOGI(TAG,"%s error! recv:%d content_length:%d\n", __func__, content_pos, content_length);
			ret = BK_ERR_STATE;
		}
	}

__exit:
	if (session)
	{
		webclient_close(session);
	}

	if (buffer)
	{
		web_free(buffer);
	}

	if (url)
	{
		web_free(url);
	}

	return ret;
}

int bk_webclient_post(bk_webclient_input_t *input) {
    int ret = 0;
	int rx_buffer_size = input->rx_buffer_size;
	int header_size = input->header_size;
	char *uri = os_strdup(input->url);
	char *post_data = NULL;
	size_t data_len = 0;
	struct webclient_session* session = RT_NULL;
	unsigned char *buffer = RT_NULL;
	int bytes_read, resp_status;

	if (input->post_data) {
		post_data = os_strdup(input->post_data);
		data_len = strlen(post_data);
	}
	char *rep_data = ( char *) os_zalloc(POST_DATA_LEN);
	if (rep_data == RT_NULL)
	{
		BK_LOGE(TAG,"no memory for receive response buffer.\n");
		ret = -5;
		goto __exit;
	}

	buffer = (unsigned char *) web_malloc(rx_buffer_size);
	if (buffer == RT_NULL)
	{
		BK_LOGE(TAG,"no memory for receive response buffer.\n");
		ret = -5;
		goto __exit;
	}

	/* create webclient session and set header response size */
	session = webclient_session_create(header_size);
	if (session == RT_NULL)
	{
		ret = -5;
		goto __exit;
	}
	char version_buf[32] ="7256000";

	/* build header for upload */
	webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
	webclient_header_fields_add(session, "Content-Type: application/x-www-form-urlencoded\r\n");
	webclient_header_fields_add(session, "%s\r\n",version_buf);

	/* send POST request by default header */
	if (post_data && ((resp_status = webclient_post(session, uri, post_data, data_len)) != 200))
	{
		BK_LOGE(TAG,"webclient POST request failed, response(%d) error.\n", resp_status);
		ret = -1;
		goto __exit;
	}

	BK_LOGI(TAG,"webclient post response data: \n");
	do
	{
		bytes_read = webclient_read(session, buffer, rx_buffer_size);
		if (bytes_read <= 0)
		{
			break;
		}
		strncat(rep_data,(char*)buffer,bytes_read);
	} while (1);

	BK_LOGI(TAG,"%s.\n", rep_data);

__exit:
	if (session)
	{
		webclient_close(session);
	}

	if (buffer)
	{
		web_free(buffer);
	}

	if (uri)
	{
		web_free(uri);
	}
	if (post_data)
	{
		web_free(post_data);
	}
	if (rep_data)
	{
		web_free(rep_data);
	}

	return ret;

    BK_LOGI(TAG,"demo_webclient post case result(%d).\n", ret);
    return ret;
}

/**************************HTTP OTA INTERFACE***************************************/
int bk_webclient_ota_get_comm(bk_webclient_input_t *input)
{
    struct webclient_session* session = RT_NULL;
    unsigned char *buffer = RT_NULL;
    int ret = 0;
    int bytes_read, resp_status;
    int content_length = -1;
	char *url = RT_NULL;

	url = web_strdup(input->url);
	if(url == RT_NULL)
	{
		BK_LOGE(TAG,"no memory for create get request uri buffer.\n");
		return BK_ERR_NO_MEM;
	}

    buffer = (unsigned char *) web_malloc(input->rx_buffer_size);
    if (buffer == RT_NULL)
    {
        BK_LOGE(TAG,"no memory for receive buffer.\n");
        ret = BK_ERR_NO_MEM;
        goto __exit;

    }

    /* create webclient session and set header response size */
    session = webclient_session_create(input->header_size);
    if (session == RT_NULL)
    {
        ret = BK_ERR_NO_MEM;
        goto __exit;
    }

    /* send GET request by default header */
    if ((resp_status = webclient_get(session, url)) != 200)
    {
        BK_LOGE(TAG,"webclient GET request failed, response(%d) error.\n", resp_status);
        ret = BK_ERR_STATE;
        goto __exit;
    }

    BK_LOGI(TAG,"webclient get response data: \n");

    content_length = webclient_content_length_get(session);
    if (content_length < 0)
    {
        BK_LOGI(TAG,"webclient GET request type is chunked.\n");
        do
        {
            bytes_read = webclient_read(session, (void *)buffer, input->rx_buffer_size);
            if (bytes_read <= 0)
            {
                break;
            }

        } while (1);

        BK_LOGI(TAG,"\n");
    }
    else
    {
        int content_pos = 0;

        do
        {
            bytes_read = webclient_read(session, (void *)buffer,
                    content_length - content_pos > input->rx_buffer_size ?
                            input->rx_buffer_size : content_length - content_pos);
            if (bytes_read <= 0)
            {
                break;
            }
          	bk_webclient_dispatch_event(input, HTTP_EVENT_ON_DATA, buffer, bytes_read);
            content_pos += bytes_read;
			BK_LOGI(TAG,"%s bytes_read:%d content_pos:%d\n", __func__, bytes_read, content_pos);
        } while (content_pos < content_length);

		if (content_pos != content_length) {
			BK_LOGI(TAG,"%s error! recv:%d content_length:%d\n", __func__, content_pos, content_length);
			bk_webclient_dispatch_event(input, HTTP_EVENT_ERROR, NULL, 0);
		 	ret = BK_ERR_STATE;
		}
		else
			bk_webclient_dispatch_event(input, HTTP_EVENT_ON_FINISH, NULL, 0);
    }

__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }

    if (url)
    {
        web_free(url);
    }

    return ret;
}

/**************************OTA DEMO***************************************/
bk_err_t demo_webclient_ota_event_cb(bk_webclient_event_t *evt)
{
    if(!evt)
    {
        return BK_FAIL;
    }

    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
	BK_LOGD(TAG, "HTTPS_EVENT_ERROR\r\n");
	break;
    case HTTP_EVENT_ON_CONNECTED:
	BK_LOGD(TAG, "HTTPS_EVENT_ON_CONNECTED\r\n");
	break;
    case HTTP_EVENT_HEADERS_SENT:
	BK_LOGD(TAG, "HTTPS_EVENT_HEADER_SENT\r\n");
	break;
    case HTTP_EVENT_ON_HEADER:
	BK_LOGD(TAG, "HTTPS_EVENT_ON_HEADER\r\n");
	break;
    case HTTP_EVENT_ON_DATA:
	//do something: evt->data, evt->data_len
	BK_LOGD(TAG, "HTTP_EVENT_ON_DATA, length:%d\r\n", evt->data_len);
	break;
    case HTTP_EVENT_ON_FINISH:
	BK_LOGD(TAG, "HTTPS_EVENT_ON_FINISH\r\n");
	break;
    case HTTP_EVENT_DISCONNECTED:
	BK_LOGD(TAG, "HTTPS_EVENT_DISCONNECTED\r\n");
	break;

    }
    return BK_OK;
}

int demo_webclient_ota_get(char *webclient_url)
{
	int err;

	if(!webclient_url)
	{
		err = BK_FAIL;
		BK_LOGD(TAG, "url is NULL\r\n");

		return err;
	}
	bk_webclient_input_t config = {
	    .url = webclient_url,
	    .event_handler = demo_webclient_ota_event_cb,
	    .header_size = 1024*2,
	    .rx_buffer_size = 1024*4
	};

	err = bk_webclient_ota_get_comm(&config);
	if(err == BK_OK){
		BK_LOGD(TAG, "webclient_ota_get_comm ok\r\n");
        bk_reboot();
	}
	else{
		BK_LOGD(TAG, "webclient_ota_get_comm fail, err:%x\r\n", err);
	}

	return err;
}

#if 1
#define RCV_BUF_SIZE                1024*3
#define SEND_HEADER_SIZE              1024
int test_http_post_case1(void)
{
    char buf_url[] = "http://www.rt-thread.com/service/echo";
    char post_data[] = "devicesCode=120002000814&userKey=6b6d71e72c70d136179177ddd231af7d&devicesName=bk7256&random=ZWVUTR&token=2c70426994";
    char *uri;
    uri = os_strdup(buf_url);
    size_t data_len;
    char rep_data[2*1024] = {0};
    data_len = sizeof(post_data);
    struct webclient_session* session = RT_NULL;
    unsigned char *buffer = RT_NULL;
    int  ret = 0;
    int bytes_read, resp_status;

    buffer = (unsigned char *) web_malloc(RCV_BUF_SIZE);
    if (buffer == RT_NULL)
    {
        BK_LOGE(TAG,"no memory for receive response buffer.\n");
        ret = -5;
        goto __exit;
    }

    /* create webclient session and set header response size */
    session = webclient_session_create(SEND_HEADER_SIZE);
    if (session == RT_NULL)
    {
        ret = -5;
        goto __exit;
    }
    char version_buf[32] ="7256000";

    /* build header for upload */
    webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
    webclient_header_fields_add(session, "Content-Type: application/x-www-form-urlencoded\r\n");
    webclient_header_fields_add(session, "%s\r\n",version_buf);

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, uri, post_data, data_len)) != 200)
    {
        BK_LOGE(TAG,"webclient POST request failed, response(%d) error.\n", resp_status);
        ret = -1;
        goto __exit;
    }

    BK_LOGI(TAG,"webclient post response data: \n");
    do
    {
        bytes_read = webclient_read(session, buffer, RCV_BUF_SIZE);
        if (bytes_read <= 0)
        {
            break;
        }
        strncat(rep_data,(char*)buffer,bytes_read);
    } while (1);

    BK_LOGI(TAG,"rep_data %s.\n", rep_data);

__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }

    return ret;

}

#define GET_HEADER_BUFSZ               1024
#define GET_RESP_BUFSZ                 1024


/* send HTTP GET request by common request interface, it used to receive longer data */
static int webclient_get_comm(const char *uri)
{
    struct webclient_session* session = RT_NULL;
    unsigned char *buffer = RT_NULL;
    int index, ret = 0;
    int bytes_read, resp_status;
    int content_length = -1;

    buffer = (unsigned char *) web_malloc(GET_RESP_BUFSZ);
    if (buffer == RT_NULL)
    {
        BK_LOGE(TAG,"no memory for receive buffer.\n");
        ret = BK_ERR_NO_MEM;
        goto __exit;

    }

    /* create webclient session and set header response size */
    session = webclient_session_create(GET_HEADER_BUFSZ);
    if (session == RT_NULL)
    {
        ret = BK_ERR_NO_MEM;
        goto __exit;
    }

    /* send GET request by default header */
    if ((resp_status = webclient_get(session, uri)) != 200)
    {
        BK_LOGE(TAG,"webclient GET request failed, response(%d) error.\n", resp_status);
        ret = BK_ERR_STATE;
        goto __exit;
    }

    BK_LOGI(TAG,"webclient get response data: \n");

    content_length = webclient_content_length_get(session);
    if (content_length < 0)
    {
        BK_LOGI(TAG,"webclient GET request type is chunked.\n");
        do
        {
            bytes_read = webclient_read(session, (void *)buffer, GET_RESP_BUFSZ);
            if (bytes_read <= 0)
            {
                break;
            }

            for (index = 0; index < bytes_read; index++)
            {
                BK_LOG_RAW("%c", buffer[index]);
            }
        } while (1);

        BK_LOGI(TAG,"\n");
    }
    else
    {
        int content_pos = 0;

        do
        {
            bytes_read = webclient_read(session, (void *)buffer,
                    content_length - content_pos > GET_RESP_BUFSZ ?
                            GET_RESP_BUFSZ : content_length - content_pos);
            if (bytes_read <= 0)
            {
                break;
            }

            for (index = 0; index < bytes_read; index++)
            {
                BK_LOG_RAW("%c", buffer[index]);
            }

            content_pos += bytes_read;
        } while (content_pos < content_length);

        BK_LOGI(TAG,"\n");
    }

__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }

    return ret;
}

#define GET_LOCAL_URI                  "http://www.rt-thread.com/service/rt-thread.txt"

int test_http_get_case1(void)
{
    char *uri = RT_NULL;


    uri = web_strdup(GET_LOCAL_URI);
    if(uri == RT_NULL)
    {
        BK_LOGE(TAG,"no memory for create get request uri buffer.\n");
        return BK_ERR_NO_MEM;
    }

    webclient_get_comm(uri);


    if (uri)
    {
        web_free(uri);
    }

    return 0;
}

/* send HTTP GET request by simplify request interface, it used to received shorter data */
static int webclient_get_smpl(const char *uri)
{
    char *response = RT_NULL;
    size_t resp_len = 0;
    int index;

    if (webclient_request(uri, RT_NULL, RT_NULL, 0, (void **)&response, &resp_len) < 0)
    {
        BK_LOGE(TAG,"webclient send get request failed.\n");
        return BK_FAIL;
    }

    BK_LOGI(TAG,"webclient send get request by simplify request interface.\n");
    BK_LOGI(TAG,"webclient get response data: \n");
    for (index = 0; index < os_strlen(response); index++)
    {
        BK_LOG_RAW("%c", response[index]);
    }
    BK_LOGI(TAG,"\n");

    if (response)
    {
        web_free(response);
    }

    return 0;
}

int test_http_get_case2(void)
{
    char *uri = RT_NULL;


    uri = web_strdup(GET_LOCAL_URI);
    if(uri == RT_NULL)
    {
        BK_LOGE(TAG,"no memory for create get request uri buffer.\n");
        return BK_ERR_NO_MEM;
    }

    webclient_get_smpl(uri);


    if (uri)
    {
        web_free(uri);
    }

    return 0;
}

int test_http(void) {
    int ret = 0;

    ret = test_http_post_case1();
    BK_LOGI(TAG,"http test post case1 result(%d).\n", ret);
    ret |= test_http_get_case1();
    BK_LOGI(TAG,"http test get case1 result(%d).\n", ret);
    ret |= test_http_get_case2();
    BK_LOGI(TAG,"http test get case2 result(%d).\n", ret);
    return ret;
}

#endif
#endif
