/*****************************************************************************
 * Copyright (C) 2001 Shie Erlich <krusader@users.sourceforge.net>           *
 * Copyright (C) 2001 Rafi Yanai <krusader@users.sourceforge.net>            *
 * Copyright (C) 2004-2018 Krusader Krew [https://krusader.org]              *
 *                                                                           *
 * This file is part of Krusader [https://krusader.org].                     *
 *                                                                           *
 * Krusader is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * Krusader is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with Krusader.  If not, see [http://www.gnu.org/licenses/].         *
 *****************************************************************************/

#include "krsearchmod.h"

// QtCore
#include <QDir>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegExp>
// QtWidgets
#include <QApplication>
#include <qplatformdefs.h>

#include <KIO/Global>

#include "../Archive/krarchandler.h"
#include "../FileSystem/defaultfilesystem.h"
#include "../FileSystem/fileitem.h"
#include "../FileSystem/krpermhandler.h"
#include "../FileSystem/krquery.h"
#include "../FileSystem/virtualfilesystem.h"

#define EVENT_PROCESS_DELAY 250 // milliseconds

static const QStringList TAR_TYPES = QStringList() << "tbz" << "tgz" << "tarz" << "tar" << "tlz";

extern KRarcHandler arcHandler;

KRSearchMod::KRSearchMod(const KRQuery *query)
    : m_defaultFileSystem(nullptr), m_virtualFileSystem(nullptr), m_stopSearch(false)
{
    m_query = new KRQuery(*query);
    connect(m_query, &KRQuery::status, this, &KRSearchMod::searching);
    connect(m_query, &KRQuery::processEvents, this, &KRSearchMod::slotProcessEvents);
}

KRSearchMod::~KRSearchMod()
{
    delete m_query;
    if (m_defaultFileSystem)
        delete m_defaultFileSystem;
    if (m_virtualFileSystem)
        delete m_virtualFileSystem;
}

void KRSearchMod::start()
{
    m_unScannedUrls.clear();
    m_scannedUrls.clear();
    m_timer.start();

    const QList<QUrl> whereToSearch = m_query->searchInDirs();

    // search every dir that needs to be searched
    for (int i = 0; i < whereToSearch.count(); ++i)
        scanUrl(whereToSearch[i]);

    emit finished();
}

void KRSearchMod::stop() { m_stopSearch = true; }

void KRSearchMod::scanUrl(const QUrl &url)
{
    if (m_stopSearch)
        return;

    m_unScannedUrls.push(url);
    while (!m_unScannedUrls.isEmpty()) {
        const QUrl url = m_unScannedUrls.pop();

        if (m_stopSearch)
            return;

        if (m_query->isExcluded(url)) {
            if (!m_query->searchInDirs().contains(url))
                continue;
        }

        if (m_scannedUrls.contains(url))
            // avoid endless loop
            continue;

        m_scannedUrls.push(url);

        emit searching(url.toDisplayString(QUrl::PreferLocalFile));

        scanDirectory(url);
    }
}

void KRSearchMod::scanDirectory(const QUrl &url)
{
    FileSystem *fileSystem = getFileSystem(url);

    // create file items
    const bool refreshed = fileSystem->scanDir(url);
    if (!refreshed) {
        emit error(url);
        return;
    }

    for (FileItem *fileItem : fileSystem->fileItems()) {
        const QUrl fileUrl = fileItem->getUrl();

        if (m_query->isRecursive() &&
            (fileItem->isDir() || (fileItem->isSymLink() && m_query->followLinks()))) {
            // query search in subdirectory
            m_unScannedUrls.push(fileUrl);
        }

        if (m_query->searchInArchives() && fileUrl.isLocalFile() &&
            KRarcHandler::arcSupported(fileItem->getMime())) {
            // query search in archive; NOTE: only supported for local files
            QUrl archiveURL = fileUrl;
            bool encrypted;
            const QString type = arcHandler.getType(encrypted, fileUrl.path(), fileItem->getMime());

            if (!encrypted) {
                archiveURL.setScheme(TAR_TYPES.contains(type) ? "tar" : "krarc");
                m_unScannedUrls.push(archiveURL);
            }
        }

        if (m_query->match(fileItem)) {
            // found!
            emit found(*fileItem, m_query->foundText()); // emitting copy of file item
        }

        if (m_timer.elapsed() >= EVENT_PROCESS_DELAY) {
            qApp->processEvents();
            m_timer.start();
            if (m_stopSearch)
                return;
        }
    }
}

FileSystem *KRSearchMod::getFileSystem(const QUrl &url)
{
    FileSystem *fileSystem;
    if (url.scheme() == QStringLiteral("virt")) {
        if (!m_virtualFileSystem)
            m_virtualFileSystem = new VirtualFileSystem();
        fileSystem = m_virtualFileSystem;
    } else {
        if (!m_defaultFileSystem)
            m_defaultFileSystem = new DefaultFileSystem();
        fileSystem = m_defaultFileSystem;
    }
    return fileSystem;
}

void KRSearchMod::slotProcessEvents(bool &stopped)
{
    qApp->processEvents();
    stopped = m_stopSearch;
}
