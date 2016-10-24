/***************************************************************************
                       default_vfs.cpp
                   -------------------
    copyright            : (C) 2000 by Rafi Yanai
    e-mail               : krusader@users.sourceforge.net
    web site             : http://krusader.sourceforge.net
 ---------------------------------------------------------------------------

 ***************************************************************************

  A

     db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
     88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
     88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
     88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
     88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
     YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                                     S o u r c e    F i l e

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "default_vfs.h"

// QtCore
#include <QEventLoop>
#include <QByteArray>
#include <QDir>

#include <KConfigCore/KSharedConfig>
#include <KCoreAddons/KUrlMimeData>
#include <KI18n/KLocalizedString>
#include <KIO/DeleteJob>
#include <KIO/DropJob>
#include <KIO/ListJob>
#include <KIO/JobUiDelegate>
#include <KIOCore/KFileItem>
#include <KIOCore/KProtocolManager>

#include "../defaults.h"
#include "../krglobal.h"
#include "../krservices.h"
#include "../MountMan/kmountman.h"

default_vfs::default_vfs(): vfs(), _watcher()
{
    _type = VFS_DEFAULT;
}

void default_vfs::copyFiles(const QList<QUrl> &urls, const QUrl &destination,
                            KIO::CopyJob::CopyMode mode, bool showProgressInfo)
{
    KIO::JobFlags flags = showProgressInfo ? KIO::DefaultFlags : KIO::HideProgressInfo;
    KIO::Job *job;
    switch (mode) {
    case KIO::CopyJob::Move:
        job = KIO::move(urls, destination, flags);
        break;
    case KIO::CopyJob::Link:
        job = KIO::link(urls, destination, flags);
        break;
    default:
        job = KIO::copy(urls, destination, flags);
    }

    connectJob(job, destination);
    if (mode == KIO::CopyJob::Move) { // notify source about removed files
        connectSourceVFS(job, urls);
    }
}

void default_vfs::dropFiles(const QUrl &destination, QDropEvent *event)
{
    KIO::DropJob *job = KIO::drop(event, destination);
    // NOTE: DropJob does not provide information about the actual user choice
    // (move/copy/link/abort). We have to assume the worst (move)
    connectJob(job, destination);
    connectSourceVFS(job, KUrlMimeData::urlsFromMimeData(event->mimeData()));
}

void default_vfs::connectSourceVFS(KJob *job, const QList<QUrl> urls)
{
    if (!urls.isEmpty()) {
        // NOTE: we assume that all files were in the same directory and only emit one signal for
        // the directory of the first file URL
        // their current directory was deleted
        const QUrl url = urls.first().adjusted(QUrl::RemoveFilename);
        connect(job, &KIO::Job::result, [=]() { emit filesystemChanged(url); });
    }
}

void default_vfs::addFiles(const QList<QUrl> &fileUrls, KIO::CopyJob::CopyMode mode, QString dir)
{
    QUrl destination(_currentDirectory);
    if (!dir.isEmpty()) {
        destination.setPath(QDir::cleanPath(destination.path() + '/' + dir));
        const QString scheme = destination.scheme();
        if (scheme == "tar" || scheme == "zip" || scheme == "krarc") {
            if (QDir(cleanUrl(destination).path()).exists())
                // if we get out from the archive change the protocol
                destination.setScheme("file");
        }
    }

    copyFiles(fileUrls, destination, mode);
}

void default_vfs::deleteFiles(const QStringList &fileNames, bool forceDeletion)
{
    // get absolute URLs for file names
    const QList<QUrl> fileUrls = getUrls(fileNames);

    // delete or move to trash?
    KIO::Job *job;
    const KConfigGroup group(krConfig, "General");
    if (!forceDeletion && isLocal() && group.readEntry("Move To Trash", _MoveToTrash)) {
        job = KIO::trash(fileUrls);
    } else {
        job = KIO::del(fileUrls);
    }
    connectJob(job, currentDirectory());
}

void default_vfs::mkDir(const QString &name)
{
    KIO::SimpleJob* job = KIO::mkdir(getUrl(name));
    connectJob(job, currentDirectory());
}

void default_vfs::rename(const QString &oldName, const QString &newName)
{
    const QUrl oldUrl = getUrl(oldName);
    const QUrl newUrl = getUrl(newName);
    KIO::Job *job = KIO::moveAs(oldUrl, newUrl, KIO::HideProgressInfo);
    connectJob(job, currentDirectory());
}

void default_vfs::connectJob(KJob *job, const QUrl &destination)
{
    // (additional) direct refresh if on local fs because watcher is too slow
    connect(job, &KIO::Job::result, this, [=](KJob* job) { slotJobResult(job, isLocal()); });
    connect(job, &KIO::Job::result, [=]() { emit filesystemChanged(destination); });
}

QUrl default_vfs::getUrl(const QString& name)
{
    // NOTE: on non-local fs file URL does not have to be path + name!
    vfile *vf = getVfile(name);
    if (vf)
        return vf->vfile_getUrl();

    QUrl absoluteUrl(_currentDirectory);
    absoluteUrl.setPath(absoluteUrl.path() + "/" + name);
    return absoluteUrl;
}

// ==== protected ====

bool default_vfs::refreshInternal(const QUrl &directory, bool showHidden)
{
    if (!KProtocolManager::supportsListing(directory)) {
        emit error(i18n("Protocol not supported by Krusader:\n%1", directory.url()));
        return false;
    }

    delete _watcher; // stop watching the old dir

    if (directory.isLocalFile()) {
        // we could read local directories with KIO but using Qt is a lot faster!
        return refreshLocal(directory);
    }

    _currentDirectory = cleanUrl(directory);

    // start the listing job
    KIO::ListJob *job = KIO::listDir(_currentDirectory, KIO::HideProgressInfo, showHidden);
    connect(job, SIGNAL(entries(KIO::Job *, const KIO::UDSEntryList &)), this,
            SLOT(slotAddFiles(KIO::Job *, const KIO::UDSEntryList &)));
    connect(job, &KIO::ListJob::redirection, this, &default_vfs::slotRedirection);
    connect(job, &KIO::ListJob::permanentRedirection, this, &default_vfs::slotRedirection);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(slotListResult(KJob*)));

    emit refreshJobStarted(job);

    _listError = false;
    // ugly: we have to wait here until the list job is finished
    QEventLoop eventLoop;
    connect(job, SIGNAL(finished(KJob*)), &eventLoop, SLOT(quit()));
    eventLoop.exec(); // blocking until quit()

    return !_listError;
}

bool default_vfs::ignoreRefresh()
{
    return !_watcher.isNull();
}

// ==== protected slots ====

void default_vfs::slotListResult(KJob *job)
{
    if (job && job->error()) {
        // we failed to refresh
        _listError = true;
        emit error(job->errorString()); // display error message (in panel)
    }
}

void default_vfs::slotAddFiles(KIO::Job *, const KIO::UDSEntryList& entries)
{
    for (const KIO::UDSEntry entry : entries) {
        vfile *vfile = vfs::createVFileFromKIO(entry, _currentDirectory);
        if (vfile) {
            addVfile(vfile);
        }
    }
}

void default_vfs::slotRedirection(KIO::Job *job, const QUrl &url)
{
   krOut << "default_vfs; redirection to " << url;

   if (url.scheme() != _currentDirectory.scheme()) {
       // abort and start over again, some protocols (iso) do this on transition to local fs
       job->kill();
       _isRefreshing = false;
       refresh(url);
       return;
   }

    _currentDirectory = cleanUrl(url);
}

void default_vfs::slotWatcherDirty(const QString& path)
{
    if (path == realPath()) {
        // this happens
        //   1. if a directory was created/deleted/renamed inside this directory. No deleted
        //   2. during and after a file operation (create/delete/rename/touch) inside this directory
        // KDirWatcher doesn't reveal the name of changed directories and we have to refresh.
        // (QFileSystemWatcher in Qt5.7 can't help here either)
        refresh();
        return;
    }

    const QString name = QUrl::fromLocalFile(path).fileName();

    vfile *vf = getVfile(name);
    if (!vf) {
        krOut << "dirty watcher file not found (unexpected): " << path;
        return;
    }

    // we have an updated file..
    vfile *newVf = createLocalVFile(name);
    *vf = *newVf;
    delete newVf;
    emit updatedVfile(vf);
}

void default_vfs::slotWatcherDeleted(const QString& path)
{
    if (path != realPath()) {
        // ignore deletion of files here, a 'dirty' signal will be send anyway
        return;
    }

    // the current directory was deleted, try a refresh, which will fail. An error message will
    // be emitted and the empty (non-existing) directory remains.
    refresh();
}

bool default_vfs::refreshLocal(const QUrl &directory) {
    const QString path = KrServices::urlToLocalPath(directory);

#ifdef Q_WS_WIN
    if (!path.contains("/")) { // change C: to C:/
        path = path + QString("/");
    }
#endif

    // check if the new directory exists
    if (!QDir(path).exists()) {
        emit error(i18n("The folder %1 does not exist.", path));
        return false;
    }

    KConfigGroup group(krConfig, "Advanced");
    if (group.readEntry("AutoMount", _AutoMount) && !_mountMan.isNull())
        _mountMan->autoMount(path);

    // set the current directory...
    _currentDirectory = directory;
    _currentDirectory.setPath(QDir::cleanPath(_currentDirectory.path()));

    // Note: we are using low-level Qt functions here.
    // It's around twice as fast as using the QDir class.

    QT_DIR* dir = QT_OPENDIR(path.toLocal8Bit());
    if (!dir) {
        emit error(i18n("Cannot open the folder %1.", path));
        return false;
    }

    // change directory to the new directory
    const QString savedDir = QDir::currentPath();
    if (!QDir::setCurrent(path)) {
        emit error(i18nc("%1=folder path", "Access to %1 denied", path));
        QT_CLOSEDIR(dir);
        return false;
    }

    QT_DIRENT* dirEnt;
    QString name;
    const bool showHidden = showHiddenFiles();
    while ((dirEnt = QT_READDIR(dir)) != NULL) {
        name = QString::fromLocal8Bit(dirEnt->d_name);

        // show hidden files?
        if (!showHidden && name.left(1) == ".") continue ;
        // we don't need the "." and ".." entries
        if (name == "." || name == "..") continue;

        vfile* temp = createLocalVFile(name);
        addVfile(temp);
    }
    // clean up
    QT_CLOSEDIR(dir);
    QDir::setCurrent(savedDir);

    // start watching the new dir for file changes
    _watcher = new KDirWatch(this);
    // if the current dir is a link path the watcher needs to watch the real path - and signal
    // parameters will be the real path
    _watcher->addDir(realPath(), KDirWatch::WatchFiles);
    connect(_watcher, SIGNAL(dirty(const QString&)), this, SLOT(slotWatcherDirty(const QString&)));
    // NOTE: not connecting 'created' signal. A 'dirty' is send after that anyway
    //connect(_watcher, SIGNAL(created(const QString&)), this, SLOT(slotWatcherCreated(const QString&)));
    connect(_watcher, SIGNAL(deleted(const QString&)), this, SLOT(slotWatcherDeleted(const QString&)));
    _watcher->startScan(false);

    return true;
}

vfile *default_vfs::createLocalVFile(const QString &name)
{
    return vfs::createLocalVFile(name, _currentDirectory.path());
}

QString default_vfs::default_vfs::realPath()
{
    return QDir(_currentDirectory.toLocalFile()).canonicalPath();
}