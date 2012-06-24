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
#include "IwTest.h"
#include "IwRandom.h"
#include "s3eDevice.h"
#include "test_iwhttp.h"

#define TEST_GROUP IwHTTP

#define EXPECTED "This is an example.\n"

int32 GotData(void*, void*);

std::map<CIwHTTP*, CHttpTest*> CHttpTest::s_Instances;

IW_TEST(test_iwhttp_GetSimple)
{
    CHttpTest test;
    test.m_TheStack.Get("http://www.google.com", CHttpTest::GotHeaders, NULL);
    return test.run();
}

IW_TEST(test_iwhttp_GetMulti)
{
    CHttpTest test;
    test.m_TheStack.Get("http://www.yahoo.com", CHttpTest::GotHeaders, NULL);

    // Also check that it's impossible to do 2 requests at once.
    if (test.m_TheStack.Get("http://www.ask.com", CHttpTest::GotHeaders, NULL) !=
        S3E_RESULT_ERROR)
    {
        IwTestError("Second simultaneous request didn't fail");
        return false;
    }

    return test.run();
}

IW_TEST(test_iwhttp_GetCheckResult)
{
    CHttpTest test;
    test.m_TheStack.Get("http://test.ideaworkslabs.com/example.txt", CHttpTest::GotHeaders, NULL);

    if (!test.run())
        return false;

    std::string ctype;
    test.m_TheStack.GetHeader("Content-Type", ctype);
    if (strcmp("text/plain", ctype.c_str()))
    {
        IwTestError("Wrong charset");
        return false;
    }

    if (test.m_TheStack.GetResponseCode() != 200)
    {
        IwTestError("Wrong result code");
        return false;
    }

    IwTrace(CANARY, ("%s", test.m_TheResponse));
    if (test.m_TheResponse && strncmp(EXPECTED, test.m_TheResponse, strlen(EXPECTED)))
    {
        IwTestError("Wrong data");
        return false;
    }

    return true;
}

IW_TEST(test_iwhttp_GetLargeResponse)
{
    // This gets a file that's a bit over a meg long,
    // thus testing non-trivial response lengths. File is also binary
    // so charset should be iso-8859-1

    CHttpTest test;
    test.m_TheStack.Get("http://test.ideaworkslabs.com/largefile.txt", CHttpTest::GotHeaders, NULL);

    if (!test.run())
        return false;

    std::string ctype;
    test.m_TheStack.GetHeader("Content-Type", ctype);
    if (strcmp("text/plain", ctype.c_str()))
    {
        IwTestError("Wrong charset");
        return false;
    }

    if (test.m_TheStack.GetResponseCode() != 200)
    {
        IwTestError("Wrong result code");
        return false;
    }

    // Check the known length of the file
    if (test.m_TheStack.ContentReceived() != 1386439)
    {
        IwTestError("Wrong size: %d");
        return false;
    }
    return true;
}

/**
 * Repeat test 2 but this time use an encrypted connection.
 */
IW_TEST(test_iwhttp_GetSSL)
{

    CHttpTest test;
    test.m_TheStack.Get("https://test.ideaworkslabs.com/example.txt", CHttpTest::GotHeaders, NULL);

    if (!test.run())
        return false;

    std::string ctype;
    test.m_TheStack.GetHeader("Content-Type", ctype);
    if (strcmp("text/plain", ctype.c_str()))
    {
        IwTestError("Wrong charset: %s != text/plain", ctype.c_str());
        return false;
    }

    if (test.m_TheStack.GetResponseCode() != 200)
    {
        IwTestError("Wrong result code");
        return false;
    }

    IwTrace(HTTP, ("%s", test.m_TheResponse));
    if (test.m_TheResponse && strncmp(EXPECTED, test.m_TheResponse, strlen(EXPECTED)))
    {
        IwTestError("Wrong data");
        return false;
    }

    return true;
}

/**
 * Test that IwHTTP objects can repeatedly be created and destroyed without
 * leaking sockets
 */
IW_TEST(test_iwhttp_GetMany)
{
    const unsigned int _TIMEOUT = 10000;
    for (int i = 0; i < 16; i++)
    {
        CHttpTest test;
        test.m_TheStack.Get("http://test.ideaworkslabs.com/example.txt", CHttpTest::GotHeaders, NULL);
        test.m_Timeout = _TIMEOUT;
        if (!test.run())
            return false;

        if (test.m_TheStack.GetStatus() != S3E_RESULT_SUCCESS || strncmp(EXPECTED, test.m_TheResponse, strlen(EXPECTED)) != 0)
        {
            IwTestError("Failed to fetch data");
            return false;
        }
    }
    return true;
}

/**
 * Test that IwHTTP objects can repeatedly be created and destroyed without
 * leaking sockets except this time for HTTPS://
 */
IW_TEST(test_iwhttp_GetManySSL)
{
    const unsigned int _TIMEOUT = 10000;
    for (int i = 0; i < 16; i++)
    {
        CHttpTest test;
        test.m_TheStack.Get("https://test.ideaworkslabs.com/example.txt", CHttpTest::GotHeaders, NULL);
        test.m_Timeout = _TIMEOUT;
        if (!test.run())
            return false;

        if (test.m_TheStack.GetStatus() != S3E_RESULT_SUCCESS || strncmp(EXPECTED, test.m_TheResponse, strlen(EXPECTED)) != 0)
        {
            IwTestError("Failed to fetch data");
            return false;
        }
    }
    return true;
}

/**
 * Test chunked-encoding with standard chunks..
 */
IW_TEST(test_iwhttp_GetChunked)
{
    int buf_sizes[] = { 10, 1000 };

    // Repeat this test 3 times with different read buffer sizes
    for (uint i = 0; i < sizeof(buf_sizes)/sizeof(int); i++)
    {
        CHttpTest test;

        // This URL will return Transfer-Encoded: chunked
        test.m_TheStack.Get("http://test.ideaworkslabs.com/index.php", CHttpTest::GotHeaders, (void*)true);
        if (!test.getHeaders())
            return false;

        std::string ctype;
        test.m_TheStack.GetHeader("Content-Type", ctype);
        if (strcmp("text/html", ctype.c_str()))
        {
            IwTestError("Wrong charset");
            return false;
        }

        if (test.m_TheStack.GetResponseCode() != 200)
        {
            IwTestError("Wrong result code");
            return false;
        }

        int32 bufSize = buf_sizes[i];

        // Read the chunked body..
        std::string reply;
        char* buf = new char[bufSize + 1];
        while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
        {
            memset(buf, 0, bufSize + 1);
            int bytes_read = test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
            if (bytes_read > bufSize)
            {
                IwTestError("Read too much");
                return false;
            }
            reply += buf;
            s3eDeviceYield(0);
        }
        delete buf;

        // Although the received data is variable, the last bytes are always the PHP license and closing HTML
        const char *expected = "<p>If you did not receive a copy of the PHP license, or have any questions about PHP licensing, please contact license@php.net.\n" \
        "</p>\n</td></tr>\n</table><br />\n</div></body></html>1"; // Server always sends that last '1' ??

        if (reply.find(expected) != reply.size() - strlen(expected))
        {
            IwTestError(
                "Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d ReplySize: %d",
                bufSize, test.checkTimeout(), test.m_TheStack.ContentReceived(), reply.size()
            );
            return false;
        }

        if (test.m_Status == S3E_RESULT_ERROR)
        {
            IwTestError("Fetch failed");
            return false;
        }
    }

    return true;
}

/**
 * Test chunked-encoding with tiny chunks..
 */
IW_TEST(test_iwhttp_GetChunkTiny)
{
    int buf_sizes[] = { 1, 10, 1000 };

    int i;
    // Repeat this test 3 times with different read buffer sizes
    for (i = 0; i < 3; i++)
    {
        CHttpTest test;

        // This URL will return Transfer-Encoded: chunked
        test.m_TheStack.Get("http://test.ideaworkslabs.com/smallchunks.php", CHttpTest::GotHeaders, (void*)true);

        if (!test.getHeaders())
            return false;

        std::string ctype;
        test.m_TheStack.GetHeader("Content-Type", ctype);
        if (strcmp("text/html", ctype.c_str()))
        {
            IwTestError("Wrong charset");
            return false;
        }

        if (test.m_TheStack.GetResponseCode() != 200)
        {
            IwTestError("Wrong result code");
            return false;
        }

        IwRandSeed(time(NULL) & 0xFFFFFFFF);
        int32 bufSize = buf_sizes[i];

        // Read the chunked body..
        std::string reply;
        char *buf = new char[bufSize + 1];
        while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
        {
            memset(buf, 0, bufSize + 1);
            int bytes_read = test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
            if (bytes_read > bufSize)
            {
                IwTestError("Read too much");
                return false;
            }

            reply += buf;

            s3eDeviceYield(10);
        }
        delete buf;

        // Although the received data is variable, the last bytes are always the PHP license and closing HTML
        const char *expected = "789";

        if (reply.find(expected) != reply.size() - strlen(expected))
        {
            IwTestError("Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d", bufSize,
                        test.checkTimeout(),
                        test.m_TheStack.ContentReceived());
            return false;
        }
    }

    return true;
}

/**
 * Test chunked-encoding with standard chunks with HTTPS
 */
IW_TEST(test_iwhttp_GetChunkSSL)
{
    int buf_sizes[] = { 10, 1000 };

    // Repeat this test 3 times with different read buffer sizes
    for (uint i = 0; i < sizeof(buf_sizes)/sizeof(int); i++)
    {
        CHttpTest test;

        // This URL will return Transfer-Encoded: chunked
        test.m_TheStack.Get("https://test.ideaworkslabs.com/index.php", CHttpTest::GotHeaders, (void*)true);

        if (!test.getHeaders())
            return false;

        std::string ctype;
        test.m_TheStack.GetHeader("Content-Type", ctype);
        if (strcmp("text/html", ctype.c_str()))
        {
            IwTestError("Wrong charset");
            return false;
        }

        if (test.m_TheStack.GetResponseCode() != 200)
        {
            IwTestError("Wrong result code");
            return false;
        }

        int32 bufSize = buf_sizes[i];

        // Read the chunked body..
        std::string reply;
        char *buf = new char[bufSize + 1];
        while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
        {
            memset(buf, 0, bufSize + 1);
            test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
            reply += buf;

            s3eDeviceYield(0);
        }
        delete buf;

        // Although the received data is variable, the last bytes are always the PHP license and closing HTML
        const char *expected = "<p>If you did not receive a copy of the PHP license, or have any questions about PHP licensing, please contact license@php.net.\n" \
        "</p>\n</td></tr>\n</table><br />\n</div></body></html>1"; // Server always sends that last '1' ??

        if (reply.find(expected) != reply.size() - strlen(expected))
        {
            IwTestError(
                "Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d ReplySize: %d",
                bufSize, test.checkTimeout(), test.m_TheStack.ContentReceived(), reply.size()
            );
            return false;
        }
    }

    return true;
}

/**
 * Test chunked-encoding with HTTPS with tiny chunks..
 */
IW_TEST(test_iwhttp_TinyChunkSSL)
{
    int buf_sizes[] = { 1, 10, 1000 };

    // Repeat this test 3 times with different read buffer sizes
    for (uint i = 0; i < sizeof(buf_sizes)/sizeof(int); i++)
    {
        CHttpTest test;

        // This URL will return Transfer-Encoded: chunked
        test.m_TheStack.Get("https://test.ideaworkslabs.com/smallchunks.php", CHttpTest::GotHeaders, (void*)true);

        if (!test.getHeaders())
            return false;

        std::string ctype;
        test.m_TheStack.GetHeader("Content-Type", ctype);
        if (strcmp("text/html", ctype.c_str()))
        {
            IwTestError("Wrong charset");
            return false;
        }

        if (test.m_TheStack.GetResponseCode() != 200)
        {
            IwTestError("Wrong result code");
            return false;
        }

        IwRandSeed(time(NULL) & 0xFFFFFFFF);
        int32 bufSize = buf_sizes[i];

        // Read the chunked body..
        std::string reply;
        char *buf = new char[bufSize + 1];
        while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
        {
            memset(buf, 0, bufSize + 1);
            test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
            reply += buf;

            s3eDeviceYield(10);
        }
        delete buf;

        const char *expected = "0123456789";
        if (reply.find(expected) != reply.size() - strlen(expected))
        {
            IwTestError("Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d", bufSize, test.checkTimeout(), test.m_TheStack.ContentReceived());
            return false;
        }
    }

    return true;
}

/**
 * A test for blocking reads, smallchunksdelay.php deliberately
 * stalls when sending, non-SSL version
 */
IW_TEST(test_iwhttp_GetBlocking)
{
    CHttpTest test;
    //int i = 0;

    // This URL will return Transfer-Encoded: chunked
    test.m_TheStack.Get("http://test.ideaworkslabs.com/smallchunksdelay.php", CHttpTest::GotHeaders, (void*)true);

    if (!test.getHeaders())
        return false;

    std::string ctype;
    test.m_TheStack.GetHeader("Content-Type", ctype);
    if (strcmp("text/html", ctype.c_str()))
    {
        IwTestError("Wrong charset");
        return false;
    }

    if (test.m_TheStack.GetResponseCode() != 200)
    {
        IwTestError("Wrong result code");
        return false;
    }

    IwRandSeed(time(NULL) & 0xFFFFFFFF);
    int bufSize = 10;

    // Read the chunked body..
    std::string reply;
    char *buf = new char[bufSize + 1];
    while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
    {
        memset(buf, 0, bufSize + 1);
        test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
        reply += buf;

        s3eDeviceYield(10);
    }
    delete buf;

    const char *expected = "0123456789";
    if (reply.find(expected) != reply.size() - strlen(expected))
    {
        IwTestError(
            "Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d", bufSize, test.checkTimeout(), test.m_TheStack.ContentReceived()
        );
        return false;
    }

    return true;
}

/**
 * A test for blocking reads, smallchunksdelay.php deliberately
 * stalls when sending, SSL version
 */
IW_TEST(test_iwhttp_GetBlockingSSL)
{
    CHttpTest test;

    // This URL will return Transfer-Encoded: chunked
    test.m_TheStack.Get("https://test.ideaworkslabs.com/smallchunksdelay.php", CHttpTest::GotHeaders, (void*)true);
    if (!test.getHeaders())
        return false;

    std::string ctype;
    test.m_TheStack.GetHeader("Content-Type", ctype);
    if (strcmp("text/html", ctype.c_str()))
    {
        IwTestError("Wrong charset");
        return false;
    }

    if (test.m_TheStack.GetResponseCode() != 200)
    {
        IwTestError("Wrong result code");
        return false;
    }

    IwRandSeed(time(NULL) & 0xFFFFFFFF);
    int32 bufSize = 10;

    // Read the chunked body..
    std::string reply;
    char *buf = new char[bufSize + 1];
    while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
    {
        memset(buf, 0, bufSize + 1);
        test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
        reply += buf;

        s3eDeviceYield(10);
    }
    delete buf;

    const char *expected = "0123456789";
    if (reply.find(expected) != reply.size() - strlen(expected))
    {
        IwTestError(
            "Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d", bufSize, test.checkTimeout(), test.m_TheStack.ContentReceived()
        );
        return false;
    }

    if (test.m_Status == S3E_RESULT_ERROR)
    {
        IwTestError("Fetch failed");
        return false;
    }

    return true;
}

static int32 GotHeaders_Get13(void *sysData, void *usrData)
{
    CIwHTTP *http = (CIwHTTP*)sysData;
    bool *ret = (bool*)usrData;

    IwTestTrace("GotHeaders_Get13: %d %d", http->ContentExpected(), http->ContentFinished());
    if (http->ContentExpected() > 100 || http->ContentFinished() == true)
        *ret = false;
    else
        *ret = true;

    CHttpTest::GetInstance(http)->HandleHeaders(true);
    return 0;
}

/**
 * A test for a common usage of GotHeaders callback under chunked-encoding
 * which causes lots of confusion amongst users..
 */
IW_TEST(test_iwhttp_ChunkedHeaders)
{
    CHttpTest test;
    bool ret = false;

    // This URL will return Transfer-Encoded: chunked
    test.m_TheStack.Get("http://test.ideaworkslabs.com/smallchunksdelay.php", GotHeaders_Get13, (void*)&ret);

    if (!test.getHeaders())
        return false;

    std::string ctype;
    test.m_TheStack.GetHeader("Content-Type", ctype);
    if (strcmp("text/html", ctype.c_str()))
    {
        IwTestError("Wrong charset");
        return false;
    }

    if (test.m_TheStack.GetResponseCode() != 200)
    {
        IwTestError("Wrong result code");
        return false;
    }

    int32 bufSize = 10;

    // Read the chunked body..
    std::string reply;
    char *buf = new char[bufSize + 1];
    while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
    {
        memset(buf, 0, bufSize + 1);
        test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
        reply += buf;

        s3eDeviceYield(10);

        if (test.m_TheStack.ContentReceived() > test.m_TheStack.ContentExpected())
            break;

        if (test.m_TheStack.ContentReceived() != reply.size())
            break;
    }
    delete buf;

    if (test.m_TheStack.ContentReceived() != reply.size())
    {
        IwTestError(
            "Received totals don't match: %d %d", test.m_TheStack.ContentReceived(), reply.size()
        );
        return false;
    }

    if (test.m_TheStack.ContentReceived() != test.m_TheStack.ContentExpected())
    {
        IwTestError(
            "Content totals don't match: %d %d", test.m_TheStack.ContentReceived(), test.m_TheStack.ContentExpected()
        );
        return false;
    }

    const char *expected = "0123456789";
    if (reply.find(expected) != reply.size() - strlen(expected))
    {
        IwTestError(
            "Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d", bufSize, test.checkTimeout(), test.m_TheStack.ContentReceived()
        );
        return false;
    }
    return ret;
}

/**
 * A test for a common usage of GotHeaders callback under chunked-encoding
 * which causes lots of confusion amongst users.. SSL version
 */
IW_TEST(test_iwhttp_ChunkedHeadersSSL)
{
    CHttpTest test;
    bool ret = false;

    // This URL will return Transfer-Encoded: chunked
    test.m_TheStack.Get("https://test.ideaworkslabs.com/smallchunksdelay.php", GotHeaders_Get13, (void*)&ret);

    if (!test.getHeaders())
        return false;

    std::string ctype;
    test.m_TheStack.GetHeader("Content-Type", ctype);
    if (strcmp("text/html", ctype.c_str()))
    {
        IwTestError("Wrong charset");
        return false;
    }

    if (test.m_TheStack.GetResponseCode() != 200)
    {
        IwTestError("Wrong result code");
        return false;
    }

    int bufSize = 100;

    // Read the chunked body..
    std::string reply;
    char *buf = new char[bufSize + 1];
    while (!test.m_TheStack.ContentFinished() && !test.checkTimeout())
    {
        memset(buf, 0, bufSize + 1);
        test.m_TheStack.ReadContent(buf, bufSize, NULL, NULL);
        reply += buf;

        s3eDeviceYield(10);

        if (test.m_TheStack.ContentReceived() > test.m_TheStack.ContentExpected())
            break;

        if (test.m_TheStack.ContentReceived() != reply.size())
            break;
    }
    delete buf;

    if (test.m_TheStack.ContentReceived() != reply.size())
    {
        IwTestError(
            "Received totals don't match: %d %d", test.m_TheStack.ContentReceived(), reply.size()
        );
        return false;
    }

    if (test.m_TheStack.ContentReceived() != test.m_TheStack.ContentExpected())
    {
        IwTestError(
            "Content totals don't match: %d %d", test.m_TheStack.ContentReceived(), test.m_TheStack.ContentExpected()
        );
        return false;
    }

    const char *expected = "0123456789";
    if (reply.find(expected) != reply.size() - strlen(expected))
    {
        IwTestError(
            "Did not read all chunked data. BufSize: %d Timeout: %d Recvd: %d", bufSize, test.checkTimeout(), test.m_TheStack.ContentReceived()
        );
        return false;
    }

    if (test.m_Status == S3E_RESULT_ERROR)
    {
        IwTestError("Fetch failed");
        return false;
    }
    return ret;
}
