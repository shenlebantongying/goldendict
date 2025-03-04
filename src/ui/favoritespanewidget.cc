/* This file is (c) 2017 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <QApplication>
#include <QDockWidget>
#include <QKeyEvent>
#include <QClipboard>
#include <QDomDocument>
#include <QMessageBox>
#include <QtAlgorithms>
#include <QMap>
#include <QSaveFile>
#include <QStringBuilder>
#include <QDebug>

#include <algorithm>
#include <functional>

#include "favoritespanewidget.hh"
#include "globalbroadcaster.hh"

#include <QFile>

/************************************************** FavoritesPaneWidget *********************************************/

void FavoritesPaneWidget::setUp( Config::Class * cfg, std::initializer_list< QAction * > actionsFromMainWindow )
{
  m_cfg                       = cfg;
  m_favoritesTree             = findChild< QTreeView * >( "favoritesTree" );
  QDockWidget * favoritesPane = qobject_cast< QDockWidget * >( parentWidget() );
  m_favoritesTree->setHeaderHidden( true );

  // Active the current folder for favoriate controlling
  m_activeFolderForFav = new QAction( this );
  m_activeFolderForFav->setToolTip( tr( "Make this folder the target of adding/removing words actions." ) );
  m_activeFolderForFav->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( m_activeFolderForFav );
  connect( m_activeFolderForFav, &QAction::triggered, this, &FavoritesPaneWidget::folderActivation );


  // Delete selected items action
  m_deleteSelectedAction = new QAction( this );
  m_deleteSelectedAction->setText( tr( "&Delete Selected" ) );
  m_deleteSelectedAction->setShortcut( QKeySequence( QKeySequence::Delete ) );
  m_deleteSelectedAction->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( m_deleteSelectedAction );
  connect( m_deleteSelectedAction, &QAction::triggered, this, &FavoritesPaneWidget::deleteSelectedItems );

  // Copy selected items to clipboard
  m_copySelectedToClipboard = new QAction( this );
  m_copySelectedToClipboard->setText( tr( "Copy Selected" ) );
  m_copySelectedToClipboard->setShortcut( QKeySequence( QKeySequence::Copy ) );
  m_copySelectedToClipboard->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( m_copySelectedToClipboard );
  connect( m_copySelectedToClipboard, &QAction::triggered, this, &FavoritesPaneWidget::copySelectedItems );

  // Add folder to tree view
  m_addFolder = new QAction( this );
  m_addFolder->setText( tr( "Add folder" ) );
  addAction( m_addFolder );
  connect( m_addFolder, &QAction::triggered, this, &FavoritesPaneWidget::addFolder );

  m_clearAll = new QAction( this );
  m_clearAll->setText( tr( "Clear All" ) );
  addAction( m_clearAll );
  connect( m_clearAll, &QAction::triggered, this, &FavoritesPaneWidget::clearAllItems );

  // Handle context menu, reusing some of the top-level window's History menu
  m_favoritesMenu = new QMenu( this );
  m_separator     = m_favoritesMenu->addSeparator();

  for ( const auto & a : actionsFromMainWindow ) {
    m_favoritesMenu->addAction( a );
  }

  // Make the favorites pane's titlebar

  favoritesLabel.setText( tr( "Favorites:" ) );
  favoritesLabel.setObjectName( "favoritesLabel" );
  if ( layoutDirection() == Qt::LeftToRight ) {
    favoritesLabel.setAlignment( Qt::AlignLeft );
  }
  else {
    favoritesLabel.setAlignment( Qt::AlignRight );
  }

  favoritesPaneTitleBarLayout.addWidget( &favoritesLabel );
  favoritesPaneTitleBarLayout.setContentsMargins( 5, 5, 5, 5 );
  favoritesPaneTitleBar.setLayout( &favoritesPaneTitleBarLayout );
  favoritesPaneTitleBar.setObjectName( "favoritesPaneTitleBar" );
  favoritesPane->setTitleBarWidget( &favoritesPaneTitleBar );

  // Favorites tree
  m_favoritesModel = new FavoritesModel( Config::getFavoritiesFileName(), this );

  listItemDelegate = new WordListItemDelegate( m_favoritesTree->itemDelegate() );
  m_favoritesTree->setItemDelegate( listItemDelegate );

  QAbstractItemModel * oldModel = m_favoritesTree->model();
  m_favoritesTree->setModel( m_favoritesModel );
  if ( oldModel ) {
    oldModel->deleteLater();
  }

  connect( m_favoritesTree, &QTreeView::expanded, m_favoritesModel, &FavoritesModel::itemExpanded );

  connect( m_favoritesTree, &QTreeView::collapsed, m_favoritesModel, &FavoritesModel::itemCollapsed );

  connect( m_favoritesModel, &FavoritesModel::expandItem, m_favoritesTree, &QTreeView::expand );

  m_favoritesModel->checkAllNodesForExpand();
  m_favoritesTree->viewport()->setAcceptDrops( true );
  m_favoritesTree->setDragEnabled( true );
  //  m_favoritesTree->setDragDropMode( QAbstractItemView::InternalMove );
  m_favoritesTree->setDragDropMode( QAbstractItemView::DragDrop );
  m_favoritesTree->setDefaultDropAction( Qt::MoveAction );

  m_favoritesTree->setRootIsDecorated( true );

  m_favoritesTree->setContextMenuPolicy( Qt::CustomContextMenu );
  m_favoritesTree->setSelectionMode( QAbstractItemView::ExtendedSelection );

  m_favoritesTree->setEditTriggers( QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed );

  m_favoritesTree->installEventFilter( this );
  m_favoritesTree->viewport()->installEventFilter( this );

  // list selection and keyboard navigation
  connect( m_favoritesTree, &QAbstractItemView::clicked, this, &FavoritesPaneWidget::onItemClicked );

  connect( m_favoritesTree->selectionModel(),
           &QItemSelectionModel::selectionChanged,
           this,
           &FavoritesPaneWidget::onSelectionChanged );

  connect( m_favoritesTree, &QWidget::customContextMenuRequested, this, &FavoritesPaneWidget::showCustomMenu );

  connect( m_favoritesModel, &FavoritesModel::itemDropped, this, [ this ] {
    emit activeFavChange();
  } );
}

FavoritesPaneWidget::~FavoritesPaneWidget()
{
  if ( listItemDelegate ) {
    delete listItemDelegate;
  }
}

bool FavoritesPaneWidget::eventFilter( QObject * obj, QEvent * ev )
{
  // unused for now

  return QWidget::eventFilter( obj, ev );
}

void FavoritesPaneWidget::copySelectedItems()
{
  QModelIndexList selectedIdxs = m_favoritesTree->selectionModel()->selectedIndexes();

  if ( selectedIdxs.isEmpty() ) {
    // nothing to do
    return;
  }

  QStringList selectedStrings = m_favoritesModel->getTextForIndexes( selectedIdxs );

  QApplication::clipboard()->setText( selectedStrings.join( QString::fromLatin1( "\n" ) ) );
}

void FavoritesPaneWidget::deleteSelectedItems()
{
  QModelIndexList selectedIdxs = m_favoritesTree->selectionModel()->selectedIndexes();

  if ( selectedIdxs.isEmpty() ) {
    // nothing to do
    return;
  }

  if ( m_cfg->preferences.confirmFavoritesDeletion ) {
    QMessageBox mb( QMessageBox::Warning,
                    "GoldenDict",
                    tr( "All selected items will be deleted. Continue?" ),
                    QMessageBox::Yes | QMessageBox::No );
    mb.exec();
    if ( mb.result() != QMessageBox::Yes ) {
      return;
    }
  }

  m_favoritesModel->removeItemsForIndexes( selectedIdxs );
}

void FavoritesPaneWidget::folderActivation()
{
  QModelIndexList selectedIdxs = m_favoritesTree->selectionModel()->selectedIndexes();

  const auto & curSelectFullPath = m_favoritesModel->getItem( selectedIdxs.first() )->fullPath();

  if ( selectedIdxs.size() == 1 && m_favoritesModel->itemType( selectedIdxs.first() ) == TreeItem::Folder ) {
    if ( m_favoritesModel->activeFolderFullPath == curSelectFullPath ) {
      m_favoritesModel->activeFolderFullPath.clear();
    }
    else {
      m_favoritesModel->activeFolderFullPath = curSelectFullPath;
    }
  }
  emit activeFavChange();
}

void FavoritesPaneWidget::showCustomMenu( QPoint const & pos )
{
  QModelIndexList selectedIdxs = m_favoritesTree->selectionModel()->selectedIndexes();

  m_favoritesMenu->removeAction( m_activeFolderForFav );
  m_favoritesMenu->removeAction( m_copySelectedToClipboard );
  m_favoritesMenu->removeAction( m_deleteSelectedAction );
  m_favoritesMenu->removeAction( m_addFolder );
  m_favoritesMenu->removeAction( m_clearAll );

  m_separator->setVisible( !selectedIdxs.isEmpty() );

  if ( selectedIdxs.size() == 1 && m_favoritesModel->itemType( selectedIdxs.first() ) == TreeItem::Folder ) {
    m_favoritesMenu->addAction( m_activeFolderForFav );

    if ( m_favoritesModel->getItem( selectedIdxs.first() )->fullPath() == m_favoritesModel->activeFolderFullPath ) {
      m_activeFolderForFav->setText( "Deactivate folder" );
    }
    else {
      m_activeFolderForFav->setText( "Activate folder" );
    }
  }

  if ( !selectedIdxs.isEmpty() ) {
    m_favoritesMenu->insertAction( m_separator, m_copySelectedToClipboard );
    m_favoritesMenu->insertAction( m_separator, m_deleteSelectedAction );
  }

  if ( selectedIdxs.size() <= 1 ) {
    m_favoritesMenu->insertAction( m_separator, m_addFolder );
    m_favoritesMenu->insertAction( m_separator, m_clearAll );
    m_separator->setVisible( true );
  }

  m_favoritesMenu->exec( m_favoritesTree->mapToGlobal( pos ) );
}

void FavoritesPaneWidget::onSelectionChanged( const QItemSelection & selection, const QItemSelection & deselected )
{
  Q_UNUSED( deselected )

  if ( m_favoritesTree->selectionModel()->selectedIndexes().size() != 1 || selection.indexes().isEmpty() ) {
    return;
  }

  itemSelectionChanged = true;
  emitFavoritesItemRequested( selection.indexes().front() );
}

void FavoritesPaneWidget::onItemClicked( QModelIndex const & idx )
{
  if ( !itemSelectionChanged && m_favoritesTree->selectionModel()->selectedIndexes().size() == 1 ) {
    emitFavoritesItemRequested( idx );
  }
  itemSelectionChanged = false;
}

void FavoritesPaneWidget::emitFavoritesItemRequested( QModelIndex const & idx )
{
  if ( m_favoritesModel->itemType( idx ) != TreeItem::Word ) {
    // Item is not headword
    return;
  }

  // User will set group->folder in format of "a/b/c"
  QString headword     = m_favoritesModel->data( idx, Qt::DisplayRole ).toString();
  QStringList fullpath = m_favoritesModel->getItem( idx )->fullPath();
  fullpath.removeLast();
  QString path = fullpath.join( "/" );

  if ( !headword.isEmpty() ) {
    emit favoritesItemRequested( headword, path );
  }
}

void FavoritesPaneWidget::addFolder()
{
  QModelIndexList selectedIdx = m_favoritesTree->selectionModel()->selectedIndexes();
  if ( selectedIdx.size() > 1 ) {
    return;
  }

  QModelIndex folderIdx;
  if ( selectedIdx.size() ) {
    folderIdx = m_favoritesModel->addNewFolder( selectedIdx.front() );
  }
  else {
    folderIdx = m_favoritesModel->addNewFolder( QModelIndex() );
  }

  if ( folderIdx.isValid() ) {
    m_favoritesTree->edit( folderIdx );
  }
}

void FavoritesPaneWidget::clearAllItems()
{
  QMessageBox::StandardButton reply;
  reply = QMessageBox::question( this,
                                 tr( "Clear All Items" ),
                                 tr( "Are you sure you want to clear all items?" ),
                                 QMessageBox::Yes | QMessageBox::No );
  if ( reply == QMessageBox::Yes ) {
    m_favoritesModel->clearAllItems();
  }
}


void FavoritesPaneWidget::addWordToActiveFav( QString const & word )
{
  m_favoritesModel->addNewWordFullPath( word );
}


bool FavoritesPaneWidget::removeWordFromActiveFav( const QString & word )
{
  return m_favoritesModel->removeWordFullPath( word );
}

void FavoritesPaneWidget::addRemoveWordInActiveFav( const QString & word )
{
  if ( m_favoritesModel->isWordPresentFullPath( word ) ) {
    m_favoritesModel->removeWordFullPath( word );
  }
  else {
    m_favoritesModel->addNewWordFullPath( word );
  }
}

bool FavoritesPaneWidget::trySetCurrentActiveFav( const QStringList & fullpath )
{
  TreeItem * v = m_favoritesModel->getItemByFullPath( fullpath );
  if ( v != nullptr ) {
    m_favoritesModel->activeFolderFullPath = fullpath;
    return true;
  }
  else {
    return false;
  }
}

bool FavoritesPaneWidget::isWordPresentInActiveFolder( const QString & headword )
{
  return m_favoritesModel->isWordPresentFullPath( headword );
}


void FavoritesPaneWidget::getDataInXml( QByteArray & dataStr )
{
  m_favoritesModel->getDataInXml( dataStr );
}

void FavoritesPaneWidget::getDataInPlainText( QString & dataStr )
{
  m_favoritesModel->getDataInPlainText( dataStr );
}

bool FavoritesPaneWidget::setDataFromXml( QString const & dataStr )
{
  return m_favoritesModel->setDataFromXml( dataStr );
}

bool FavoritesPaneWidget::setDataFromTxt( QString const & dataStr )
{
  return m_favoritesModel->setDataFromTxt( dataStr );
}

void FavoritesPaneWidget::setSaveInterval( unsigned interval )
{
  if ( timerId ) {
    killTimer( timerId );
    timerId = 0;
  }
  if ( interval ) {
    m_favoritesModel->saveData();
    timerId = startTimer( interval * 60000 );
  }
}

void FavoritesPaneWidget::timerEvent( QTimerEvent * ev )
{
  Q_UNUSED( ev )
  m_favoritesModel->saveData();
}

void FavoritesPaneWidget::saveData()
{
  m_favoritesModel->saveData();
}

/************************************************** TreeItem *********************************************/

TreeItem::TreeItem( const QVariant & data, TreeItem * parent, Type type ):
  itemData( data ),
  parentItem( parent ),
  m_type( type ),
  m_expanded( false )
{
}

TreeItem::~TreeItem()
{
  clearChildren();
}

void TreeItem::clearChildren()
{
  qDeleteAll( childItems );
  childItems.clear();
}

void TreeItem::appendChild( TreeItem * item )
{
  childItems.append( item );
}

void TreeItem::insertChild( int row, TreeItem * item )
{
  if ( row > childItems.count() ) {
    row = childItems.count();
  }
  childItems.insert( row, item );
}

TreeItem * TreeItem::child( int row ) const
{
  return childItems.value( row );
}

void TreeItem::deleteChild( int row )
{
  if ( row < 0 || row >= childItems.count() ) {
    return;
  }

  TreeItem * it = childItems.at( row );
  childItems.removeAt( row );
  delete it;
}

int TreeItem::childCount() const
{
  return childItems.count();
}

QVariant TreeItem::data() const
{
  return itemData;
}

void TreeItem::setData( const QVariant & newData )
{
  itemData = newData;
}

int TreeItem::row() const
{
  if ( parentItem ) {
    return parentItem->childItems.indexOf( const_cast< TreeItem * >( this ) );
  }

  return 0;
}

TreeItem * TreeItem::parent()
{
  return parentItem;
}

Qt::ItemFlags TreeItem::flags() const
{
  Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
  if ( m_type == Folder ) {
    f |= Qt::ItemIsEditable | Qt::ItemIsDropEnabled;
  }
  else if ( m_type == Root ) {
    f |= Qt::ItemIsDropEnabled;
  }

  return f;
}

QStringList TreeItem::fullPath() const
{
  // Get full path from root item
  QStringList path = { data().toString() };
  TreeItem * par   = parentItem;

  for ( ;; ) {
    if ( !par ) {
      break;
    }
    if ( par->type() != TreeItem::Root ) {
      path.prepend( par->data().toString() );
    }
    par = par->parentItem;
  }
  return path;
}

TreeItem * TreeItem::duplicateItem( TreeItem * newParent ) const
{
  TreeItem * newItem = new TreeItem( itemData, newParent, m_type );
  if ( m_type == Folder ) {
    QList< TreeItem * >::const_iterator it = childItems.begin();
    for ( ; it != childItems.end(); ++it ) {
      newItem->appendChild( ( *it )->duplicateItem( newItem ) );
    }
  }
  return newItem;
}

bool TreeItem::haveAncestor( TreeItem * item )
{
  TreeItem * par = parentItem;
  for ( ;; ) {
    if ( !par ) {
      break;
    }
    if ( par == item ) {
      return true;
    }
    par = par->parent();
  }
  return false;
}

bool TreeItem::haveSameItem( TreeItem * item, bool allowSelf )
{
  QList< TreeItem * >::const_iterator it = childItems.begin();
  QString name                           = item->data().toString();
  for ( ; it != childItems.end(); ++it ) {
    if ( *it == item && !allowSelf ) {
      return true;
    }
    if ( ( *it )->data().toString() == name && ( *it )->type() == item->type() && ( *it ) != item ) {
      return true;
    }
  }

  return false;
}

QStringList TreeItem::getTextFromAllChilds() const
{
  QStringList list;
  QList< TreeItem * >::const_iterator it = childItems.begin();
  for ( ; it != childItems.end(); ++it ) {
    if ( ( *it )->type() == Word ) {
      QString txt = ( *it )->data().toString();
      list.append( txt );
    }
    else // Folder
    {
      QStringList childList = ( *it )->getTextFromAllChilds();
      list.append( childList );
    }
  }
  return list;
}

/************************************************** FavoritesModel *********************************************/

FavoritesModel::FavoritesModel( QString favoritesFilename, QObject * parent ):
  QAbstractItemModel( parent ),
  m_favoritesFilename( favoritesFilename ),
  rootItem( 0 ),
  dirty( false )
{
  readData();
  dirty = false;
}

FavoritesModel::~FavoritesModel()
{
  if ( rootItem ) {
    delete rootItem;
  }
}

Qt::ItemFlags FavoritesModel::flags( const QModelIndex & idx ) const
{
  TreeItem * item = getItem( idx );
  return item->flags();
}

QVariant FavoritesModel::headerData( int, Qt::Orientation, int ) const
{
  return QVariant();
}

QModelIndex FavoritesModel::index( int row, int column, const QModelIndex & parentIdx ) const
{
  //  if(!hasIndex(row, column, parent))
  //    return QModelIndex();

  TreeItem * parentItem = getItem( parentIdx );

  TreeItem * childItem = parentItem->child( row );
  if ( childItem ) {
    return createIndex( row, column, childItem );
  }

  return QModelIndex();
}

QModelIndex FavoritesModel::parent( const QModelIndex & index ) const
{
  if ( !index.isValid() ) {
    return QModelIndex();
  }

  TreeItem * childItem = getItem( index );
  if ( childItem == rootItem ) {
    return QModelIndex();
  }

  TreeItem * parentItem = childItem->parent();

  if ( parentItem == rootItem ) {
    return QModelIndex();
  }

  return createIndex( parentItem->row(), 0, parentItem );
}

int FavoritesModel::rowCount( const QModelIndex & parent ) const
{
  if ( parent.column() > 0 ) {
    return 0;
  }

  TreeItem * parentItem = getItem( parent );

  return parentItem->childCount();
}

int FavoritesModel::columnCount( const QModelIndex & ) const
{
  return 1;
}

bool FavoritesModel::removeRows( int row, int count, const QModelIndex & parent )
{
  TreeItem * parentItem = getItem( parent );

  beginRemoveRows( parent, row, row + count - 1 );

  for ( int i = 0; i < count; i++ ) {
    parentItem->deleteChild( row );
  }

  endRemoveRows();

  dirty = true;

  return true;
}

bool FavoritesModel::setData( const QModelIndex & index, const QVariant & value, int role )
{
  if ( role != Qt::EditRole || !index.isValid() || value.toString().isEmpty() ) {
    return false;
  }

  QModelIndex parentIdx = parent( index );
  if ( findItemInFolder( value.toString(), TreeItem::Folder, parentIdx ).isValid() ) {
    // Such folder is already presented in parent folder
    return false;
  }

  TreeItem * item = getItem( index );
  item->setData( value );

  dirty = true;

  return true;
}

QVariant FavoritesModel::data( QModelIndex const & index, int role ) const
{
  if ( !index.isValid() ) {
    return QVariant();
  }

  TreeItem * item = getItem( index );
  if ( item == rootItem ) {
    return QVariant();
  }

  if ( role == Qt::DisplayRole || role == Qt::ToolTipRole ) {
    return item->data();
  }
  else if ( role == Qt::DecorationRole ) {
    if ( item->type() == TreeItem::Folder || item->type() == TreeItem::Root ) {
      if ( item->fullPath() == activeFolderFullPath ) {
        return QIcon( ":/icons/folder_active" );
      }
      else {
        return QIcon( ":/icons/folder.svg" );
      }
    }

    return QVariant();
  }
  if ( role == Qt::EditRole ) {
    if ( item->type() == TreeItem::Folder ) {
      return item->data();
    }

    return QVariant();
  }

  return QVariant();
}

Qt::DropActions FavoritesModel::supportedDropActions() const
{
  return Qt::MoveAction | Qt::CopyAction;
}

void FavoritesModel::readData()
{
  // Read data from "favorities" file

  beginResetModel();

  if ( rootItem ) {
    delete rootItem;
  }

  rootItem = new TreeItem( QVariant(), 0, TreeItem::Root );

  QFile favoritesFile( m_favoritesFilename );
  if ( !favoritesFile.open( QFile::ReadOnly ) ) {
    qDebug( "No favorites file found" );
    return;
  }

  QString errorStr;
  int errorLine, errorColumn;
  dom.clear();

  if ( !dom.setContent( &favoritesFile, false, &errorStr, &errorLine, &errorColumn ) ) {
    // Mailformed file
    qWarning( "Favorites file parsing error: %s at %d,%d", errorStr.toUtf8().data(), errorLine, errorColumn );

    QMessageBox mb( QMessageBox::Warning, "GoldenDict", tr( "Error in favorities file" ), QMessageBox::Ok );
    mb.exec();

    dom.clear();
    favoritesFile.close();
    QFile::rename( m_favoritesFilename,
                   m_favoritesFilename % QStringLiteral( "." )
                     % QDateTime::currentDateTime().toString( QStringLiteral( "yyyyMMdd_HHmmss" ) )
                     % QStringLiteral( ".bad" ) );
  }
  else {
    favoritesFile.close();
  }

  QDomNode rootNode = dom.documentElement();
  addFolder( rootItem, rootNode );
  dom.clear();

  endResetModel();
  dirty = false;
}

void FavoritesModel::saveData()
{
  if ( !dirty ) {
    return;
  }

  QSaveFile tmpFile( m_favoritesFilename );
  if ( !tmpFile.open( QFile::WriteOnly ) ) {
    qWarning( "Can't write favorites file, error: %s", tmpFile.errorString().toUtf8().data() );
    return;
  }

  dom.clear();

  QDomElement el = dom.createElement( "root" );
  dom.appendChild( el );
  storeFolder( rootItem, el );

  QByteArray result( dom.toByteArray() );

  if ( tmpFile.write( result ) != result.size() ) {
    qWarning( "Can't write favorites file, error: %s", tmpFile.errorString().toUtf8().data() );
    return;
  }

  if ( tmpFile.commit() ) {
    dirty = false;
  }
  else {
    qDebug() << "Failed to save favorite file";
  }
  dom.clear();
}

void FavoritesModel::addFolder( TreeItem * parent, QDomNode & node )
{
  QDomNodeList nodes = node.childNodes();
  for ( int i = 0; i < nodes.count(); i++ ) {
    QDomElement el = nodes.at( i ).toElement();
    if ( el.nodeName() == "folder" ) {
      // New subfolder
      QString name    = el.attribute( "name", "" );
      TreeItem * existingItem = findFolderByName( parent, name, TreeItem::Folder );
      TreeItem * item         = existingItem != nullptr ? existingItem : new TreeItem( name, parent, TreeItem::Folder );
      if ( existingItem == nullptr ) {
        item->setExpanded( el.attribute( "expanded", "0" ) == "1" );
        parent->appendChild( item );
      }
      addFolder( item, el );
    }
    else {
      QString word = el.text();
      TreeItem * existingItem = findFolderByName( parent, word, TreeItem::Word );
      if ( existingItem != nullptr ) {
        continue;
      }
      parent->appendChild( new TreeItem( word, parent, TreeItem::Word ) );
    }
  }
  dirty = true;
}

void FavoritesModel::storeFolder( TreeItem * folder, QDomNode & node )
{
  int n = folder->childCount();
  for ( int i = 0; i < n; i++ ) {
    TreeItem * child = folder->child( i );
    QString name     = child->data().toString();
    if ( child->type() == TreeItem::Folder ) {
      QDomElement el = dom.createElement( "folder" );
      el.setAttribute( "name", name );
      el.setAttribute( "expanded", child->isExpanded() ? "1" : "0" );
      node.appendChild( el );
      storeFolder( child, el );
    }
    else {
      QDomElement el = dom.createElement( "headword" );
      el.appendChild( dom.createTextNode( name ) );
      node.appendChild( el );
    }
  }
}

void FavoritesModel::itemExpanded( const QModelIndex & index )
{
  if ( index.isValid() ) {
    TreeItem * item = getItem( index );
    item->setExpanded( true );
  }
}

void FavoritesModel::itemCollapsed( const QModelIndex & index )
{
  if ( index.isValid() ) {
    TreeItem * item = getItem( index );
    item->setExpanded( false );
  }
}

void FavoritesModel::checkAllNodesForExpand()
{
  checkNodeForExpand( rootItem, QModelIndex() );
}

void FavoritesModel::checkNodeForExpand( const TreeItem * item, const QModelIndex & parent )
{
  for ( int i = 0; i < item->childCount(); i++ ) {
    TreeItem * ch = item->child( i );
    if ( ch->type() == TreeItem::Folder && ch->isExpanded() ) {
      // We need to expand this node...
      QModelIndex idx = index( i, 0, parent );
      emit expandItem( idx );

      // ...and check it for children nodes
      checkNodeForExpand( item->child( i ), idx );
    }
  }
}

QStringList FavoritesModel::mimeTypes() const
{
  return QStringList( QString::fromLatin1( FAVORITES_MIME_TYPE ) );
}

QMimeData * FavoritesModel::mimeData( const QModelIndexList & indexes ) const
{
  FavoritesMimeData * data = new FavoritesMimeData();
  data->setIndexesList( indexes );
  return data;
}

bool FavoritesModel::dropMimeData(
  const QMimeData * data, Qt::DropAction action, int row, int, const QModelIndex & par )
{
  if ( action == Qt::MoveAction || action == Qt::CopyAction ) {
    if ( data->hasFormat( FAVORITES_MIME_TYPE ) ) {
      FavoritesMimeData const * mimeData = qobject_cast< FavoritesMimeData const * >( data );
      if ( mimeData ) {
        QModelIndexList const & list = mimeData->getIndexesList();

        if ( list.isEmpty() ) {
          return false;
        }

        TreeItem * parentItem = getItem( par );
        QModelIndex parentIdx = par;

        if ( row < 0 ) {
          row = 0;
        }

        QList< QModelIndex >::const_iterator it = list.begin();
        QList< TreeItem * > movedItems;
        for ( ; it != list.end(); ++it ) {
          TreeItem * item = getItem( *it );

          // Check if we can copy/move this item
          if ( parentItem->haveAncestor( item ) || parentItem->haveSameItem( item, action == Qt::MoveAction ) ) {
            return false;
          }

          movedItems.append( item );
        }

        // Insert items to new place

        beginInsertRows( parentIdx, row, row + movedItems.count() - 1 );
        for ( int i = 0; i < movedItems.count(); i++ ) {
          TreeItem * item    = movedItems.at( i );
          TreeItem * newItem = item->duplicateItem( parentItem );
          parentItem->insertChild( row + i, newItem );
        }
        endInsertRows();

        dirty = true;
        emit itemDropped();

        return true;
      }
    }
  }
  return false;
}

QModelIndex
FavoritesModel::findItemInFolder( const QString & itemName, TreeItem::Type itemType, const QModelIndex & parentIdx )
{
  TreeItem * parentItem = getItem( parentIdx );
  for ( int i = 0; i < parentItem->childCount(); i++ ) {
    TreeItem * item = parentItem->child( i );
    if ( item->data().toString() == itemName && item->type() == itemType ) {
      return createIndex( i, 0, item );
    }
  }
  return QModelIndex();
}

TreeItem * FavoritesModel::findFolderByName( TreeItem * parent, const QString & name, TreeItem::Type type )
{
  for ( int i = 0; i < parent->childCount(); i++ ) {
    TreeItem * child = parent->child( i );
    if ( child->type() == type && child->data().toString() == name ) {
      return child;
    }
  }
  return nullptr;
}

TreeItem * FavoritesModel::getItem( const QModelIndex & index ) const
{
  if ( index.isValid() ) {
    TreeItem * item = static_cast< TreeItem * >( index.internalPointer() );
    if ( item ) {
      return item;
    }
  }
  return rootItem;
}

TreeItem * FavoritesModel::getItemByFullPath( const QStringList & fullPath ) const
{
  TreeItem * parentItem = getItem( QModelIndex() );
  for ( const auto & pathPart : fullPath ) {
    auto childItems   = parentItem->children();
    auto folder_found = std::find_if( childItems.begin(), childItems.end(), [ &pathPart ]( TreeItem * item ) {
      return item->type() == TreeItem::Folder && item->data().toString() == pathPart;
    } );

    if ( folder_found == childItems.end() ) {
      return nullptr; // early return as no match found and no need to loop further
    }
    else {
      parentItem = *folder_found;
    }
  }
  return parentItem; // return the last matched item
}

QModelIndex FavoritesModel::getModelIndexByFullPath( const QStringList & fullPath ) const
{
  QModelIndex targetIndex = QModelIndex();

  for ( const auto & pathPart : fullPath ) {
    QList< TreeItem * > childItems = getItem( targetIndex )->children();
    auto folder_found = std::find_if( childItems.begin(), childItems.end(), [ &pathPart ]( TreeItem * item ) {
      return ( item->type() == TreeItem::Folder || item->type() == TreeItem::Root )
        && item->data().toString() == pathPart;
    } );

    if ( folder_found == childItems.end() ) {
      return {}; // early return as no match found and no need to loop further
    }
    else {
      qsizetype rowIndex           = std::distance( childItems.begin(), folder_found );
      targetIndex                  = createIndex( rowIndex, 0, *folder_found );
    }
  }
  return targetIndex; // return the last matched item;
}

QStringList FavoritesModel::getTextForIndexes( const QModelIndexList & idxList ) const
{
  QStringList list;
  QModelIndexList::const_iterator it = idxList.begin();
  for ( ; it != idxList.end(); ++it ) {
    TreeItem * item = getItem( *it );
    if ( item->type() == TreeItem::Word ) {
      list.append( item->data().toString() );
    }
    else {
      list.append( item->getTextFromAllChilds() );
    }
  }
  return list;
}

void FavoritesModel::removeItemsForIndexes( const QModelIndexList & idxList )
{
  // We should delete items from lowest tree level and in decreasing order
  // so that first deletions won't affect the indexes for subsequent deletions.

  QMap< int, QModelIndexList > itemsToDelete;
  int lowestLevel = 0;

  QModelIndexList::const_iterator it = idxList.begin();
  for ( ; it != idxList.end(); ++it ) {
    int n = level( *it );
    if ( n > lowestLevel ) {
      lowestLevel = n;
    }
    itemsToDelete[ n ].append( *it );
  }

  for ( int i = lowestLevel; i >= 0; i-- ) {
    QModelIndexList idxSublist = itemsToDelete[ i ];
// std::greater does not work as operator < not implemented
#if __cplusplus >= 201703L
    std::sort( idxSublist.begin(), idxSublist.end(), std::not_fn( std::less< QModelIndex >() ) );
#else
    std::sort( idxSublist.begin(), idxSublist.end(), std::not2( std::less< QModelIndex >() ) );
#endif

    it = idxSublist.begin();
    for ( ; it != idxSublist.end(); ++it ) {
      QModelIndex parentIdx = parent( *it );
      removeRows( ( *it ).row(), 1, parentIdx );
    }
  }
}

QModelIndex FavoritesModel::addNewFolder( const QModelIndex & idx )
{
  QString baseName = QString::fromLatin1( "New folder" );

  TreeItem * parentItem = getItem( idx );
  QModelIndex parentIdx = idx;
  int row;
  if ( parentItem->type() != TreeItem::Folder ) {
    parentIdx  = parent( idx );
    parentItem = getItem( parentIdx );
    // Insert after selected element
    row = idx.row() + 1;
  }
  else {
    row = parentItem->childCount();
  }
  // Create unique name

  QString name = baseName;
  if ( findItemInFolder( name, TreeItem::Folder, parentIdx ).isValid() ) {
    //name, with date as part of the name
    name = baseName + QString::number( QDateTime::currentDateTime().toSecsSinceEpoch() );
  }

  // Create folder with unique name
  beginInsertRows( parentIdx, row, row );
  TreeItem * newFolder = new TreeItem( name, parentItem, TreeItem::Folder );
  parentItem->insertChild( row, newFolder );
  endInsertRows();

  dirty = true;

  return createIndex( row, 0, newFolder );
}


bool FavoritesModel::addNewWordFullPath( const QString & headword )
{
  QModelIndex index = getModelIndexByFullPath( activeFolderFullPath );
  return addHeadword( headword, index );
}


bool FavoritesModel::removeWordFullPath( const QString & headword )
{
  QModelIndex parentIndex{};
  if ( !activeFolderFullPath.empty() ) {
    parentIndex = getModelIndexByFullPath( activeFolderFullPath );
  }
  for ( int i = 0; i < rowCount( parentIndex ); ++i ) {
    TreeItem * c = getItem( index( i, 0, parentIndex ) );
    if ( c->type() == TreeItem::Word && c->data().toString() == headword ) {
      removeRows( i, 1, parentIndex );
      return true;
    }
  }
  return false;
}

bool FavoritesModel::isWordPresentFullPath( const QString & headword )
{
  TreeItem * targetFolder =
    activeFolderFullPath.empty() ? getItem( QModelIndex() ) : getItemByFullPath( activeFolderFullPath );

  if ( targetFolder != nullptr ) {
    for ( int i = 0; i < targetFolder->childCount(); i++ ) {
      TreeItem * item = targetFolder->child( i );
      if ( item->type() == TreeItem::Word ) {
        if ( item->data().toString() == headword ) {
          return true;
        }
      }
    }
  }

  return false;
};


QModelIndex FavoritesModel::forceFolder( QString const & name, const QModelIndex & parentIdx )
{
  QModelIndex idx = findItemInFolder( name, TreeItem::Folder, parentIdx );
  if ( idx.isValid() ) {
    return idx;
  }

  // Folder not found, create it
  TreeItem * parentItem = getItem( parentIdx );
  TreeItem * newItem    = new TreeItem( name, parentItem, TreeItem::Folder );
  int row               = parentItem->childCount();

  beginInsertRows( parentIdx, row, row );
  parentItem->appendChild( newItem );
  endInsertRows();

  dirty = true;

  return createIndex( row, 0, newItem );
}

bool FavoritesModel::addHeadword( const QString & word, const QModelIndex & parentIdx )
{
  QModelIndex idx = findItemInFolder( word, TreeItem::Word, parentIdx );
  if ( idx.isValid() ) {
    return false;
  }

  // Headword not found, append it
  TreeItem * parentItem = getItem( parentIdx );
  TreeItem * newItem    = new TreeItem( word, parentItem, TreeItem::Word );
  int row               = parentItem->childCount();

  beginInsertRows( parentIdx, row, row );
  parentItem->appendChild( newItem );
  endInsertRows();

  dirty = true;

  return true;
}

int FavoritesModel::level( QModelIndex const & idx )
{
  int n                 = 0;
  QModelIndex parentIdx = parent( idx );
  while ( parentIdx.isValid() ) {
    n++;
    parentIdx = parent( parentIdx );
  }
  return n;
}

QString FavoritesModel::pathToItem( QModelIndex const & idx )
{
  QString path;
  QModelIndex parentIdx = parent( idx );
  while ( parentIdx.isValid() ) {
    if ( !path.isEmpty() ) {
      path = "/" + path;
    }

    path = data( parentIdx, Qt::DisplayRole ).toString() + path;

    parentIdx = parent( parentIdx );
  }
  return path;
}

void FavoritesModel::getDataInXml( QByteArray & dataStr )
{
  dom.clear();

  QDomElement el = dom.createElement( "root" );
  dom.appendChild( el );
  storeFolder( rootItem, el );

  dataStr = dom.toByteArray();
  dom.clear();
}

void FavoritesModel::getDataInPlainText( QString & dataStr )
{
  QModelIndexList list;
  list.append( QModelIndex() );
  dataStr = getTextForIndexes( list ).join( QString::fromLatin1( "\n" ) );
}

bool FavoritesModel::setDataFromXml( QString const & dataStr )
{
  QString errorStr;
  int errorLine, errorColumn;
  dom.clear();

  if ( !dom.setContent( dataStr, false, &errorStr, &errorLine, &errorColumn ) ) {
    // Mailformed data
    qWarning( "XML parsing error: %s at %d,%d", errorStr.toUtf8().data(), errorLine, errorColumn );
    dom.clear();
    return false;
  }

  beginResetModel();

  if ( !rootItem ) {
    rootItem = new TreeItem( QVariant(), 0, TreeItem::Root );
  }

  QDomNode rootNode = dom.documentElement();
  addFolder( rootItem, rootNode );

  endResetModel();

  dom.clear();
  dirty = true;
  return true;
}

bool FavoritesModel::setDataFromTxt( QString const & dataStr )
{
  auto words = dataStr.split( '\n', Qt::SkipEmptyParts );

  beginResetModel();

  if ( !rootItem ) {
    rootItem = new TreeItem( QVariant(), 0, TreeItem::Root );
  }

  for ( auto const & word : std::as_const( words ) ) {
    rootItem->appendChild( new TreeItem( word, rootItem, TreeItem::Word ) );
  }
  endResetModel();

  dirty = true;
  return true;
}
void FavoritesModel::clearAllItems()
{
  beginResetModel();

  if ( rootItem ) {
    rootItem->clearChildren();
  }

  endResetModel();

  dirty = true;
}
