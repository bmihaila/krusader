/***************************************************************************
                         kgarchives.h  -  description
                             -------------------
    copyright            : (C) 2004 by Csaba Karai
    e-mail               : krusader@users.sourceforge.net
    web site             : http://krusader.sourceforge.net
 ---------------------------------------------------------------------------
  Description
 ***************************************************************************

  A

     db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
     88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
     88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
     88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
     88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
     YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                                     H e a d e r    F i l e

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __KGARCHIVES_H__
#define __KGARCHIVES_H__

#include "konfiguratorpage.h"
 
class KgArchives : public KonfiguratorPage
{
  Q_OBJECT

public:
  KgArchives( bool first, QWidget* parent=0,  const char* name=0 );
  virtual bool apply();
  virtual void setDefaults();

public slots:
  void slotAutoConfigure();
  
protected:
  QPtrList<KonfiguratorCheckBox>  packerCbs;
  QPtrList<KonfiguratorCheckBox>  tunerCbs;

  void disableNonExistingPackers();
};

typedef enum
{
  PK_Tar             = 0,
  PK_GZip            = 1,
  PK_BZip2           = 2,
  PK_UnZip           = 3,
  PK_UnRar           = 4,
  PK_UnArj           = 5,
  PK_RPM             = 6,
  PK_UnAce           = 7
} PACKERS;

#endif /* __KGARCHIVES_H__ */
