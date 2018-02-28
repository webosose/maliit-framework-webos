/* @@@LICENSE
 *
 *      Copyright (c) 2013-2017 LG Electronics, Inc.
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

#include "mimsettingslunasettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "webosloginfo.h"

#include <QDebug>

typedef QList<MImSettingsLunaSettingsBackendPrivate *> SettingsList;

static SettingsList g_managerSettingsList;
static SettingsList g_pluginSettingsList;

struct MImSettingsLunaSettingsBackendPrivate {
    MImSettingsLunaSettingsBackend* backend;
    QString key;
    MImSettings::Group group;
    QVariant value;

    void setJsonValue(QVariant jsonValue)
    {
        if (value != jsonValue) {
            value = jsonValue;
            Q_EMIT backend->valueChanged();
        }
    }

    void registerInstance()
    {
        switch (group) {
        case MImSettings::GroupManager:
            if (!g_managerSettingsList.contains(this))
                g_managerSettingsList.append(this);
            break;
        case MImSettings::GroupPlugin:
            if (!g_pluginSettingsList.contains(this))
                g_pluginSettingsList.append(this);
            break;
        }
    }

    void unregisterInstance()
    {
        switch (group) {
        case MImSettings::GroupManager:
            if (!g_managerSettingsList.contains(this))
                g_managerSettingsList.append(this);
            break;
        case MImSettings::GroupPlugin:
            if (g_pluginSettingsList.contains(this))
                g_pluginSettingsList.removeOne(this);
            break;
        }
    }
};

QString MImSettingsLunaSettingsBackend::key() const
{
    Q_D(const MImSettingsLunaSettingsBackend);

    return d->key;
}

QVariant MImSettingsLunaSettingsBackend::value(const QVariant &def) const
{
    Q_D(const MImSettingsLunaSettingsBackend);

    if (d->value.isNull())
        return def;
    else
        return d->value;
}

void MImSettingsLunaSettingsBackend::set(const QVariant &val)
{
    Q_UNUSED(val);
}

void MImSettingsLunaSettingsBackend::unset()
{
}

QList<QString> MImSettingsLunaSettingsBackend::listDirs() const
{
    return QList<QString>();
}

QList<QString> MImSettingsLunaSettingsBackend::listEntries() const
{
    return QList<QString>();
}

MImSettingsLunaSettingsBackend::MImSettingsLunaSettingsBackend(const QString &key, const MImSettings::Group group, QObject *parent)
    : MImSettingsBackend(parent)
    , d_ptr(new MImSettingsLunaSettingsBackendPrivate)
{
    Q_D(MImSettingsLunaSettingsBackend);

    d->backend = this;
    d->key = key;
    d->group = group;
    d->registerInstance();
}

MImSettingsLunaSettingsBackend::~MImSettingsLunaSettingsBackend()
{
    Q_D(MImSettingsLunaSettingsBackend);

    d->unregisterInstance();
}

static inline void processResponse(MImSettingsLunaSettingsBackendPrivate *d, QJsonObject &response)
{
    QJsonObject::const_iterator it;
    if ((it = response.find("settings")) != response.end()) {
        QJsonObject settings = it.value().toObject();
        QJsonObject::const_iterator ite = settings.find(d->key);
        if (ite != settings.end())
            d->setJsonValue(ite.value().toVariant());
    } else if ((it = response.find("configs")) != response.end()) {
        QJsonObject settings = it.value().toObject();
        QJsonObject::const_iterator ite = settings.find(d->key);
        if (ite != settings.end())
            d->setJsonValue(ite.value().toVariant());
    }
}

static bool getSystemSettingsCallback(LSHandle *handle, LSMessage *message, void *ctx)
{
    Q_UNUSED(handle);
    Q_UNUSED(ctx);

    if (message) {
        const char *jsonString = LSMessageGetPayload(message);
        if (jsonString) {
            QJsonObject json = QJsonDocument::fromJson(jsonString).object();

            webOSLogInfo("SYSTEMSETTINGS", "STATE_CHANGE", jsonString);

            // Deliver changes to manager first as it may affect plugins due to the change
            SettingsList::iterator i;
            for (i = g_managerSettingsList.begin(); i != g_managerSettingsList.end(); i++) {
                processResponse(*i, json);
            }
            for (i = g_pluginSettingsList.begin(); i != g_pluginSettingsList.end(); i++) {
                processResponse(*i, json);
            }
        }
    }
    return true;
}

static bool serverConnectCallback(LSHandle *handle, LSMessage *message, void *ctx)
{
    MImSettingsLunaSettingsBackendFactory *factory =
        static_cast<MImSettingsLunaSettingsBackendFactory *>(ctx);
    return factory->serverConnectCallback(handle, message, ctx);
}

struct LunaServiceRequestInfoMap {
    const char *key;
    const char *serviceUrl;
    const char *parameter;
};

static const LunaServiceRequestInfoMap g_lunaServiceRequestInfoMap[] = {
    { "localeInfo", "luna://com.webos.settingsservice/getSystemSettings",
        "{\"subscribe\":true, \"keys\":[\"%1\"]}" },
    { "country", "luna://com.webos.settingsservice/getSystemSettings",
        "{\"subscribe\":true, \"keys\":[\"%1\"], \"category\":\"option\"}" },
    { "com.webos.service.ime.timeout", "luna://com.webos.service.config/getConfigs",
        "{\"subscribe\":true, \"configNames\":[\"%1\"]}" },
    { "com.webos.service.ime.static", "luna://com.webos.service.config/getConfigs",
        "{\"subscribe\":true, \"configNames\":[\"%1\"]}" },
    { 0, 0, 0 }
};

void MImSettingsLunaSettingsBackendFactory::subscribeSettings(const QString &key)
{
    bool ret;
    LSError error;
    LSErrorInit(&error);
    LSMessageToken token;

    if (m_subscriptionMap.contains(key) && m_subscriptionMap.value(key) != NULL)
        return;

    const LunaServiceRequestInfoMap *map;
    for (map = g_lunaServiceRequestInfoMap; map->key != 0; map++)
        if (!key.compare(map->key))
            break;

    if (!map->key) {
        qWarning() << "key not registered in service request map: " << key;
        return;
    }

    QString parameter = QString(map->parameter).arg(map->key);
    ret = LSCall(m_handle, map->serviceUrl, parameter.toUtf8().data(), getSystemSettingsCallback, this, &token, &error);

    if (!ret) {
        qWarning() << "failed LSCall " << map->serviceUrl << ": " << error.message;
        return;
    }

    m_subscriptionMap.insert(key, token);
}

void MImSettingsLunaSettingsBackendFactory::unsubscribeSettings(const QString &key)
{
    bool ret;
    LSError error;
    LSErrorInit(&error);
    LSMessageToken token;

    if (m_subscriptionMap.contains(key) && m_subscriptionMap.value(key) != NULL) {
        token = (LSMessageToken)m_subscriptionMap.value(key);
        ret = LSCallCancel(m_handle, token, &error);

        m_subscriptionMap.insert(key, NULL);
    }
}

void MImSettingsLunaSettingsBackendFactory::unsubscribeAll()
{
    QMapIterator<QString, LSMessageToken> iter(m_subscriptionMap);
    while (iter.hasNext()) {
        iter.next();
        unsubscribeSettings(iter.key());
    }
}

void MImSettingsLunaSettingsBackendFactory::restoreSubscriptions()
{
    QMapIterator<QString, LSMessageToken> iter(m_subscriptionMap);
    while (iter.hasNext()) {
        iter.next();
        subscribeSettings(iter.key());
    }
}

bool MImSettingsLunaSettingsBackendFactory::serverConnectCallback(LSHandle *handle, LSMessage *message, void *ctx)
{
    Q_UNUSED(handle);
    Q_UNUSED(ctx);

    if (!message)
        return false;

    const char *jsonString = LSMessageGetPayload(message);
    if (!jsonString)
        return false;

    QJsonObject json = QJsonDocument::fromJson(jsonString).object();
    if (json["connected"].toBool() != true)
    {
        unsubscribeAll();
        return false;
    } else {
        restoreSubscriptions();
    }

    return true;
}

void MImSettingsLunaSettingsBackendFactory::registerService()
{
    bool ret;
    LSError error;
    LSErrorInit(&error);

    ret = LSRegister("com.webos.service.ime.settings", &m_handle, &error);

    if (!ret) {
        g_critical("Failed to register handler: %s", error.message);
        LSErrorFree(&error);
        exit(1);
    }

    ret = LSGmainAttach(m_handle, m_mainLoop, &error);
    if (!ret) {
        qCritical() << "Failed to attach service to main loop: " << error.message;
        exit(1);
    }

    ret = LSCall(m_handle,
            "palm://com.palm.lunabus/signal/registerServerStatus",
            "{\"serviceName\":\"com.webos.settingsservice\"}",
            ::serverConnectCallback, this, NULL, &error);
    if (!ret) {
        qCritical() << "Failed in calling palm://com.palm.lunabus/signal/registerServerStatus: " << error.message;
        exit(1);
    }
}

void MImSettingsLunaSettingsBackendFactory::unregisterService()
{
    if (m_handle) {
        unsubscribeAll();
        m_subscriptionMap.clear();

        LSError error;
        LSErrorInit(&error);
        LSUnregister(m_handle, &error);
        m_handle = NULL;
    }
}

MImSettingsLunaSettingsBackendFactory::MImSettingsLunaSettingsBackendFactory()
    : MImSettingsQSettingsBackendFactory()
{
    m_mainCtx = g_main_context_default();
    m_mainLoop = g_main_loop_new(m_mainCtx, TRUE);

    registerService();
}

MImSettingsLunaSettingsBackendFactory::MImSettingsLunaSettingsBackendFactory(const QString &organization, const QString &application)
    : MImSettingsQSettingsBackendFactory(organization, application)
{
    m_mainCtx = g_main_context_default();
    m_mainLoop = g_main_loop_new(m_mainCtx, TRUE);

    registerService();
}

MImSettingsLunaSettingsBackendFactory::~MImSettingsLunaSettingsBackendFactory()
{
    unregisterService();
}

static const char *ACCESSORY_ENABLED = "/maliit/accessoryenabled";
static const char *ONSCREEN_ACTIVE = "/maliit/onscreen/active";
static const char *CURRENT_LANGUAGE = "/maliit/onscreen/currentlanguage";

MImSettingsBackend *MImSettingsLunaSettingsBackendFactory::create(const QString &key, const MImSettings::Group group, QObject *parent)
{
    qInfo() << "Creating MImSettingsBackend for" << key;

    if (key.endsWith("localeInfo")) {
        MImSettingsBackend *settings = new MImSettingsLunaSettingsBackend("localeInfo", group, parent);
        subscribeSettings("localeInfo");
        return settings;
    } else if (key.endsWith("country")) {
        MImSettingsBackend *settings = new MImSettingsLunaSettingsBackend("country", group, parent);
        subscribeSettings("country");
        return settings;
    } else if (key.endsWith("timeout")) {
        MImSettingsBackend *settings = new MImSettingsLunaSettingsBackend("com.webos.service.ime.timeout", group, parent);
        subscribeSettings("com.webos.service.ime.timeout");
        return settings;
    } else if (key.endsWith("static")) {
        MImSettingsBackend *settings = new MImSettingsLunaSettingsBackend("com.webos.service.ime.static", group, parent);
        subscribeSettings("com.webos.service.ime.static");
        return settings;
    } else if (key.endsWith("currentLanguage")) {
        return MImSettingsQSettingsBackendFactory::create(CURRENT_LANGUAGE, group, parent);
    } else if (key.endsWith("accessoryenabled")) {
        return MImSettingsQSettingsBackendFactory::create(ACCESSORY_ENABLED, group, parent);
    } else if (key.endsWith("onscreen/active")) {
        return MImSettingsQSettingsBackendFactory::create(ONSCREEN_ACTIVE, group, parent);
    } else {
        return MImSettingsQSettingsBackendFactory::create(key, group, parent);
    }
}
