#!/usr/bin/env python
#
# Copyright (C) 2001-2012 Ideaworks3D Ltd.
# All Rights Reserved.
#
# This document is protected by copyright, and contains information
# proprietary to Ideaworks3D.
# This file consists of source code released by Ideaworks3D under
# the terms of the accompanying End User License Agreement (EULA).
# Please do not use this program/source code before you have read the
# EULA and have agreed to be bound by its terms.
#
import sys

out = ""
while 1:
    line = sys.stdin.readline()
    if line.strip() == "":
        break
    out += line

print "HTTP/1.1 200 OK\r\n"
print "Content-Type: text-plain\r\n"
print "Content-Length: %s\r\n\r\n" % len(out)
print out
print "\r\n"
