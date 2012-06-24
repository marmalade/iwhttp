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
#include "IwHTTP.h"
#include <time.h>
#include <map>

#define TIMEOUT 60
#define CHUNK_SIZE 1024

class CHttpTest
{
public:
    CHttpTest() :
        m_Finished(false),
        m_GotHeaders(false),
        m_TheResponse(NULL),
        m_Status(S3E_RESULT_SUCCESS),
        m_ExpectedData(0),
        m_Timeout(TIMEOUT),
        m_StartTime(0)
    {
        s_Instances[&m_TheStack] = this;
    }

    ~CHttpTest()
    {
        if (m_TheResponse)
            delete m_TheResponse;
        s_Instances.erase(&m_TheStack);
    }

    static CHttpTest* GetInstance(CIwHTTP* http)
    {
        return s_Instances[http];
    }

    void startClock()
    {
        if (!m_StartTime)
            m_StartTime = time(NULL);
    }

    bool checkTimeout()
    {
        return (time(NULL) - m_StartTime > m_Timeout) || s3eDeviceCheckQuitRequest();
    }

    int32 HandleHeaders(bool dontRead)
    {
        IwAssert(CANARY, m_GotHeaders == false);

        m_GotHeaders = true;

        if (m_TheStack.GetStatus() == S3E_RESULT_ERROR)
        {
            IwTestTrace("iwhttp error in HandleHeaders");
            m_Status = S3E_RESULT_ERROR;
            m_Finished = true;
            return 0;
        }

        if (!m_TheStack.ContentExpected() && m_TheStack.ContentFinished())
        {
            m_Finished = true;
            return 0;
        }

        if (dontRead)
            return 0;

        m_ExpectedData = m_TheStack.ContentExpected();

        if (m_TheResponse)
            delete m_TheResponse;

        // Read in chunks..
        m_TheResponse = new char[MIN(CHUNK_SIZE, m_ExpectedData)];
        m_TheStack.ReadContent(
            m_TheResponse, MIN(CHUNK_SIZE, m_ExpectedData),
            GotData, this);

        return 0;
    }

    int32 HandleData()
    {
        // As long as we're receiving data, don't timeout
        startClock();

        m_ExpectedData -= MIN(CHUNK_SIZE, m_ExpectedData);

        if (m_ExpectedData)
        {
            m_TheStack.ReadContent(
                m_TheResponse,
                MIN(CHUNK_SIZE, m_ExpectedData),
                GotData, this);
        }
        else
        {
            m_Finished = true;
            m_Status = m_TheStack.GetStatus();
        }

        return 0;
    }

    bool getHeaders()
    {
        startClock();
        while (!m_GotHeaders && !checkTimeout())
            s3eDeviceYield(0);

        if (!m_GotHeaders)
        {
            IwTestError("Timeout getting header - Network slow or down perhaps?");
            return false;
        }

        return true;
    }

    bool run()
    {
        if (!getHeaders())
            return false;

        while (!m_Finished && !checkTimeout())
            s3eDeviceYield(0);

        if (!m_Finished)
        {
            IwTestError("Timeout - Network slow or down perhaps?");
            return false;
        }

        if (m_Status == S3E_RESULT_ERROR)
        {
            IwTestError("Fetch failed");
            return false;
        }

        return true;
    }

    CIwHTTP m_TheStack;
    bool m_Finished;
    bool m_GotHeaders;
    char* m_TheResponse;
    s3eResult m_Status;
    uint32 m_ExpectedData;
    time_t m_Timeout;

    static int32 GotHeaders(void* sysData, void *usrData)
    {
        bool dontRead = (usrData != NULL);
        CIwHTTP* http = (CIwHTTP*)sysData;
        return CHttpTest::GetInstance(http)->HandleHeaders(dontRead);
    }

private:
    static int32 GotData(void*, void* userData)
    {
        CHttpTest* self = (CHttpTest*)userData;
        return self->HandleData();
    }
    static std::map<CIwHTTP*, CHttpTest*> s_Instances;

    time_t m_StartTime;
};
