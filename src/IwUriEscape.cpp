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

#include "IwUriEscape.h"

std::string CIwUriEscape::Escape(std::string in)
{
    std::string ret;
    for (std::string::iterator it = in.begin(); it != in.end(); it++)
    {
        char c = *it;
        if (isalnum(c))
        {
            ret += c;
        }
        else
        {
            char buf[5];
            snprintf(buf, 5, "%%%.2x", c);
            ret += buf;
        }
    }
    return ret;
}
