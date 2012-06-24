/*
 * Copyright (C) 2001-2012 Ideaworks3D Ltd.
 * All Rights Reserved.
 *
 * This document is protected by copyright, and contains information
 * proprietary to Ideaworks Labs.
 * This file consists of source code released by Ideaworks Labs under
 * the terms of the accompanying End User License Agreement (EULA).
 * Please do not use this program/source code before you have read the
 * EULA and have agreed to be bound by its terms.
 */
#ifndef IW_HTTP_H
#define IW_HTTP_H

#include "s3eSocket.h"
#include "s3eThread.h"
#include "s3eFile.h"
#include "IwURI.h"
#include "IwArray.h"

#include <list>
#include <string>

/**
 * @defgroup iwhttpgroup IwHTTP API Reference
 *
 * IwHTTP provides an HTTP client, with the CIwHTTP class,
 * and URI parsing functionality with the CIwURI class.
 *
 * @addtogroup iwhttpgroup
 * @defgroup iwhttpclientobject HTTP Client Object
 *
 * The HTTP client object for performing HTTP requests.
 *
 * Supports all common usages of the HTTP and HTTPS (TLSv1) protocol. Cookies,
 * HTTP-Basic authentication and similar can all be implemented using the
 * @ref SetRequestHeader and @ref GetHeader functions.
 *
 * @note For more information on the HTTP Client Object, see the
 * @ref httpclient "HTTP Client" section in the
 * <i>IwHTTP API Documentation</i>.
 */

/**
 * @}
 */

#ifdef IW_HTTP_SSL
typedef struct SSL SSL;
typedef struct SSL_CTX SSL_CTX;
#endif

#define POST_BUFFER_PREFIX_SIZE 6
#define POST_BUFFER_SIZE 4096
#define POST_BUFFER_POSTFIX_SIZE 4

/**
 * @addtogroup iwhttpclientobject
 * @{
 */

/**
 * HTTP Client Class
 * @nosubgrouping
 */
class CIwHTTP
{
public:
    typedef enum ErrorStatus
    {
        NONE,
        READ_TIMEOUT
    } ErrorStatus;

protected:
    CIwURI m_URI;
    int m_socket;
    s3eSocket *m_pSocket;
    s3eInetAddress m_addr;

    // General flags.
    bool m_bGetInProgress;

#ifdef IW_HTTP_SSL
    // Concerning SSL
    SSL *m_SSL;
    SSL_CTX *m_SSL_CTX;
    bool m_bSecureSocket;
    bool m_bSSLHandshaking;
#endif

    // Concerning chunked transfer encoding.
    bool m_chunked;
    bool m_post_chunked;
    int m_chunk_size;
    bool m_last_chunk_seen;
    bool m_reading_chunk_header;

    // Concerning the use of proxies
    int m_proxyPort;
    bool m_firstDns;
    bool m_usingProxy;
    bool m_neverUseProxy;

    // General status.
    s3eResult m_Status;

    // Specific error status
    unsigned int m_error_status;

    // Callback on completion/failure
    void *m_user_data;
    s3eCallback m_callback;
    s3eCallback m_header_callback;

    // In/Out buffers..
    int m_request_idx;
    size_t m_headers_end;
    std::string m_response;

    // Chunk header buffer
    int m_chunk_header_idx;
    std::string m_chunk_header;

    // Response code cache..
    int32 m_response_code;

    // Track content reading..
    int m_max_bytes;
    char *m_content_buf;
    char *m_orig_content_buf;
    int m_content_length;
    int m_total_transferred;
    int m_read_content_transferred;
    bool m_pending_read_callback;
    int m_read_timeout;
    int m_connect_timeout;
    int m_callback_timer;

    struct ReqHeader
    {
        std::string m_name;
        std::string m_value;
    };
    struct Data
    {
        std::string m_value;
        s3eFile* m_file;
        int m_size;
        int m_idx;

        Data() : m_file(NULL), m_idx(0) {}
    };

    // Post and Get data to send
    int m_data_len;
    int m_data_sent;
    std::list<Data>::iterator m_data_it;
    std::list<Data> m_data;

    CIwArray<ReqHeader> m_req_headers;

    // Universal fail.
    void Fail();

    // DNS..
    static bool s_bLookupInProgress;
    void DoDNSCallback(s3eInetAddress *);
    static int32 DNSCallback(void *, void *);
    bool CheckProxy(const char* name);

    struct DNSRequest
    {
        CIwHTTP *m_caller;
        std::string m_host;
        s3eInetAddress *m_addr;
    };

    typedef std::list<DNSRequest *> DNSRequestList;


    // DNS is single a shared resource..
    static DNSRequestList* s_pendingDNS;
    static void IssueDNSRequest();
    static int32 DoIssueDNSRequest(void *, void *);
    bool EnqueueDNSRequest(const char *host, s3eInetAddress *);

    //Thread safety
    static s3eThreadLock* m_dnsLock;

    // Connecting..
    void DoConnectTimeout();
    void DoConnectCallback(s3eResult);
    static int32 ConnectCallback(s3eSocket *, void *, void *);
    static int32 ConnectTimeoutCallback(void *, void *);

#ifdef IW_HTTP_SSL
    // Secure sockets..
    void DestroySSL();
    void StartSSLHandshake();
    void ContinueSSLHandshake();
#endif

    // Sending..
    void SendRequest();
    int SendBytes(const char *buf, int len);
    static int32 WriteableCallback(s3eSocket *, void *, void *);

    // Event driven I/O..
    void Readable();
    void Writeable();

    // Receiving..
    bool GotHeaders();
    void ReadResponse();
    int ReadBytes(char *buf, int len);
    static int32 ReadableCallback(s3eSocket *, void *, void *);

    // Transferring content..
    int TransferContent(char *, int);
    int TransferContentInternal(char *, int);
    int DoTransferCallback();
    static int32 TransferCallback(s3eSocket *, void *, void *);

    void GetChunkHeader();
    bool ParseChunkHeader();

    // Read timer..
    static int32 ReadTimeoutCallback(void *, void *);
    void onReadTimeout();

    // DoCallback in yield to avoid deep recursion
    static int32 DoCallback(void *, void *);

    void AddChunked(Data& f);
public:
    /**
     * Performs a GET request. If supplied, the callback will be
     * called when all the headers have been received.
     * @param URI The URI to fetch.
     * @param callback A callback that is called when the headers have
     * been received. The callback is also called when the operation fails;
     * use GetStatus to find out why.
     * Get and Posts are not concurrent. Calling Get whilst another
     * Get or Post is outstanding will cancel the previous operation.
     * @param data User data argument to the callback.
     * @return A standard s3e result code to indicate if the operation
     * succeeded.
     */
    s3eResult Get(const char *URI, s3eCallback callback, void *data);

    /**
     * Performs a POST request. The callback will be
     * called when all the headers have been received. The body must
     * be supplied all at once, and must remain valid until the
     * response headers are received.
     * @param URI The URI to fetch.
     * @param callback A callback that is called when all the headers have
     * been received. The callback is also called when the operation fails;
     * it should call GetStatus to find out why it has been called.
     * Get and Posts are not concurrent. Calling Post whilst another
     * Get or Post is outstanding will cancel the previous operation.
     * @param Body A pointer to the request body
     * @param BodyLength the length of the request body in bytes.
     * @param data User data argument to the callback.
     * @return A standard s3e result code to indicate if the operation
     * succeeded.
     */
    s3eResult Post(const char *URI, const char* Body, int32 BodyLength,
                   s3eCallback callback, void *data);

    /**
     * Cancel the current GET or POST request.
     * @return A standard s3e result code to indicate if the operation
     * succeeded.
     */
    s3eResult Cancel();

    /**
     * Returns the response code of the request. This will return 0
     * if the headers have yet to be received. e.g. 200 means OK, 404
     * means not found.
     * @return The response code.
     */
    uint32 GetResponseCode();

    /**
     * Returns the status of the current or last operation.
     *
     * @return If the operation is in progress or has succeeded,
     * S3E_RESULT_SUCCESS is returned. Otherwise S3E_RESULT_ERROR is
     * returned.
     *
     * @note Note that any callback should check the status by
     * calling this function to see if the operation has actually
     * succeeded.
     */
    s3eResult GetStatus() const { return m_Status;};

    /**
     * Gets a response header as an integer. This will only work
     * after the response headers have been received.
     * @param pName The name of the header (without colon)
     * @param val The value is filled in here
     * @return true if the header is present.
     */
    bool GetHeader(const char *pName, int32 &val);

    /**
     * Gets a response header as a string. This will only work
     * after the response headers have been received.
     * @param pName The name of the header (without colon)
     * @param val The value is filled in here
     * @return true if the header is present.
     */
    bool GetHeader(const char *pName, std::string &val);

    /**
     * DEPRECATED: Use GetHeader("Content-Length:", int32 &); instead.
     * Returns the length of the response content. This assumes the
     * response has a Content-Length: header (and the header has been
     * received)
     */
    uint32 ContentLength();

    /**
     * Returns the total amount of data so far received
     * @return The ammount of data so far received
     */
    uint32 ContentReceived();

    /**
     * Returns the amount of data currently known to be
     * expected. If a Content-Length: header has been received
     * then the return will be that value. If the transfer encoding is
     * chunked then this value will increase over time as chunk sizes
     * are received.
     * @return the minumum expected content size
     */
    uint32 ContentExpected();

    /**
     * Returns true when the data has all been received
     * @return true when the data has all been received
     */
    bool ContentFinished();

    /**
     * DEPRECATED: Use ReadData or ReadDataAsync instead.
     * Receives the content. The content may not be immediately
     * available, but the callback is called when it is.  If no
     * callback is supplied then only the immediately-available data
     * is returned. If more data is requested than is ever delivered
     * then the callback will be called either when the server socket
     * closes the connection or, if set in the icf, after httpreadtimeout
     * milliseconds. In the latter case the connection may still be active
     * and new data arriving will also trigger the callback in the normal way.
     * If httpreadtimeout is not set or is 0 then no timeout is set.
     * @param pBuf A buffer to fill with data
     * @param max_bytes The length of the buffer
     * @param callback An optional callback to call when either the
     * buffer is full, the socket has closed or the timeout elapsed.
     * @param cb_data A user data argument passed to the callback
     * @return the number of bytes returned so far.
     */
    uint32 ReadContent(char *buf, uint32 max_bytes, s3eCallback callback = NULL, void *cb_data = NULL);

    /**
     * Read data into buffer synchronously.
     * Reads up to max_bytes into the supplied buffer. The buffer must be at least
     * @ref max_bytes long. The function always returns immediately i.e. it does not
     * block. Calling ReadData before the headers have been received will return 0 bytes.
     * It's safe to interleave ReadData and ReadDataAsync calls but calling
     * ReadData whilst an uncompleted ReadDataAsync is in progress results in undefined
     * behaviour.
     *
     * @param pBuf A buffer to fill with data
     * @param max_bytes The length of the buffer
     * @return the number of bytes transferred to pBuf.
     */
    uint32 ReadData(char *pBuf, uint32 max_bytes);

    /**
     * Read data into buffer asynchronously.
     * Reads up to max_bytes into the supplied buffer. The buffer must be at least
     * @ref max_bytes long. The function always returns immediately. The supplied callback
     * function will be called when either max_bytes data is available, an error occurs on
     * the connection, all content is received or the read timeout elapses. This is guaranteed by the
     * system not to occur until after this function returns.
     * When the callback is made the systemData parameter contains the number of bytes succesfully
     * read and the userData parameter will contain the supplied user data.
     * Calling ReadDataAsync before the headers have been received results in the callback being
     * called at the next opportunity with 0 bytes read.
     * It's safe to interleave ReadData and ReadDataAsync calls but calling
     * ReadData whilst an uncompleted ReadDataAsync is in progress results in undefined
     * behaviour.
     *
     * @param pBuf A buffer to fill with data
     * @param max_bytes The length of the buffer
     * @param callback Pointer to @ref s3eCallback function that will be called when the read completes or some
     * other condition occurs (See notes above).
     * @param useData A user supplied argument passed to the callback
     */
    void ReadDataAsync(char *pBuf, uint32 max_bytes, uint32 timeout, s3eCallback cb, void *usrData = NULL);

    /**
     * Sets a request header as an integer. This will be used for all
     * subsequent requests. To remove it, set it to the empty string
     * using the other overload of this function.
     *
     * @note Host and Content-Length headers set using this function
     * are always ignored. Any header starting in Content- will be
     * ignored on GET requests. If User-Agent is not set then a
     * default User-Agent header is added. For POST requests where
     * Content-Type has not been set, then a default Content-Type of
     * multipart/form-data is added.
     *
     * @param pName The name of the header, excluding the colon.
     * @param val The value
     */
    void SetRequestHeader(const char *pName, int32 val);

    /**
     * Sets a request header as a string. This will be used for all
     * subsequent requests. To unset them, set them to the empty string.
     *
     * @note Host and Content-Length headers set using this function
     * are always ignored. Any header starting in Content- will be
     * ignored on GET requests. If User-Agent is not set then a
     * default User-Agent header is added. For POST requests where
     * Content-Type has not been set, then a default Content-Type of
     * multipart/form-data is added.
     *
     * @param pName The name of the header, excluding the colon.
     * @param val The value
     */
    void SetRequestHeader(const char *pName, const std::string &val);

    /**
     * Sets a form data section as a string. This will be used for the
     * next request.
     *
     * @note This sets Content-Type to multipart/form-data with a boundary
     *
     * @param pName The name of the part.
     * @param val The value
     */
    void SetFormData(const char *pName, const std::string &val);

    /**
     * Sets a form data section from a file. This will be used for the
     * next request.
     *
     * @note This sets Content-Type to multipart/form-data with a boundary
     *
     * @param pName The name of the part.
     * @param sourceFile The file to send the the server.
     * @param destName The filename the file is to be named on the server.
     * @param mimeType The mime type of the file.
     * @return whether the file was successfully opened.
     */
    s3eResult SetFormDataFile(const char *pName, const char* sourceFile, const char* destName, const char* mimeType);

    /**
     * Sets whether GET and POST send in chunked mode
     *
     * @note You must set this before calling SetFormData/SetFormDataFile
     *
     * @param isChunked Do we send in chunked mode.
     */
    void SetPostChunkedMode(bool isChunked);

    /**
     * Returns the amount of data sent in the last POST
     * @return the amount sent
     */
    uint32 ContentSent();

    /**
     * Constructor
     */
    CIwHTTP();

    /**
     * Destructor
     */
    virtual ~CIwHTTP();
};

/** @} */

#endif /* !IW_HTTP_H */
