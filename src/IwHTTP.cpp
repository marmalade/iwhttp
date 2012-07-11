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

#include "IwHTTP.h"

#include <string>
#include <sstream>
#include <algorithm>
#include <stdlib.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "IwMath.h"
#include "s3eConfig.h"
#include "s3eTimer.h"

#include "errno.h"


#define MULTIPART_BOUNDARY "--------:fjksalgjalkgjlk:"


CCl1::CCl1() 
{
}

CCl1::~CCl1()
{
}


