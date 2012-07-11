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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

CCl::CCl() : m_p1(NULL)
{
}

CCl::~CCl()
{
    if (m_p1)
        delete m_p1;
}

void CCl::f2()
{
}

void CCl::f3() const
{
}

void CCl::f4() const
{
}

const char *CCl::f9() const
{
    return m_p1;
}
