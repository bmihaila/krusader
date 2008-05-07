#include "krbookmarkbutton.h"
#include "krbookmarkhandler.h"
#include "../krusader.h"
#include <qpixmap.h>
#include <kiconloader.h>
#include <kaction.h>
#include <klocale.h>
#include <kmenu.h>
#include <kdebug.h>

KrBookmarkButton::KrBookmarkButton(QWidget *parent): QToolButton(parent) {
	QPixmap icon = krLoader->loadIcon("bookmark", KIconLoader::Toolbar, 16);
	setFixedSize(icon.width() + 4, icon.height() + 4);
	setIcon( QIcon( icon ) );
	setText(i18n("BookMan II"));
	setToolTip(i18n("BookMan II"));
	setPopupMode( QToolButton::InstantPopup );
	setAcceptDrops(false);

	acmBookmarks = new KActionMenu(KIcon("bookmark"), i18n("Bookmarks"), 0);
	acmBookmarks->setDelayed(false);
	acmBookmarks->menu()->setKeyboardShortcutsEnabled(true);
	acmBookmarks->menu()->setKeyboardShortcutsExecute(true);

	setMenu(acmBookmarks->menu());
	connect(acmBookmarks->menu(), SIGNAL( aboutToShow() ), this, SLOT(populate()));
	connect(acmBookmarks->menu(), SIGNAL( aboutToShow() ), this, SIGNAL( aboutToShow() ) );
	populate();
}

void KrBookmarkButton::populate() {
	krBookMan->populate(static_cast<KMenu*>(menu()));
}

void KrBookmarkButton::showMenu() {
	populate();
	menu()->exec(mapToGlobal(QPoint(0, height())));
}

#include "krbookmarkbutton.moc"
