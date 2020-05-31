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

#include <QtGlobal>

#include "connectionfactory.h"
#include "mimserver.h"
#include "mimserveroptions.h"
#include "mimglobalsettings.h"
#ifndef NOXCB
#include "xcbplatform.h"
#endif
#ifdef HAVE_WAYLAND
#include "waylandplatform.h"
#endif // HAVE_WAYLAND
#include "unknownplatform.h"
#ifdef HAS_PMLOGLIB
#include <PmLogLib.h>
#endif

#include <QGuiApplication>
#include <QtDebug>

namespace {

void disableMInputContextPlugin()
{
    // none is a special value for QT_IM_MODULE, which disables loading of any
    // input method module in Qt 5.
    setenv("QT_IM_MODULE", "none", true);
}

#ifndef HAS_PMLOGLIB
bool isDebugEnabled()
{
    static int debugEnabled = -1;

    if (debugEnabled == -1) {
        QByteArray debugEnvVar = qgetenv("MALIIT_DEBUG");
        if (!debugEnvVar.isEmpty() && debugEnvVar != "0") {
            debugEnabled = 1;
        } else {
            debugEnabled = 0;
        }
    }

    return debugEnabled == 1;
}
#endif

#ifdef HAS_PMLOGLIB
void outputMessages(QtMsgType type,
                    const QMessageLogContext &context,
                    const QString &msg)
{
    PmLogContext pmContext;
    PmLogGetContext("IME", &pmContext);
    static const char *msgId = "default";

    QString funcName = QString("unknown");
    QStringList parser = QString(context.function).split('(', QString::SkipEmptyParts);
    if (!parser.isEmpty()) {
        parser = parser.first().split(' ', QString::SkipEmptyParts);
        if (!parser.isEmpty())
            funcName = parser.last();
    }

    switch (type) {
    case QtDebugMsg:
        PmLogDebug(pmContext, "%s, %s", funcName.toLatin1().constData(), msg.toStdString().c_str());
        break;
    case QtInfoMsg:
        PmLogInfo(pmContext, msgId, 1, PMLOGKS("func", funcName.toLatin1().constData()), "%s", msg.toStdString().c_str());
        break;
    case QtWarningMsg:
        PmLogWarning(pmContext, msgId, 1, PMLOGKS("func", funcName.toLatin1().constData()), "%s", msg.toStdString().c_str());
        break;
    case QtCriticalMsg:
        PmLogError(pmContext, msgId, 1, PMLOGKS("func", funcName.toLatin1().constData()), "%s", msg.toStdString().c_str());
        break;
    case QtFatalMsg:
        PmLogCritical(pmContext, msgId, 1, PMLOGKS("func", funcName.toLatin1().constData()), "%s", msg.toStdString().c_str());
        abort();
    }
}
#else
void outputMessages(QtMsgType type,
                    const QMessageLogContext &context,
                    const QString &msg)
{
    Q_UNUSED(context);
    QByteArray utf_text(msg.toUtf8());
    const char *raw(utf_text.constData());

    switch (type) {
    case QtDebugMsg:
        if (isDebugEnabled())
            fprintf(stderr, "DEBUG: %s\n", raw);
        break;
    case QtInfoMsg:
        fprintf(stderr, "INFO: %s\n", raw);
        break;
    case QtWarningMsg:
        fprintf(stderr, "WARNING: %s\n", raw);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "CRITICAL: %s\n", raw);
        break;
    case QtFatalMsg:
        fprintf(stderr, "FATAL: %s\n", raw);
        abort();
    }
}
#endif

QSharedPointer<MInputContextConnection> createConnection(const MImServerConnectionOptions &options)
{
    Q_UNUSED(options);
#ifdef HAVE_WAYLAND
    if (QGuiApplication::platformName().startsWith("wayland")) {
        return QSharedPointer<MInputContextConnection>(Maliit::createWestonIMProtocolConnection());
    }
#endif
#ifdef HAVE_QTDBUS
    if (options.overriddenAddress.isEmpty()) {
        return QSharedPointer<MInputContextConnection>(Maliit::DBus::createInputContextConnectionWithDynamicAddress());
    } else {
        return QSharedPointer<MInputContextConnection>(Maliit::DBus::createInputContextConnectionWithFixedAddress(options.overriddenAddress,
                                                                                                                  options.allowAnonymous));
    }
#endif

    // Return null pointer
    return QSharedPointer<MInputContextConnection>();
}

QSharedPointer<Maliit::AbstractPlatform> createPlatform()
{
#ifdef HAVE_WAYLAND
    if (QGuiApplication::platformName().startsWith("wayland")) {
        return QSharedPointer<Maliit::AbstractPlatform>(new Maliit::WaylandPlatform);
    } else
#endif
#ifndef NOXCB
    if (QGuiApplication::platformName() == "xcb") {
        return QSharedPointer<Maliit::AbstractPlatform>(new Maliit::XCBPlatform);
    } else {
#else
    {
#endif
        return QSharedPointer<Maliit::AbstractPlatform>(new Maliit::UnknownPlatform);
    }
}

} // unnamed namespace

int main(int argc, char **argv)
{
    qInstallMessageHandler(outputMessages);

    // QT_IM_MODULE, MApplication and QtMaemo5Style all try to load
    // MInputContext, which is fine for the application. For the passthrough
    // server itself, we absolutely need to prevent that.
    disableMInputContextPlugin();

    MImServerCommonOptions serverCommonOptions;
    MImServerConnectionOptions connectionOptions;

    const bool allRecognized = parseCommandLine(argc, argv);
    if (serverCommonOptions.showHelp) {
        printHelpMessage();
        return 1;
    } else if (not allRecognized) {
        printHelpMessage();
    }

    // Create Singleton globalsettings & put instanceId.
    MImGlobalSettings::instance()->setInstanceId(connectionOptions.instanceId);
    qInfo() << "MaliitServer: Using instance number " << MImGlobalSettings::instance()->getInstanceId();

    MImGlobalSettings::instance()->setNoLS2Service(connectionOptions.noLS2Service);

    QGuiApplication app(argc, argv);

    // Input Context Connection
    QSharedPointer<MInputContextConnection> icConnection(createConnection(connectionOptions));

    if (icConnection.isNull()) {
        qCritical("Unable to create connection, aborting.");
        return 1;
    }

    QSharedPointer<Maliit::AbstractPlatform> platform(createPlatform());

    // The actual server
    MImServer::configureSettings(MImServer::PersistentSettings);
    MImServer imServer(icConnection, platform);
    Q_UNUSED(imServer);

    int ret = 1;

    try {
        ret = app.exec();
    } catch (const std::bad_alloc &) {
        qCritical("There is not enough memory. bad_alloc exception catched");
        return 1;
    }

    return ret;
}
