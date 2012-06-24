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
#include "IwHTTP.h"
#include "IwTest.h"
#include "s3eDevice.h"

#define TEST_GROUP IwHTTP

static CIwHTTP* g_TheStack = NULL;
static bool g_Finished = false;
static char* g_TheResponse;
static s3eResult g_status = S3E_RESULT_SUCCESS;

static const unsigned char base64_table[65] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
char * base64_encode(const char *src, size_t len, size_t *out_len)
{
        char *out, *pos;
        const char *end, *in;
        size_t olen;
#ifdef BASE64_ENCODE_ADD_NEWLINES
        int line_len;
#endif

        olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
        olen += olen / 72; /* line feeds */
        olen++; /* nul termination */
        out = (char *)malloc(olen);
        if (out == NULL)
                return NULL;

        end = src + len;
        in = src;
        pos = out;
#ifdef BASE64_ENCODE_ADD_NEWLINES
        line_len = 0;
#endif
        while (end - in >= 3) {
                *pos++ = base64_table[in[0] >> 2];
                *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
                *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
                *pos++ = base64_table[in[2] & 0x3f];
                in += 3;
#ifdef BASE64_ENCODE_ADD_NEWLINES
                line_len += 4;
                if (line_len >= 72) {
                        *pos++ = '\r\n';
                        line_len = 0;
                }
#endif
        }

        if (end - in) {
                *pos++ = base64_table[in[0] >> 2];
                if (end - in == 1) {
                        *pos++ = base64_table[(in[0] & 0x03) << 4];
                        *pos++ = '=';
                } else {
                        *pos++ = base64_table[((in[0] & 0x03) << 4) |
                                              (in[1] >> 4)];
                        *pos++ = base64_table[(in[1] & 0x0f) << 2];
                }
                *pos++ = '=';
#ifdef BASE64_ENCODE_ADD_NEWLINES
               line_len += 4;
#endif
        }

#ifdef BASE64_ENCODE_ADD_NEWLINES
        if (line_len)
               *pos++ = '\r\n';
#endif

        *pos = '\0';
        if (out_len)
                *out_len = pos - out;
        return out;
}
enum HTTPStatus
{
    kNone,
    kUploading,
    kOK,
    kError,
};

#define HTTP_URL "http://test.ideaworkslabs.com/post.php"
#define HTTP_RAW_URL "http://test.ideaworkslabs.com/largepost.php"

static int32 GotData(void*, void*)
{
    g_TheResponse[g_TheStack->ContentReceived()] = 0;
    g_Finished = true;
    g_status = g_TheStack->GetStatus();
    return 0;
}

static int32 GotHeaders(void*, void*)
{
    if (g_TheStack->GetStatus() == S3E_RESULT_ERROR)
    {
        IwTestError("GotHeaders failed");
        g_status = S3E_RESULT_ERROR;
        g_Finished = true;
        return 0;
    }

    if (!g_TheStack->ContentExpected())
        g_Finished = true;
    else
    {
        g_TheResponse = new char[g_TheStack->ContentExpected()+1];
        g_TheStack->ReadContent(g_TheResponse, g_TheStack->ContentExpected(),
                                GotData, NULL);
    }
    return 0;
}

static void Tidy()
{
    delete g_TheStack;
    g_TheStack = NULL;
    delete g_TheResponse;
    g_TheResponse = NULL;
    g_Finished = false;
}

IW_TEST(test_iwhttp_Post_SetContentType)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    const char* body = "Mary had a little lamb\n"
                       "Its fleece was white as snow\n"
                       "And everywhere that mary went\n"
                       "The lamb was sure to go\n\r\n";
    g_TheStack->SetRequestHeader("Content-Type","text/plain");
    g_TheStack->Post(HTTP_RAW_URL, body, strlen(body) - 2,
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    if (g_TheResponse == NULL || strcmp(g_TheResponse, body))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, body);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Post_Chunked)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    const char* body = "Mary had a little lamb\n"
                       "Its fleece was white as snow\n"
                       "And everywhere that mary went\n"
                       "The lamb was sure to go\n";
    g_TheStack->SetPostChunkedMode(true);
    g_TheStack->SetRequestHeader("Content-Type","text/plain");
    g_TheStack->Post(HTTP_RAW_URL, body, strlen(body),
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    if (g_TheResponse == NULL || strcmp(g_TheResponse, body))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, body);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Post_Form)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    g_TheStack->SetFormData("test", "Mary had a little lamb");
    g_TheStack->SetFormData("test2", "Its fleece was white as snow\n");
    g_TheStack->Post(HTTP_URL, "", 0,
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    const char output[] = "test: Mary had a little lamb\ntest2: Its fleece was white as snow\n\n";
    if (g_TheResponse == NULL || strcmp(g_TheResponse, output))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, output);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Post_Form_Chunked)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    g_TheStack->SetPostChunkedMode(true);
    g_TheStack->SetFormData("test", "Mary had a little lamb");
    g_TheStack->SetFormData("test2", "Its fleece was white as snow\n");
    g_TheStack->Post(HTTP_URL, "", 0,
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    const char output[] = "test: Mary had a little lamb\ntest2: Its fleece was white as snow\n\n";
    if (g_TheResponse == NULL || strcmp(g_TheResponse, output))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, output);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Post_SmallFile)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    if (g_TheStack->SetFormDataFile("file", "app.icf", "app.icf", "text/plain")
        != S3E_RESULT_SUCCESS)
    {
        IwTestError("File Read Failed");
        Tidy();
        return false;
    }
    g_TheStack->Post(HTTP_URL, "", 0,
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    const char output[] = "file: app.icf text/plain 1223\n";
    if (g_TheResponse == NULL || strcmp(g_TheResponse, output))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, output);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Post_LargeFile)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    if (g_TheStack->SetFormDataFile("file", "iw2d/textures/withVariableAlpha.png", "file.png", "image/png")
        != S3E_RESULT_SUCCESS)
    {
        IwTestError("File Read Failed");
        Tidy();
        return false;
    }
    g_TheStack->Post(HTTP_URL, "", 0,
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    const char output[] = "file: file.png image/png 5940\n";
    if (g_TheResponse == NULL || strcmp(g_TheResponse, output))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, output);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Post_SmallFile_Chunked)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    g_TheStack->SetPostChunkedMode(true);
    if (g_TheStack->SetFormDataFile("file", "app.icf", "app.icf", "text/plain")
        != S3E_RESULT_SUCCESS)
    {
        IwTestError("File Read Failed");
        Tidy();
        return false;
    }
    g_TheStack->Post(HTTP_URL, "", 0,
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    const char output[] = "file: app.icf text/plain 1223\n";
    if (g_TheResponse == NULL || strcmp(g_TheResponse, output))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, output);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Post_LargeFile_Chunked)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;
    g_TheStack->SetPostChunkedMode(true);
    if (g_TheStack->SetFormDataFile("file", "iw2d/textures/withVariableAlpha.png", "file.png", "image/png")
        != S3E_RESULT_SUCCESS)
    {
        IwTestError("File Read Failed");
        Tidy();
        return false;
    }
    g_TheStack->Post(HTTP_URL, "", 0,
                     GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    const char output[] = "file: file.png image/png 5940\n";
    if (g_TheResponse == NULL || strcmp(g_TheResponse, output))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, output);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}

IW_TEST(test_iwhttps_Post_SmallFile)
{
    g_Finished = false;
    g_TheStack = new CIwHTTP;

    char buf[256];
    const char *passString = "tobe:ryloth";
    char *base64;
    base64 = base64_encode(passString, strlen(passString), NULL);
    snprintf(buf, 256, "Basic %s", base64);
    free(base64);

    if (g_TheStack->SetFormDataFile("file", "app.icf", "app.icf", "text/plain")
        != S3E_RESULT_SUCCESS)
    {
        IwTestError("File Read Failed");
        Tidy();
        return false;
    }

    g_TheStack->SetRequestHeader("Authorization", buf);
    g_TheStack->SetRequestHeader("Cache-Control", "max-age=0");
    g_TheStack->SetRequestHeader("Accept", "application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,* /*;q=0.5");
    g_TheStack->SetRequestHeader("Accept-Encoding", "gzip,deflate,sdch");
    g_TheStack->SetRequestHeader("Accept-Language", "en-GB");
    g_TheStack->SetRequestHeader("Accept-Charset", "ISO-8859-1,utf-8;q=0.7,*;q=0.3");

    g_TheStack->Post(HTTP_URL, "", 0, GotHeaders, NULL);
    while (!g_Finished)
    {
        s3eDeviceYield(10);
    }
    if (g_TheStack->GetResponseCode() != 200)
    {
        IwTestError("Wrong response code: %d", g_TheStack->GetResponseCode());
        Tidy();
        return false;
    }

    const char output[] = "file: app.icf text/plain 1223\n";
    if (g_TheResponse == NULL || strcmp(g_TheResponse, output))
    {
        IwTestError("Output did not match expected result:\n%s\n%s", g_TheResponse, output);
        g_status = S3E_RESULT_ERROR;
    }

    Tidy();
    if (g_status == S3E_RESULT_ERROR)
        return false;
    return true;
}
