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

#ifndef IW_URI_ESCAPE_H
#define IW_URI_ESCAPE_H

#include <string>

/**
 * @addtogroup iwhttpgroup
 * @{
 */

/// A class for URI-escaping functions.
class CIwUriEscape
{
public:
    /// URI-escapes a string, for use in the middle of a query.
    /// in other words, the text is turned into something%20like%20this.
    /// @param in The string to escape
    /// @return The escaped string
    static std::string Escape(std::string in);
};

/** @} */
#endif
