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
#ifndef IW_URI_H
#define IW_URI_H

#include "s3eTypes.h"

/**
 * @addtogroup iwhttpgroup
 * @{
 *
 * @defgroup iwhttpuriobject URI Object
 * The URI Object for representing URIs as defined in RFC 3986.
 *
 * For more information on the URI Object, see the
 * @ref uriparsing "URI Parsing" section in the
 * <i>IwHTTP API Documentation</i>.
 *
 * @{
 */

/**
 * URI Class
 */
class CIwURI
{
public:
    /**
     * The protocol (or rather the scheme, to use the official
     * naming). Note that currently the CIwHTTP class only supports
     * HTTP.
     */
    enum PROTOCOL
    {
        HTTP,   ///< The Hypertext transfer protocol
        HTTPS,  ///< HTTP running over SSL or TLS
        FTP,    ///< The File Transfer Protocol
        FILE,   ///< A local file
        UNKNOWN ///< An unknown scheme.
    };

protected:
    char *m_pURI;
    char *m_pHost;
    char *m_pTail;
    uint16 m_port;
    char m_removed;
    mutable char* m_pRemoved;

    PROTOCOL m_protocol;

    void ParseURI();
    void TerminateHost() const;
    void UnterminateHost() const;
public:
    /**
     * Constructs the object but doesn't assign a value to it.
     */
    CIwURI();

    /**
     * Copy constructor
     * @param that The URI to copy
     */
    CIwURI(const CIwURI &that);

    /**
     * Construct from a string
     * @param URI The string
     */
    CIwURI(const char *URI);

    /**
     * Destructor
     */
    virtual ~CIwURI();

    /**
     * Assignment operator
     * @param URI The string to assign to the URI
     * @return this
     */
    CIwURI &operator=(const char *URI);

    /**
     * Assignment operator
     * @param that The string to assign to the URI
     * @return this
     */
    CIwURI &operator=(const CIwURI& that);

    /**
     * Returns the port, if present or 0 otherwise.
     * @return the port, if present or 0 otherwise.
     */
    uint16 GetPort() const { return m_port; }

    /**
     * Returns the host, if present or NULL otherwise. The returned
     * string is owned by the CIwURI object and only remains valid
     * for the lifetime of the CIwURI or until the next call to any
     * other accesor
     * @return The host
     */
    const char *GetHost() const { TerminateHost(); return m_pHost; }

    /**
     * Returns everything after the host and the port. In URI terms  this
     * is the path, the query and the fragment, not the query.
     * @return The tail
     */
    char *GetTail() { UnterminateHost();return m_pTail; }

    /**
     * Returns the protocol, or UNKNOWN if it is missing or unknown.
     * The protocol is also known as the scheme.
     * @return The protocol.
     */
    CIwURI::PROTOCOL GetProtocol() const { return m_protocol; }

    /**
     * Returns the entire URI. The returned string is owned by the
     * CIwURI class and is only valid for the lifetime of the CIwURI
     * or until any other accessor is called.
     * @return The URI
     */
    const char *GetAll() const;
};

/** @} */
/** @} */

#endif /* !IW_URI */
