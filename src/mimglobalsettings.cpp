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

#include "mimglobalsettings.h"
#include <QString>

int MImGlobalSettings::getInstanceId() const {
    return m_instanceId;
}

void MImGlobalSettings::setInstanceId(int instanceId) {
    m_instanceId = instanceId;
}

QString MImGlobalSettings::getAppId() const {
    return m_appId;
}

QString MImGlobalSettings::getServiceName() const {
    if (getNoLS2Service())
        return getAppId() + m_separator + QString::number(getInstanceId());
    return getAppId();
}

QString MImGlobalSettings::getAppIdForSettingsService() const {
    return getServiceName() + "." + m_settingsSuffix;
}

bool MImGlobalSettings::getNoLS2Service() const {
    return m_noLS2Service;
}

void MImGlobalSettings::setNoLS2Service(bool value) {
    m_noLS2Service = value;
}
