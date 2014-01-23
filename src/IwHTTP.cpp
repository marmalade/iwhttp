/*
 * (C) 2001-2012 Marmalade. All Rights Reserved.
 *
 * This document is protected by copyright, and contains information
 * proprietary to Marmalade.
 *
 * This file consists of source code released by Marmalade under
 * the terms of the accompanying End User License Agreement (EULA).
 * Please do not use this program/source code before you have read the
 * EULA and have agreed to be bound by its terms.
 */

#include "IwHTTP.h"

#include <string>
#include <sstream>
#include <algorithm>
#include <stdlib.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "IwMath.h"
#include "s3eConfig.h"
#include "s3eTimer.h"

#include "errno.h"

#if defined IW_HTTP_SSL
#include "openssl/ssl.h"
#endif

#define MULTIPART_BOUNDARY "--------:fjksalgjalkgjlk:"

#ifdef IW_TRACE_CHANNEL_HTTP
static void Log(const char* b, uint32 l)
{
    int loglevel = 1;
    s3eConfigGetInt("trace", "http", &loglevel);

    if (loglevel < 3)
        return;

    if (loglevel == 3 && l > 256)
        l = 256;

    char buf[120];
    for (uint32 i = 0; i < l; i += 16)
    {
        sprintf(buf, "%04x: ", i);
        for (uint32 j = i; j < i + 16; j++)
        {
            char buf1[10];
            if (j < l)
            {
                sprintf(buf1, "%02x ", b[j]);
                strcat(buf, buf1);
            }
            else
                strcat(buf, "   ");
        }

        for (uint32 k = i; k < i + 16; k++)
        {
            char buf1[2];
            buf1[1] = 0;
            if (k < l)
            {
                if (b[k] >= 32 && b[k] < 128)
                {
                    buf1[0] = b[k];
                    strcat(buf, buf1);
                }
                else
                {
                    buf1[0] = '.';
                    strcat(buf, buf1);
                }
            }
            else
            {
                buf1[0] = ' ';
                strcat(buf, buf1);
            }
        }

        IwTrace(HTTP, ("%s", buf));
    }
}

static void LogHeaders(std::string response, int response_code, size_t len)
{
    std::string r1 = response.substr(0, len);
    int loglevel = 0;
    s3eConfigGetInt("trace", "http", &loglevel);

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (r1[i] == '\n')
        {
            std::string r2 = r1.substr(j, i-j);
            IwTrace(HTTP, ("%s", r2.c_str()));
            if (/*response_code == 200 &&*/ loglevel < 2)
                return;
            j = i+1;
        }
    }
}
#else
#define Log(X,Y) ((void)0)
#define LogHeaders(X,Y) ((void)0)
#endif

bool CIwHTTP::s_bLookupInProgress = false;
CIwHTTP::DNSRequestList* CIwHTTP::s_pendingDNS = NULL;
s3eThreadLock* CIwHTTP::m_dnsLock = NULL; //never gets destroyed, but delay init

CIwHTTP::CIwHTTP() :
    m_user_data(NULL),
    m_callback(NULL),
    m_header_callback(NULL),
    m_request_idx(0),
    m_response_code(0),
    m_socket(-1),
    m_pSocket(NULL),
    m_bGetInProgress(false),
    m_Status(S3E_RESULT_SUCCESS),
    m_headers_end(std::string::npos),
    m_chunk_header_idx(0),
    m_chunked(false),
    m_post_chunked(false),
    m_reading_chunk_header(false),
    m_connect_timeout(0),
    m_read_timeout(0),
    m_callback_timer(0),
    m_pending_read_callback(false),
    m_error_status(NONE),
    m_last_chunk_seen(false),
    m_data_sent(0)
#ifdef IW_HTTP_SSL
,m_bSSLHandshaking(false), m_bSecureSocket(false), m_SSL(NULL), m_SSL_CTX(NULL)
#endif
{
    if (m_dnsLock == NULL && s3eThreadAvailable())
        m_dnsLock = s3eThreadLockCreate();
}

CIwHTTP::~CIwHTTP()
{
    Cancel();

    // clear form data
    for (std::list<Data>::iterator it = m_data.begin(); it != m_data.end(); ++it)
        if (it->m_file != NULL)
            s3eFileClose(it->m_file);
    m_data.clear();
}

bool CIwHTTP::EnqueueDNSRequest(const char *host, s3eInetAddress *addr)
{
    //This will be threadsafe via the callee
    DNSRequest *r = new DNSRequest;
    if (!r) return false;

    r->m_host = host;
    r->m_addr = addr;
    r->m_caller = this;

    if (!s_pendingDNS)
        s_pendingDNS = new DNSRequestList;


    s_pendingDNS->push_back(r);
    if (s_pendingDNS->size() == 1)
        IssueDNSRequest();

    return true;
}

int32 CIwHTTP::DoIssueDNSRequest(void *sysData, void *usrData)
{
    //Don't lock here - it should be protected by the callee
    if (!s_pendingDNS || s_pendingDNS->size() == 0)
        return 0;

    IwAssert(HTTP, s_bLookupInProgress == false);

    s_bLookupInProgress =
        (s3eInetLookup(
            s_pendingDNS->front()->m_host.c_str(), s_pendingDNS->front()->m_addr, DNSCallback, s_pendingDNS->front()->m_caller
        ) == S3E_RESULT_SUCCESS);

    if (!s_bLookupInProgress)
    {
        IwTrace(HTTP, ("DNS lookup failed"));
        // Failed to even start the lookup, inform immediately..
        s_pendingDNS->front()->m_caller->DoDNSCallback(NULL);
    }

    return 0;
}

void CIwHTTP::IssueDNSRequest()
{
    // On platforms with true async DNS via a thread s3eInetLookup can
    // have called it's callback before returning. If you issue the next
    // lookup during that callback the lookup ID's can get out of sync
    // and lookups will fail thereafter. Doing the lookup on the next
    // yield fixes this at the module level.
    // Only seen on iPhone so far.
    s3eTimerSetTimer(0, DoIssueDNSRequest, NULL);
}

int32 CIwHTTP::DNSCallback(void *pSysData, void *pUserData)
{
    if (pUserData)
        ((CIwHTTP *)pUserData)->DoDNSCallback((s3eInetAddress *)pSysData);

    return S3E_RESULT_SUCCESS;
}

int32 CIwHTTP::ConnectCallback(s3eSocket *, void *pSysData, void *pUserData)
{
    //Ensure that we are safe if pSysData came from a loader using variable size enums
    s3eResult res = static_cast<s3eResult>(*(static_cast<char*>(pSysData)));
    if (pUserData)
        ((CIwHTTP *)pUserData)->DoConnectCallback(res);

    return S3E_RESULT_SUCCESS;
}

int32 CIwHTTP::ConnectTimeoutCallback(void *pSysData, void *pUserData)
{
    if (pUserData)
        ((CIwHTTP *)pUserData)->DoConnectTimeout();

    return S3E_RESULT_SUCCESS;
}

bool CIwHTTP::CheckProxy(const char* name)
{
    if (!name || !strlen(name)) // Sanity check
        return false;

    char tmpstr[S3E_CONFIG_STRING_MAX];
    if (!s3eConfigGetString("connection", "httpproxiestoignore", tmpstr))
    {
        // There is a set of ignorable proxies
        const char* find = strstr(tmpstr, name);
        if (!find) // But our proxy isn't in it.
            return true;
        // Ensure that the string found is an entire proxy entry
        char f = find[strlen(name)];
        if (f == 0 || f == ',')
        {
            IwTrace(HTTP, ("(Rejecting proxy %s)", name));
            return false;
        }
    }
    return true;
}

void CIwHTTP::DoDNSCallback(s3eInetAddress *pAddr)
{
    s_bLookupInProgress = false;

    IwAssert(HTTP, s_pendingDNS);
    if (!s_pendingDNS)
        return; //shouldn't happen

    if (m_dnsLock != NULL)
        s3eThreadLockAcquire(m_dnsLock);

    // Must be the front request..
    delete s_pendingDNS->front();
    s_pendingDNS->pop_front();
    if (s_pendingDNS->empty())
    {
        delete s_pendingDNS;
        s_pendingDNS = NULL;
    }

    if (pAddr)
    {
        // DNS lookup successful so start connecting..
        IwTrace(HTTP, ("(DNS OK)"));
        if (m_socket != -1)
            close(m_socket);

        if ((m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            IwTrace(HTTP, ("(Socket creation failed)"));
            Fail();

            // Need to keep DNS running..
            IssueDNSRequest();
            if (m_dnsLock != NULL)
                s3eThreadLockRelease(m_dnsLock);
            return;
        }

        // Set non-blocking..
        int non_blocking = 1;
        ioctl(m_socket, FIONBIO, &non_blocking);

        // Grab the s3eSocket that backs the BSD-style one..
        m_pSocket = s3esocket(m_socket);

        if (!m_neverUseProxy && !m_usingProxy && m_firstDns)
        {
            // Now that we've done the DNS lookup, we've probably
            // established the correct connection. We now check s3e's
            // proxy setting since it now applies to the right
            // connection. If there is a proxy, we do another DNS
            // lookup to get the right settings.
            m_firstDns = false;
            const char* proxy = s3eSocketGetString(S3E_SOCKET_HTTP_PROXY);

            if (CheckProxy(proxy))
            {
                // There is a proxy.
                const char* host;
                char host_buf[128];

                char* colon = strchr(proxy, ':');
                if (colon)
                {
                    strncpy(host_buf, proxy, MIN(128, colon - proxy));
                    host_buf[colon - proxy] = 0;
                    host = host_buf;
                    sscanf(colon + 1, "%d", &m_proxyPort);
                }
                else
                {
                    host = proxy;
                    m_proxyPort = 80;
                }

                IwTrace(HTTP, ("(Using proxy %s)", host));
                EnqueueDNSRequest(host, pAddr);
                m_usingProxy = true;

                if (m_dnsLock != NULL)
                    s3eThreadLockRelease(m_dnsLock);
                return;
            }
        }

        if (!m_usingProxy)
            pAddr->m_Port = s3eInetHtons(m_URI.GetPort());
        else
            pAddr->m_Port = s3eInetHtons(m_proxyPort);

        s3eResult result = s3eSocketConnect(m_pSocket, &m_addr, ConnectCallback, this);
        if (result != S3E_RESULT_SUCCESS)
        {
            s3eSocketError error = s3eSocketGetError();
            if (error != S3E_SOCKET_ERR_INPROGRESS)
            {
                IwTrace(HTTP, ("(Connect fail)"));
                Fail();

                // Need to keep DNS running..
                IssueDNSRequest();
                if (m_dnsLock != NULL)
                    s3eThreadLockRelease(m_dnsLock);
                return;
            }
        }

        int ms = 60000; // 1 minute
        s3eConfigGetInt("connection", "httpconnecttimeout", &ms);
        if (ms)
        {
            m_connect_timeout = ms;
            s3eTimerSetTimer(ms, ConnectTimeoutCallback, this);
        }

        IwTrace(HTTP, ("(Connecting...)"));

        // Need to keep DNS running..
        IssueDNSRequest();
        if (m_dnsLock != NULL)
            s3eThreadLockRelease(m_dnsLock);
        return;
    }

    IwTrace(HTTP, ("(DNS Failed)"));

    if (m_dnsLock != NULL)
        s3eThreadLockRelease(m_dnsLock);

    Fail();

    // Need to keep DNS running..
    IssueDNSRequest();
}

void CIwHTTP::DoConnectCallback(s3eResult result)
{
    // We can now cancel the connect timeout..
    if (m_connect_timeout)
    {
        m_connect_timeout = 0;
        s3eTimerCancelTimer(ConnectTimeoutCallback, this);
    }

    if (result != S3E_RESULT_SUCCESS)
    {
        IwTrace(HTTP, ("(Connect failed)"));
        Fail();
        return;
    }

    IwTrace(HTTP, ("(Connected)"));
    m_request_idx = 0;

    Data data;
    bool post = false;

    if (!m_data.empty())
        post = true;

    // Connected, fire off the the request
    if (!post)
        data.m_value += "GET ";
    else
        data.m_value += "POST ";

    if (m_usingProxy)
    {
        const char* all = m_URI.GetAll();
        if (all)
            data.m_value += all;
    }
    else
    {
        data.m_value += "/";
        const char* tail = m_URI.GetTail();
        if (tail)
            data.m_value += tail;
    }

    bool skip_ua = false;
    bool skip_ct = false;

    data.m_value += " HTTP/1.1\r\nHost: ";
    data.m_value += m_URI.GetHost();

    for (uint32 i = 0; i < m_req_headers.size(); i++)
    {
        if (m_req_headers[i].m_name == "Host" ||
            m_req_headers[i].m_name == "Content-Length" ||
          (!post && m_req_headers[i].m_name.find("Content-") == 0))
            continue;

        if (m_req_headers[i].m_name == "User-Agent")
            skip_ua = true;

        if (m_req_headers[i].m_name == "Content-Type")
            skip_ct = true;

        data.m_value += "\r\n";
        data.m_value += m_req_headers[i].m_name;
        data.m_value += ": ";
        data.m_value += m_req_headers[i].m_value;
    }

    if (!skip_ua)
    {
        data.m_value += "\r\nUser-Agent: S3E/";
        data.m_value += s3eDeviceGetString(S3E_DEVICE_OS);
        data.m_value += "/";
        data.m_value += s3eDeviceGetString(S3E_DEVICE_ID);
        data.m_value += "/";
        data.m_value += s3eDeviceGetString(S3E_DEVICE_S3E_VERSION);
    }

    if (post)
    {
        if (m_post_chunked)
        {
            data.m_value += "\r\nTransfer-Encoding: chunked";
        }
        else
        {
            data.m_value += "\r\nContent-Length: ";
            std::ostringstream b;
            b << m_data_len;
            data.m_value += b.str();
        }

        if (!skip_ct)
            data.m_value += "\r\nContent-Type: multipart/form-data; boundary=" MULTIPART_BOUNDARY;
    }

    data.m_value += "\r\n\r\n";
    data.m_size = data.m_value.size();
    m_data.push_front(data);

    m_data_it = m_data.begin();
    m_data_len += data.m_size;

    IwTrace(HTTP_VERBOSE, ("(Request Built)"));
    IwTrace(HTTP_VERBOSE, ("%s", data.m_value.c_str()));

#ifdef IW_HTTP_SSL
    if (m_bSecureSocket)
        StartSSLHandshake();
    else
#endif
        SendRequest();
}

void CIwHTTP::DoConnectTimeout()
{
    IwTrace(HTTP, ("(Connect Timeout)"));
    Fail();
}

void CIwHTTP::Fail()
{
    IwTrace(HTTP, ("(FAIL)"));
    Cancel();

    m_Status = S3E_RESULT_ERROR;
    if (m_callback)
    {
        m_pending_read_callback = false;
        m_callback((void *)(intptr_t)m_read_content_transferred, m_user_data);
    }
    if (m_header_callback)
    {
        m_header_callback(this, m_user_data);
    }
}

s3eResult CIwHTTP::Get(const char *URI, s3eCallback cb, void *pUserData)
{
    // For now we actually indicate if it's GET or POST by whether we
    // have a post body. So what we do for GET and POST is exactly the
    // same, only with a NULL body for GET
    return Post(URI, NULL, 0, cb, pUserData);
}

s3eResult CIwHTTP::Post(const char *URI, const char* Body, int32 BodyLength,
                        s3eCallback cb, void *pUserData)
{
    IwAssert(HTTP, URI);

    if (m_bGetInProgress)
    {
        return S3E_RESULT_ERROR;
    }

    Cancel();

    m_URI = URI;

    m_response.clear();
    m_chunk_header.clear();

    m_data_sent = 0;

    m_chunk_header_idx = 0;
    m_chunked = false;
    m_last_chunk_seen = false;
    m_reading_chunk_header = false;

    m_response_code = 0;
    m_headers_end = std::string::npos;
    m_total_transferred = m_chunk_size = 0;
    m_firstDns = true;

    if (m_URI.GetProtocol() != CIwURI::HTTP
#ifdef IW_HTTP_SSL
         && m_URI.GetProtocol() != CIwURI::HTTPS
#endif
    )
    {
        IwTrace(HTTP, ("(Unsupported protocol: %s)", URI));
        return S3E_RESULT_ERROR;
    }

#ifdef IW_HTTP_SSL
    // Set secure if https://
    m_bSecureSocket = m_URI.GetProtocol() == CIwURI::HTTPS;
#endif

    m_Status = S3E_RESULT_SUCCESS;
    const char *pHost = m_URI.GetHost();
    IwTrace(HTTP, ("(Fetching %s)", URI));

    // If a proxy is defined in the icf, talk to it, not the defined host.
    int useProxy = 1;
    s3eConfigGetInt("connection", "usehttpproxy", &useProxy);
    m_neverUseProxy = (useProxy == 0);
    char tmpstr[S3E_CONFIG_STRING_MAX];
    if (useProxy && !s3eConfigGetString("connection", "httpproxy", tmpstr))
    {
        pHost = tmpstr;
        m_usingProxy = true;
        m_proxyPort = 80;
        s3eConfigGetInt("connection", "httpproxyport", &m_proxyPort);
        IwTrace(HTTP, ("(Using proxy %s from icf)", pHost));
    }
    else
        m_usingProxy = false;

    if (!pHost)
    {
        // TODO: Set appropriate error code
        return S3E_RESULT_ERROR;
    }

    m_bGetInProgress = true;

    m_header_callback = cb;
    m_callback = NULL;
    m_user_data = pUserData;

    m_content_length = 0;

    // add end marker if we have multi part data
    if (!m_data.empty())
    {
        Data f;

        f.m_value = "--" MULTIPART_BOUNDARY "--\r\n";

        if (m_post_chunked)
            AddChunked(f);

        f.m_size = f.m_value.size();

        m_data.push_back(f);
    }

    bool ispost = false;

    // insert post data
    if (Body != NULL && BodyLength > 0)
    {
        ispost = true;
        Data f;

        // Use length to make sure null characters don't terminate str
        std::string tmp(Body, BodyLength);
        f.m_value.append(tmp);

        f.m_size = BodyLength;

        if (m_post_chunked)
            AddChunked(f);

        m_data.push_front(f);
    }

    // insert chunk end marker
    if (m_post_chunked && !m_data.empty())
    {
        m_data.back().m_value += "0\r\n\r\n";
        m_data.back().m_size += 5;
    }

    m_data_len = 0;
    for (std::list<Data>::iterator it = m_data.begin(); it != m_data.end(); ++it)
        m_data_len += it->m_size;

    // close off http (do after length so length does not include this)
    // do not append crlf to post body
    if (!m_data.empty() && !ispost)
    {
        m_data.back().m_value += "\r\n";
        m_data.back().m_size += 2;
    }

    // Start the whole process by looking up the host
    if (m_dnsLock != NULL)
        s3eThreadLockAcquire(m_dnsLock);
    EnqueueDNSRequest(pHost, &m_addr);
    if (m_dnsLock != NULL)
        s3eThreadLockRelease(m_dnsLock);

    return m_Status;
}

s3eResult CIwHTTP::Cancel()
{
    IwTrace(HTTP, ("(Cancel)"));

    // Cancel any possible callbacks..
    if (m_callback_timer)
    {
        s3eTimerCancelTimer(DoCallback, this);
        m_callback_timer = 0;
    }

    if (m_read_timeout)
    {
        m_read_timeout = 0;
        s3eTimerCancelTimer(ReadTimeoutCallback, this);
    }

    if (m_connect_timeout)
    {
        m_connect_timeout = 0;
        s3eTimerCancelTimer(ConnectTimeoutCallback, this);
    }

    if (s_pendingDNS)
    {
        if (m_dnsLock != NULL)
            s3eThreadLockAcquire(m_dnsLock);
        // Remove any enqueued DNS requests relating to this object
        if(s_pendingDNS) //double check locking pattern
        {
            std::list<DNSRequest *>::iterator i = s_pendingDNS->begin();
            while (i != s_pendingDNS->end())
            {
                if ((*i)->m_caller == this)
                {
                    bool bFirst = (i == s_pendingDNS->begin());

                    delete (*i);
                    i = s_pendingDNS->erase(i);

                    if (bFirst)
                    {
                        s3eInetLookupCancel();
                        s_bLookupInProgress = false;
                        IssueDNSRequest(); // Pump the queue
                    }

                    break; // Should only be one..
                }
                i++;
            }

        #ifdef IW_DEBUG
            i = s_pendingDNS->begin();
            while (i != s_pendingDNS->end())
            {
                IwAssert(HTTP, (*i)->m_caller != this);
                i++;
            }
        #endif

            if (s_pendingDNS->size() == 0)
            {
                delete s_pendingDNS;
                s_pendingDNS = NULL;
            }
        }
        if (m_dnsLock != NULL)
            s3eThreadLockRelease(m_dnsLock);
    }

#ifdef IW_HTTP_SSL
    // Bring down SSL before the socket..
    DestroySSL();
#endif

    if (m_pSocket)
    {
        close(m_socket);
        m_socket = -1;
        m_pSocket = NULL;
    }


    m_bGetInProgress = false;

    return S3E_RESULT_SUCCESS;
}

void CIwHTTP::Writeable()
{
    IwTrace(HTTP_VERBOSE, ("(Writeable)"));

#ifdef IW_HTTP_SSL
    if (m_bSSLHandshaking)
        ContinueSSLHandshake();
    else
#endif
        SendRequest();
}

void CIwHTTP::Readable()
{
    IwTrace(HTTP_VERBOSE, ("(Readable)"));

#ifdef IW_HTTP_SSL
    if (m_bSSLHandshaking)
        ContinueSSLHandshake();
    else
#endif
        ReadResponse();
}

int32 CIwHTTP::WriteableCallback(s3eSocket *pSocket, void *pSysData, void *pUserData)
{
    if (pUserData)
        ((CIwHTTP *)pUserData)->Writeable();

    return S3E_RESULT_SUCCESS;
}

int32 CIwHTTP::ReadableCallback(s3eSocket *pSocket, void *pSysData, void *pUserData)
{
    IwTrace(HTTP_VERBOSE, ("ReadableCallback"));

    if (pUserData)
        ((CIwHTTP *)pUserData)->Readable();

    return S3E_RESULT_SUCCESS;
}

int CIwHTTP::SendBytes(const char *buf, int len)
{
#ifdef IW_HTTP_SSL
    if (m_bSecureSocket)
    {
        int ret = SSL_write(m_SSL, buf, len);
        if (ret == -1)
        {
            int err = SSL_get_error(m_SSL, ret);
            if (err == SSL_ERROR_WANT_WRITE)
                // Blocked on send, SSL_write in current mode is atomic
                // so entire send must be repeated.
                // Read carefully: http://www.openssl.org/docs/ssl/SSL_write.html
                ret = 0;
        }
        IwTrace(HTTP, ("SSL_write returns %d", ret));
        return ret;
    }
    else
    {
        return s3eSocketSend(m_pSocket, buf, len, 0);
    }
#else
    return s3eSocketSend(m_pSocket, buf, len, 0);
#endif
}

int CIwHTTP::ReadBytes(char *buf, int len)
{
#ifdef IW_HTTP_SSL
    return m_bSecureSocket ? SSL_read(m_SSL, buf, len) : recv(m_socket, buf, len, 0);
#else
    return recv(m_socket, buf, len, 0);
#endif
}

#ifdef IW_HTTP_SSL
void CIwHTTP::ContinueSSLHandshake()
{
    IwAssert(HTTP, m_bSSLHandshaking);

    IwTrace(HTTP, ("ContinueSSLHandshake:0"));

    int ret;
    if ((ret = SSL_connect(m_SSL)) != SSL_SUCCESS)
    {
        IwTrace(HTTP, ("ContinueSSLHandshake"));

        int err = SSL_get_error(m_SSL, 0);

        if (err == SSL_ERROR_WANT_READ)
        {
            IwTrace(HTTP, ("Waiting until Readable"));
            s3eSocketReadable(m_pSocket, ReadableCallback, this);
        }
        else if (err == SSL_ERROR_WANT_WRITE)
        {
            IwTrace(HTTP, ("Waiting until Writeable"));
            s3eSocketWritable(m_pSocket, WriteableCallback, this);
        }
        else
        {
            m_bSSLHandshaking = false;
            IwTrace(HTTP, ("SSL Error"));
            Fail();
        }
    }
    else
    {
        // Succesful SSL handshake
        m_bSSLHandshaking = false;

        IwTrace(HTTP, ("Handshake Done"));

        // Connection is now secure, send the request..
        SendRequest();
    }
}

void CIwHTTP::StartSSLHandshake()
{
    if (m_SSL)
        DestroySSL();

    // Just TLSv1 right now..
    SSL_METHOD *method  = TLSv1_client_method();

    m_SSL_CTX = SSL_CTX_new(method);

    // Turn off verification of certs..
    SSL_CTX_set_verify(m_SSL_CTX, SSL_VERIFY_NONE, NULL);

    m_SSL = SSL_new(m_SSL_CTX);

    // Set socket to use..
    SSL_set_fd(m_SSL, m_socket);

    m_bSSLHandshaking = true;
    ContinueSSLHandshake();
}

void CIwHTTP::DestroySSL()
{
    if (m_SSL)
    {
        SSL_shutdown(m_SSL);
        SSL_free(m_SSL);
        m_SSL = NULL;
    }

    if (m_SSL_CTX)
    {
        SSL_CTX_free(m_SSL_CTX);
        m_SSL_CTX = NULL;
    }
}
#endif

void CIwHTTP::SendRequest()
{
    IwTrace(HTTP, ("(SendRequest [%d->%d])", m_request_idx, m_data_len));
    if (m_request_idx < m_data_len)
    {
        int result;
        int extra_size = 0;

        if (m_data_it->m_idx < (int)m_data_it->m_value.size())
        {
            result = SendBytes(m_data_it->m_value.c_str() + m_data_it->m_idx, m_data_it->m_value.size() - m_data_it->m_idx);
        }
        else
        {
            char buffer[POST_BUFFER_PREFIX_SIZE + POST_BUFFER_SIZE + POST_BUFFER_POSTFIX_SIZE];
            int size;

            if (m_post_chunked)
            {
                char buffer2[8];

                s3eFileSeek(m_data_it->m_file, m_data_it->m_idx - m_data_it->m_value.size(), S3E_FILESEEK_SET);
                size = s3eFileRead(buffer + POST_BUFFER_PREFIX_SIZE, 1, POST_BUFFER_SIZE, m_data_it->m_file);

                // add ending CR to chunk size
                if (m_data_it->m_idx + size >= m_data_it->m_size)
                    sprintf(buffer2, "%04x\r\n", size + 2);
                else
                    sprintf(buffer2, "%04x\r\n", size);

                memcpy(buffer, buffer2, POST_BUFFER_PREFIX_SIZE);
                extra_size += POST_BUFFER_PREFIX_SIZE;

                buffer[size + extra_size    ] = '\r';
                buffer[size + extra_size + 1] = '\n';
                extra_size += 2;
            }
            else
            {
                s3eFileSeek(m_data_it->m_file, m_data_it->m_idx - m_data_it->m_value.size(), S3E_FILESEEK_SET);
                size = s3eFileRead(buffer, 1, POST_BUFFER_SIZE, m_data_it->m_file);
            }

            // ending CR
            if (m_data_it->m_idx + size >= m_data_it->m_size)
            {
                buffer[size + extra_size    ] = '\r';
                buffer[size + extra_size + 1] = '\n';
                extra_size += 2;
            }

            result = SendBytes(buffer, size + extra_size);

            if(result != -1)
                result -= extra_size;
        }

        if (result == -1)
        {
            IwTrace(HTTP, ("(Failed to send request)"));
            Fail();
            return;
        }

        m_request_idx += result;
        m_data_it->m_idx += result;

        if (m_data_it->m_idx >= m_data_it->m_size)
            ++m_data_it;

        if (m_request_idx < m_data_len)
        {
            IwTrace(HTTP_VERBOSE, ("Didn't send all request. Requesting writable (%p)", this));

            // Request callback if we didn't send everything
            s3eSocketWritable(m_pSocket, WriteableCallback, this);
        }
        else
        {
            IwTrace(HTTP_VERBOSE, ("Request sent. Reading results... (%p)", this));

            // clear form data
            for (std::list<Data>::iterator it = m_data.begin(); it != m_data.end(); ++it)
                if (it->m_file != NULL)
                    s3eFileClose(it->m_file);
            m_data.clear();
            m_data_sent = m_request_idx;

            // All sent so request callback when the reply comes in
            ReadResponse();
        }
    }
}

void CIwHTTP::ReadResponse()
{
    const int BUF_SIZE(64);
    char recv_buf[BUF_SIZE];

    // Read headers into response..

#ifdef IW_HTTP_SSL
    do
    {
        // SSL buffers underneath us so make sure
        // we exhaust the buffer before going back to
        // waiting for select to come true
#endif

    int32 bytes_read = ReadBytes(recv_buf, BUF_SIZE);
    IwTrace(HTTP_VERBOSE, ("ReadResponse: %d", bytes_read));

    if (bytes_read > 0)
    {
        m_response.append(recv_buf, bytes_read);
    }
    else if(bytes_read == 0)
    {
        IwTrace(HTTP, ("(Socket connection ended before all headers were read)"));
        Fail();
        return;
    }
    else
    {
        if (errno != EAGAIN)
        {
            IwTrace(HTTP, ("(Socket error whilst reading headers)"));
            Fail();
            return;
        }
    }
#ifdef IW_HTTP_SSL
    }
    // Only for secure sockets..
    while (m_bSecureSocket && SSL_pending(m_SSL));
#endif

    if (!GotHeaders())
    {
        // Keep reading until all headers received..
        s3eSocketReadable(m_pSocket, ReadableCallback, this);
    }
    else
    {
        IwTrace(HTTP, ("Got headers"));

        m_bGetInProgress = false;

        // Got all the headers, callback..
        if (m_header_callback)
        {
            m_header_callback(this, m_user_data);
        }
    }
}

bool CIwHTTP::GotHeaders()
{
    if (m_headers_end != std::string::npos)
        return true;

    m_chunked = false;
    if ((m_headers_end = m_response.find("\r\n\r\n")) == std::string::npos)
        return false;

    m_headers_end += 4;
    m_total_transferred = 0;

    GetHeader("Content-Length", m_content_length);

    m_request_idx = m_headers_end;

    // Try to determine the transfer-encoding..

    std::string te;
    if (GetHeader("Transfer-Encoding", te))
    {
        if (te.find("chunked") != std::string::npos)
        {
            m_chunked = true;

            // Transfer residual from response buffer to
            // chunk_header buffer.

            m_chunk_size = 0;
            m_chunk_header_idx = 0;
            m_reading_chunk_header = true;
            m_chunk_header = m_response.substr(m_headers_end);

            // Try to parse the first chunk header..
            ParseChunkHeader();
        }
        else
        {
            // Insert your additional transfer-encoding handlers here...
        }
    }

    GetResponseCode();
    LogHeaders(m_response, m_response_code, m_headers_end);
    return true;
}

uint32 CIwHTTP::ContentLength()
{
    if (GotHeaders())
        return m_content_length;
    return 0;
}

uint32 CIwHTTP::ContentReceived()
{
    return m_total_transferred;
}

uint32 CIwHTTP::ContentExpected()
{
    if (m_content_length)
        return m_content_length;

    int expected = m_total_transferred;
    if (m_chunked && m_chunk_size)
        expected += m_chunk_size - 2;
    return expected;
}

int CIwHTTP::TransferContent(char *pBuf, int max_bytes)
{
    int total_bytes_read = 0;

    if (!m_chunked)
    {
        total_bytes_read = TransferContentInternal(pBuf, max_bytes);
    }
    else
    {
        bool blocked = false;

        while (total_bytes_read < max_bytes && !blocked && !ContentFinished())
        {
            if (m_chunk_size > 2)
            {
                // Don't attempt to read past end of chunk
                int max = MIN(max_bytes - total_bytes_read, m_chunk_size - 2);
                int bytes_read = TransferContentInternal(
                    &pBuf[total_bytes_read], max
                );

                if (bytes_read > 0)
                    m_chunk_size -= bytes_read;
                else if (bytes_read == -1)
                    bytes_read = 0;

                total_bytes_read += bytes_read;
                blocked = (bytes_read == 0);
            }

            if (m_chunk_size && m_chunk_size <= 2 && !m_reading_chunk_header)
            {
                char discard[2];
                // Throw away the trailing CR/LF
                int discarded = TransferContentInternal(discard, m_chunk_size);

                // Adjust for ContentReceived
                m_total_transferred -= discarded;

                if (discarded > 0)
                    m_chunk_size -= discarded;

                blocked = (discarded == 0);
            }

            if (!m_chunk_size)
            {
                // Check to see if we can parse another
                // block header from the chunk header buffer
                if (!ParseChunkHeader())
                {
                    // Fetch more data if we can't.
                    GetChunkHeader();
                }
            }
        }
    }

    return total_bytes_read;
}

void CIwHTTP::GetChunkHeader()
{
    m_reading_chunk_header = true;

    const int BUF_SIZE(128);
    char recv_buf[BUF_SIZE];

#ifdef IW_HTTP_SSL
    do
    {
        // SSL buffers underneath us so make sure
        // we exhaust the buffer before going back to
        // waiting for select to come true
#endif

    int32 bytes_read;
    // Read headers into response..
    bytes_read = ReadBytes(recv_buf, BUF_SIZE);

    if (bytes_read > 0)
    {
        m_chunk_header.append(recv_buf, bytes_read);
    }
    else
    {
        if (errno != EAGAIN || !bytes_read)
        {
            if (!bytes_read)
                IwTrace(HTTP, ("(Socket connection ended whilst reading chunk header)"));
            else
                IwTrace(HTTP, ("(Socket error whilst reading chunk header)"));
            Fail();
            return;
        }
    }
#ifdef IW_HTTP_SSL
    }
    // Only for secure sockets..
    while (m_bSecureSocket && SSL_pending(m_SSL));
#endif

    ParseChunkHeader();
}

bool CIwHTTP::ParseChunkHeader()
{
    // Chunked header is:
    // nn+ CF LF where n is 1 or more HEX digits as alphanumeric characters
    // Final chunk is indicated by 0 length chunk

    size_t end = m_chunk_header.find("\r\n", m_chunk_header_idx);
    if (end == std::string::npos)
        return false;


    std::string head = m_chunk_header.substr(m_chunk_header_idx, end);

    uint len;
    if (sscanf(head.c_str(), "%x", &len) != 1)
    {
        IwTrace(HTTP, ("(Malformed Chunk Header)"));
        Fail();
        return false;
    }

    // We have a valid chunk header
    m_chunk_size = len;
    m_chunk_header_idx = end + 2;
    m_reading_chunk_header = false;

    IwTrace(HTTP, ("ChunkSize: %x", m_chunk_size));

    // Reset the chunk buffer when exhausted
    if (m_chunk_header_idx >= (int)m_chunk_header.size())
    {
        m_chunk_header.clear();
        m_chunk_header_idx = 0;
    }

    if (!m_chunk_size)
    {
        // Last chunk
        m_last_chunk_seen = true;
        m_content_length = m_total_transferred;

        if (m_read_timeout)
        {
            m_read_timeout = 0;
            s3eTimerCancelTimer(ReadTimeoutCallback, this);
        }

        // We're finished, callback..
        if (m_callback)
        {
            m_callback_timer = 1;
            s3eTimerSetTimer(0, DoCallback, this);
        }
    }

    // Adjust for the trailing CR/LF
    m_chunk_size += 2;
    return true;
}

int32 CIwHTTP::TransferContentInternal(char *pBuf, const int max_bytes)
{
    if (m_reading_chunk_header)
        return -1;

    int transferred = 0;

    if (!m_chunked)
    {
        if (m_request_idx < (int)m_response.size())
        {
            // There may be some content left over in the
            // response buffer..
            transferred = MIN((int)m_response.size() - m_request_idx, max_bytes);
            memcpy(pBuf, &m_response.data()[m_request_idx], transferred);
            m_request_idx += transferred;
        }
    }
    else
    {
        if (m_chunk_header_idx < (int)m_chunk_header.size())
        {
            // There may also be some content left over in the chunk header
            // buffer if chunked encoding is being used..
            transferred = MIN((int)m_chunk_header.size() - m_chunk_header_idx, max_bytes);
            memcpy(pBuf, &m_chunk_header.data()[m_chunk_header_idx], transferred);
            m_chunk_header_idx += transferred;

            // Reset the chunk buffer when exhausted
            if (m_chunk_header_idx >= (int)m_chunk_header.size())
            {
                m_chunk_header.clear();
                m_chunk_header_idx = 0;
            }
        }
    }

    if (transferred < max_bytes)
    {
#ifdef IW_HTTP_SSL
    do
    {
        // SSL buffers underneath us so make sure
        // we exhaust the buffer before going back to
        // waiting for select to come true
#endif
        int32 read = ReadBytes(&pBuf[transferred], max_bytes - transferred);
        if (read > 0)
            transferred += read;

        // I've added a second check here to see if the content has all been transferred, but only in
        // the case the content length is know (i.e. we got content-length headers).  The problem here
        // is that if we don't close down the socket when this happens then the next ReadBytes call
        // can block for a considerable period of time thus stalling the download.  I don't want to
        // use ContentFinished call as it's check is different to this.  I also only want this to
        // happen for known content length as I think m_content_length == m_total_transferred is always
        // true when content length is unknown.  A few unknowns here so playing it safe.
        if (!read || (m_content_length == m_total_transferred + transferred && m_content_length))
        {
            // The socket has been closed. Assume all data received.
            if ((m_content_length
                  && m_content_length != (m_total_transferred + transferred))
                || m_chunked)
            {
                // There's a content length header and we haven't
                // got all the data yet, or we're using chunked
                // encoding.
                IwTrace(
                    HTTP,
                    ("(Socket closed before all data received. Chunked: %s)", m_chunked ? "true" : "false")
                );

                Fail();
            }

            // If we get here, the length is being communicated by
            // closing the socket, and that has happened. So we've
            // finished and it's all good.
            IwTrace(HTTP, ("Remote side has closed"));
            m_content_length = m_total_transferred + transferred;

            if (m_read_timeout)
            {
                m_read_timeout = 0;
                s3eTimerCancelTimer(ReadTimeoutCallback, this);
            }

            // Clean up (which will clear callbacks)
            Cancel();

            // Now schedule the callback after the cleanup, but
            // only if there's not already a pending read callback
            if (!m_pending_read_callback && m_callback)
            {
                m_callback_timer = 1;
                s3eTimerSetTimer(0, DoCallback, this);
            }
        }
        else if (read == -1 && errno != EAGAIN)
        {
            IwTrace(HTTP, ("(Unspecified socket error: %d)", errno));
            Fail();
        }

#ifdef IW_HTTP_SSL
    }
    // Only for secure sockets..
    while (m_bSecureSocket && m_SSL && SSL_pending(m_SSL) && max_bytes > transferred);
#endif
    }

    m_total_transferred += transferred;
    if (m_total_transferred == m_content_length && m_content_length)
    {
        std::string connection;
        if (GetHeader("Connection", connection))
        {
            if (connection == "Close")
            {
                IwTrace(HTTP, ("got Close header"));
                Cancel();
            }
        }
    }

    return transferred;
}

int32 CIwHTTP::TransferCallback(s3eSocket *, void *, void *pUserData)
{
    if (pUserData)
        return ((CIwHTTP *)pUserData)->DoTransferCallback();

    return 0;
}

int32 CIwHTTP::DoTransferCallback()
{
    // Called as a result of ReadData/ReadContent not being able to receive
    // the full buffer.
    IwTrace(HTTP_VERBOSE, ("Transfer callback"));

    int32 transferred;

    do
    {
        //Must keep calling transfer content because it can be reading
        //out of the header buffer but will only return one chunk at a time,
        //and if it hasn't called s3eSocketRecv then we won't necessarily get
        //a readable callback
        //Would prob be better if ReadBytes called Readable so you register for
        //socket callbacks if socket runs out of data
        transferred = TransferContent(m_content_buf, m_max_bytes);

        // I'm almost sure this can't happen anymore.  transferred should always
        // be >= 0.  A failed read, i.e. read == -1, is handled inside TransferContent.
        if (transferred == -1)
        {
            IwTrace(HTTP, ("(Failed to transfer content)"));
            Fail();
        }
        else
        {
            m_max_bytes -= transferred;
            m_read_content_transferred += transferred;
            m_content_buf = &m_content_buf[transferred];
        }
    } while (m_content_length != m_total_transferred && transferred > 0);

    // I have a strong suspicion that this will never return true.  The ContentFinished function
    // will return false if m_pending_read_callback is true.  I'm pretty sure there will always be
    // a pending callback at this stage though!
    if (ContentFinished())
        return 0;

    // If the user supplied buffer is still not full && the socket is still valid (which
    // should be the case unless Cancel() was called).
    if (m_max_bytes && m_pSocket != NULL)
    {
        // We're still connected and waiting for data so enqueue another callback and return.
        s3eSocketReadable(m_pSocket, TransferCallback, this);
    }
    else
    {
        m_pending_read_callback = false;

        if (m_read_timeout)
        {
            m_read_timeout = 0;
            s3eTimerCancelTimer(ReadTimeoutCallback, this);
        }

        Log(m_orig_content_buf, m_content_length);
        m_callback((void *)(intptr_t)m_read_content_transferred, m_user_data);
    }

    return transferred;
}

bool CIwHTTP::ContentFinished()
{
    // Don't report finished until the final async read callback..
    if (!m_chunked)
        return !m_pending_read_callback && (ContentExpected() == ContentReceived());
    else
        return !m_pending_read_callback && (ContentExpected() == ContentReceived()) && m_last_chunk_seen;
}

uint32 CIwHTTP::ReadContent(char *pBuf, uint32 max_bytes, s3eCallback cb, void *userData)
{
    static int once = 1;
    if (once)
    {
        IwTrace(HTTP, ("DEPRECATED: ReadContent. Please consider using ReadData/ReadDataAsync instead"));
        once = 0;
    }

    if (m_socket == -1)
    {
        IwAssertMsg(HTTP, false, ("HTTP ReadContent called when no connection is present. Post or Get should be called first."));
        return 0;
    }

    if (m_bGetInProgress)
    {
        // Don't start reading content until we've got
        // the headers..
        return 0;
    }

    m_orig_content_buf = pBuf;
    int transferred = TransferContent(pBuf, max_bytes);
    IwTrace(HTTP_VERBOSE, ("ReadContent: Read %d/%d bytes", transferred, max_bytes));
    if (cb)
    {
        m_pending_read_callback = true;
        m_callback = cb;
        m_user_data = userData;

        if (transferred < (int)max_bytes)
        {
            m_content_buf = &pBuf[transferred];
            m_max_bytes = max_bytes - transferred;

            // Start the timeout..
            int ms = 0;
            s3eConfigGetInt("connection", "httpreadtimeout", &ms);
            if (ms)
                s3eTimerSetTimer(ms, ReadTimeoutCallback, m_user_data);

            // Call me back when there's something to read..
            s3eSocketReadable(m_pSocket, TransferCallback, this);
        }
        else
        {
            // We got all we asked for queue a callback..
            Log(m_orig_content_buf, max_bytes);
            m_callback_timer = 1;
            s3eTimerSetTimer(0, DoCallback, this);
        }
    }
    else
    {
        m_callback = NULL;
    }

    return transferred;
}

uint32 CIwHTTP::ReadData(char *buf, uint32 max_bytes)
{
    if (m_bGetInProgress)
    {
        // Don't start reading content until we've got
        // the headers..
        return 0;
    }

    if (m_socket == -1)
    {
        IwAssertMsg(HTTP, false, ("HTTP ReadData called when no connection is present. Post or Get should be called first."));
        return 0;
    }

    return TransferContent(buf, max_bytes);
}

void CIwHTTP::ReadDataAsync(char *buf, uint32 max_bytes, uint32 timeout, s3eCallback cb, void *userData)
{
    m_read_content_transferred = 0;

    // Cache user supplied parms..
    m_callback = cb;
    m_user_data = userData;

    if (m_socket == -1)
    {
        IwAssertMsg(HTTP, false, ("HTTP ReadDataAsync called when no connection is present. Post or Get should be called first."));
        return;
    }

    if (m_bGetInProgress)
    {
        // If we're still receiving the headers then call back
        // with a 0 byte value..
        m_callback_timer = 1;
        s3eTimerSetTimer(0, DoCallback, this);
        return;
    }

    // Clamp max bytes at content-length if it was provided..
    if (m_content_length && (int)max_bytes > m_content_length)
        max_bytes = m_content_length;

    m_orig_content_buf = buf;
    m_read_content_transferred = TransferContent(buf, max_bytes);

    if(cb)
    {
        m_pending_read_callback = true;
        if (m_read_content_transferred < (int)max_bytes && m_socket)
        {
            m_content_buf = &buf[m_read_content_transferred];
            m_max_bytes = max_bytes - m_read_content_transferred;

            // Start the timeout..
            if (timeout)
            {
                m_read_timeout = timeout;
                s3eTimerSetTimer(timeout, ReadTimeoutCallback, this);
            }

            // Call me back when there's something to read..
            s3eSocketReadable(m_pSocket, TransferCallback, this);
        }
        else
        {
            // We got everything, do callback (via timer to avoid recursion)..
            m_callback_timer = 1;
            s3eTimerSetTimer(0, DoCallback, this);
        }
    }
}

int32 CIwHTTP::DoCallback(void *sysData, void *usrData)
{
    CIwHTTP *self = (CIwHTTP *)usrData;
    self->m_pending_read_callback = false;
    self->m_callback_timer = 0;
    if (self->m_callback)
        self->m_callback((void *)(intptr_t)self->m_read_content_transferred, self->m_user_data);
    return 0;
}

uint32 CIwHTTP::GetResponseCode()
{
    if (m_response_code == 0)
    {
        uint32 response_start = m_response.find(" ");
        if (response_start != std::string::npos)
        {
            response_start += 1;
            if (response_start < m_response.size())
                m_response_code = atoi(&m_response.c_str()[response_start]);
        }
    }
    return m_response_code;
}

bool CIwHTTP::GetHeader(const char *name, int32 &result)
{
    if (!GotHeaders())
        return false;

    std::string str;
    if (GetHeader(name, str))
    {
        result = atoi(str.c_str());
        return true;
    }
    return false;
}

bool CIwHTTP::GetHeader(const char *name, std::string &result)
{
    if (!GotHeaders())
        return false;

    // Need to perform this search case-insensitively but
    // can't change m_response or name so we're going to have
    // to copy and convert..

    std::string search(name);
    std::string searched(m_response);

    // Make it clear that you don't add the colon
    // to the search string
    search += ':';

    std::transform(search.begin(), search.end(), search.begin(), std::tolower);
    std::transform(searched.begin(), searched.end(), searched.begin(), std::tolower);

    size_t start;
    if ((start = searched.find(search)) != std::string::npos)
    {
        start += search.size() + 1; // Skip space
        if (start < searched.size())
        {
            size_t end;
            if ((end = searched.find("\r\n", start)) != std::string::npos)
            {
                // Return the actual header text, not the transformed
                // version..
                result.assign(m_response, start, end - start);
                return true;
            }
        }
    }

    return false;
}

void CIwHTTP::SetRequestHeader(const char *pName, int32 val)
{
    std::ostringstream b;
    b << val;
    SetRequestHeader(pName, b.str());
}

void CIwHTTP::SetRequestHeader(const char *pName, const std::string &val)
{
    for (uint32 i = 0; i < m_req_headers.size(); i++)
    {
        if (m_req_headers[i].m_name == pName)
        {
            if (val == "")
            {
                // Erase header
                m_req_headers.erase(i);
            }
            else
            {
                m_req_headers[i].m_value = val;
            }
            return;
        }
    }

    ReqHeader r;
    r.m_name = pName;
    r.m_value = val;
    m_req_headers.append(r);
}

void CIwHTTP::AddChunked(Data& f)
{
    char buffer[32];
    sprintf(buffer, "%x\r\n", (unsigned int)f.m_value.size());

    f.m_value.insert(0, buffer);
    f.m_value += "\r\n";
}

void CIwHTTP::SetFormData(const char *pName, const std::string &val)
{
    Data f;

    f.m_value = "--" MULTIPART_BOUNDARY "\r\n";
    f.m_value += "Content-Disposition: form-data; name=\"";
    f.m_value += pName;
    f.m_value += "\"\r\n\r\n";
    f.m_value += val;
    f.m_value += "\r\n";

    if (m_post_chunked)
        AddChunked(f);

    f.m_size = f.m_value.size();

    m_data.push_back(f);
}

s3eResult CIwHTTP::SetFormDataFile(const char *pName, const char* sourceFile, const char* destName, const char* mimeType)
{
    Data f;

    f.m_file = s3eFileOpen(sourceFile, "rb");

    if (f.m_file == NULL)
        return S3E_RESULT_ERROR;

    f.m_value = "--" MULTIPART_BOUNDARY "\r\n";
    f.m_value += "Content-Disposition: form-data; name=\"";
    f.m_value += pName;
    f.m_value += "\"; filename=\"";
    f.m_value += destName;
    f.m_value += "\"\r\nContent-Type: ";
    f.m_value += mimeType;
    f.m_value += "\r\n\r\n";

    if (m_post_chunked)
        AddChunked(f);

    f.m_size = f.m_value.size() + s3eFileGetSize(f.m_file);

    m_data.push_back(f);

    return S3E_RESULT_SUCCESS;
}

void CIwHTTP::SetPostChunkedMode(bool isChunked)
{
    m_post_chunked = isChunked;
}

uint32 CIwHTTP::ContentSent()
{
    return m_data_sent;
}

int32 CIwHTTP::ReadTimeoutCallback(void *, void *usrData)
{
    CIwHTTP *self = (CIwHTTP *)usrData;
    if (self)
        self->onReadTimeout();
    return 0;
}

void CIwHTTP::onReadTimeout()
{
    IwTrace(HTTP, ("Read Timeout ocurred"));

    m_read_timeout = 0;
    m_error_status = READ_TIMEOUT;

    // Make the callback, let the user decide whether to close
    // or try another read..
    if (m_callback)
    {
        m_pending_read_callback = false;
        m_callback((void *)(intptr_t)m_read_content_transferred, m_user_data);
    }
}
