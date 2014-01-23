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

#include "IwURI.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

CIwURI::CIwURI() : m_pURI(NULL)
{
}

CIwURI::CIwURI(const char *URI) : m_pURI(NULL)
{
    *this = URI;
}

CIwURI::CIwURI(const CIwURI& URI) : m_pURI(NULL)
{
    *this = URI;
}

CIwURI::~CIwURI()
{
    if (m_pURI)
        delete m_pURI;
}

CIwURI& CIwURI::operator=(const char *URI)
{
    if (m_pURI)
        delete m_pURI;

    m_pURI = new char[strlen(URI) + 1];
    strcpy(m_pURI, URI);

    ParseURI();

    return *this;
}

CIwURI& CIwURI::operator=(const CIwURI& URI)
{
    *this = URI.GetAll();
    return *this;
}

void CIwURI::ParseURI()
{
    struct _PROTOCOLS { const char *p;    CIwURI::PROTOCOL e; uint16 port;    };
    static _PROTOCOLS prots[] = {
        { "http://", HTTP, 80 },
        { "https://", HTTPS, 443 },
        { "ftp://", FTP, 21 },
        { "file://", FILE, 0 },
        { NULL, UNKNOWN }
    };


    m_pHost = NULL;
    m_pTail = NULL;
    m_protocol = UNKNOWN;
    m_pRemoved = NULL;
    m_port = 0;

    unsigned int uri_len = strlen(m_pURI);

    struct _PROTOCOLS *prot = &prots[0];
    while (prot->p)
    {
        unsigned int len = strlen(prot->p);

        if (strncmp(prot->p, m_pURI, len) == 0)
        {
            m_pHost = &m_pURI[len];
            m_protocol = prot->e;
            m_port = prot->port;
            break;
        }
        prot++;
    }

    if (!m_pHost)
    {
        // We didn't recognise the protocol but should
        // still try to find the host part.

        char *delim = strstr(m_pURI, "://");
        if (delim && ((delim - m_pURI + 3) < (int)uri_len) )
            m_pHost = &delim[3];
    }

    char *pChar = m_pHost;
    while (pChar && *pChar && *pChar != '/' && *pChar != ':' && *pChar != '?') pChar++;
    if (pChar && *pChar)
    {
        if (*pChar == '/' || *pChar == '?')
        {
            m_pRemoved = pChar;
            m_removed = *m_pRemoved;
            *pChar = 0; // Null-terminate host
        }
        else if (*pChar == ':')
        {
            m_pRemoved = pChar;
            m_removed = *m_pRemoved;
            *pChar = 0; // Null-terminate host
            m_port = atoi(++pChar);
            while (isdigit(*pChar)) pChar++;
        }

        if (m_removed == '?')
            m_pTail = pChar;
        else if ((m_pRemoved == pChar && m_removed) || *pChar) // If the character that was at pChar is not NULL
            m_pTail = ++pChar; // Then there's a tail
    }
    if (m_pHost && !*m_pHost) // If the host is empty, claim it's not there
    {
        m_pHost = NULL;
    }
}

void CIwURI::TerminateHost() const
{
    if (m_pRemoved && *m_pRemoved)
    {
        *m_pRemoved = 0;
    }
}

void CIwURI::UnterminateHost() const
{
    if (m_pRemoved && !*m_pRemoved)
    {
        *m_pRemoved = m_removed;
    }
}

const char *CIwURI::GetAll() const
{
    UnterminateHost();
    return m_pURI;
}
