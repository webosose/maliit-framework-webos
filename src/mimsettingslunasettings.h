/* @@@LICENSE
 *
 *      Copyright (c) 2013-2020 LG Electronics, Inc.
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

#ifndef MIMSETTINGSLUNASETTINGS_H
#define MIMSETTINGSLUNASETTINGS_H

#include <luna-service2/lunaservice.h>
#include <QMap>

#include "mimsettingsqsettings.h"

struct MImSettingsLunaSettingsBackendPrivate;

class MImSettingsLunaSettingsBackend : public MImSettingsBackend
{
    Q_OBJECT
public:
    explicit MImSettingsLunaSettingsBackend(const QString &key, const MImSettings::Group group, QObject *parent = 0);
    virtual ~MImSettingsLunaSettingsBackend();
    virtual QString key() const;
    virtual QVariant value(const QVariant &def) const;
    virtual void set(const QVariant &val);
    virtual void unset();
    virtual QList<QString> listDirs() const;
    virtual QList<QString> listEntries() const;

private:
    QScopedPointer<MImSettingsLunaSettingsBackendPrivate> d_ptr;

    Q_DISABLE_COPY(MImSettingsLunaSettingsBackend)
    Q_DECLARE_PRIVATE(MImSettingsLunaSettingsBackend)
};

class MImSettingsLunaSettingsBackendFactory : public MImSettingsQSettingsBackendFactory
{
public:
    explicit MImSettingsLunaSettingsBackendFactory();
    explicit MImSettingsLunaSettingsBackendFactory(const QString &organization, const QString &application);
    virtual ~MImSettingsLunaSettingsBackendFactory();
    MImSettingsLunaSettingsBackendFactory(const MImSettingsLunaSettingsBackendFactory&) = delete;
    MImSettingsLunaSettingsBackendFactory &operator=(const MImSettingsLunaSettingsBackendFactory&) = delete;
    virtual MImSettingsBackend *create(const QString &key, const MImSettings::Group group, QObject *parent);

    bool serverConnectCallback(LSHandle *handle, LSMessage *message, void *ctx);

private:
    void registerService();
    void unregisterService();

    void subscribeSettings(const QString &key);
    void unsubscribeSettings(const QString &key);
    void unsubscribeAll();
    void restoreSubscriptions();

    GMainContext *m_mainCtx;
    GMainLoop *m_mainLoop;
    LSHandle *m_handle;
    QMap <QString, LSMessageToken> m_subscriptionMap;
};

#endif // MIMSETTINGSLUNASETTINGS_H
