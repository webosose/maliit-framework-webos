/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#include "mimsubviewoverride.h"
#include "mimonscreenplugins.h"

MImSubViewOverride::MImSubViewOverride(MImOnScreenPlugins *plugins,
                                       QObject *parent)
    : QObject(parent)
    , mPlugins(plugins)
{}

MImSubViewOverride::~MImSubViewOverride()
{
    // This will undo the effects of any other active attribute extension that
    // currently enabled all subviews!
    if (not mPlugins.isNull() && mPlugins.data()) {
        mPlugins.data()->setAllSubViewsEnabled(false);
    }
}
