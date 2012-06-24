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
#include "IwURI.h"
#include "IwTest.h"

#define TEST_GROUP IwHTTP

#define URI1 "http://www.ideaworks3d.com"
#define URI2 "ftp://www.ideaworks3d.com/somestuff?query"
#define URI3 "thing://www.ideaworks3d.com:81"
#define URI4 "https://www.ideaworks3d.com?query"
#define URI5 "file:///stuff"
#define URI6 "http://www.ideaworks3d.com:81/path?query"
#define URI7 "mailto:abuse@ideaworks3d.com"

IW_TEST(test_iwhttp_HostPresent1)
{
    CIwURI t(URI1);
    if (strcmp(t.GetHost(), "www.ideaworks3d.com"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_HostPresent2)
{
    CIwURI s(URI2);
    CIwURI t("http://www.google.com");
    t = s;
    if (strcmp(t.GetHost(), "www.ideaworks3d.com"))
        return false;
    return true;
}
IW_TEST(test_iwhttp_HostPresent3)
{
    CIwURI s(URI3);
    CIwURI t(s);
    if (strcmp(t.GetHost(), "www.ideaworks3d.com"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_HostPresent4)
{
    CIwURI t;
    t = URI4;
    if (strcmp(t.GetHost(), "www.ideaworks3d.com"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_HostPresent5)
{
    CIwURI t(URI5);
    if (t.GetHost())
        return false;
    return true;
}

IW_TEST(test_iwhttp_HostPresent6)
{
    CIwURI t(URI6);
    if (strcmp(t.GetHost(), "www.ideaworks3d.com"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_HostPresent7)
{
    CIwURI t(URI5);
    if (t.GetHost())
        return false;
    return true;
}

IW_TEST(test_iwhttp_Scheme1)
{
    CIwURI t(URI1);
    if (t.GetProtocol() != CIwURI::HTTP)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Scheme2)
{
    CIwURI t(URI2);
    if (t.GetProtocol() != CIwURI::FTP)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Scheme3)
{
    CIwURI t(URI3);
    if (t.GetProtocol() != CIwURI::UNKNOWN)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Scheme4)
{
    CIwURI t(URI4);
    if (t.GetProtocol() != CIwURI::HTTPS)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Scheme5)
{
    CIwURI t(URI5);
    if (t.GetProtocol() != CIwURI::FILE)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Scheme6)
{
    CIwURI t(URI6);
    if (t.GetProtocol() != CIwURI::HTTP)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Scheme7)
{
    CIwURI t(URI7);
    if (t.GetProtocol() != CIwURI::UNKNOWN)
        return false;
    return true;
}


IW_TEST(test_iwhttp_Port1)
{
    CIwURI t(URI1);
    if (t.GetPort() != 80)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Port2)
{
    CIwURI t(URI2);
    if (t.GetPort() != 21)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Port3)
{
    CIwURI t(URI3);
    if (t.GetPort() != 81)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Port4)
{
    CIwURI t(URI4);
    if (t.GetPort() != 443)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Port5)
{
    CIwURI t(URI5);
    if (t.GetPort() != 0)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Port6)
{
    CIwURI t(URI6);
    if (t.GetPort() != 81)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Port7)
{
    CIwURI t(URI7);
    if (t.GetPort() != 0)
        return false;
    return true;
}

IW_TEST(test_iwhttp_Tail1)
{
    CIwURI t(URI1);
    if (t.GetTail())
        return false;
    return true;
}

IW_TEST(test_iwhttp_Tail2)
{
    CIwURI t(URI2);
    if (strcmp(t.GetTail(), "somestuff?query"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_Tail3)
{
    CIwURI t(URI3);
    if (t.GetTail())
        return false;
    return true;
}

IW_TEST(test_iwhttp_Tail4)
{
    CIwURI t(URI4);
    if (strcmp(t.GetTail(), "?query"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_Tail5)
{
    CIwURI t(URI5);
    if (strcmp(t.GetTail(), "stuff"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_Tail6)
{
    CIwURI t(URI6);
    if (strcmp(t.GetTail(), "path?query"))
        return false;
    return true;
}

IW_TEST(test_iwhttp_Tail7)
{
    CIwURI t(URI7);
    if (t.GetTail())
        return false;
    return true;
}

IW_TEST(test_iwhttp_All1)
{
    CIwURI t(URI1);
    if (strcmp(t.GetAll(), URI1))
        return false;
    return true;
}

IW_TEST(test_iwhttp_All2)
{
    CIwURI t(URI2);
    if (strcmp(t.GetAll(), URI2))
        return false;
    return true;
}

IW_TEST(test_iwhttp_All3)
{
    CIwURI t(URI3);
    if (strcmp(t.GetAll(), URI3))
        return false;
    return true;
}

IW_TEST(test_iwhttp_All4)
{
    CIwURI t(URI4);
    if (strcmp(t.GetAll(), URI4))
        return false;
    return true;
}

IW_TEST(test_iwhttp_All5)
{
    CIwURI t(URI5);
    if (strcmp(t.GetAll(), URI5))
        return false;
    return true;
}

IW_TEST(test_iwhttp_All6)
{
    CIwURI t(URI6);
    if (strcmp(t.GetAll(), URI6))
        return false;
    return true;
}

IW_TEST(test_iwhttp_All7)
{
    CIwURI t(URI7);
    if (strcmp(t.GetAll(), URI7))
        return false;
    return true;
}
