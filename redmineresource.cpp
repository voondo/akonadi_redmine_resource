#include "redmineresource.h"

#include "settings.h"
#include "settingsadaptor.h" 
#include <kconfigdialog.h>

#include <QtDBus/QDBusConnection>
#include <QDebug>

using namespace Akonadi;

redmineResource::redmineResource( const QString &id )
  : ResourceBase( id )
{
  new SettingsAdaptor( Settings::self() );
  QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                            Settings::self(), QDBusConnection::ExportAdaptors );
  
  connect (this, SIGNAL( userChanged() ), this, SLOT(retrieveUser()));

  mimeTypes << QLatin1String("text/calendar");
  if(!Settings::self()->endpoint().isEmpty()){
    emit userChanged();
  }
}

redmineResource::~redmineResource()
{
}

KUrl redmineResource::url( const QString &path )
{
  QString url = Settings::self()->endpoint() + path;
  kWarning() << url;
  return KUrl(url);
}

void redmineResource::retrieveUser()
{
  kWarning() << "retrieve user "; 
  
  userBuffer.clear();
  
  KIO::TransferJob *job = KIO::get(url("/users/current.xml?include=memberships"));
  connect (job, SIGNAL( data(KIO::Job *, const QByteArray & ) ), this, SLOT(userDataReceived(KIO::Job *,const QByteArray &)));
  connect (job, SIGNAL( result(KJob*) ) , this, SLOT( userDataResult(KJob *) ));

}

void redmineResource::userDataReceived(KIO::Job* job, const QByteArray&  data)
{
//   kWarning() << "user data received : " << data.length() << " bytes";
  
  if(data.isEmpty()){
//     kWarning() << "empty data : skip";
    return;
  }
  
  userBuffer.append(data);
}

void redmineResource::userDataResult(KJob* job)
{
  kWarning() << "user data result";
  
  if(job->error()){
    kWarning() << job->errorString();
    return;
  }
  
  if(userBuffer.isEmpty()){
    kWarning() << "empty buffer : skip";
    return;
  }

  QDomDocument doc;
  doc.setContent(userBuffer);
  QDomElement docEl = doc.documentElement(); 
  
  userId = readEl(docEl, "id");
  kWarning() << "user ID : " << userId;
 
  synchronize(); 
}

void redmineResource::retrieveCollections()
{
  kWarning() << "retrieve collections";
  
  collectionsBuffer = "";
  
  KIO::TransferJob *job = KIO::get(url("/projects.xml"));
  connect (job, SIGNAL(  data(KIO::Job *, const QByteArray & ) ), this, SLOT(collectionsDataReceived(KIO::Job *,const QByteArray &)));
  connect (job, SIGNAL( result( KJob * ) ), this, SLOT( collectionsDataResult(KJob *) ));
}

void redmineResource::collectionsDataReceived(KIO::Job* job, const QByteArray& data )
{
//   kWarning() << "collections data received : " << QString::number(data.length());
  
  if(data.isEmpty()){
//     kWarning() << "empty data : skip";
    return;
  }
  
  collectionsBuffer.append(data);
}

void redmineResource::collectionsDataResult(KJob* job)
{
  kWarning() << "collections received";
  
  if ( job->error() ){
      kWarning() << job->errorString();
      return;
  }
  
  if ( collectionsBuffer.isEmpty() ){
    kWarning() << "empty buffer : skip";
    return;
  }
  
  QDomDocument doc;
  doc.setContent(collectionsBuffer);
  QDomElement docEl = doc.documentElement(); 
  QDomNodeList nodes  = docEl.elementsByTagName("project"); 
  Collection::List collectionList;
  
  uint j = nodes.length();
  kWarning() << "projects found :" << j;
  for(uint i=0; i<j; ++i){
    
    QDomElement el = nodes.item(i).toElement();
    
    Collection collection;
    collection.setParent( Collection::root() );
    collection.setRemoteId( readEl(el, "id") );
    collection.setParentCollection( Collection::root() );
    collection.setName( i18n("Project") + " - " + readEl(el, "name") );
    collection.setContentMimeTypes(mimeTypes);
    kWarning() << "collection :" << collection.name();
    collectionList << collection;
  }
  
  collectionsRetrieved( collectionList );
}

void redmineResource::retrieveItems( const Akonadi::Collection &collection )
{
  kWarning() << "retrieve items for collection " << collection.name(); 
  
  KIO::TransferJob *job = KIO::get(url("/issues.xml?project_id="+collection.remoteId()
    +"&assigned_to="+QString(userId)
    +"&offset=0"
    +"&limit="+QString::number(Settings::self()->limit())), KIO::Reload);
  
  itemsBuffers[job] = "";
  kWarning() << "new job " << job;
  
  connect (job, SIGNAL(  data(KIO::Job *, const QByteArray & )), this, SLOT(itemsDataReceived(KIO::Job *,const QByteArray &)));
  connect (job, SIGNAL( result( KJob * ) ), this, SLOT( itemsDataResult(KJob *) ));
}

void redmineResource::itemsDataReceived(KIO::Job* job, const QByteArray& data )
{
//   kWarning() << "items data received. job : " << job;
  
  if( data.isEmpty() ){
//     kWarning() << "empty data : skip";
    return;
  }
  
  itemsBuffers[job].append(data);
//   kWarning() << "writting " << data.length() << " bytes into the buffer (total : " << itemsBuffers[job].length() << " bytes)";
}

void redmineResource::itemsDataResult(KJob* job)
{ 
  kWarning() << "items result. job : " << job;
  
  Item::List items;
  
  if ( job->error() ){
      kError() << job->errorString();
  } else {
  
    kWarning() << itemsBuffers[job].length() << "bytes";
      
    QByteArray data = itemsBuffers[job];
    
    if( data.isEmpty() ){
      kWarning() << "empty buffer : skip";
    } else {
  
      QDomDocument doc;
      doc.setContent(data);
      QDomElement docEl = doc.documentElement(); 
      QDomNodeList nodes  = docEl.elementsByTagName("issue"); 
      
      uint j = nodes.length();
      kWarning() << "issues found :" << j;
      
      if(j==0){
        kWarning() << data;
      }
      
      for(uint i=0; i<j; ++i){
        
        QDomElement el = nodes.item(i).toElement();
        
        KCalCore::Todo::Ptr todo(new KCalCore::Todo);
        todo->setSummary(readEl(el, "subject"));
        todo->setDescription(readEl(el, "description"));
        if(hasEl(el, "due_date")){
          todo->setDtDue(KDateTime(readElDate(el, "due_date")));
        }
        if(hasEl(el, "start_date")){
          todo->setDtStart(KDateTime(readElDate(el, "start_date")));
        }
        if(hasEl(el, "done_ratio")){
          todo->setPercentComplete((int) readEl(el, "done_ratio").toFloat()*100);
        }
        
        Item item("application/x-vnd.akonadi.calendar.todo");
        item.setRemoteId(readEl(el, "id"));
        item.setPayload<KCalCore::Todo::Ptr>(todo);
        
        items << item;
      }
    }
  }
  itemsBuffers.remove(job);
  itemsRetrieved( items );
}

bool redmineResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  kWarning() << "retrieve item : " << item.url() << ". Collection : " << parts; 

  // TODO: this method is called when Akonadi wants more data for a given item.
  // You can only provide the parts that have been requested but you are allowed
  // to provide all in one go

  return true;
}

void redmineResource::aboutToQuit()
{
  // TODO: any cleanup you need to do while there is still an active
  // event loop. The resource will terminate after this method returns
}

void redmineResource::configure( WId windowId )
{
  Q_UNUSED( windowId );

  if ( KConfigDialog::showDialog( "settings" ) )
    return; 
  
  KConfigDialog* dialog = new KConfigDialog(0,"settings",
                                          Settings::self()); 
  QWidget *widget = new QWidget;
  Ui::SettingsForm ui;
  ui.setupUi(widget); 
 
  dialog->addPage( widget, i18n("Connection"), "connection" ); 
  
  dialog->show();
  
  connect (dialog, SIGNAL(settingsChanged(QString)), this, SIGNAL(userChanged()));
 
  synchronize(); 
}

void redmineResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  kWarning() << "item added : " << item.url() << ". Collection : " << collection.name(); 

  // TODO: this method is called when somebody else, e.g. a client application,
  // has created an item in a collection managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
  changeCommitted( item );
}

void redmineResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  kWarning() << "item changed : " << item.url() << ". Parts : " << parts; 

  // TODO: this method is called when somebody else, e.g. a client application,
  // has changed an item managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
  changeCommitted( item );
}

void redmineResource::itemRemoved( const Akonadi::Item &item )
{
  kWarning() << "item removed : " << item.url(); 

  // TODO: this method is called when somebody else, e.g. a client application,
  // has deleted an item managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
  changeCommitted( item );
}

bool redmineResource::hasEl(const QDomElement& el, const QString &name )
{
  return readEl(el, name) != "";
}

QString redmineResource::readEl(const QDomElement& el, const QString &name )
{
  QDomNodeList nodes = el.elementsByTagName(name);
  if(!nodes.isEmpty()){
    QString value = nodes.item(0).firstChild().toText().data();
    return value;
  } else {
    kWarning() << "cannot read element : " << name;
    return "";
  }
}

QDate redmineResource::readElDate(const QDomElement& el, const QString &name )
{
  QStringList str = readEl(el, name).split('-');
  return QDate(str[0].toUInt(), str[1].toUInt(), str[2].toUInt());
}

AKONADI_RESOURCE_MAIN( redmineResource )

#include "redmineresource.moc"
