/***************************************************************************
 *   Copyright (C) 2002 by Wilco Greven <greven@kde.org>                   *
 *   Copyright (C) 2002 by Chris Cheney <ccheney@cheney.cx>                *
 *   Copyright (C) 2002 by Malcolm Hunter <malcolm.hunter@gmx.co.uk>       *
 *   Copyright (C) 2003-2004 by Christophe Devriese                        *
 *                         <Christophe.Devriese@student.kuleuven.ac.be>    *
 *   Copyright (C) 2003 by Daniel Molkentin <molkentin@kde.org>            *
 *   Copyright (C) 2003 by Andy Goossens <andygoossens@telenet.be>         *
 *   Copyright (C) 2003 by Dirk Mueller <mueller@kde.org>                  *
 *   Copyright (C) 2003 by Laurent Montel <montel@kde.org>                 *
 *   Copyright (C) 2004 by Dominique Devriese <devriese@kde.org>           *
 *   Copyright (C) 2004 by Christoph Cullmann <crossfire@babylon2k.de>     *
 *   Copyright (C) 2004 by Henrique Pinto <stampede@coltec.ufmg.br>        *
 *   Copyright (C) 2004 by Waldo Bastian <bastian@kde.org>                 *
 *   Copyright (C) 2004-2008 by Albert Astals Cid <aacid@kde.org>          *
 *   Copyright (C) 2004 by Antti Markus <antti.markus@starman.ee>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "part.h"

// qt/kde includes
#include <qapplication.h>
#include <qfile.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qtimer.h>
#include <QtGui/QPrinter>
#include <QtGui/QPrintDialog>
#include <QScrollBar>

#include <kvbox.h>
#include <kaboutapplicationdialog.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kdirwatch.h>
#include <kstandardaction.h>
#include <kpluginfactory.h>
#include <kfiledialog.h>
#include <kinputdialog.h>
#include <kmessagebox.h>
#include <knuminput.h>
#include <kio/netaccess.h>
#include <kmenu.h>
#include <kxmlguiclient.h>
#include <kxmlguifactory.h>
#include <kservicetypetrader.h>
#include <kstandarddirs.h>
#include <kstandardshortcut.h>
#include <ktemporaryfile.h>
#include <ktoggleaction.h>
#include <ktogglefullscreenaction.h>
#include <kio/job.h>
#include <kicon.h>
#include <kfilterdev.h>
#include <kfilterbase.h>
#if 0
#include <knewstuff2/engine.h>
#endif
#include <kdeprintdialog.h>
#include <kprintpreview.h>
#include <kbookmarkmenu.h>
#include <kpassworddialog.h>
#include <kwallet.h>

// local includes
#include "aboutdata.h"
#include "extensions.h"
#include "ui/pageview.h"
#include "ui/toc.h"
#include "ui/searchwidget.h"
#include "ui/thumbnaillist.h"
#include "ui/side_reviews.h"
#include "ui/minibar.h"
#include "ui/embeddedfilesdialog.h"
#include "ui/propertiesdialog.h"
#include "ui/presentationwidget.h"
#include "ui/pagesizelabel.h"
#include "ui/bookmarklist.h"
#include "ui/findbar.h"
#include "ui/sidebar.h"
#include "ui/fileprinterpreview.h"
#include "ui/guiutils.h"
#include "ui/layers.h"
#include "conf/preferencesdialog.h"
#include "settings.h"
#include "core/action.h"
#include "core/annotations.h"
#include "core/bookmarkmanager.h"
#include "core/document.h"
#include "core/generator.h"
#include "core/page.h"
#include "core/fileprinter.h"

#include <cstdio>
#include <memory>

class FileKeeper
{
    public:
        FileKeeper()
            : m_handle( NULL )
        {
        }

        ~FileKeeper()
        {
        }

        void open( const QString & path )
        {
            if ( !m_handle )
                m_handle = std::fopen( QFile::encodeName( path ), "r" );
        }

        void close()
        {
            if ( m_handle )
            {
                int ret = std::fclose( m_handle );
                Q_UNUSED( ret )
                m_handle = NULL;
            }
        }

        KTemporaryFile* copyToTemporary() const
        {
            if ( !m_handle )
                return 0;

            KTemporaryFile * retFile = new KTemporaryFile;
            retFile->open();

            std::rewind( m_handle );
            int c = -1;
            do
            {
                c = std::fgetc( m_handle );
                if ( c == EOF )
                    break;
                if ( !retFile->putChar( (char)c ) )
                    break;
            } while ( !feof( m_handle ) );

            retFile->flush();

            return retFile;
        }

    private:
        std::FILE * m_handle;
};

Okular::PartFactory::PartFactory()
: KPluginFactory(okularAboutData( "okular", I18N_NOOP( "Okular" ) ))
{
}

Okular::PartFactory::~PartFactory()
{
}

QObject *Okular::PartFactory::create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword)
{
    Q_UNUSED ( keyword );

    Okular::Part *object = new Okular::Part( parentWidget, parent, args, componentData() );
    object->setReadWrite( QLatin1String(iface) == QLatin1String("KParts::ReadWritePart") );
    return object;
}

K_EXPORT_PLUGIN( Okular::PartFactory() )

static QAction* actionForExportFormat( const Okular::ExportFormat& format, QObject *parent = 0 )
{
    QAction *act = new QAction( format.description(), parent );
    if ( !format.icon().isNull() )
    {
        act->setIcon( format.icon() );
    }
    return act;
}

static QString compressedMimeFor( const QString& mime_to_check )
{
    // The compressedMimeMap is here in case you have a very old shared mime database
    // that doesn't have inheritance info for things like gzeps, etc
    // Otherwise the "is()" calls below are just good enough
    static QHash< QString, QString > compressedMimeMap;
    static bool supportBzip = false;
    static bool supportXz = false;
    const QString app_gzip( QString::fromLatin1( "application/x-gzip" ) );
    const QString app_bzip( QString::fromLatin1( "application/x-bzip" ) );
    const QString app_xz( QString::fromLatin1( "application/x-xz" ) );
    if ( compressedMimeMap.isEmpty() )
    {
        std::unique_ptr< KFilterBase > f;
        compressedMimeMap[ QString::fromLatin1( "image/x-gzeps" ) ] = app_gzip;
        // check we can read bzip2-compressed files
        f.reset( KFilterBase::findFilterByMimeType( app_bzip ) );
        if ( f.get() )
        {
            supportBzip = true;
            compressedMimeMap[ QString::fromLatin1( "application/x-bzpdf" ) ] = app_bzip;
            compressedMimeMap[ QString::fromLatin1( "application/x-bzpostscript" ) ] = app_bzip;
            compressedMimeMap[ QString::fromLatin1( "application/x-bzdvi" ) ] = app_bzip;
            compressedMimeMap[ QString::fromLatin1( "image/x-bzeps" ) ] = app_bzip;
        }
        // check we can read XZ-compressed files
        f.reset( KFilterBase::findFilterByMimeType( app_xz ) );
        if ( f.get() )
        {
            supportXz = true;
        }
    }
    QHash< QString, QString >::const_iterator it = compressedMimeMap.constFind( mime_to_check );
    if ( it != compressedMimeMap.constEnd() )
        return it.value();

    KMimeType::Ptr mime = KMimeType::mimeType( mime_to_check );
    if ( mime )
    {
        if ( mime->is( app_gzip ) )
            return app_gzip;
        else if ( supportBzip && mime->is( app_bzip ) )
            return app_bzip;
        else if ( supportXz && mime->is( app_xz ) )
            return app_xz;
    }

    return QString();
}

static Okular::EmbedMode detectEmbedMode( QWidget *parentWidget, QObject *parent, const QVariantList &args )
{
    Q_UNUSED( parentWidget );

    if ( parent
         && ( parent->objectName().startsWith( QLatin1String( "okular::Shell" ) )
              || parent->objectName().startsWith( QLatin1String( "okular/okular__Shell" ) ) ) )
        return Okular::NativeShellMode;

    if ( parent
         && ( QByteArray( "KHTMLPart" ) == parent->metaObject()->className() ) )
        return Okular::KHTMLPartMode;

    Q_FOREACH ( const QVariant &arg, args )
    {
        if ( arg.type() == QVariant::String )
        {
            if ( arg.toString() == QLatin1String( "Print/Preview" ) )
            {
                return Okular::PrintPreviewMode;
            }
            else if ( arg.toString() == QLatin1String( "ViewerWidget" ) )
            {
                return Okular::ViewerWidgetMode;
            }
        }
    }

    return Okular::UnknownEmbedMode;
}

static QString detectConfigFileName( const QVariantList &args )
{
    Q_FOREACH ( const QVariant &arg, args )
    {
        if ( arg.type() == QVariant::String )
        {
            QString argString = arg.toString();
            int separatorIndex = argString.indexOf( "=" );
            if ( separatorIndex >= 0 && argString.left( separatorIndex ) == QLatin1String( "ConfigFileName" ) )
            {
                return argString.mid( separatorIndex + 1 );
            }
        }
    }

    return QString();
}

#undef OKULAR_KEEP_FILE_OPEN

#ifdef OKULAR_KEEP_FILE_OPEN
static bool keepFileOpen()
{
    static bool keep_file_open = !qgetenv("OKULAR_NO_KEEP_FILE_OPEN").toInt();
    return keep_file_open;
}
#endif

int Okular::Part::numberOfParts = 0;

namespace Okular
{

Part::Part(QWidget *parentWidget,
QObject *parent,
const QVariantList &args,
KComponentData componentData )
: KParts::ReadWritePart(parent),
m_tempfile( 0 ), m_fileWasRemoved( false ), m_showMenuBarAction( 0 ), m_showFullScreenAction( 0 ), m_actionsSearched( false ),
m_cliPresentation(false), m_cliPrint(false), m_embedMode(detectEmbedMode(parentWidget, parent, args)), m_generatorGuiClient(0), m_keeper( 0 )
{
    // first, we check if a config file name has been specified
    QString configFileName = detectConfigFileName( args );
    if ( configFileName.isEmpty() )
    {
        configFileName = KStandardDirs::locateLocal( "config", "okularpartrc" );
        // first necessary step: copy the configuration from kpdf, if available
        if ( !QFile::exists( configFileName ) )
        {
            QString oldkpdfconffile = KStandardDirs::locateLocal( "config", "kpdfpartrc" );
            if ( QFile::exists( oldkpdfconffile ) )
                QFile::copy( oldkpdfconffile, configFileName );
        }
    }
    Okular::Settings::instance( configFileName );
    
    numberOfParts++;
    if (numberOfParts == 1) {
        QDBusConnection::sessionBus().registerObject("/okular", this, QDBusConnection::ExportScriptableSlots);
    } else {
        QDBusConnection::sessionBus().registerObject(QString("/okular%1").arg(numberOfParts), this, QDBusConnection::ExportScriptableSlots);
    }

    // connect the started signal to tell the job the mimetypes we like,
    // and get some more information from it
    connect(this, SIGNAL(started(KIO::Job*)), this, SLOT(slotJobStarted(KIO::Job*)));

    // connect the completed signal so we can put the window caption when loading remote files
    connect(this, SIGNAL(completed()), this, SLOT(setWindowTitleFromDocument()));
    connect(this, SIGNAL(canceled(QString)), this, SLOT(loadCancelled(QString)));

    // create browser extension (for printing when embedded into browser)
    m_bExtension = new BrowserExtension(this);
    // create live connect extension (for integrating with browser scripting)
    new OkularLiveConnectExtension( this );

    // we need an instance
    setComponentData( componentData );

    GuiUtils::addIconLoader( iconLoader() );

    m_sidebar = new Sidebar( parentWidget );
    setWidget( m_sidebar );
    connect( m_sidebar, SIGNAL(urlsDropped(KUrl::List)), SLOT(handleDroppedUrls(KUrl::List)) );

    // build the document
    m_document = new Okular::Document(widget());
    connect( m_document, SIGNAL(linkFind()), this, SLOT(slotFind()) );
    connect( m_document, SIGNAL(linkGoToPage()), this, SLOT(slotGoToPage()) );
    connect( m_document, SIGNAL(linkPresentation()), this, SLOT(slotShowPresentation()) );
    connect( m_document, SIGNAL(linkEndPresentation()), this, SLOT(slotHidePresentation()) );
    connect( m_document, SIGNAL(openUrl(KUrl)), this, SLOT(openUrlFromDocument(KUrl)) );
    connect( m_document->bookmarkManager(), SIGNAL(openUrl(KUrl)), this, SLOT(openUrlFromBookmarks(KUrl)) );
    connect( m_document, SIGNAL(close()), this, SLOT(close()) );

    if ( parent && parent->metaObject()->indexOfSlot( QMetaObject::normalizedSignature( "slotQuit()" ) ) != -1 )
        connect( m_document, SIGNAL(quit()), parent, SLOT(slotQuit()) );
    else
        connect( m_document, SIGNAL(quit()), this, SLOT(cannotQuit()) );
    // widgets: ^searchbar (toolbar containing label and SearchWidget)
    //      m_searchToolBar = new KToolBar( parentWidget, "searchBar" );
    //      m_searchToolBar->boxLayout()->setSpacing( KDialog::spacingHint() );
    //      QLabel * sLabel = new QLabel( i18n( "&Search:" ), m_searchToolBar, "kde toolbar widget" );
    //      m_searchWidget = new SearchWidget( m_searchToolBar, m_document );
    //      sLabel->setBuddy( m_searchWidget );
    //      m_searchToolBar->setStretchableWidget( m_searchWidget );

    // [left toolbox: Table of Contents] | []
    m_toc = new TOC( 0, m_document );
    connect( m_toc, SIGNAL(hasTOC(bool)), this, SLOT(enableTOC(bool)) );
    m_sidebar->addItem( m_toc, KIcon(QApplication::isLeftToRight() ? "format-justify-left" : "format-justify-right"), i18n("Contents") );
    enableTOC( false );

    // [left toolbox: Layers] | []
    m_layers = new Layers( 0, m_document );
    connect( m_layers, SIGNAL(hasLayers(bool)), this, SLOT(enableLayers(bool)) );
    m_sidebar->addItem( m_layers, KIcon( "draw-freehand" ), i18n( "Layers" ) );
    enableLayers( false );

    // [left toolbox: Thumbnails and Bookmarks] | []
    KVBox * thumbsBox = new ThumbnailsBox( 0 );
    thumbsBox->setSpacing( 6 );
    m_searchWidget = new SearchWidget( thumbsBox, m_document );
    m_thumbnailList = new ThumbnailList( thumbsBox, m_document );
    //	ThumbnailController * m_tc = new ThumbnailController( thumbsBox, m_thumbnailList );
    connect( m_thumbnailList, SIGNAL(rightClick(const Okular::Page*,QPoint)), this, SLOT(slotShowMenu(const Okular::Page*,QPoint)) );
    m_sidebar->addItem( thumbsBox, KIcon( "view-preview" ), i18n("Thumbnails") );

    m_sidebar->setCurrentItem( thumbsBox );

    // [left toolbox: Reviews] | []
    m_reviewsWidget = new Reviews( 0, m_document );
    m_sidebar->addItem( m_reviewsWidget, KIcon("draw-freehand"), i18n("Reviews") );
    m_sidebar->setItemEnabled( m_reviewsWidget, false );

    // [left toolbox: Bookmarks] | []
    m_bookmarkList = new BookmarkList( m_document, 0 );
    m_sidebar->addItem( m_bookmarkList, KIcon("bookmarks"), i18n("Bookmarks") );
    m_sidebar->setItemEnabled( m_bookmarkList, false );

    // widgets: [../miniBarContainer] | []
#ifdef OKULAR_ENABLE_MINIBAR
    QWidget * miniBarContainer = new QWidget( 0 );
    m_sidebar->setBottomWidget( miniBarContainer );
    QVBoxLayout * miniBarLayout = new QVBoxLayout( miniBarContainer );
    miniBarLayout->setMargin( 0 );
    // widgets: [../[spacer/..]] | []
    miniBarLayout->addItem( new QSpacerItem( 6, 6, QSizePolicy::Fixed, QSizePolicy::Fixed ) );
    // widgets: [../[../MiniBar]] | []
    QFrame * bevelContainer = new QFrame( miniBarContainer );
    bevelContainer->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    QVBoxLayout * bevelContainerLayout = new QVBoxLayout( bevelContainer );
    bevelContainerLayout->setMargin( 4 );
    m_progressWidget = new ProgressWidget( bevelContainer, m_document );
    bevelContainerLayout->addWidget( m_progressWidget );
    miniBarLayout->addWidget( bevelContainer );
    miniBarLayout->addItem( new QSpacerItem( 6, 6, QSizePolicy::Fixed, QSizePolicy::Fixed ) );
#endif

    // widgets: [] | [right 'pageView']
    QWidget * rightContainer = new QWidget( 0 );
    m_sidebar->setMainWidget( rightContainer );
    QVBoxLayout * rightLayout = new QVBoxLayout( rightContainer );
    rightLayout->setMargin( 0 );
    rightLayout->setSpacing( 0 );
    //	KToolBar * rtb = new KToolBar( rightContainer, "mainToolBarSS" );
    //	rightLayout->addWidget( rtb );
    m_topMessage = new KMessageWidget( rightContainer );
    m_topMessage->setVisible( false );
    m_topMessage->setWordWrap( true );
    m_topMessage->setMessageType( KMessageWidget::Information );
    m_topMessage->setText( i18n( "This document has embedded files. <a href=\"okular:/embeddedfiles\">Click here to see them</a> or go to File -> Embedded Files." ) );
    m_topMessage->setIcon( KIcon( "mail-attachment" ) );
    connect( m_topMessage, SIGNAL(linkActivated(QString)), this, SLOT(slotShowEmbeddedFiles()) );
    rightLayout->addWidget( m_topMessage );
    m_formsMessage = new KMessageWidget( rightContainer );
    m_formsMessage->setVisible( false );
    m_formsMessage->setWordWrap( true );
    m_formsMessage->setMessageType( KMessageWidget::Information );
    rightLayout->addWidget( m_formsMessage );
    m_infoMessage = new KMessageWidget( rightContainer );
    m_infoMessage->setVisible( false );
    m_infoMessage->setWordWrap( true );
    m_infoMessage->setMessageType( KMessageWidget::Information );
    rightLayout->addWidget( m_infoMessage );
    m_infoTimer = new QTimer();
    m_infoTimer->setSingleShot( true );
    connect( m_infoTimer, SIGNAL(timeout()), m_infoMessage, SLOT(animatedHide()) );
    m_pageView = new PageView( rightContainer, m_document );
    QMetaObject::invokeMethod( m_pageView, "setFocus", Qt::QueuedConnection );      //usability setting
//    m_splitter->setFocusProxy(m_pageView);
    connect( m_pageView, SIGNAL(rightClick(const Okular::Page*,QPoint)), this, SLOT(slotShowMenu(const Okular::Page*,QPoint)) );
    connect( m_document, SIGNAL(error(QString,int)), this, SLOT(errorMessage(QString,int)) );
    connect( m_document, SIGNAL(warning(QString,int)), this, SLOT(warningMessage(QString,int)) );
    connect( m_document, SIGNAL(notice(QString,int)), this, SLOT(noticeMessage(QString,int)) );
    connect( m_document, SIGNAL(sourceReferenceActivated(const QString&,int,int,bool*)), this, SLOT(slotHandleActivatedSourceReference(const QString&,int,int,bool*)) );
    connect( m_pageView, SIGNAL(fitWindowToPage(QSize,QSize)), this, SIGNAL(fitWindowToPage(QSize,QSize)) );
    rightLayout->addWidget( m_pageView );
    m_layers->setPageView( m_pageView );
    m_findBar = new FindBar( m_document, rightContainer );
    rightLayout->addWidget( m_findBar );
    m_bottomBar = new QWidget( rightContainer );
    QHBoxLayout * bottomBarLayout = new QHBoxLayout( m_bottomBar );
    m_pageSizeLabel = new PageSizeLabel( m_bottomBar, m_document );
    bottomBarLayout->setMargin( 0 );
    bottomBarLayout->setSpacing( 0 );
    bottomBarLayout->addItem( new QSpacerItem( 5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum ) );
    m_miniBarLogic = new MiniBarLogic( this, m_document );
    m_miniBar = new MiniBar( m_bottomBar, m_miniBarLogic );
    bottomBarLayout->addWidget( m_miniBar );
    bottomBarLayout->addWidget( m_pageSizeLabel );
    rightLayout->addWidget( m_bottomBar );

    m_pageNumberTool = new MiniBar( 0, m_miniBarLogic );

    connect( m_findBar, SIGNAL(forwardKeyPressEvent(QKeyEvent*)), m_pageView, SLOT(externalKeyPressEvent(QKeyEvent*)));
    connect( m_findBar, SIGNAL(onCloseButtonPressed()), m_pageView, SLOT(setFocus()));
    connect( m_miniBar, SIGNAL(forwardKeyPressEvent(QKeyEvent*)), m_pageView, SLOT(externalKeyPressEvent(QKeyEvent*)));
    connect( m_pageView, SIGNAL(escPressed()), m_findBar, SLOT(resetSearch()) );
    connect( m_pageNumberTool, SIGNAL(forwardKeyPressEvent(QKeyEvent*)), m_pageView, SLOT(externalKeyPressEvent(QKeyEvent*)));

    connect( m_reviewsWidget, SIGNAL(openAnnotationWindow(Okular::Annotation*,int)),
        m_pageView, SLOT(openAnnotationWindow(Okular::Annotation*,int)) );

    // add document observers
    m_document->addObserver( this );
    m_document->addObserver( m_thumbnailList );
    m_document->addObserver( m_pageView );
    m_document->registerView( m_pageView );
    m_document->addObserver( m_toc );
    m_document->addObserver( m_miniBarLogic );
#ifdef OKULAR_ENABLE_MINIBAR
    m_document->addObserver( m_progressWidget );
#endif
    m_document->addObserver( m_reviewsWidget );
    m_document->addObserver( m_pageSizeLabel );
    m_document->addObserver( m_bookmarkList );

    connect( m_document->bookmarkManager(), SIGNAL(saved()),
        this, SLOT(slotRebuildBookmarkMenu()) );

    setupViewerActions();

    if ( m_embedMode != ViewerWidgetMode )
    {
        setupActions();
    }
    else
    {
        setViewerShortcuts();
    }

    // document watcher and reloader
    m_watcher = new KDirWatch( this );
    connect( m_watcher, SIGNAL(dirty(QString)), this, SLOT(slotFileDirty(QString)) );
    m_dirtyHandler = new QTimer( this );
    m_dirtyHandler->setSingleShot( true );
    connect( m_dirtyHandler, SIGNAL(timeout()),this, SLOT(slotDoFileDirty()) );

    slotNewConfig();

    // keep us informed when the user changes settings
    connect( Okular::Settings::self(), SIGNAL(configChanged()), this, SLOT(slotNewConfig()) );

    // [SPEECH] check for KTTSD presence and usability
    const KService::Ptr kttsd = KService::serviceByDesktopName("kttsd");
    Okular::Settings::setUseKTTSD( kttsd );
    Okular::Settings::self()->writeConfig();

    rebuildBookmarkMenu( false );

    if ( m_embedMode == ViewerWidgetMode ) {
        // set the XML-UI resource file for the viewer mode
        setXMLFile("part-viewermode.rc");
    }
    else
    {
        // set our main XML-UI resource file
        setXMLFile("part.rc");
    }

    m_pageView->setupBaseActions( actionCollection() );

    m_sidebar->setSidebarVisibility( false );
    if ( m_embedMode != PrintPreviewMode )
    {
        // now set up actions that are required for all remaining modes
        m_pageView->setupViewerActions( actionCollection() );
        // and if we are not in viewer mode, we want the full GUI
        if ( m_embedMode != ViewerWidgetMode )
        {
            unsetDummyMode();
        }
    }

    // ensure history actions are in the correct state
    updateViewActions();

    // also update the state of the actions in the page view
    m_pageView->updateActionState( false, false, false );

    if ( m_embedMode == NativeShellMode )
        m_sidebar->setAutoFillBackground( false );

#ifdef OKULAR_KEEP_FILE_OPEN
    m_keeper = new FileKeeper();
#endif
}

void Part::setupViewerActions()
{
    // ACTIONS
    KActionCollection * ac = actionCollection();

    // Page Traversal actions
    m_gotoPage = KStandardAction::gotoPage( this, SLOT(slotGoToPage()), ac );
    m_gotoPage->setShortcut( QKeySequence(Qt::CTRL + Qt::Key_G) );
    // dirty way to activate gotopage when pressing miniBar's button
    connect( m_miniBar, SIGNAL(gotoPage()), m_gotoPage, SLOT(trigger()) );
    connect( m_pageNumberTool, SIGNAL(gotoPage()), m_gotoPage, SLOT(trigger()) );

    m_prevPage = KStandardAction::prior(this, SLOT(slotPreviousPage()), ac);
    m_prevPage->setIconText( i18nc( "Previous page", "Previous" ) );
    m_prevPage->setToolTip( i18n( "Go back to the Previous Page" ) );
    m_prevPage->setWhatsThis( i18n( "Moves to the previous page of the document" ) );
    m_prevPage->setShortcut( 0 );
    // dirty way to activate prev page when pressing miniBar's button
    connect( m_miniBar, SIGNAL(prevPage()), m_prevPage, SLOT(trigger()) );
    connect( m_pageNumberTool, SIGNAL(prevPage()), m_prevPage, SLOT(trigger()) );
#ifdef OKULAR_ENABLE_MINIBAR
    connect( m_progressWidget, SIGNAL(prevPage()), m_prevPage, SLOT(trigger()) );
#endif

    m_nextPage = KStandardAction::next(this, SLOT(slotNextPage()), ac );
    m_nextPage->setIconText( i18nc( "Next page", "Next" ) );
    m_nextPage->setToolTip( i18n( "Advance to the Next Page" ) );
    m_nextPage->setWhatsThis( i18n( "Moves to the next page of the document" ) );
    m_nextPage->setShortcut( 0 );
    // dirty way to activate next page when pressing miniBar's button
    connect( m_miniBar, SIGNAL(nextPage()), m_nextPage, SLOT(trigger()) );
    connect( m_pageNumberTool, SIGNAL(nextPage()), m_nextPage, SLOT(trigger()) );
#ifdef OKULAR_ENABLE_MINIBAR
    connect( m_progressWidget, SIGNAL(nextPage()), m_nextPage, SLOT(trigger()) );
#endif

    m_beginningOfDocument = KStandardAction::firstPage( this, SLOT(slotGotoFirst()), ac );
    ac->addAction("first_page", m_beginningOfDocument);
    m_beginningOfDocument->setText(i18n( "Beginning of the document"));
    m_beginningOfDocument->setWhatsThis( i18n( "Moves to the beginning of the document" ) );

    m_endOfDocument = KStandardAction::lastPage( this, SLOT(slotGotoLast()), ac );
    ac->addAction("last_page",m_endOfDocument);
    m_endOfDocument->setText(i18n( "End of the document"));
    m_endOfDocument->setWhatsThis( i18n( "Moves to the end of the document" ) );

    // we do not want back and next in history in the dummy mode
    m_historyBack = 0;
    m_historyNext = 0;

    m_addBookmark = KStandardAction::addBookmark( this, SLOT(slotAddBookmark()), ac );
    m_addBookmarkText = m_addBookmark->text();
    m_addBookmarkIcon = m_addBookmark->icon();

    m_renameBookmark = ac->addAction("rename_bookmark");
    m_renameBookmark->setText(i18n( "Rename Bookmark" ));
    m_renameBookmark->setIcon(KIcon( "edit-rename" ));
    m_renameBookmark->setWhatsThis( i18n( "Rename the current bookmark" ) );
    connect( m_renameBookmark, SIGNAL(triggered()), this, SLOT(slotRenameCurrentViewportBookmark()) );

    m_prevBookmark = ac->addAction("previous_bookmark");
    m_prevBookmark->setText(i18n( "Previous Bookmark" ));
    m_prevBookmark->setIcon(KIcon( "go-up-search" ));
    m_prevBookmark->setWhatsThis( i18n( "Go to the previous bookmark" ) );
    connect( m_prevBookmark, SIGNAL(triggered()), this, SLOT(slotPreviousBookmark()) );

    m_nextBookmark = ac->addAction("next_bookmark");
    m_nextBookmark->setText(i18n( "Next Bookmark" ));
    m_nextBookmark->setIcon(KIcon( "go-down-search" ));
    m_nextBookmark->setWhatsThis( i18n( "Go to the next bookmark" ) );
    connect( m_nextBookmark, SIGNAL(triggered()), this, SLOT(slotNextBookmark()) );

    m_copy = 0;

    m_selectAll = 0;

    // Find and other actions
    m_find = KStandardAction::find( this, SLOT(slotShowFindBar()), ac );
    QList<QKeySequence> s = m_find->shortcuts();
    s.append( QKeySequence( Qt::Key_Slash ) );
    m_find->setShortcuts( s );
    m_find->setEnabled( false );

    m_findNext = KStandardAction::findNext( this, SLOT(slotFindNext()), ac);
    m_findNext->setEnabled( false );

    m_findPrev = KStandardAction::findPrev( this, SLOT(slotFindPrev()), ac );
    m_findPrev->setEnabled( false );

    m_saveCopyAs = 0;
    m_saveAs = 0;

    QAction * prefs = KStandardAction::preferences( this, SLOT(slotPreferences()), ac);
    if ( m_embedMode == NativeShellMode )
    {
        prefs->setText( i18n( "Configure Okular..." ) );
    }
    else
    {
        // TODO: improve this message
        prefs->setText( i18n( "Configure Viewer..." ) );
    }

    KAction * genPrefs = new KAction( ac );
    ac->addAction("options_configure_generators", genPrefs);
    if ( m_embedMode == ViewerWidgetMode )
    {
        genPrefs->setText( i18n( "Configure Viewer Backends..." ) );
    }
    else
    {
        genPrefs->setText( i18n( "Configure Backends..." ) );
    }
    genPrefs->setIcon( KIcon( "configure" ) );
    genPrefs->setEnabled( m_document->configurableGenerators() > 0 );
    connect( genPrefs, SIGNAL(triggered(bool)), this, SLOT(slotGeneratorPreferences()) );

    m_printPreview = KStandardAction::printPreview( this, SLOT(slotPrintPreview()), ac );
    m_printPreview->setEnabled( false );

    m_showLeftPanel = 0;
    m_showBottomBar = 0;

    m_showProperties = ac->addAction("properties");
    m_showProperties->setText(i18n("&Properties"));
    m_showProperties->setIcon(KIcon("document-properties"));
    connect(m_showProperties, SIGNAL(triggered()), this, SLOT(slotShowProperties()));
    m_showProperties->setEnabled( false );

    m_showEmbeddedFiles = 0;
    m_showPresentation = 0;

    m_exportAs = 0;
    m_exportAsMenu = 0;
    m_exportAsText = 0;
    m_exportAsDocArchive = 0;

    m_aboutBackend = ac->addAction("help_about_backend");
    m_aboutBackend->setText(i18n("About Backend"));
    m_aboutBackend->setEnabled( false );
    connect(m_aboutBackend, SIGNAL(triggered()), this, SLOT(slotAboutBackend()));

    KAction *reload = ac->add<KAction>( "file_reload" );
    reload->setText( i18n( "Reloa&d" ) );
    reload->setIcon( KIcon( "view-refresh" ) );
    reload->setWhatsThis( i18n( "Reload the current document from disk." ) );
    connect( reload, SIGNAL(triggered()), this, SLOT(slotReload()) );
    reload->setShortcut( KStandardShortcut::reload() );
    m_reload = reload;

    m_closeFindBar = ac->addAction( "close_find_bar", this, SLOT(slotHideFindBar()) );
    m_closeFindBar->setText( i18n("Close &Find Bar") );
    m_closeFindBar->setShortcut( QKeySequence(Qt::Key_Escape) );
    m_closeFindBar->setEnabled( false );

    KAction *pageno = new KAction( i18n( "Page Number" ), ac );
    pageno->setDefaultWidget( m_pageNumberTool );
    ac->addAction( "page_number", pageno );
}

void Part::setViewerShortcuts()
{
    KActionCollection * ac = actionCollection();

    m_gotoPage->setShortcut( QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_G) );
    m_find->setShortcuts( QList<QKeySequence>() );

    m_findNext->setShortcut( QKeySequence() );
    m_findPrev->setShortcut( QKeySequence() );

    m_addBookmark->setShortcut( QKeySequence( Qt::CTRL + Qt::ALT + Qt::Key_B ) );

    m_beginningOfDocument->setShortcut( QKeySequence( Qt::CTRL + Qt::ALT + Qt::Key_Home ) );
    m_endOfDocument->setShortcut( QKeySequence( Qt::CTRL + Qt::ALT + Qt::Key_End ) );

    KAction *action = static_cast<KAction*>( ac->action( "file_reload" ) );
    if( action )  action->setShortcuts( QList<QKeySequence>() << QKeySequence( Qt::ALT + Qt::Key_F5 ) );
}

void Part::setupActions()
{
    KActionCollection * ac = actionCollection();

    m_copy = KStandardAction::create( KStandardAction::Copy, m_pageView, SLOT(copyTextSelection()), ac );

    m_selectAll = KStandardAction::selectAll( m_pageView, SLOT(selectAll()), ac );

    m_saveCopyAs = KStandardAction::saveAs( this, SLOT(slotSaveCopyAs()), ac );
    m_saveCopyAs->setText( i18n( "Save &Copy As..." ) );
    m_saveCopyAs->setShortcut( KShortcut() );
    ac->addAction( "file_save_copy", m_saveCopyAs );
    m_saveCopyAs->setEnabled( false );

    m_saveAs = KStandardAction::saveAs( this, SLOT(slotSaveFileAs()), ac );
    m_saveAs->setEnabled( false );

    m_showLeftPanel = ac->add<KToggleAction>("show_leftpanel");
    m_showLeftPanel->setText(i18n( "Show &Navigation Panel"));
    m_showLeftPanel->setIcon(KIcon( "view-sidetree" ));
    connect( m_showLeftPanel, SIGNAL(toggled(bool)), this, SLOT(slotShowLeftPanel()) );
    m_showLeftPanel->setShortcut( Qt::Key_F7 );
    m_showLeftPanel->setChecked( Okular::Settings::showLeftPanel() );
    slotShowLeftPanel();

    m_showBottomBar = ac->add<KToggleAction>("show_bottombar");
    m_showBottomBar->setText(i18n( "Show &Page Bar"));
    connect( m_showBottomBar, SIGNAL(toggled(bool)), this, SLOT(slotShowBottomBar()) );
    m_showBottomBar->setChecked( Okular::Settings::showBottomBar() );
    slotShowBottomBar();

    m_showEmbeddedFiles = ac->addAction("embedded_files");
    m_showEmbeddedFiles->setText(i18n("&Embedded Files"));
    m_showEmbeddedFiles->setIcon( KIcon( "mail-attachment" ) );
    connect(m_showEmbeddedFiles, SIGNAL(triggered()), this, SLOT(slotShowEmbeddedFiles()));
    m_showEmbeddedFiles->setEnabled( false );

    m_exportAs = ac->addAction("file_export_as");
    m_exportAs->setText(i18n("E&xport As"));
    m_exportAs->setIcon( KIcon( "document-export" ) );
    m_exportAsMenu = new QMenu();
    connect(m_exportAsMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotExportAs(QAction*)));
    m_exportAs->setMenu( m_exportAsMenu );
    m_exportAsText = actionForExportFormat( Okular::ExportFormat::standardFormat( Okular::ExportFormat::PlainText ), m_exportAsMenu );
    m_exportAsMenu->addAction( m_exportAsText );
    m_exportAs->setEnabled( false );
    m_exportAsText->setEnabled( false );
    m_exportAsDocArchive = actionForExportFormat( Okular::ExportFormat(
            i18nc( "A document format, Okular-specific", "Document Archive" ),
            KMimeType::mimeType( "application/vnd.kde.okular-archive" ) ), m_exportAsMenu );
    m_exportAsMenu->addAction( m_exportAsDocArchive );
    m_exportAsDocArchive->setEnabled( false );

    m_showPresentation = ac->addAction("presentation");
    m_showPresentation->setText(i18n("P&resentation"));
    m_showPresentation->setIcon( KIcon( "view-presentation" ) );
    connect(m_showPresentation, SIGNAL(triggered()), this, SLOT(slotShowPresentation()));
    m_showPresentation->setShortcut( QKeySequence( Qt::CTRL + Qt::SHIFT + Qt::Key_P ) );
    m_showPresentation->setEnabled( false );

    QAction * importPS = ac->addAction("import_ps");
    importPS->setText(i18n("&Import PostScript as PDF..."));
    importPS->setIcon(KIcon("document-import"));
    connect(importPS, SIGNAL(triggered()), this, SLOT(slotImportPSFile()));
#if 0
    QAction * ghns = ac->addAction("get_new_stuff");
    ghns->setText(i18n("&Get Books From Internet..."));
    ghns->setIcon(KIcon("get-hot-new-stuff"));
    connect(ghns, SIGNAL(triggered()), this, SLOT(slotGetNewStuff()));
    // TEMP, REMOVE ME!
    ghns->setShortcut( Qt::Key_G );
#endif

    KToggleAction *blackscreenAction = new KToggleAction( i18n( "Switch Blackscreen Mode" ), ac );
    ac->addAction( "switch_blackscreen_mode", blackscreenAction );
    blackscreenAction->setShortcut( QKeySequence( Qt::Key_B ) );
    blackscreenAction->setIcon( KIcon( "view-presentation" ) );
    blackscreenAction->setEnabled( false );

    KToggleAction *drawingAction = new KToggleAction( i18n( "Toggle Drawing Mode" ), ac );
    ac->addAction( "presentation_drawing_mode", drawingAction );
    drawingAction->setIcon( KIcon( "draw-freehand" ) );
    drawingAction->setEnabled( false );

    KAction *eraseDrawingAction = new KAction( i18n( "Erase Drawings" ), ac );
    ac->addAction( "presentation_erase_drawings", eraseDrawingAction );
    eraseDrawingAction->setIcon( KIcon( "draw-eraser" ) );
    eraseDrawingAction->setEnabled( false );

    KAction *configureAnnotations = new KAction( i18n( "Configure Annotations..." ), ac );
    ac->addAction( "options_configure_annotations", configureAnnotations );
    configureAnnotations->setIcon( KIcon( "configure" ) );
    connect(configureAnnotations, SIGNAL(triggered()), this, SLOT(slotAnnotationPreferences()));

    KAction *playPauseAction = new KAction( i18n( "Play/Pause Presentation" ), ac );
    ac->addAction( "presentation_play_pause", playPauseAction );
    playPauseAction->setEnabled( false );
}

Part::~Part()
{
    GuiUtils::removeIconLoader( iconLoader() );
    m_document->removeObserver( this );

    if ( m_document->isOpened() )
        Part::closeUrl( false );

    delete m_toc;
    delete m_layers;
    delete m_pageView;
    delete m_thumbnailList;
    delete m_miniBar;
    delete m_pageNumberTool;
    delete m_miniBarLogic;
    delete m_bottomBar;
#ifdef OKULAR_ENABLE_MINIBAR
    delete m_progressWidget;
#endif
    delete m_pageSizeLabel;
    delete m_reviewsWidget;
    delete m_bookmarkList;
    delete m_infoTimer;

    delete m_document;

    delete m_tempfile;

    qDeleteAll( m_bookmarkActions );

    delete m_exportAsMenu;

#ifdef OKULAR_KEEP_FILE_OPEN
    delete m_keeper;
#endif
}


bool Part::openDocument(const KUrl& url, uint page)
{
    Okular::DocumentViewport vp( page - 1 );
    vp.rePos.enabled = true;
    vp.rePos.normalizedX = 0;
    vp.rePos.normalizedY = 0;
    vp.rePos.pos = Okular::DocumentViewport::TopLeft;
    if ( vp.isValid() )
        m_document->setNextDocumentViewport( vp );
    return openUrl( url );
}


void Part::startPresentation()
{
    m_cliPresentation = true;
}


QStringList Part::supportedMimeTypes() const
{
    return m_document->supportedMimeTypes();
}


KUrl Part::realUrl() const
{
    if ( !m_realUrl.isEmpty() )
        return m_realUrl;

    return url();
}

// ViewerInterface

void Part::showSourceLocation(const QString& fileName, int line, int, bool showGraphically)
{
    const QString u = QString( "src:%1 %2" ).arg( line + 1 ).arg( fileName );
    GotoAction action( QString(), u );
    m_document->processAction( &action );
    if( showGraphically )
    {
        m_pageView->setLastSourceLocationViewport( m_document->viewport() );
    }
}

void Part::clearLastShownSourceLocation()
{
    m_pageView->clearLastSourceLocationViewport();
}

bool Part::isWatchFileModeEnabled() const
{
    return !m_watcher->isStopped();
}

void Part::setWatchFileModeEnabled(bool enabled)
{
    if ( enabled && m_watcher->isStopped() )
    {
        m_watcher->startScan();
    }
    else if( !enabled && !m_watcher->isStopped() )
    {
        m_dirtyHandler->stop();
        m_watcher->stopScan();
    }
}

bool Part::areSourceLocationsShownGraphically() const
{
    return m_pageView->areSourceLocationsShownGraphically();
}

void Part::setShowSourceLocationsGraphically(bool show)
{
    m_pageView->setShowSourceLocationsGraphically(show);
}

bool Part::openNewFilesInTabs() const
{
    return Okular::Settings::self()->shellOpenFileInTabs();
}

void Part::slotHandleActivatedSourceReference(const QString& absFileName, int line, int col, bool *handled)
{
    emit openSourceReference( absFileName, line, col );
    if ( m_embedMode == Okular::ViewerWidgetMode )
    {
        *handled = true;
    }
}

void Part::openUrlFromDocument(const KUrl &url)
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    if (KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, widget()))
    {
        m_bExtension->openUrlNotify();
        m_bExtension->setLocationBarUrl(url.prettyUrl());
        openUrl(url);
    } else {
        KMessageBox::error( widget(), i18n("Could not open '%1'. File does not exist", url.pathOrUrl() ) );
    }
}

void Part::openUrlFromBookmarks(const KUrl &_url)
{
    KUrl url = _url;
    Okular::DocumentViewport vp( _url.htmlRef() );
    if ( vp.isValid() )
        m_document->setNextDocumentViewport( vp );
    url.setHTMLRef( QString() );
    if ( m_document->currentDocument() == url )
    {
        if ( vp.isValid() )
            m_document->setViewport( vp );
    }
    else
        openUrl( url );
}

void Part::handleDroppedUrls( const KUrl::List& urls )
{
    if ( urls.isEmpty() )
        return;

    if ( m_embedMode != NativeShellMode || !openNewFilesInTabs() )
    {
        openUrlFromDocument( urls.first() );
        return;
    }

    emit urlsDropped( urls );
}

void Part::slotJobStarted(KIO::Job *job)
{
    if (job)
    {
        QStringList supportedMimeTypes = m_document->supportedMimeTypes();
        job->addMetaData("accept", supportedMimeTypes.join(", ") + ", */*;q=0.5");

        connect(job, SIGNAL(result(KJob*)), this, SLOT(slotJobFinished(KJob*)));
    }
}

void Part::slotJobFinished(KJob *job)
{
    if ( job->error() == KIO::ERR_USER_CANCELED )
    {
        m_pageView->displayMessage( i18n( "The loading of %1 has been canceled.", realUrl().pathOrUrl() ) );
    }
}

void Part::loadCancelled(const QString &reason)
{
    emit setWindowCaption( QString() );
    resetStartArguments();

    // when m_viewportDirty.pageNumber != -1 we come from slotDoFileDirty
    // so we don't want to show an ugly messagebox just because the document is
    // taking more than usual to be recreated
    if (m_viewportDirty.pageNumber == -1)
    {
        if (!reason.isEmpty())
        {
            KMessageBox::error( widget(), i18n("Could not open %1. Reason: %2", url().prettyUrl(), reason ) );
        }
    }
}

void Part::setWindowTitleFromDocument()
{
    // If 'DocumentTitle' should be used, check if the document has one. If
    // either case is false, use the file name.
    QString title = Okular::Settings::displayDocumentNameOrPath() == Okular::Settings::EnumDisplayDocumentNameOrPath::Path ? realUrl().pathOrUrl() : realUrl().fileName();

    if ( Okular::Settings::displayDocumentTitle() )
    {
        const QString docTitle = m_document->metaData( "DocumentTitle" ).toString();
        if ( !docTitle.isEmpty() && !docTitle.trimmed().isEmpty() )
        {
             title = docTitle;
        }
    }

    emit setWindowCaption( title );
}

KConfigDialog * Part::slotGeneratorPreferences( )
{
    // Create dialog
    KConfigDialog * dialog = new KConfigDialog( m_pageView, "generator_prefs", Okular::Settings::self() );
    dialog->setAttribute( Qt::WA_DeleteOnClose );

    if( m_embedMode == ViewerWidgetMode )
    {
        dialog->setCaption( i18n( "Configure Viewer Backends" ) );
    }
    else
    {
        dialog->setCaption( i18n( "Configure Backends" ) );
    }

    m_document->fillConfigDialog( dialog );

    // Show it
    dialog->setWindowModality( Qt::ApplicationModal );
    dialog->show();

    return dialog;
}


void Part::notifySetup( const QVector< Okular::Page * > & /*pages*/, int setupFlags )
{
    if ( !( setupFlags & Okular::DocumentObserver::DocumentChanged ) )
        return;

    rebuildBookmarkMenu();
    updateAboutBackendAction();
    m_findBar->resetSearch();
    m_searchWidget->setEnabled( m_document->supportsSearching() );
}

void Part::notifyViewportChanged( bool /*smoothMove*/ )
{
    updateViewActions();
}

void Part::notifyPageChanged( int page, int flags )
{
    if ( flags & Okular::DocumentObserver::NeedSaveAs )
        setModified();

    if ( !(flags & Okular::DocumentObserver::Bookmark ) )
        return;

    rebuildBookmarkMenu();
    if ( page == m_document->viewport().pageNumber )
        updateBookmarksActions();
}


void Part::goToPage(uint i)
{
    if ( i <= m_document->pages() )
        m_document->setViewportPage( i - 1 );
}


void Part::openDocument( const QString &doc )
{
    openUrl( KUrl( doc ) );
}


uint Part::pages()
{
    return m_document->pages();
}


uint Part::currentPage()
{
    return m_document->pages() ? m_document->currentPage() + 1 : 0;
}


QString Part::currentDocument()
{
    return m_document->currentDocument().pathOrUrl();
}


QString Part::documentMetaData( const QString &metaData ) const
{
    const Okular::DocumentInfo info = m_document->documentInfo();
    return info.get( metaData );
}


bool Part::slotImportPSFile()
{
    QString app = KStandardDirs::findExe( "ps2pdf" );
    if ( app.isEmpty() )
    {
        // TODO point the user to their distro packages?
        KMessageBox::error( widget(), i18n( "The program \"ps2pdf\" was not found, so Okular can not import PS files using it." ), i18n("ps2pdf not found") );
        return false;
    }

    KUrl url = KFileDialog::getOpenUrl( KUrl(), "application/postscript", this->widget() );
    if ( url.isLocalFile() )
    {
        KTemporaryFile tf;
        tf.setSuffix( ".pdf" );
        tf.setAutoRemove( false );
        if ( !tf.open() )
            return false;
        m_temporaryLocalFile = tf.fileName();
        tf.close();

        setLocalFilePath( url.toLocalFile() );
        QStringList args;
        QProcess *p = new QProcess();
        args << url.toLocalFile() << m_temporaryLocalFile;
        m_pageView->displayMessage(i18n("Importing PS file as PDF (this may take a while)..."));
        connect(p, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(psTransformEnded(int,QProcess::ExitStatus)));
        p->start(app, args);
        return true;
    }

    m_temporaryLocalFile.clear();
    return false;
}

static void addFileToWatcher( KDirWatch *watcher, const QString &filePath)
{
    if ( !watcher->contains( filePath ) ) watcher->addFile(filePath);
    const QFileInfo fi(filePath);
    if ( !watcher->contains( fi.absolutePath() ) ) watcher->addDir(fi.absolutePath());
    if ( fi.isSymLink() ) watcher->addFile( fi.readLink() );
}

Document::OpenResult Part::doOpenFile( const KMimeType::Ptr &mimeA, const QString &fileNameToOpenA, bool *isCompressedFile )
{
    Document::OpenResult openResult = Document::OpenError;
    bool uncompressOk = true;
    KMimeType::Ptr mime = mimeA;
    QString fileNameToOpen = fileNameToOpenA;
    QString compressedMime = compressedMimeFor( mime->name() );
    if ( compressedMime.isEmpty() )
    {
        for (QString mimeType: mime->parentMimeTypes())
        {
            compressedMime = compressedMimeFor( mimeType );
            if ( !compressedMime.isEmpty() )
                break;
        }
    }
    if ( !compressedMime.isEmpty() )
    {
        *isCompressedFile = true;
        uncompressOk = handleCompressed( fileNameToOpen, localFilePath(), compressedMime );
        mime = KMimeType::findByPath( fileNameToOpen );
    }
    else
    {
        *isCompressedFile = false;
    }

    isDocumentArchive = false;
    if ( uncompressOk )
    {
        if ( mime->is( "application/vnd.kde.okular-archive" ) )
        {
            openResult = m_document->openDocumentArchive( fileNameToOpen,  url() );
            isDocumentArchive = true;
        }
        else
        {
            openResult = m_document->openDocument( fileNameToOpen,  url(), mime );
        }

        // if the file didn't open correctly it might be encrypted, so ask for a pass
        QString walletName, walletFolder, walletKey;
        m_document->walletDataForFile(fileNameToOpen, &walletName, &walletFolder, &walletKey);
        bool firstInput = true;
        bool triedWallet = false;
        KWallet::Wallet * wallet = 0;
        bool keep = true;
        while ( openResult == Document::OpenNeedsPassword )
        {
            QString password;

            // 1.A. try to retrieve the first password from the kde wallet system
            if ( !triedWallet && !walletKey.isNull() )
            {
                const WId parentwid = widget()->effectiveWinId();
                wallet = KWallet::Wallet::openWallet( walletName, parentwid );
                if ( wallet )
                {
                    // use the KPdf folder (and create if missing)
                    if ( !wallet->hasFolder( walletFolder ) )
                        wallet->createFolder( walletFolder );
                    wallet->setFolder( walletFolder );

                    // look for the pass in that folder
                    QString retrievedPass;
                    if ( !wallet->readPassword( walletKey, retrievedPass ) )
                        password = retrievedPass;
                }
                triedWallet = true;
            }

            // 1.B. if not retrieved, ask the password using the kde password dialog
            if ( password.isNull() )
            {
                QString prompt;
                if ( firstInput )
                    prompt = i18n( "Please enter the password to read the document:" );
                else
                    prompt = i18n( "Incorrect password. Try again:" );
                firstInput = false;

                // if the user presses cancel, abort opening
                KPasswordDialog dlg( widget(), wallet ? KPasswordDialog::ShowKeepPassword : KPasswordDialog::KPasswordDialogFlags() );
                dlg.setCaption( i18n( "Document Password" ) );
                dlg.setPrompt( prompt );
                if( !dlg.exec() )
                    break;
                password = dlg.password();
                if ( wallet )
                    keep = dlg.keepPassword();
            }

            // 2. reopen the document using the password
            if ( mime->is( "application/vnd.kde.okular-archive" ) )
            {
                openResult = m_document->openDocumentArchive( fileNameToOpen,  url(), password );
                isDocumentArchive = true;
            }
            else
            {
                openResult = m_document->openDocument( fileNameToOpen,  url(), mime, password );
            }

            // 3. if the password is correct and the user chose to remember it, store it to the wallet
            if ( openResult == Document::OpenSuccess && wallet && /*safety check*/ wallet->isOpen() && keep )
            {
                wallet->writePassword( walletKey, password );
            }
        }
    }

    return openResult;
}

bool Part::openFile()
{
    QList<KMimeType::Ptr> mimes;
    QString fileNameToOpen = localFilePath();
    const bool isstdin = url().isLocalFile() && url().fileName( KUrl::ObeyTrailingSlash ) == QLatin1String( "-" );
    const QFileInfo fileInfo( fileNameToOpen );
    if ( !isstdin && !fileInfo.exists() )
        return false;
    KMimeType::Ptr pathMime = KMimeType::findByPath( fileNameToOpen );
    const QString argMimeType = arguments().mimeType();

    if ( !argMimeType.isEmpty() )
    {
        KMimeType::Ptr argMime = KMimeType::mimeType( argMimeType );

        // Select the "childmost" mimetype, if none of them
        // inherits the other trust more what pathMime says
        // but still do a second try if that one fails
        if ( argMime && argMime->is( pathMime->name() ) )
        {
            mimes << argMime;
        }
        else if ( !argMime || pathMime->is( argMime->name() ) )
        {
            mimes << pathMime;
        }
        else
        {
            mimes << pathMime << argMime;
        }

        if (mimes[0]->name() == "text/plain") {
            KMimeType::Ptr contentMime = KMimeType::findByFileContent( fileNameToOpen );
            mimes.prepend( contentMime );
        }
    }
    else
    {
        mimes << pathMime;
    }

    KMimeType::Ptr mime;
    Document::OpenResult openResult = Document::OpenError;
    bool isCompressedFile = false;
    while ( !mimes.isEmpty() && openResult == Document::OpenError ) {
        mime = mimes.takeFirst();
        openResult = doOpenFile( mime, fileNameToOpen, &isCompressedFile );
    }

    bool canSearch = m_document->supportsSearching();
    emit mimeTypeChanged( mime );

    // update one-time actions
    const bool ok = openResult == Document::OpenSuccess;
    emit enableCloseAction( ok );
    m_find->setEnabled( ok && canSearch );
    m_findNext->setEnabled( ok && canSearch );
    m_findPrev->setEnabled( ok && canSearch );
    if( m_saveAs ) m_saveAs->setEnabled( ok && (m_document->canSaveChanges() || isDocumentArchive) );
    if( m_saveCopyAs ) m_saveCopyAs->setEnabled( ok );
    emit enablePrintAction( ok && m_document->printingSupport() != Okular::Document::NoPrinting );
    m_printPreview->setEnabled( ok && m_document->printingSupport() != Okular::Document::NoPrinting );
    m_showProperties->setEnabled( ok );
    bool hasEmbeddedFiles = ok && m_document->embeddedFiles() && m_document->embeddedFiles()->count() > 0;
    if ( m_showEmbeddedFiles ) m_showEmbeddedFiles->setEnabled( hasEmbeddedFiles );
    m_topMessage->setVisible( hasEmbeddedFiles && Okular::Settings::showOSD() );

    // Warn the user that XFA forms are not supported yet (NOTE: poppler generator only)
    if ( ok && m_document->metaData( "HasUnsupportedXfaForm" ).toBool() == true )
    {
        m_formsMessage->setText( i18n( "This document has XFA forms, which are currently <b>unsupported</b>." ) );
        m_formsMessage->setIcon( KIcon( "dialog-warning" ) );
        m_formsMessage->setMessageType( KMessageWidget::Warning );
        m_formsMessage->setVisible( true );
    }
    // m_pageView->toggleFormsAction() may be null on dummy mode
    else if ( ok && m_pageView->toggleFormsAction() && m_pageView->toggleFormsAction()->isEnabled() )
    {
        m_formsMessage->setText( i18n( "This document has forms. Click on the button to interact with them, or use View -> Show Forms." ) );
        m_formsMessage->setMessageType( KMessageWidget::Information );
        m_formsMessage->setVisible( true );
    }
    else
    {
        m_formsMessage->setVisible( false );
    }

    if ( m_showPresentation ) m_showPresentation->setEnabled( ok );
    if ( ok )
    {
        if ( m_exportAs )
        {
            m_exportFormats = m_document->exportFormats();
            QList<Okular::ExportFormat>::ConstIterator it = m_exportFormats.constBegin();
            QList<Okular::ExportFormat>::ConstIterator itEnd = m_exportFormats.constEnd();
            QMenu *menu = m_exportAs->menu();
            for ( ; it != itEnd; ++it )
            {
                menu->addAction( actionForExportFormat( *it ) );
            }
        }
        if ( isCompressedFile )
        {
            m_realUrl = url();
        }
#ifdef OKULAR_KEEP_FILE_OPEN
        if ( keepFileOpen() )
            m_keeper->open( fileNameToOpen );
#endif
    }
    if ( m_exportAsText ) m_exportAsText->setEnabled( ok && m_document->canExportToText() );
    if ( m_exportAsDocArchive ) m_exportAsDocArchive->setEnabled( ok );
    if ( m_exportAs ) m_exportAs->setEnabled( ok );

    // update viewing actions
    updateViewActions();

    m_fileWasRemoved = false;

    if ( !ok )
    {
        // if can't open document, update windows so they display blank contents
        m_pageView->viewport()->update();
        m_thumbnailList->update();
        setUrl( KUrl() );
        return false;
    }

    // set the file to the fileWatcher
    if ( url().isLocalFile() )
    {
        addFileToWatcher( m_watcher, localFilePath() );
    }

    // if the 'OpenTOC' flag is set, open the TOC
    if ( m_document->metaData( "OpenTOC" ).toBool() && m_sidebar->isItemEnabled( m_toc ) && !m_sidebar->isCollapsed() && m_sidebar->currentItem() != m_toc )
    {
        m_sidebar->setCurrentItem( m_toc, Sidebar::DoNotUncollapseIfCollapsed );
    }
    // if the 'StartFullScreen' flag is set, or the command line flag was
    // specified, start presentation
    if ( m_document->metaData( "StartFullScreen" ).toBool() || m_cliPresentation )
    {
        bool goAheadWithPresentationMode = true;
        if ( !m_cliPresentation )
        {
            const QString text = i18n( "The document requested to be launched in presentation mode.\n"
                                       "Do you want to allow it?" );
            const QString caption = i18n( "Presentation Mode" );
            const KGuiItem yesItem = KGuiItem( i18n( "Allow" ), "dialog-ok", i18n( "Allow the presentation mode" ) );
            const KGuiItem noItem = KGuiItem( i18n( "Do Not Allow" ), "process-stop", i18n( "Do not allow the presentation mode" ) );
            const int result = KMessageBox::questionYesNo( widget(), text, caption, yesItem, noItem );
            if ( result == KMessageBox::No )
                goAheadWithPresentationMode = false;
        }
        m_cliPresentation = false;
        if ( goAheadWithPresentationMode )
            QMetaObject::invokeMethod( this, "slotShowPresentation", Qt::QueuedConnection );
    }
    m_generatorGuiClient = factory() ? m_document->guiClient() : 0;
    if ( m_generatorGuiClient )
        factory()->addClient( m_generatorGuiClient );
    if ( m_cliPrint )
    {
        m_cliPrint = false;
        slotPrint();
    }
    return true;
}

bool Part::openUrl(const KUrl &_url)
{
    // Close current document if any
    if ( !closeUrl() )
        return false;

    KUrl url( _url );
    if ( url.hasHTMLRef() )
    {
        const QString dest = url.htmlRef();
        bool ok = true;
        const int page = dest.toInt( &ok );
        if ( ok )
        {
            Okular::DocumentViewport vp( page - 1 );
            vp.rePos.enabled = true;
            vp.rePos.normalizedX = 0;
            vp.rePos.normalizedY = 0;
            vp.rePos.pos = Okular::DocumentViewport::TopLeft;
            m_document->setNextDocumentViewport( vp );
        }
        else
        {
            m_document->setNextDocumentDestination( dest );
        }
        url.setHTMLRef( QString() );
    }

    // this calls in sequence the 'closeUrl' and 'openFile' methods
    bool openOk = KParts::ReadWritePart::openUrl( url );

    if ( openOk )
    {
        m_viewportDirty.pageNumber = -1;

        setWindowTitleFromDocument();
    }
    else
    {
        resetStartArguments();
        KMessageBox::error( widget(), i18n("Could not open %1", url.pathOrUrl() ) );
    }

    return openOk;
}

bool Part::queryClose()
{
    if ( !isReadWrite() || !isModified() )
        return true;

    const int res = KMessageBox::warningYesNoCancel( widget(),
                        i18n( "Do you want to save your annotation changes or discard them?" ),
                        i18n( "Close Document" ),
                        KStandardGuiItem::saveAs(),
                        KStandardGuiItem::discard() );

    switch ( res )
    {
        case KMessageBox::Yes: // Save as
            slotSaveFileAs();
            return !isModified(); // Only allow closing if file was really saved
        case KMessageBox::No: // Discard
            return true;
        default: // Cancel
            return false;
    }
}

bool Part::closeUrl(bool promptToSave)
{
    if ( promptToSave && !queryClose() )
        return false;

    setModified( false );

    if (!m_temporaryLocalFile.isNull() && m_temporaryLocalFile != localFilePath())
    {
        QFile::remove( m_temporaryLocalFile );
        m_temporaryLocalFile.clear();
    }

    slotHidePresentation();
    emit enableCloseAction( false );
    m_find->setEnabled( false );
    m_findNext->setEnabled( false );
    m_findPrev->setEnabled( false );
    if( m_saveAs )  m_saveAs->setEnabled( false );
    if( m_saveCopyAs ) m_saveCopyAs->setEnabled( false );
    m_printPreview->setEnabled( false );
    m_showProperties->setEnabled( false );
    if ( m_showEmbeddedFiles ) m_showEmbeddedFiles->setEnabled( false );
    if ( m_exportAs ) m_exportAs->setEnabled( false );
    if ( m_exportAsText ) m_exportAsText->setEnabled( false );
    if ( m_exportAsDocArchive ) m_exportAsDocArchive->setEnabled( false );
    m_exportFormats.clear();
    if ( m_exportAs )
    {
        QMenu *menu = m_exportAs->menu();
        QList<QAction*> acts = menu->actions();
        int num = acts.count();
        for ( int i = 2; i < num; ++i )
        {
            menu->removeAction( acts.at(i) );
            delete acts.at(i);
        }
    }
    if ( m_showPresentation ) m_showPresentation->setEnabled( false );
    emit setWindowCaption("");
    emit enablePrintAction(false);
    m_realUrl = KUrl();
    if ( url().isLocalFile() )
    {
        m_watcher->removeFile( localFilePath() );
        QFileInfo fi(localFilePath());
        m_watcher->removeDir( fi.absolutePath() );
        if ( fi.isSymLink() ) m_watcher->removeFile( fi.readLink() );
    }
    m_fileWasRemoved = false;
    if ( m_generatorGuiClient )
        factory()->removeClient( m_generatorGuiClient );
    m_generatorGuiClient = 0;
    m_document->closeDocument();
    updateViewActions();
    delete m_tempfile;
    m_tempfile = 0;
    if ( widget() )
    {
        m_searchWidget->clearText();
        m_topMessage->setVisible( false );
        m_formsMessage->setVisible( false );
    }
#ifdef OKULAR_KEEP_FILE_OPEN
    m_keeper->close();
#endif
    bool r = KParts::ReadWritePart::closeUrl();
    setUrl(KUrl());

    return r;
}

bool Part::closeUrl()
{
    return closeUrl( true );
}

void Part::guiActivateEvent(KParts::GUIActivateEvent *event)
{
    updateViewActions();

    KParts::ReadWritePart::guiActivateEvent(event);

    setWindowTitleFromDocument();
}

void Part::close()
{
    if ( m_embedMode == NativeShellMode )
    {
        closeUrl();
    }
    else KMessageBox::information( widget(), i18n( "This link points to a close document action that does not work when using the embedded viewer." ), QString(), "warnNoCloseIfNotInOkular" );
}


void Part::cannotQuit()
{
    KMessageBox::information( widget(), i18n( "This link points to a quit application action that does not work when using the embedded viewer." ), QString(), "warnNoQuitIfNotInOkular" );
}


void Part::slotShowLeftPanel()
{
    bool showLeft = m_showLeftPanel->isChecked();
    Okular::Settings::setShowLeftPanel( showLeft );
    Okular::Settings::self()->writeConfig();
    // show/hide left panel
    m_sidebar->setSidebarVisibility( showLeft );
}

void Part::slotShowBottomBar()
{
    const bool showBottom = m_showBottomBar->isChecked();
    Okular::Settings::setShowBottomBar( showBottom );
    Okular::Settings::self()->writeConfig();
    // show/hide bottom bar
    m_bottomBar->setVisible( showBottom );
}

void Part::slotFileDirty( const QString& path )
{
    // The beauty of this is that each start cancels the previous one.
    // This means that timeout() is only fired when there have
    // no changes to the file for the last 750 milisecs.
    // This ensures that we don't update on every other byte that gets
    // written to the file.
    if ( path == localFilePath() )
    {
        // Only start watching the file in case if it wasn't removed
        if (QFile::exists(localFilePath()))
            m_dirtyHandler->start( 750 );
        else
            m_fileWasRemoved = true;
    }
    else
    {
        const QFileInfo fi(localFilePath());
        if ( fi.absolutePath() == path )
        {
            // Our parent has been dirtified
            if (!QFile::exists(localFilePath()))
            {
                m_fileWasRemoved = true;
            }
            else if (m_fileWasRemoved && QFile::exists(localFilePath()))
            {
                // we need to watch the new file
                m_watcher->removeFile(localFilePath());
                m_watcher->addFile(localFilePath());
                m_dirtyHandler->start( 750 );
            }
        }
        else if ( fi.isSymLink() && fi.readLink() == path )
        {
            if ( QFile::exists( fi.readLink() ))
                m_dirtyHandler->start( 750 );
            else
                m_fileWasRemoved = true;
        }
    }
}


void Part::slotDoFileDirty()
{
    bool tocReloadPrepared = false;
    
    // do the following the first time the file is reloaded
    if ( m_viewportDirty.pageNumber == -1 )
    {
        // store the url of the current document
        m_oldUrl = url();

        // store the current viewport
        m_viewportDirty = m_document->viewport();

        // store the current toolbox pane
        m_dirtyToolboxItem = m_sidebar->currentItem();
        m_wasSidebarVisible = m_sidebar->isSidebarVisible();
        m_wasSidebarCollapsed = m_sidebar->isCollapsed();

        // store if presentation view was open
        m_wasPresentationOpen = ((PresentationWidget*)m_presentationWidget != 0);
        
        // preserves the toc state after reload
        m_toc->prepareForReload();
        tocReloadPrepared = true;

        // store the page rotation
        m_dirtyPageRotation = m_document->rotation();

        // inform the user about the operation in progress
        // TODO: Remove this line and integrate reload info in queryClose
        m_pageView->displayMessage( i18n("Reloading the document...") );
    }

    // close and (try to) reopen the document
    if ( !closeUrl() )
    {
        m_viewportDirty.pageNumber = -1;

        if ( tocReloadPrepared ) 
        {
            m_toc->rollbackReload();
        }
        return;
    }
    
    if ( tocReloadPrepared )
        m_toc->finishReload();

    // inform the user about the operation in progress
    m_pageView->displayMessage( i18n("Reloading the document...") );

    if ( KParts::ReadWritePart::openUrl( m_oldUrl ) )
    {
        // on successful opening, restore the previous viewport
        if ( m_viewportDirty.pageNumber >= (int) m_document->pages() )
            m_viewportDirty.pageNumber = (int) m_document->pages() - 1;
        m_document->setViewport( m_viewportDirty );
        m_oldUrl = KUrl();
        m_viewportDirty.pageNumber = -1;
        m_document->setRotation( m_dirtyPageRotation );
        if ( m_sidebar->currentItem() != m_dirtyToolboxItem && m_sidebar->isItemEnabled( m_dirtyToolboxItem )
            && !m_sidebar->isCollapsed() )
        {
            m_sidebar->setCurrentItem( m_dirtyToolboxItem );
        }
        if ( m_sidebar->isSidebarVisible() != m_wasSidebarVisible )
        {
            m_sidebar->setSidebarVisibility( m_wasSidebarVisible );
        }
        if ( m_sidebar->isCollapsed() != m_wasSidebarCollapsed )
        {
            m_sidebar->setCollapsed( m_wasSidebarCollapsed );
        }
        if (m_wasPresentationOpen) slotShowPresentation();
        emit enablePrintAction(true && m_document->printingSupport() != Okular::Document::NoPrinting);
    }
    else
    {
        // start watching the file again (since we dropped it on close) 
        addFileToWatcher( m_watcher, localFilePath() );
        m_dirtyHandler->start( 750 );
    }
}


void Part::updateViewActions()
{
    bool opened = m_document->pages() > 0;
    if ( opened )
    {
        m_gotoPage->setEnabled( m_document->pages() > 1 );
        
        // Check if you are at the beginning or not
        if (m_document->currentPage() != 0)
        {
            m_beginningOfDocument->setEnabled( true );
            m_prevPage->setEnabled( true );
        }
        else 
        {
            if (m_pageView->verticalScrollBar()->value() != 0)
            {
                // The page isn't at the very beginning
                m_beginningOfDocument->setEnabled( true );
            }
            else
            {
                // The page is at the very beginning of the document
                m_beginningOfDocument->setEnabled( false );
            }
            // The document is at the first page, you can go to a page before
            m_prevPage->setEnabled( false );
        }
        
        if (m_document->pages() == m_document->currentPage() + 1 )
        {
            // If you are at the end, disable go to next page
            m_nextPage->setEnabled( false );
            if (m_pageView->verticalScrollBar()->value() == m_pageView->verticalScrollBar()->maximum())
            {
                // If you are the end of the page of the last document, you can't go to the last page
                m_endOfDocument->setEnabled( false );
            }
            else 
            {
                // Otherwise you can move to the endif
                m_endOfDocument->setEnabled( true );
            }
        }
        else 
        {
            // If you are not at the end, enable go to next page
            m_nextPage->setEnabled( true );
            m_endOfDocument->setEnabled( true );
        }

        if (m_historyBack) m_historyBack->setEnabled( !m_document->historyAtBegin() );
        if (m_historyNext) m_historyNext->setEnabled( !m_document->historyAtEnd() );
        m_reload->setEnabled( true );
        if (m_copy) m_copy->setEnabled( true );
        if (m_selectAll) m_selectAll->setEnabled( true );
    }
    else
    {
        m_gotoPage->setEnabled( false );
        m_beginningOfDocument->setEnabled( false );
        m_endOfDocument->setEnabled( false );
        m_prevPage->setEnabled( false );
        m_nextPage->setEnabled( false );
        if (m_historyBack) m_historyBack->setEnabled( false );
        if (m_historyNext) m_historyNext->setEnabled( false );
        m_reload->setEnabled( false );
        if (m_copy) m_copy->setEnabled( false );
        if (m_selectAll) m_selectAll->setEnabled( false );
    }

    if ( factory() )
    {
        QWidget *menu = factory()->container("menu_okular_part_viewer", this);
        if (menu) menu->setEnabled( opened );

        menu = factory()->container("view_orientation", this);
        if (menu) menu->setEnabled( opened );
    }
    emit viewerMenuStateChange( opened );

    updateBookmarksActions();
}


void Part::updateBookmarksActions()
{
    bool opened = m_document->pages() > 0;
    if ( opened )
    {
        m_addBookmark->setEnabled( true );
        if ( m_document->bookmarkManager()->isBookmarked( m_document->viewport() ) )
        {
            m_addBookmark->setText( i18n( "Remove Bookmark" ) );
            m_addBookmark->setIcon( KIcon( "edit-delete-bookmark" ) );
            m_renameBookmark->setEnabled( true );
        }
        else
        {
            m_addBookmark->setText( m_addBookmarkText );
            m_addBookmark->setIcon( m_addBookmarkIcon );
            m_renameBookmark->setEnabled( false );
        }
    }
    else
    {
        m_addBookmark->setEnabled( false );
        m_addBookmark->setText( m_addBookmarkText );
        m_addBookmark->setIcon( m_addBookmarkIcon );
        m_renameBookmark->setEnabled( false );
    }
}


void Part::enableTOC(bool enable)
{
    m_sidebar->setItemEnabled(m_toc, enable);

    // If present, show the TOC when a document is opened
    if ( enable && m_sidebar->currentItem() != m_toc )
    {
        m_sidebar->setCurrentItem( m_toc, Sidebar::DoNotUncollapseIfCollapsed );
    }
}

void Part::slotRebuildBookmarkMenu()
{
    rebuildBookmarkMenu();
}

void Part::enableLayers(bool enable)
{
    m_sidebar->setItemVisible( m_layers, enable );
}

void Part::slotShowFindBar()
{
    m_findBar->show();
    m_findBar->focusAndSetCursor();
    m_closeFindBar->setEnabled( true );
}

void Part::slotHideFindBar()
{
    if ( m_findBar->maybeHide() )
    {
        m_pageView->setFocus();
        m_closeFindBar->setEnabled( false );
    }
}

//BEGIN go to page dialog
class GotoPageDialog : public KDialog
{
    public:
        GotoPageDialog(QWidget *p, int current, int max) : KDialog(p)
        {
            setCaption(i18n("Go to Page"));
            setButtons(Ok | Cancel);
            setDefaultButton(Ok);

            QWidget *w = new QWidget(this);
            setMainWidget(w);

            QVBoxLayout *topLayout = new QVBoxLayout(w);
            topLayout->setMargin(0);
            topLayout->setSpacing(spacingHint());
            e1 = new KIntNumInput(current, w);
            e1->setRange(1, max);
            e1->setEditFocus(true);
            e1->setSliderEnabled(true);

            QLabel *label = new QLabel(i18n("&Page:"), w);
            label->setBuddy(e1);
            topLayout->addWidget(label);
            topLayout->addWidget(e1);
                                 // A little bit extra space
            topLayout->addSpacing(spacingHint());
            topLayout->addStretch(10);
            e1->setFocus();
        }

        int getPage() const
        {
            return e1->value();
        }

    protected:
        KIntNumInput *e1;
};
//END go to page dialog

void Part::slotGoToPage()
{
    GotoPageDialog pageDialog( m_pageView, m_document->currentPage() + 1, m_document->pages() );
    if ( pageDialog.exec() == QDialog::Accepted )
        m_document->setViewportPage( pageDialog.getPage() - 1 );
}


void Part::slotPreviousPage()
{
    if ( m_document->isOpened() && !(m_document->currentPage() < 1) )
        m_document->setViewportPage( m_document->currentPage() - 1 );
}


void Part::slotNextPage()
{
    if ( m_document->isOpened() && m_document->currentPage() < (m_document->pages() - 1) )
        m_document->setViewportPage( m_document->currentPage() + 1 );
}


void Part::slotGotoFirst()
{
    if ( m_document->isOpened() ) {
        m_document->setViewportPage( 0 );
        m_beginningOfDocument->setEnabled( false );
    }
}


void Part::slotGotoLast()
{
    if ( m_document->isOpened() )
    {
        DocumentViewport endPage(m_document->pages() -1 );
        endPage.rePos.enabled = true;
        endPage.rePos.normalizedX = 0;
        endPage.rePos.normalizedY = 1;
        endPage.rePos.pos = Okular::DocumentViewport::TopLeft;
        m_document->setViewport(endPage);
        m_endOfDocument->setEnabled(false);
    }
}


void Part::slotHistoryBack()
{
    m_document->setPrevViewport();
}


void Part::slotHistoryNext()
{
    m_document->setNextViewport();
}


void Part::slotAddBookmark()
{
    DocumentViewport vp = m_document->viewport();
    if ( m_document->bookmarkManager()->isBookmarked( vp ) )
    {
        m_document->bookmarkManager()->removeBookmark( vp );
    }
    else
    {
        m_document->bookmarkManager()->addBookmark( vp );
    }
}

void Part::slotRenameBookmark( const DocumentViewport &viewport )
{
    Q_ASSERT(m_document->bookmarkManager()->isBookmarked( viewport ));
    if ( m_document->bookmarkManager()->isBookmarked( viewport ) )
    {
        KBookmark bookmark = m_document->bookmarkManager()->bookmark( viewport );
        const QString newName = KInputDialog::getText( i18n( "Rename Bookmark" ), i18n( "Enter the new name of the bookmark:" ), bookmark.fullText(), 0, widget());
        if (!newName.isEmpty())
        {
            m_document->bookmarkManager()->renameBookmark(&bookmark, newName);
        }
    }
}

void Part::slotRenameBookmarkFromMenu()
{
    QAction *action = dynamic_cast<QAction *>(sender());
    Q_ASSERT( action );
    if ( action )
    {
        DocumentViewport vp( action->data().toString() );
        slotRenameBookmark( vp );
    }
}

void Part::slotRenameCurrentViewportBookmark()
{
    slotRenameBookmark( m_document->viewport() );
}

void Part::slotAboutToShowContextMenu(KMenu * /*menu*/, QAction *action, QMenu *contextMenu)
{
    const QList<QAction*> actions = contextMenu->findChildren<QAction*>("OkularPrivateRenameBookmarkActions");
    foreach(QAction *a, actions)
    {
        contextMenu->removeAction(a);
        delete a;
    }

    KBookmarkAction *ba = dynamic_cast<KBookmarkAction*>(action);
    if (ba != NULL)
    {
        QAction *separatorAction = contextMenu->addSeparator();
        separatorAction->setObjectName("OkularPrivateRenameBookmarkActions");
        QAction *renameAction = contextMenu->addAction( KIcon( "edit-rename" ), i18n( "Rename this Bookmark" ), this, SLOT(slotRenameBookmarkFromMenu()) );
        renameAction->setData(ba->property("htmlRef").toString());
        renameAction->setObjectName("OkularPrivateRenameBookmarkActions");
    }
}

void Part::slotPreviousBookmark()
{
    const KBookmark bookmark = m_document->bookmarkManager()->previousBookmark( m_document->viewport() );

    if ( !bookmark.isNull() )
    {
        DocumentViewport vp( bookmark.url().htmlRef() );
        m_document->setViewport( vp );
    }
}


void Part::slotNextBookmark()
{
    const KBookmark bookmark = m_document->bookmarkManager()->nextBookmark( m_document->viewport() );

    if ( !bookmark.isNull() )
    {
        DocumentViewport vp( bookmark.url().htmlRef() );
        m_document->setViewport( vp );
    }
}


void Part::slotFind()
{
    // when in presentation mode, there's already a search bar, taking care of
    // the 'find' requests
    if ( (PresentationWidget*)m_presentationWidget != 0 )
    {
        m_presentationWidget->slotFind();
    }
    else
    {
        slotShowFindBar();
    }
}


void Part::slotFindNext()
{
    if (m_findBar->isHidden())
        slotShowFindBar();
    else
        m_findBar->findNext();
}


void Part::slotFindPrev()
{
    if (m_findBar->isHidden())
        slotShowFindBar();
    else
        m_findBar->findPrev();
}

bool Part::saveFile()
{
    kDebug() << "Okular part doesn't support saving the file in the location from which it was opened";
    return false;
}

void Part::slotSaveFileAs()
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    /* Show a warning before saving if the generator can't save annotations,
     * unless we are going to save a .okular archive. */
    if ( !isDocumentArchive && !m_document->canSaveChanges( Document::SaveAnnotationsCapability ) )
    {
        /* Search local annotations */
        bool containsLocalAnnotations = false;
        const int pagecount = m_document->pages();

        for ( int pageno = 0; pageno < pagecount; ++pageno )
        {
            const Okular::Page *page = m_document->page( pageno );
            foreach ( const Okular::Annotation *ann, page->annotations() )
            {
                if ( !(ann->flags() & Okular::Annotation::External) )
                {
                    containsLocalAnnotations = true;
                    break;
                }
            }
            if ( containsLocalAnnotations )
                break;
        }

        /* Don't show it if there are no local annotations */
        if ( containsLocalAnnotations )
        {
            int res = KMessageBox::warningContinueCancel( widget(), i18n("Your annotations will not be exported.\nYou can export the annotated document using File -> Export As -> Document Archive") );
            if ( res != KMessageBox::Continue )
                return; // Canceled
        }
    }

    KUrl saveUrl = KFileDialog::getSaveUrl( url(),
                                            QString(), widget(), QString(),
                                            KFileDialog::ConfirmOverwrite );
    if ( !saveUrl.isValid() || saveUrl.isEmpty() )
        return;

    saveAs( saveUrl );
}

bool Part::saveAs( const KUrl & saveUrl )
{
    KTemporaryFile tf;
    QString fileName;
    if ( !tf.open() )
    {
        KMessageBox::information( widget(), i18n("Could not open the temporary file for saving." ) );
            return false;
    }
    fileName = tf.fileName();
    tf.close();

    QString errorText;
    bool saved;

    if ( isDocumentArchive )
        saved = m_document->saveDocumentArchive( fileName );
    else
        saved = m_document->saveChanges( fileName, &errorText );

    if ( !saved )
    {
        if (errorText.isEmpty())
        {
            KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", fileName ) );
        }
        else
        {
            KMessageBox::information( widget(), i18n("File could not be saved in '%1'. %2", fileName, errorText ) );
        }
        return false;
    }

    KIO::Job *copyJob = KIO::file_copy( fileName, saveUrl, -1, KIO::Overwrite );
    if ( !KIO::NetAccess::synchronousRun( copyJob, widget() ) )
    {
        KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", saveUrl.prettyUrl() ) );
        return false;
    }

    setModified( false );
    return true;
}


void Part::slotSaveCopyAs()
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    KUrl saveUrl = KFileDialog::getSaveUrl( KUrl("kfiledialog:///okular/" + url().fileName()),
                                            QString(), widget(), QString(),
                                            KFileDialog::ConfirmOverwrite );
    if ( saveUrl.isValid() && !saveUrl.isEmpty() )
    {
        // make use of the already downloaded (in case of remote URLs) file,
        // no point in downloading that again
        KUrl srcUrl = KUrl::fromPath( localFilePath() );
        KTemporaryFile * tempFile = 0;
        // duh, our local file disappeared...
        if ( !QFile::exists( localFilePath() ) )
        {
            if ( url().isLocalFile() )
            {
#ifdef OKULAR_KEEP_FILE_OPEN
                // local file: try to get it back from the open handle on it
                if ( ( tempFile = m_keeper->copyToTemporary() ) )
                    srcUrl = KUrl::fromPath( tempFile->fileName() );
#else
                const QString msg = i18n( "Okular cannot copy %1 to the specified location.\n\nThe document does not exist anymore.", localFilePath() );
                KMessageBox::sorry( widget(), msg );
                return;
#endif
            }
            else
            {
                // we still have the original remote URL of the document,
                // so copy the document from there
                srcUrl = url();
            }
        }

        KIO::Job *copyJob = KIO::file_copy( srcUrl, saveUrl, -1, KIO::Overwrite );
        if ( !KIO::NetAccess::synchronousRun( copyJob, widget() ) )
            KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", saveUrl.prettyUrl() ) );

        delete tempFile;
    }
}


void Part::slotGetNewStuff()
{
#if 0
    KNS::Engine engine(widget());
    engine.init( "okular.knsrc" );
    // show the modal dialog over pageview and execute it
    KNS::Entry::List entries = engine.downloadDialogModal( m_pageView );
    Q_UNUSED( entries )
#endif
}


void Part::slotPreferences()
{
    // Create dialog
    PreferencesDialog * dialog = new PreferencesDialog( m_pageView, Okular::Settings::self(), m_embedMode );
    dialog->setAttribute( Qt::WA_DeleteOnClose );

    // Show it
    dialog->show();
}


void Part::slotAnnotationPreferences()
{
    // Create dialog
    PreferencesDialog * dialog = new PreferencesDialog( m_pageView, Okular::Settings::self(), m_embedMode );
    dialog->setAttribute( Qt::WA_DeleteOnClose );

    // Show it
    dialog->switchToAnnotationsPage();
    dialog->show();
}


void Part::slotNewConfig()
{
    // Apply settings here. A good policy is to check whether the setting has
    // changed before applying changes.

    // Watch File
    setWatchFileModeEnabled(Okular::Settings::watchFile());

    // Main View (pageView)
    m_pageView->reparseConfig();

    // update document settings
    m_document->reparseConfig();

    // update TOC settings
    if ( m_sidebar->isItemEnabled(m_toc) )
        m_toc->reparseConfig();

    // update ThumbnailList contents
    if ( Okular::Settings::showLeftPanel() && !m_thumbnailList->isHidden() )
        m_thumbnailList->updateWidgets();

    // update Reviews settings
    if ( m_sidebar->isItemEnabled(m_reviewsWidget) )
        m_reviewsWidget->reparseConfig();

    setWindowTitleFromDocument ();
}


void Part::slotPrintPreview()
{
    if (m_document->pages() == 0) return;

    QPrinter printer;

    // Native printing supports KPrintPreview, Postscript needs to use FilePrinterPreview
    if ( m_document->printingSupport() == Okular::Document::NativePrinting )
    {
        KPrintPreview previewdlg( &printer, widget() );
        setupPrint( printer );
        doPrint( printer );
        previewdlg.exec();
    }
    else
    {
        // Generate a temp filename for Print to File, then release the file so generator can write to it
        KTemporaryFile tf;
        tf.setAutoRemove( true );
        tf.setSuffix( ".ps" );
        tf.open();
        printer.setOutputFileName( tf.fileName() );
        tf.close();
        setupPrint( printer );
        doPrint( printer );
        if ( QFile::exists( printer.outputFileName() ) )
        {
            Okular::FilePrinterPreview previewdlg( printer.outputFileName(), widget() );
            previewdlg.exec();
        }
    }
}


void Part::slotShowMenu(const Okular::Page *page, const QPoint &point)
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    bool reallyShow = false;
    const bool currentPage = page && page->number() == m_document->viewport().pageNumber;

    if (!m_actionsSearched)
    {
        // the quest for options_show_menubar
        KActionCollection *ac;
        QAction *act;

        if (factory())
        {
            const QList<KXMLGUIClient*> clients(factory()->clients());
            for(int i = 0 ; (!m_showMenuBarAction || !m_showFullScreenAction) && i < clients.size(); ++i)
            {
                ac = clients.at(i)->actionCollection();
                // show_menubar
                act = ac->action("options_show_menubar");
                if (act && qobject_cast<KToggleAction*>(act))
                    m_showMenuBarAction = qobject_cast<KToggleAction*>(act);
                // fullscreen
                act = ac->action("fullscreen");
                if (act && qobject_cast<KToggleFullScreenAction*>(act))
                    m_showFullScreenAction = qobject_cast<KToggleFullScreenAction*>(act);
            }
        }
        m_actionsSearched = true;
    }

    KMenu *popup = new KMenu( widget() );
    QAction *addBookmark = 0;
    QAction *removeBookmark = 0;
    QAction *fitPageWidth = 0;
    if (page)
    {
        popup->addTitle( i18n( "Page %1", page->number() + 1 ) );
        if ( ( !currentPage && m_document->bookmarkManager()->isBookmarked( page->number() ) ) ||
                ( currentPage && m_document->bookmarkManager()->isBookmarked( m_document->viewport() ) ) )
            removeBookmark = popup->addAction( KIcon("edit-delete-bookmark"), i18n("Remove Bookmark") );
        else
            addBookmark = popup->addAction( KIcon("bookmark-new"), i18n("Add Bookmark") );
        if ( m_pageView->canFitPageWidth() )
            fitPageWidth = popup->addAction( KIcon("zoom-fit-best"), i18n("Fit Width") );
        popup->addAction( m_prevBookmark );
        popup->addAction( m_nextBookmark );
        reallyShow = true;
    }
    /*
        //Albert says: I have not ported this as i don't see it does anything
        if ( d->mouseOnRect ) // and rect->objectType() == ObjectRect::Image ...
        {
            m_popup->insertItem( SmallIcon("document-save"), i18n("Save Image..."), 4 );
            m_popup->setItemEnabled( 4, false );
    }*/

    if ((m_showMenuBarAction && !m_showMenuBarAction->isChecked()) || (m_showFullScreenAction && m_showFullScreenAction->isChecked()))
    {
        popup->addTitle( i18n( "Tools" ) );
        if (m_showMenuBarAction && !m_showMenuBarAction->isChecked()) popup->addAction(m_showMenuBarAction);
        if (m_showFullScreenAction && m_showFullScreenAction->isChecked()) popup->addAction(m_showFullScreenAction);
        reallyShow = true;

    }

    if (reallyShow)
    {
        QAction *res = popup->exec(point);
        if (res)
        {
            if (res == addBookmark)
            {
                if (currentPage)
                    m_document->bookmarkManager()->addBookmark( m_document->viewport() );
                else
                    m_document->bookmarkManager()->addBookmark( page->number() );
            }
            else if (res == removeBookmark)
            {
                if (currentPage)
                    m_document->bookmarkManager()->removeBookmark( m_document->viewport() );
                else
                    m_document->bookmarkManager()->removeBookmark( page->number() );
            }
            else if (res == fitPageWidth)
            {
                m_pageView->fitPageWidth( page->number() );
            }
        }
    }
    delete popup;
}


void Part::slotShowProperties()
{
    PropertiesDialog *d = new PropertiesDialog(widget(), m_document);
    d->exec();
    delete d;
}


void Part::slotShowEmbeddedFiles()
{
    EmbeddedFilesDialog *d = new EmbeddedFilesDialog(widget(), m_document);
    d->exec();
    delete d;
}


void Part::slotShowPresentation()
{
    if ( !m_presentationWidget )
    {
        m_presentationWidget = new PresentationWidget( widget(), m_document, actionCollection() );
    }
}


void Part::slotHidePresentation()
{
    if ( m_presentationWidget )
        delete (PresentationWidget*) m_presentationWidget;
}


void Part::slotTogglePresentation()
{
    if ( m_document->isOpened() )
    {
        if ( !m_presentationWidget )
            m_presentationWidget = new PresentationWidget( widget(), m_document, actionCollection() );
        else delete (PresentationWidget*) m_presentationWidget;
    }
}


void Part::reload()
{
    if ( m_document->isOpened() )
    {
        slotReload();
    }
}

void Part::enableStartWithPrint()
{
    m_cliPrint = true;
}

void Part::slotAboutBackend()
{
    const KComponentData *data = m_document->componentData();
    if ( !data )
        return;

    KAboutData aboutData( *data->aboutData() );

    if ( aboutData.programIconName().isEmpty() || aboutData.programIconName() == aboutData.appName() )
    {
        const Okular::DocumentInfo documentInfo = m_document->documentInfo(QSet<DocumentInfo::Key>() << DocumentInfo::MimeType);
        const QString mimeTypeName = documentInfo.get(DocumentInfo::MimeType);
        if ( !mimeTypeName.isEmpty() )
        {
            if ( KMimeType::Ptr type = KMimeType::mimeType( mimeTypeName ) )
                aboutData.setProgramIconName( type->iconName() );
        }
    }

    KAboutApplicationDialog dlg( &aboutData, widget() );
    dlg.exec();
}


void Part::slotExportAs(QAction * act)
{
    QList<QAction*> acts = m_exportAs->menu() ? m_exportAs->menu()->actions() : QList<QAction*>();
    int id = acts.indexOf( act );
    if ( ( id < 0 ) || ( id >= acts.count() ) )
        return;

    QString filter;
    switch ( id )
    {
        case 0:
            filter = "text/plain";
            break;
        case 1:
            filter = "application/vnd.kde.okular-archive";
            break;
        default:
            filter = m_exportFormats.at( id - 2 ).mimeType()->name();
            break;
    }
    QString fileName = KFileDialog::getSaveFileName( url().isLocalFile() ? url().directory() : QString(),
                                                     filter, widget(), QString(),
                                                     KFileDialog::ConfirmOverwrite );
    if ( !fileName.isEmpty() )
    {
        bool saved = false;
        switch ( id )
        {
            case 0:
                saved = m_document->exportToText( fileName );
                break;
            case 1:
                saved = m_document->saveDocumentArchive( fileName );
                break;
            default:
                saved = m_document->exportTo( fileName, m_exportFormats.at( id - 2 ) );
                break;
        }
        if ( !saved )
            KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", fileName ) );
    }
}


void Part::slotReload()
{
    // stop the dirty handler timer, otherwise we may conflict with the
    // auto-refresh system
    m_dirtyHandler->stop();

    slotDoFileDirty();
}


void Part::slotPrint()
{
    if (m_document->pages() == 0) return;

#ifdef Q_WS_WIN
    QPrinter printer(QPrinter::HighResolution);
#else
    QPrinter printer;
#endif
    QPrintDialog *printDialog = 0;
    QWidget *printConfigWidget = 0;

    // Must do certain QPrinter setup before creating QPrintDialog
    setupPrint( printer );

    // Create the Print Dialog with extra config widgets if required
    if ( m_document->canConfigurePrinter() )
    {
        printConfigWidget = m_document->printConfigurationWidget();
    }
    if ( printConfigWidget )
    {
        printDialog = KdePrint::createPrintDialog( &printer, QList<QWidget*>() << printConfigWidget, widget() );
    }
    else
    {
        printDialog = KdePrint::createPrintDialog( &printer, widget() );
    }

    if ( printDialog )
    {

        // Set the available Print Range
        printDialog->setMinMax( 1, m_document->pages() );
        printDialog->setFromTo( 1, m_document->pages() );

        // If the user has bookmarked pages for printing, then enable Selection
        if ( !m_document->bookmarkedPageRange().isEmpty() )
        {
            printDialog->addEnabledOption( QAbstractPrintDialog::PrintSelection );
        }

        // If the Document type doesn't support print to both PS & PDF then disable the Print Dialog option
        if ( printDialog->isOptionEnabled( QAbstractPrintDialog::PrintToFile ) &&
             !m_document->supportsPrintToFile() )
        {
            printDialog->setEnabledOptions( printDialog->enabledOptions() ^ QAbstractPrintDialog::PrintToFile );
        }

#if QT_VERSION >= KDE_MAKE_VERSION(4,7,0)
        // Enable the Current Page option in the dialog.
        if ( m_document->pages() > 1 && currentPage() > 0 )
        {
            printDialog->setOption( QAbstractPrintDialog::PrintCurrentPage );
        }
#endif

        if ( printDialog->exec() )
            doPrint( printer );
        delete printDialog;
    }
}


void Part::setupPrint( QPrinter &printer )
{
    printer.setOrientation(m_document->orientation());

    // title
    QString title = m_document->metaData( "DocumentTitle" ).toString();
    if ( title.isEmpty() )
    {
        title = m_document->currentDocument().fileName();
    }
    if ( !title.isEmpty() )
    {
        printer.setDocName( title );
    }
}


void Part::doPrint(QPrinter &printer)
{
    if (!m_document->isAllowed(Okular::AllowPrint))
    {
        KMessageBox::error(widget(), i18n("Printing this document is not allowed."));
        return;
    }

    if (!m_document->print(printer))
    {
        const QString error = m_document->printError();
        if (error.isEmpty())
        {
            KMessageBox::error(widget(), i18n("Could not print the document. Unknown error. Please report to bugs.kde.org"));
        }
        else
        {
            KMessageBox::error(widget(), i18n("Could not print the document. Detailed error is \"%1\". Please report to bugs.kde.org", error));
        }
    }
}

void Part::psTransformEnded(int exit, QProcess::ExitStatus status)
{
    Q_UNUSED( exit )
    if ( status != QProcess::NormalExit )
        return;

    QProcess *senderobj = sender() ? qobject_cast< QProcess * >( sender() ) : 0;
    if ( senderobj )
    {
        senderobj->close();
        senderobj->deleteLater();
    }

    setLocalFilePath( m_temporaryLocalFile );
    openUrl( m_temporaryLocalFile );
    m_temporaryLocalFile.clear();
}


void Part::displayInfoMessage( const QString &message, KMessageWidget::MessageType messageType, int duration )
{
    if ( !Okular::Settings::showOSD() )
    {
        if (messageType == KMessageWidget::Error)
        {
            KMessageBox::error( widget(), message );
        }
        return;
    }

    // hide messageWindow if string is empty
    if ( message.isEmpty() )
        m_infoMessage->animatedHide();

    // display message (duration is length dependant)
    if ( duration < 0 )
    {
        duration = 500 + 100 * message.length();
    }
    m_infoTimer->start( duration );
    m_infoMessage->setText( message );
    m_infoMessage->setMessageType( messageType );
    m_infoMessage->setVisible( true );
}


void Part::errorMessage( const QString &message, int duration )
{
    displayInfoMessage( message, KMessageWidget::Error, duration );
}

void Part::warningMessage( const QString &message, int duration )
{
    displayInfoMessage( message, KMessageWidget::Warning, duration );
}

void Part::noticeMessage( const QString &message, int duration )
{
    // less important message -> simpleer display widget in the PageView
    m_pageView->displayMessage( message, QString(), PageViewMessage::Info, duration );
}

void Part::moveSplitter(int sideWidgetSize)
{
    m_sidebar->moveSplitter( sideWidgetSize );
}


void Part::unsetDummyMode()
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    m_sidebar->setItemEnabled( m_reviewsWidget, true );
    m_sidebar->setItemEnabled( m_bookmarkList, true );
    m_sidebar->setSidebarVisibility( Okular::Settings::showLeftPanel() );

    // add back and next in history
    m_historyBack = KStandardAction::documentBack( this, SLOT(slotHistoryBack()), actionCollection() );
    m_historyBack->setWhatsThis( i18n( "Go to the place you were before" ) );
    connect(m_pageView, SIGNAL(mouseBackButtonClick()), m_historyBack, SLOT(trigger()));

    m_historyNext = KStandardAction::documentForward( this, SLOT(slotHistoryNext()), actionCollection());
    m_historyNext->setWhatsThis( i18n( "Go to the place you were after" ) );
    connect(m_pageView, SIGNAL(mouseForwardButtonClick()), m_historyNext, SLOT(trigger()));

    m_pageView->setupActions( actionCollection() );

    // attach the actions of the children widgets too
    m_formsMessage->addAction( m_pageView->toggleFormsAction() );
    m_formsMessage->setVisible( m_pageView->toggleFormsAction() != 0 );

    // ensure history actions are in the correct state
    updateViewActions();
}


bool Part::handleCompressed( QString &destpath, const QString &path, const QString &compressedMimetype )
{
    m_tempfile = 0;

    // we are working with a compressed file, decompressing
    // temporary file for decompressing
    KTemporaryFile *newtempfile = new KTemporaryFile();
    newtempfile->setAutoRemove(true);

    if ( !newtempfile->open() )
    {
        KMessageBox::error( widget(),
            i18n("<qt><strong>File Error!</strong> Could not create temporary file "
            "<nobr><strong>%1</strong></nobr>.</qt>",
            strerror(newtempfile->error())));
        delete newtempfile;
        return false;
    }

    // decompression filer
    QIODevice* filterDev = KFilterDev::deviceForFile( path, compressedMimetype );
    if (!filterDev)
    {
        delete newtempfile;
        return false;
    }

    if ( !filterDev->open(QIODevice::ReadOnly) )
    {
        KMessageBox::detailedError( widget(),
            i18n("<qt><strong>File Error!</strong> Could not open the file "
            "<nobr><strong>%1</strong></nobr> for uncompression. "
            "The file will not be loaded.</qt>", path),
            i18n("<qt>This error typically occurs if you do "
            "not have enough permissions to read the file. "
            "You can check ownership and permissions if you "
            "right-click on the file in the Dolphin "
            "file manager and then choose the 'Properties' tab.</qt>"));

        delete filterDev;
        delete newtempfile;
        return false;
    }

    char buf[65536];
    int read = 0, wrtn = 0;

    while ((read = filterDev->read(buf, sizeof(buf))) > 0)
    {
        wrtn = newtempfile->write(buf, read);
        if ( read != wrtn )
            break;
    }
    delete filterDev;
    if ((read != 0) || (newtempfile->size() == 0))
    {
        KMessageBox::detailedError(widget(),
            i18n("<qt><strong>File Error!</strong> Could not uncompress "
            "the file <nobr><strong>%1</strong></nobr>. "
            "The file will not be loaded.</qt>", path ),
            i18n("<qt>This error typically occurs if the file is corrupt. "
            "If you want to be sure, try to decompress the file manually "
            "using command-line tools.</qt>"));
        delete newtempfile;
        return false;
    }
    m_tempfile = newtempfile;
    destpath = m_tempfile->fileName();
    return true;
}

void Part::rebuildBookmarkMenu( bool unplugActions )
{
    if ( unplugActions )
    {
        unplugActionList( "bookmarks_currentdocument" );
        qDeleteAll( m_bookmarkActions );
        m_bookmarkActions.clear();
    }
    KUrl u = m_document->currentDocument();
    if ( u.isValid() )
    {
        m_bookmarkActions = m_document->bookmarkManager()->actionsForUrl( u );
    }
    bool havebookmarks = true;
    if ( m_bookmarkActions.isEmpty() )
    {
        havebookmarks = false;
        QAction * a = new KAction( 0 );
        a->setText( i18n( "No Bookmarks" ) );
        a->setEnabled( false );
        m_bookmarkActions.append( a );
    }
    plugActionList( "bookmarks_currentdocument", m_bookmarkActions );
    
    if (factory())
    {
        const QList<KXMLGUIClient*> clients(factory()->clients());
        bool containerFound = false;
        for (int i = 0; !containerFound && i < clients.size(); ++i)
        {
            QWidget *container = factory()->container("bookmarks", clients[i]);
            if (container && container->actions().contains(m_bookmarkActions.first()))
            {
                Q_ASSERT(dynamic_cast<KMenu*>(container));
                disconnect(container, 0, this, 0);
                connect(container, SIGNAL(aboutToShowContextMenu(KMenu*,QAction*,QMenu*)), this, SLOT(slotAboutToShowContextMenu(KMenu*,QAction*,QMenu*)));
                containerFound = true;
            }
        }
    }

    m_prevBookmark->setEnabled( havebookmarks );
    m_nextBookmark->setEnabled( havebookmarks );
}

void Part::updateAboutBackendAction()
{
    const KComponentData *data = m_document->componentData();
    if ( data )
    {
        m_aboutBackend->setEnabled( true );
    }
    else
    {
        m_aboutBackend->setEnabled( false );
    }
}

void Part::resetStartArguments()
{
    m_cliPrint = false;
}

void Part::setReadWrite(bool readwrite)
{
    m_document->setAnnotationEditingEnabled( readwrite );
    ReadWritePart::setReadWrite( readwrite );
}

} // namespace Okular

#include "moc_part.cpp"

/* kate: replace-tabs on; indent-width 4; */
