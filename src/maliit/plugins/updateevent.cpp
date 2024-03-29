/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Copyright (C) 2014-2021 LG Electronics, Inc.
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#include <maliit/plugins/updateevent.h>
#include <maliit/plugins/updateevent_p.h>
#include <maliit/plugins/extensionevent_p.h>
#include <maliit/namespace.h>
#include <maliit/namespaceinternal.h>

MImUpdateEventPrivate::MImUpdateEventPrivate()
    : update()
    , changedProperties()
    , lastHints(Qt::ImhNone)
{}

MImUpdateEventPrivate::MImUpdateEventPrivate(const QMap<QString, QVariant> &newUpdate,
                                             const QStringList &newChangedProperties,
                                             const Qt::InputMethodHints &newLastHints)
    : update(newUpdate)
    , changedProperties(newChangedProperties)
    , lastHints(newLastHints)
{}

bool MImUpdateEventPrivate::isFlagSet(Qt::InputMethodHint hint,
                                      bool *changed) const
{
    bool result = false;

    if (update.contains(Maliit::Internal::inputMethodHints)) {
        long long int inputMethodHint = update.value(Maliit::Internal::inputMethodHints).toLongLong();
        if (inputMethodHint < INT_MIN || inputMethodHint > INT_MAX) {
            qWarning() << "This conversion from long long int to int may result in data lost, because the value exceeds INT range. inputMethodHint: " << inputMethodHint;
            return false;
        }
        const Qt::InputMethodHints hints(static_cast<int>(inputMethodHint));
        result = (hints & hint);
    }

    if (changed) {
        *changed = (result != ((lastHints & hint) != 0));
    }

    return result;
}

QVariant MImUpdateEventPrivate::extractProperty(const QString &key,
                                                bool *changed) const
{
    if (changed) {
        *changed = changedProperties.contains(key);
    }

    return update.value(key);
}

MImUpdateEvent::MImUpdateEvent(const QMap<QString, QVariant> &update,
                               const QStringList &changedProperties)
    : MImExtensionEvent(new MImUpdateEventPrivate(update, changedProperties, Qt::InputMethodHints()),
                        MImExtensionEvent::Update)
{}

MImUpdateEvent::MImUpdateEvent(const QMap<QString, QVariant> &update,
                               const QStringList &changedProperties,
                               const Qt::InputMethodHints &lastHints)
    : MImExtensionEvent(new MImUpdateEventPrivate(update, changedProperties, lastHints),
                        MImExtensionEvent::Update)
{}

QVariant MImUpdateEvent::value(const QString &key) const
{
    Q_D(const MImUpdateEvent);
    return d->update.value(key);
}

QStringList MImUpdateEvent::propertiesChanged() const
{
    Q_D(const MImUpdateEvent);
    return d->changedProperties;
}

Qt::InputMethodHints MImUpdateEvent::hints(bool *changed) const
{
    Q_D(const MImUpdateEvent);
    long long int inputMethodHint = d->extractProperty(Maliit::Internal::inputMethodHints, changed).toLongLong();
    if (inputMethodHint > INT_MAX || inputMethodHint < INT_MIN) {
        qWarning() << "This conversion from long long int to int may result in data lost, because the value exceeds INT range. inputMethodHint: " << inputMethodHint;
        return Qt::ImhNone;
    }
    return Qt::InputMethodHints(static_cast<int>(inputMethodHint));
}

bool MImUpdateEvent::westernNumericInputEnforced(bool *changed) const
{
    Q_D(const MImUpdateEvent);
    return d->extractProperty(Maliit::InputMethodQuery::westernNumericInputEnforced, changed).toBool();
}

bool MImUpdateEvent::preferNumbers(bool *changed) const
{
    Q_D(const MImUpdateEvent);
    return d->isFlagSet(Qt::ImhPreferNumbers, changed);
}

bool MImUpdateEvent::translucentInputMethod(bool *changed) const
{
    Q_D(const MImUpdateEvent);
    return d->extractProperty(Maliit::InputMethodQuery::translucentInputMethod, changed).toBool();
}
