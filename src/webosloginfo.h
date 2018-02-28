/* @@@LICENSE
*
*      Copyright (c) 2013 LG Electronics, Inc.
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

#ifndef WEBOSLOGINFO_H
#define WEBOSLOGINFO_H

#ifdef HAS_PMLOGLIB
#include <PmLogLib.h>
#else
#include <QDebug>
#endif

#ifdef HAS_PMLOGLIB
// CAUTION: due to restriction of PmLog, key MUST NOT be a variable.
#define webOSLogInfo(msgid, key, message) \
    do { \
        PmLogContext context; \
        PmLogGetContext("IME", &context); \
        QString messageJsonEscape = QString(message) \
            .replace(' ', "") \
            .replace('\"', "\\\""); \
        PmLogInfo(context, msgid, 1, \
                PMLOGKS(key, messageJsonEscape.toStdString().c_str()), \
                ""); \
    } while (0)
#else
#define webOSLogInfo(msgid, key, message) \
    qDebug() << (msgid) << (key) << (message);
#endif

#endif // WEBOSLOGINFO_H
