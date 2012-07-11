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

#include "IwURI.h"



class CCl1
{
public:
    typedef enum ES
    {
        NONE,
    } ES;

protected:
    CCl m_Cl;

    s3eResult m_p10;


public:

    s3eResult GetStatus() const { return m_p10;};

    CCl1();
    virtual ~CCl1();
};

#endif /* !IW_HTTP_H */
