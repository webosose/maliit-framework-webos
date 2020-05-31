/* @@@LICENSE
*
*      Copyright (c) 2019 LG Electronics, Inc.
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

#ifndef MIMGLOBALSETTINGS_H
#define MIMGLOBALSETTINGS_H

#include <QString>

//! \internal
/*! \ingroup maliitserver
 * \brief Maliit Input Method Global Settings
 *
 * Work as singleton and bring global datas for maliit server.
 */

class MImGlobalSettings
{
public:
    static MImGlobalSettings * instance() {
        static MImGlobalSettings instance;
        return &instance;
    }

    MImGlobalSettings() {}
    ~MImGlobalSettings() {}

    /*!
     * \brief Get instance ID.
     */
    int getInstanceId() const;

    /*!
     * \brief Get base appId.
     */
    QString getAppId() const;

    /*!
     * \brief Get serviceName of current instance.
     */
    QString getServiceName() const;

    /*!
     * \brief Get appId for sending requests to settingsService.
    */
    QString getAppIdForSettingsService() const;

    /*!
     * \brief Set instance ID for this application. m_instanceId will used by all other functions.
     * \param instance Id, 0~(N-1) for total N instances. Must be set in earliest step on this process.
     */
    void setInstanceId(int instanceId);

    /*!
     * \brief Set bool value whether ls2 service is allowed or not.
     * \param value true if allowed or false.
     */
    void setNoLS2Service(bool value);

    /*!
     * \brief Get no-ls2-service.
     */
    bool getNoLS2Service() const;

private:
    int m_instanceId = 0;
    const QString m_appId = "com.webos.service.ime";
    const QString m_settingsSuffix = "settings";
    const QString m_separator = "_";
    bool m_noLS2Service = false;
};

#endif // MIMGLOBALSETTINGS_H
