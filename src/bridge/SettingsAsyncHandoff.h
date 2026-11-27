// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QSettings>
#include <QThread>

namespace decodium {
// Called from the settings object's owning thread before dispatch. The object
// is parentless and no event loop may re-enter while it is being staged.
inline void detachSettingsForWorker(QSettings* settings)
{
    Q_ASSERT(settings && !settings->parent());
    Q_ASSERT(settings->thread() == QThread::currentThread());
    settings->moveToThread(nullptr);
}
// Called from the worker; ownership transfers and ends here.
inline void flushDetachedSettings(QSettings* settings)
{
    Q_ASSERT(settings && !settings->thread());
    settings->moveToThread(QThread::currentThread());
    settings->sync();
    delete settings;
}
} // namespace decodium
