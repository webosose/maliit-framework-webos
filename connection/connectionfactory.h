/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Copyright (C) 2016-2021 LG Electronics, Inc.
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#ifndef MALIIT_CONNECTIONFACTORY_H
#define MALIIT_CONNECTIONFACTORY_H

#include "minputcontextconnection.h"

namespace Maliit {
#ifdef HAVE_WAYLAND
MInputContextConnection *createWestonIMProtocolConnection();
#endif
} // namespace Maliit

#endif // MALIIT_CONNECTIONFACTORY_H
