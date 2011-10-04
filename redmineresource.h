/*
 * © 2011 Romain Lalaut <romain.lalaut@laposte.net>
 * License: GPL-2+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef REDMINERESOURCE_H
#define REDMINERESOURCE_H

#include <akonadi/resourcebase.h>
#include <kio/scheduler.h>
#include <kurl.h>
#include <kdatetime.h>
#include <kio/jobclasses.h>
#include <QtXml>
#include <kcalcore/todo.h>
#include <kcalcore/incidence.h>
#include <akonadi/resourcebase.h>
#include "ui_settingsform.h"

class redmineResource : public Akonadi::ResourceBase,
                           public Akonadi::AgentBase::Observer
{
  Q_OBJECT

  public:
    redmineResource( const QString &id );
    ~redmineResource();

  public Q_SLOTS:
    virtual void configure( WId windowId );

  protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems( const Akonadi::Collection &col );
    bool retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts );
    void retrieveUser();
    void userDataReceived(KIO::Job *,const QByteArray &);
    void collectionsDataReceived(KIO::Job *,const QByteArray &);
    void itemsDataReceived(KIO::Job *,const QByteArray &);
    void userDataResult(KJob *);
    void collectionsDataResult(KJob *);
    void itemsDataResult(KJob *);

  protected:
    virtual void aboutToQuit();

    virtual void itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection );
    virtual void itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts );
    virtual void itemRemoved( const Akonadi::Item &item );
    KUrl url(const QString &path);
    
    bool hasEl(const QDomElement &el, const QString &name);
    QString readEl(const QDomElement &el, const QString &name);
    QDate readElDate(const QDomElement &el, const QString &name);
    
    QStringList mimeTypes;
    QString userId;
    QByteArray userBuffer;
    QByteArray collectionsBuffer;
    QHash<KJob*,QByteArray> itemsBuffers;
    
  signals:
    void userChanged();
};

#endif
