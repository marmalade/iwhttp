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
#ifndef IW_URI_H
#define IW_URI_H

#include "s3eTypes.h"

class CCl
{

protected:
    char *m_p1;
    char *m_p2;
    char *m_p3;
    uint16 m_p4;
    char m_5;
    mutable char* m_p6;

    void f2();
    void f3() const;
    void f4() const;

public:
    CCl();

    virtual ~CCl();

    uint16 f5() const { return m_p4; }

    const char *f6() const { f3(); return m_p2; }

    const char *f9() const;
};


#endif /* !IW_URI */
