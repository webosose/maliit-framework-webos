/* @@@LICENSE
*
*      Copyright (c) 2013-2019 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#ifndef IMELUNASERVICE_H
#define IMELUNASERVICE_H

#include <QtCore>
#include <QJsonObject>
#include <QHash>
#include <QSharedPointer>
#include "glib.h"
#include "luna-service2/lunaservice.h"

class MInputContextConnection;

struct RemoteKeyboardClient
{
    QString token;
};

class IMELunaService : public QObject
{
    Q_OBJECT

public:
    explicit IMELunaService(QSharedPointer<MInputContextConnection> connection);
    virtual ~IMELunaService();

protected Q_SLOTS:
    void onWidgetStateChanged(unsigned int clientId, const QMap<QString, QVariant> &newState,
                              const QMap<QString, QVariant> &oldState, bool focusChanged);

    void onReset();

protected:
    enum DeleteMode { BackspaceMode, DirectMode, MixedMode };

    void startService();
    void broadcastWidgetState();
    void broadcastToSubscribers(QJsonObject response);
    bool hasSubscribers() const;

    QJsonObject getWidgetStateJson() const;
    void insertText(const QString& text, bool replace, ssize_t length = 0);
    void deleteCharacters(int numChars, DeleteMode mode);
    void sendEnterKey();

    static bool handleRegisterRemoteKeyboard(LSHandle *handle, LSMessage *message, void *data);
    static bool handleInsertText(LSHandle *handle, LSMessage *message, void *data);
    static bool handleDeleteCharacters(LSHandle *handle, LSMessage *message, void *data);
    static bool handleSendEnterKey(LSHandle *handle, LSMessage *message, void *data);

    static bool handleSubscriptionCancel(LSHandle *handle, LSMessage *message, void *data);

    static LSMethod ime_bus_methods[];

    static const char *SubscriberKey;

    QSharedPointer<MInputContextConnection> m_connection;
    GMainLoop *m_mainLoop;
    LSHandle *m_handle;

    bool m_focusChangedSinceLastBroadcast;
    QJsonObject m_lastWidgetState;
    QTimer *m_broadcastTimer;

    QHash<QString, QSharedPointer<RemoteKeyboardClient> > m_clientByToken;
};

#endif // IMELUNASERVICE_H5
