/***************************************************************************
 *   Copyright (C) 2006 by Albert Astals Cid <aacid@kde.org>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "embeddedfilesdialog.h"

#include <QAction>
#include <QCursor>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QMenu>
#include <QTreeWidget>
#include <QTemporaryFile>

#include <kglobal.h>
#include <kicon.h>
#include <klocale.h>
#include <kmimetype.h>
#include <kstandardguiitem.h>
#include <krun.h>

#include "core/document.h"
#include "guiutils.h"

Q_DECLARE_METATYPE( Okular::EmbeddedFile* )

static const int EmbeddedFileRole = Qt::UserRole + 100;

static QString dateToString( const QDateTime & date )
{
	return date.isValid()
		? KGlobal::locale()->formatDateTime( date, KLocale::LongDate, true )
		: i18nc( "Unknown date", "Unknown" );
}

EmbeddedFilesDialog::EmbeddedFilesDialog(QWidget *parent, const Okular::Document *document) : KDialog(parent)
{
	setCaption(i18nc("@title:window", "Embedded Files"));
	setButtons(Close | User1 | User2);
	setDefaultButton(Close);
	setButtonGuiItem(User1, KStandardGuiItem::save());
	setButtonGuiItem(User2, KGuiItem(i18nc("@action:button", "View"), "document-open"));
	enableButton(User1, false);
	enableButton(User2, false);

	m_tw = new QTreeWidget(this);
	setMainWidget(m_tw);
	QStringList header;
	header.append(i18nc("@title:column", "Name"));
	header.append(i18nc("@title:column", "Description"));
	header.append(i18nc("@title:column", "Size"));
	header.append(i18nc("@title:column", "Created"));
	header.append(i18nc("@title:column", "Modified"));
	m_tw->setHeaderLabels(header);
	m_tw->setRootIsDecorated(false);
	m_tw->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_tw->setContextMenuPolicy(Qt::CustomContextMenu);

	foreach(Okular::EmbeddedFile* ef, *document->embeddedFiles())
	{
		QTreeWidgetItem *twi = new QTreeWidgetItem();
		twi->setText(0, ef->name());
		KMimeType::Ptr mime = KMimeType::findByPath( ef->name(), 0, true );
		if (mime)
		{
			twi->setIcon(0, KIcon(mime->iconName()));
		}
		twi->setText(1, ef->description());
		twi->setText(2, ef->size() <= 0 ? i18nc("Not available size", "N/A") : KGlobal::locale()->formatByteSize(ef->size()));
		twi->setText(3, dateToString( ef->creationDate() ) );
		twi->setText(4, dateToString( ef->modificationDate() ) );
		twi->setData( 0, EmbeddedFileRole, qVariantFromValue( ef ) );
		m_tw->addTopLevelItem(twi);
	}
        // Having filled the columns, it is nice to resize them to be able to read the contents
        for (int lv = 0; lv <  m_tw->columnCount(); ++lv) {
                m_tw->resizeColumnToContents(lv);
        }
        // This is a bit dubious, but I'm not seeing a nice way to say "expand to fit contents"
        m_tw->setMinimumWidth(640);
        m_tw->updateGeometry();

	connect(this, SIGNAL(user1Clicked()), this, SLOT(saveFile()));
	connect(this, SIGNAL(user2Clicked()), this, SLOT(viewFile()));

	connect(m_tw, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(attachViewContextMenu(QPoint)));
	connect(m_tw, SIGNAL(itemSelectionChanged()), this, SLOT(updateSaveButton()));
	connect(m_tw, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(viewFileItem(QTreeWidgetItem*,int)));
}

void EmbeddedFilesDialog::updateSaveButton()
{
	bool enable = (m_tw->selectedItems().count() > 0);
	enableButton(User1, enable);
	enableButton(User2, enable);
}

void EmbeddedFilesDialog::saveFile()
{
	QList<QTreeWidgetItem *> selected = m_tw->selectedItems();
	foreach(QTreeWidgetItem *twi, selected)
	{
		Okular::EmbeddedFile* ef = qvariant_cast< Okular::EmbeddedFile* >( twi->data( 0, EmbeddedFileRole ) );
		saveFile(ef);
	}
}

void EmbeddedFilesDialog::viewFile()
{
	QList<QTreeWidgetItem *> selected = m_tw->selectedItems();
	foreach(QTreeWidgetItem *twi, selected)
	{
		Okular::EmbeddedFile* ef = qvariant_cast< Okular::EmbeddedFile* >( twi->data( 0, EmbeddedFileRole ) );
		viewFile( ef );
	}
}

void EmbeddedFilesDialog::viewFileItem( QTreeWidgetItem* item, int /*column*/ )
{
	Okular::EmbeddedFile* ef = qvariant_cast< Okular::EmbeddedFile* >( item->data( 0, EmbeddedFileRole ) );
	viewFile( ef );
}

void EmbeddedFilesDialog::attachViewContextMenu( const QPoint& /*pos*/ )
{
    QList<QTreeWidgetItem *> selected = m_tw->selectedItems();
    if ( selected.isEmpty() )
        return;

    if ( selected.size() > 1 )
        return;

    QMenu menu( this );
    QAction* saveAsAct = menu.addAction( KIcon( "document-save-as" ), i18nc( "@action:inmenu", "&Save As..." ) );
    QAction* viewAct = menu.addAction( KIcon( "document-open" ), i18nc( "@action:inmenu", "&View..." ) );

    QAction* act = menu.exec( QCursor::pos() );
    if ( !act )
        return;

    Okular::EmbeddedFile* ef = qvariant_cast< Okular::EmbeddedFile* >( selected.at( 0 )->data( 0, EmbeddedFileRole ) );
    if ( act == saveAsAct )
    {
        saveFile( ef );
    }
    else if ( act == viewAct )
    {
        viewFile( ef );
    }
}

void EmbeddedFilesDialog::viewFile( Okular::EmbeddedFile* ef )
{
	// get name and extension
	QFileInfo fileInfo(ef->name());

	// save in temporary directory with a unique name resembling the attachment name,
	// using QTemporaryFile's XXXXXX placeholder
	QTemporaryFile *tmpFile = new QTemporaryFile(
		QDir::tempPath()
		+ QDir::separator()
		+ fileInfo.baseName()
		+ ".XXXXXX"
		+ (fileInfo.completeSuffix().isEmpty() ? QString("") : "." + fileInfo.completeSuffix())
	);
	GuiUtils::writeEmbeddedFile( ef, this, *tmpFile );

	// set readonly to prevent the viewer application from modifying it
	tmpFile->setPermissions( QFile::ReadOwner );

	// keep temporary file alive while the dialog is open
	m_openedFiles.push_back( QSharedPointer< QTemporaryFile >( tmpFile ) );

	// view the temporary file with the default application
	new KRun( QUrl( "file://" + tmpFile->fileName() ), this );
}

void EmbeddedFilesDialog::saveFile( Okular::EmbeddedFile* ef )
{
    GuiUtils::saveEmbeddedFile( ef, this );
}

#include "moc_embeddedfilesdialog.cpp"
