/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Copyright (C) 2013-2021 LG Electronics, Inc.
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#include "mimserver.h"

#include "mimpluginmanager.h"
#include "mimsettings.h"
#include "imelunaservice.h"
#include "webosloginfo.h"

class MImServerPrivate
{
public:
    explicit MImServerPrivate();

    // Manager for loading and handling all plugins
    MIMPluginManager *pluginManager = nullptr;

    // Connection to application side (input-context)
    QSharedPointer<MInputContextConnection> icConnection;

    QSharedPointer<IMELunaService> lunaService;

private:
    Q_DISABLE_COPY(MImServerPrivate)
};

MImServerPrivate::MImServerPrivate()
{
}

MImServer::MImServer(const QSharedPointer<MInputContextConnection> &icConnection,
                     const QSharedPointer<Maliit::AbstractPlatform> &platform,
                     QObject *parent)
  : QObject(parent)
  , d_ptr(new MImServerPrivate)
{
    Q_D(MImServer);

    d->icConnection = icConnection;
    d->pluginManager = new MIMPluginManager(d->icConnection, platform);
    d->lunaService.reset(new IMELunaService(d->icConnection));

    webOSLogInfo("VKB_VERSION", "FRAMEWORK", MALIIT_VERSION);
}

MImServer::~MImServer()
{
}

void MImServer::configureSettings(MImServer::SettingsType settingsType)
{
    switch (settingsType) {

    case TemporarySettings:
        MImSettings::setPreferredSettingsType(MImSettings::TemporarySettings);
        break;

    case PersistentSettings:
        MImSettings::setPreferredSettingsType(MImSettings::PersistentSettings);
        break;

    default:
        qCritical() << "Invalid value for preferredSettingType." << settingsType;
    }
}
