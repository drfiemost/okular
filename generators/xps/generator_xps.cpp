/*
  Copyright (C)  2006  Brad Hards <bradh@frogmouth.net>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
*/

#include <qdatetime.h>
#include <qfile.h>
#include <qfontdatabase.h>
#include <qimage.h>
#include <qlist.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qthread.h>
#include <kglobal.h>
#include <kimageeffect.h>
#include <klocale.h>
#include <QFileInfo>

#include <okular/core/document.h>
#include <okular/core/page.h>
#include "generator_xps.h"

OKULAR_EXPORT_PLUGIN(XpsGenerator)

// From Qt4
static int hex2int(char hex)
{
    QChar hexchar = QLatin1Char(hex);
    int v;
    if (hexchar.isDigit())
        v = hexchar.digitValue();
    else if (hexchar >= QLatin1Char('A') && hexchar <= QLatin1Char('F'))
        v = hexchar.cell() - 'A' + 10;
    else if (hexchar >= QLatin1Char('a') && hexchar <= QLatin1Char('f'))
        v = hexchar.cell() - 'a' + 10;
    else
        v = -1;
    return v;
}

// Modified from Qt4
static QColor hexToRgba(const char *name)
{
    if(name[0] != '#')
        return QColor();
    name++; // eat the leading '#'
    int len = qstrlen(name);
    int r, g, b;
    int a = 255;
    if (len == 6) {
        r = (hex2int(name[0]) << 4) + hex2int(name[1]);
        g = (hex2int(name[2]) << 4) + hex2int(name[3]);
        b = (hex2int(name[4]) << 4) + hex2int(name[5]);
    } else if (len == 8) {
        a = (hex2int(name[0]) << 4) + hex2int(name[1]);
        r = (hex2int(name[2]) << 4) + hex2int(name[3]);
        g = (hex2int(name[4]) << 4) + hex2int(name[5]);
        b = (hex2int(name[6]) << 4) + hex2int(name[7]);
    } else {
        r = g = b = -1;
    }
    if ((uint)r > 255 || (uint)g > 255 || (uint)b > 255) {
        return QColor();
    }
    return QColor(r,g,b,a);
}

static QRectF stringToRectF( const QString &data )
{
    QStringList numbers = data.split(',');
    QPointF origin( numbers.at(0).toDouble(), numbers.at(1).toDouble() );
    QSizeF size( numbers.at(2).toDouble(), numbers.at(3).toDouble() );
    return QRectF( origin, size );
}

static bool parseGUID( const QString guidString, unsigned short guid[16]) {

    if (guidString.length() <= 35) {
        return false;
    }

    // Maps bytes to positions in guidString
    const static int indexes[] = {6, 4, 2, 0, 11, 9, 16, 14, 19, 21, 24, 26, 28, 30, 32, 34};

    for (int i = 0; i < 16; i++) {
        int hex1 = hex2int(guidString[indexes[i]].cell());
        int hex2 = hex2int(guidString[indexes[i]+1].cell());

        if ((hex1 < 0) || (hex2 < 0))
        {
            return false;
        }

        guid[i] = hex1 * 16 + hex2;
    }

    return true;

}


// Read next token of abbreviated path data
static bool nextAbbPathToken(AbbPathToken *token)
{
    int *curPos = &token->curPos;
    QString data = token->data;

    while ((*curPos < data.length()) && (data.at(*curPos).isSpace())) 
    { 
        (*curPos)++;
    }

    if (*curPos == data.length())
    {
        token->type = abtEOF;
        return true;
    }

    QChar ch = data.at(*curPos);

    if (ch.isNumber() || (ch == '+') || (ch == '-'))
    {
        int start = *curPos;
        while ((*curPos < data.length()) && (!data.at(*curPos).isSpace()) && (data.at(*curPos) != ',') && !data.at(*curPos).isLetter())
    {
        (*curPos)++;
    }
    token->number = data.mid(start, *curPos - start).toDouble();
    token->type = abtNumber;

    } else if (ch == ',')
    {
        token->type = abtComma;
        (*curPos)++;
    } else if (ch.isLetter())  
    {
        token->type = abtCommand;
        token->command = data.at(*curPos).cell();
        (*curPos)++;
    } else 
    {
        return false;
    }

    return true;
}

/**
    Read point (two reals delimited by comma) from abbreviated path data
*/
static QPointF getPointFromString(AbbPathToken *token, bool relative, const QPointF currentPosition) {
    //TODO Check grammar
    
    QPointF result;
    result.rx() = token->number;
    nextAbbPathToken(token);
    nextAbbPathToken(token); // ,
    result.ry() = token->number;
    nextAbbPathToken(token);

    if (relative)
    {
        result += currentPosition;
    }    

    return result;
}

/**
    Parse an abbreviated path "Data" description
    \param data the string containing the whitespace separated values
        
    \see XPS specification 4.2.3 and Appendix G
*/
static QPainterPath parseAbbreviatedPathData( const QString &data)
{
    QPainterPath path = QPainterPath();

    AbbPathToken token;

    token.data = data;
    token.curPos = 0;

    nextAbbPathToken(&token);
    
    // Used by Smooth cubic curve (command s)
    char lastCommand = ' '; 
    QPointF lastSecondControlPoint;

    while (true)
    {
        if (token.type != abtCommand)
        {
            if (token.type != abtEOF)
            {
                kDebug() << "Error in parsing abbreviated path data" << endl;
            }
            return path;
        }

        char command = QChar(token.command).toLower().cell();
        bool isRelative = QChar(token.command).isLower();
        QPointF currPos = path.currentPosition();
        nextAbbPathToken(&token);

        switch (command) {
            case 'f':
                int rule;
                rule = (int)token.number;
                if (rule == 0)
                {
                    path.setFillRule(Qt::OddEvenFill);
                }
                else if (rule == 1)
                {
                    // In xps specs rule 1 means NonZero fill. I think it's equivalent to WindingFill but I'm not sure
                    path.setFillRule(Qt::WindingFill);
                }
                nextAbbPathToken(&token); 
                break;
            case 'm': // Move
                while (token.type == abtNumber)
                {
                    QPointF point = getPointFromString(&token, isRelative, currPos);
                    path.moveTo(point);
                }
                break;
            case 'l': // Line
                while (token.type == abtNumber)
                {
                    QPointF point = getPointFromString(&token, isRelative, currPos);
                    path.lineTo(point);
                }
                break;
            case 'h': // Horizontal line
                while (token.type == abtNumber)
                {
                    double x = token.number + isRelative?path.currentPosition().x():0;
                    path.lineTo(x, path.currentPosition().y());
                    nextAbbPathToken(&token);
                }
                break;
            case 'v': // Vertical line
                while (token.type == abtNumber)
                {
                    double y = token.number + isRelative?path.currentPosition().y():0;
                    path.lineTo(path.currentPosition().x(), y);
                    nextAbbPathToken(&token);
                }
                break;
            case 'c': // Cubic bezier curve
                while (token.type == abtNumber)
                {
                    QPointF firstControl = getPointFromString(&token, isRelative, currPos);
                    QPointF secondControl = getPointFromString(&token, isRelative, currPos);
                    QPointF endPoint = getPointFromString(&token, isRelative, currPos);
                    path.cubicTo(firstControl, secondControl, endPoint);

                    lastSecondControlPoint = secondControl;
                }
                break;
            case 'q': // Quadratic bezier curve
                while (token.type == abtNumber)
                {
                    QPointF point1 = getPointFromString(&token, isRelative, currPos);
                    QPointF point2 = getPointFromString(&token, isRelative, currPos);
                    path.quadTo(point2, point2);
                }
                break;
            case 's': // Smooth cubic bezier curve
                while (token.type == abtNumber)
                {
                    QPointF firstControl;
                    if ((lastCommand == 's') || (lastCommand == 'c'))
                    {
                        firstControl = lastSecondControlPoint + (lastSecondControlPoint + path.currentPosition());
                    } 
                    else
                    {
                        firstControl = path.currentPosition();
                    }
                    QPointF secondControl = getPointFromString(&token, isRelative, currPos);
                    QPointF endPoint = getPointFromString(&token, isRelative, currPos);
                    path.cubicTo(firstControl, secondControl, endPoint);
                }
                break;
            case 'a': // Arc
                //TODO Implement Arc drawing
                while (token.type == abtNumber)
                {
                    /*QPointF rp =*/ getPointFromString(&token, isRelative, currPos);
                    /*double r = token.number;*/
                    nextAbbPathToken(&token);
                    /*double fArc = token.number; */
                    nextAbbPathToken(&token);
                    /*double fSweep = token.number; */
                    nextAbbPathToken(&token);
                    /*QPointF point = */getPointFromString(&token, isRelative, currPos);
                }
                break;
            case 'z': // Close path
                path.closeSubpath();
                break;
        }

        lastCommand = command;
    }

    return path;
}

QMatrix XpsHandler::attsToMatrix( const QString &csv )
{
    QStringList values = csv.split( ',' );
    if ( values.count() != 6 ) {
        return QMatrix(); // that is an identity matrix - no effect
    }
    return QMatrix( values.at(0).toDouble(), values.at(1).toDouble(),
                    values.at(2).toDouble(), values.at(3).toDouble(),
                    values.at(4).toDouble(), values.at(5).toDouble() ); 
}

QBrush XpsHandler::parseRscRefColor( const QString &data )
{
    if (data[0] == '{') {
        //TODO
        kDebug() << "Reference" << data << endl;
        return QBrush();
    } else {
        return QBrush( hexToRgba( data.toLatin1() ) );
    }
}

QMatrix XpsHandler::parseRscRefMatrix( const QString &data )
{
    if (data[0] == '{') {
        //TODO
        kDebug() << "Reference" << data << endl;
        return QMatrix();
    } else {
        return attsToMatrix( data );
    }
}

XpsHandler::XpsHandler(XpsPage *page): m_page(page)
{
    m_painter = new QPainter(m_page->m_pageImage);
}

XpsHandler::~XpsHandler()
{
    delete m_painter;
}

bool XpsHandler::startDocument()
{
    kDebug() << "start document" << m_page->m_fileName  << endl;
    m_page->m_pageImage->fill( QColor("White").rgba() );

    XpsRenderNode node;
    node.name = "document";
    m_nodes.push(node);

    return true;
}

bool XpsHandler::startElement( const QString &nameSpace,
                               const QString &localName,
                               const QString &qname,
                               const QXmlAttributes & atts )
{
    Q_UNUSED( nameSpace )
    Q_UNUSED( qname )

    XpsRenderNode node;
    node.name = localName;
    node.attributes = atts;
    processStartElement( node );
    m_nodes.push(node);


    return true;
}

bool XpsHandler::endElement( const QString &nameSpace,
                             const QString &localName,
                             const QString &qname)
{
    Q_UNUSED( nameSpace )
    Q_UNUSED( qname )


    XpsRenderNode node = m_nodes.pop();
    if (node.name != localName) {
        kDebug() << "Name doesn't match" << endl;
    }
    processEndElement( node );
    node.children.clear();
    m_nodes.top().children.append(node);

    return true;
}

void XpsHandler::processGlyph( XpsRenderNode &node )
{
    //TODO Currently ignored attributes: BidiLevel, CaretStops, DeviceFontName, IsSideways, Indices, StyleSimulation, Clip, Opacity, OpacityMask, Name, FixedPage.NavigateURI, xml:lang, x:key
    //TODO Currently ignored child elements: Clip, OpacityMask
    //Handled separately: RenderTransform
    
    QString att;

    m_painter->save();

    // Get font (doesn't work well because qt doesn't allow to load font from file)
    int fontId = m_page->getFontByName( node.attributes.value("FontUri") );
    // kDebug() << "Font families: (" << fontId << ") " << QFontDatabase::applicationFontFamilies( fontId ).at(0) << endl;
    QString fontFamily = m_page->m_fontDatabase.applicationFontFamilies( fontId ).at(0);
    // kDebug() << "Styles: " << m_page->m_fontDatabase.styles( fontFamily ) << endl;
    QString fontStyle =  m_page->m_fontDatabase.styles( fontFamily ).at(0);
    // TODO: We may not be picking the best font size here
    QFont font = m_page->m_fontDatabase.font(fontFamily, fontStyle, qRound(node.attributes.value("FontRenderingEmSize").toFloat()) );
    m_painter->setFont(font);

    //Origin
    QPointF origin( node.attributes.value("OriginX").toDouble(), node.attributes.value("OriginY").toDouble() );

    //Fill
    QBrush brush;
    att = node.attributes.value("Fill");
    if (att.isEmpty()) {
        XpsRenderNode * child = node.findChild("Glyphs.Fill");
        if (child != NULL) {
            brush = *(XpsFill *)child->data;
        } else {
            brush = QBrush();
        }
    } else {
        brush = parseRscRefColor( att );
    }
    m_painter->setBrush( brush );
    m_painter->setPen( QPen( brush, 0 ) );

    //RenderTransform
    att = node.attributes.value("RenderTransform");
    if (!att.isEmpty()) {
        m_painter->setWorldMatrix( parseRscRefMatrix( att ), true);
    }

    m_painter->drawText( origin, node.attributes.value("UnicodeString") );
    // kDebug() << "Glyphs: " << atts.value("Fill") << ", " << atts.value("FontUri") << endl;
    // kDebug() << "    Origin: " << atts.value("OriginX") << "," << atts.value("OriginY") << endl;
    // kDebug() << "    Unicode: " << atts.value("UnicodeString") << endl;
    
    m_painter->restore();
}

void XpsHandler::processFill( XpsRenderNode &node )
{
    //TODO Ignored child elements: ImageBrush, LinearGradientBrush, RadialGradientBrush, VirtualBrush

    QBrush brush;
    XpsRenderNode * child;

    child = node.findChild("SolidColorBrush");
    if (child != NULL) {
        brush = QBrush( *(QColor *) child->data );
    }

    child = node.findChild("ImageBrush");
    if (child != NULL) {
        brush = *(XpsImageBrush *) child->data;
    }

    node.data = new QBrush( brush );
}

void XpsHandler::processImageBrush( XpsRenderNode &node )
{
    //TODO Ignored attributes: Opacity, x:key, Transform, Viewbox, Viewport, TileMode, ViewBoxUnits, ViewPortUnits
    //TODO Ignored child elements: ImageBrush.Transform

    QPixmap image = m_page->loadImageFromFile( node.attributes.value( "ImageSource" ) );
    // image = image.convertToFormat( QImage::Format_ARGB32 );
    kDebug() << image.size() << endl;
    QBrush brush = QBrush( image );
    kDebug() << brush << endl;
    kDebug() << brush.isOpaque() << endl;
    node.data = new XpsImageBrush ( brush );
}

void XpsHandler::processPath( XpsRenderNode &node )
{
    //TODO Ignored attributes: Clip, Opacity, OpacityMask, Stroke, StrokeDashArray, StrokeDashCap, StrokeDashOffset, StrokeEndLineCap, StorkeStartLineCap, StrokeLineJoin, StrokeMitterLimit, StrokeThickness, Name, FixedPage.NavigateURI, xml:lang, x:key, AutomationProperties.Name, AutomationProperties.HelpText, SnapsToDevicePixels
    //TODO Ignored child elements: RenderTransform, Clip, OpacityMask, Stroke, Data
    // Handled separately: RenderTransform
    m_painter->save();

    QString att;
    QPainterPath path;

    // Get path
    att = node.attributes.value( "Data" );
    if (! att.isEmpty() ) {
        path = parseAbbreviatedPathData( att );
    } else {
        path = QPainterPath(); //TODO 
    }
    
    // Set Fill
    att = node.attributes.value( "Fill" );
    QBrush brush;
    if (! att.isEmpty() ) {
        brush = parseRscRefColor( att );   
    } else {
        XpsRenderNode * child = node.findChild("Path.Fill");
        if (child != NULL) {
            brush = *(XpsFill *)child->data;
        } else {
            brush = QBrush();
        }
    }
    m_painter->setBrush( brush );
    m_painter->setPen( QPen( brush, 0 ) );

    // RenderTransform
    att = node.attributes.value( "RenderTransform" );
    if (! att.isEmpty() ) {
        m_painter->setWorldMatrix( parseRscRefMatrix( att ), true );
    }

    m_painter->drawPath( path );

    m_painter->restore();
}

void XpsHandler::processStartElement( XpsRenderNode &node )
{
    if (node.name == "Canvas") {
        m_painter->save();
    }
}

void XpsHandler::processEndElement( XpsRenderNode &node )
{
    if (node.name == "Glyphs") {
        processGlyph( node );
    } else if (node.name == "Path") {
        processPath( node );
    } else if (node.name == "MatrixTransform") {
        //TODO Ignoring x:key
        node.data = new QMatrix ( attsToMatrix( node.attributes.value( "Matrix" ) ) ); 
    } else if ((node.name == "Canvas.RenderTransform") || (node.name == "Glyphs.RenderTransform") || (node.name == "Path.RenderTransform"))  {
        XpsRenderNode * child = node.findChild( "MatrixTransform" );
        if (child == NULL) {
            kDebug() << "Required element MatrixTransform is missing" << endl;
        } else {
            m_painter->setWorldMatrix( *(XpsMatrixTransform *)child->data, true );
        }
        node.data = NULL;
    } else if (node.name == "Canvas") {
        m_painter->restore();
    } else if ((node.name == "Path.Fill") || (node.name == "Glyphs.Fill")) {
        processFill( node );
    } else if (node.name == "SolidColorBrush") {
        //TODO Ignoring opacity, x:key
        node.data = new QColor (hexToRgba( node.attributes.value( "Color" ).toLatin1() ));
    } else if (node.name == "ImageBrush") {
        processImageBrush( node );
    } else {
        //kDebug() << "Unknown element: " << node->name << endl;
    }
}

bool XpsPageSizeHandler::startElement ( const QString &nameSpace, const QString &localName, const QString &qname, const QXmlAttributes &atts)
{
    Q_UNUSED(nameSpace);
    Q_UNUSED(qname);
    if (localName == "FixedPage")
    {
        m_width = atts.value("Width").toInt();
        m_height = atts.value("Height").toInt();
        m_parsed_successfully = true;
    } else {
        m_parsed_successfully = false;
    }

    // No need to parse any more
    return false;
}

XpsPage::XpsPage(KZip *archive, const QString &fileName): m_archive( archive ),
    m_fileName( fileName ), m_pageIsRendered(false)
{
    m_pageImage = NULL;

    kDebug() << "page file name: " << fileName << endl;

    const KZipFileEntry* pageFile = static_cast<const KZipFileEntry *>(archive->directory()->entry( fileName ));

    QIODevice* pageDevice  = pageFile->createDevice();

    XpsPageSizeHandler *handler = new XpsPageSizeHandler();
    QXmlSimpleReader *parser = new QXmlSimpleReader();
    parser->setContentHandler( handler );
    parser->setErrorHandler( handler );
    QXmlInputSource *source = new QXmlInputSource(pageDevice);
    parser->parse ( source );

    if (handler->m_parsed_successfully)
    {
        m_pageSize.setWidth( handler->m_width );
        m_pageSize.setHeight( handler->m_height );
    }
    else
    {
        kDebug() << "Could not parse XPS page" << endl;
    }

    delete parser;
    delete handler;
    delete source;
    delete pageDevice;
}

XpsPage::~XpsPage()
{
    m_fontCache.clear();
    delete m_pageImage;
}

bool XpsPage::renderToImage( QImage *p )
{
    
    if ((m_pageImage == NULL) || (m_pageImage->size() != p->size())) {
        delete m_pageImage;
        m_pageImage = new QImage( p->size(), QImage::Format_ARGB32 );
        m_pageIsRendered = false;
    }
    if (! m_pageIsRendered) {
        XpsHandler *handler = new XpsHandler( this );
        handler->m_painter->setWorldMatrix(QMatrix().scale((qreal)p->size().width() / size().width(), (qreal)p->size().height() / size().height()));
        QXmlSimpleReader *parser = new QXmlSimpleReader();
        parser->setContentHandler( handler );
        parser->setErrorHandler( handler );
        const KZipFileEntry* pageFile = static_cast<const KZipFileEntry *>(m_archive->directory()->entry( m_fileName ));
        QIODevice* pageDevice  = pageFile->createDevice();
        QXmlInputSource *source = new QXmlInputSource(pageDevice);
        bool ok = parser->parse( source );
        kDebug() << "Parse result: " << ok << endl;
        delete source;
        delete parser;
        delete handler;
        delete pageDevice;
        m_pageIsRendered = true;
    }

    *p = *m_pageImage;

    return true;
}
QSize XpsPage::size() const
{
    return m_pageSize;
}

int XpsPage::getFontByName( const QString &fileName )
{
    int index = m_fontCache.value(fileName, -1);
    if (index == -1) 
    {
        index = loadFontByName(fileName);
        m_fontCache[fileName] = index;
    }
    return index;
}

int XpsPage::loadFontByName( const QString &fileName )
{
    // kDebug() << "font file name: " << fileName << endl;

    const KZipFileEntry* fontFile = static_cast<const KZipFileEntry *>(m_archive->directory()->entry( fileName ));

    QByteArray fontData = fontFile->data(); // once per file, according to the docs

    int result = m_fontDatabase.addApplicationFontFromData( fontData );
    if (-1 == result) {
        // Try to deobfuscate font 
       // TODO Use deobfuscation depending on font content type, don't do it always when standard loading fails
    
        QFileInfo* fileInfo = new QFileInfo(fileName);
        QString baseName = fileInfo->baseName();
        delete fileInfo;

        unsigned short guid[16];
        if (!parseGUID(baseName, guid))
        {
            kDebug() << "File to load font - file name isn't a GUID" << endl;
        }
        else
        {
        if (fontData.length() < 32)
            {
                kDebug() << "Font file is too small" << endl;
            } else {
                // Obfuscation - xor bytes in font binary with bytes from guid (font's filename)
                const static int mapping[] = {15, 14, 13, 12, 11, 10, 9, 8, 6, 7, 4, 5, 0, 1, 2, 3};
                for (int i = 0; i < 16; i++) {
                    fontData[i] = fontData[i] ^ guid[mapping[i]];
                    fontData[i+16] = fontData[i+16] ^ guid[mapping[i]];
                }
                result = m_fontDatabase.addApplicationFontFromData( fontData );
            }
        }
    }


    // kDebug() << "Loaded font: " << m_fontDatabase.applicationFontFamilies( result ) << endl;

    return result; // a font ID
}

QPixmap XpsPage::loadImageFromFile( const QString &fileName )
{
    //kDebug() << "image file name: " << fileName << endl;

    const KZipFileEntry* imageFile = static_cast<const KZipFileEntry *>(m_archive->directory()->entry( fileName ));

    QByteArray imageData = imageFile->data(); // once per file, according to the docs

    QPixmap image;
    image.loadFromData( imageData);
    //kDebug() << "Image load result: " << result << ", " << image.size() << endl;
    return image;
}

XpsDocument::XpsDocument(KZip *archive, const QString &fileName)
{
    kDebug() << "document file name: " << fileName << endl;

    const KZipFileEntry* documentFile = static_cast<const KZipFileEntry *>(archive->directory()->entry( fileName ));

    QIODevice* documentDevice = documentFile->createDevice();

    QDomDocument documentDom;
    QString errMsg;
    int errLine, errCol;
    if ( documentDom.setContent( documentDevice, true, &errMsg, &errLine, &errCol ) == false ) {
        // parse error
        kDebug() << "Could not parse XPS document: " << errMsg << " : "
                 << errLine << " : " << errCol << endl;
    }

    QDomNode node = documentDom.documentElement().firstChild();

    while( !node.isNull() ) {
        QDomElement element = node.toElement();
        if( !element.isNull() ) {
            if (element.tagName() == "PageContent") {
                QString pagePath = element.attribute("Source");
                if (pagePath.startsWith('/') == false ) {
                    int offset = fileName.lastIndexOf('/');
                    QString thisDir = fileName.mid(0,  offset) + '/';
                    pagePath.prepend(thisDir);
                }
                XpsPage *page = new XpsPage( archive, pagePath );
                m_pages.append(page);
            } else {
                kDebug() << "Unhandled entry in FixedDocument" << element.tagName() << endl;
            }
        }
        node = node.nextSibling();
    }

    delete documentDevice;
}

XpsDocument::~XpsDocument()
{
    for (int i = 0; i < m_pages.size(); i++) {
        delete m_pages.at( i );
    }
    m_pages.clear();
}

int XpsDocument::numPages() const
{
    return m_pages.size();
}

XpsPage* XpsDocument::page(int pageNum) const
{
    return m_pages.at(pageNum);
}

XpsFile::XpsFile() : m_docInfo( 0 )
{
}


XpsFile::~XpsFile()
{
}


bool XpsFile::loadDocument(const QString &filename)
{
    xpsArchive = new KZip( filename );
    if ( xpsArchive->open( QIODevice::ReadOnly ) == true ) {
        kDebug() << "Successful open of " << xpsArchive->fileName() << endl;
    } else {
        kDebug() << "Could not open XPS archive: " << xpsArchive->fileName() << endl;
        delete xpsArchive;
        return false;
    }

    // The only fixed entry in XPS is _rels/.rels
    const KZipFileEntry* relFile = static_cast<const KZipFileEntry *>(xpsArchive->directory()->entry("_rels/.rels"));

    if ( !relFile ) {
        // this might occur if we can't read the zip directory, or it doesn't have the relationships entry
        return false;
    }

    QIODevice* relDevice = relFile->createDevice();
    QDomDocument relDom;
    QString errMsg;
    int errLine, errCol;
    if ( relDom.setContent( relDevice, true, &errMsg, &errLine, &errCol ) == false ) {
        // parse error
        kDebug() << "Could not parse relationship document: " << errMsg << " : "
                     << errLine << " : " << errCol << endl;
        return false;
    }

    QString fixedRepresentationFileName;
    // We work through the relationships document and pull out each element.
    QDomNode n = relDom.documentElement().firstChild();
    while( !n.isNull() ) {
        QDomElement e = n.toElement();
        if( !e.isNull() ) {
            if ("http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail" == e.attribute("Type") ) {
                m_thumbnailFileName = e.attribute("Target");
            } else if ("http://schemas.microsoft.com/xps/2005/06/fixedrepresentation" == e.attribute("Type") ) {
                fixedRepresentationFileName = e.attribute("Target");
            } else if ("http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" == e.attribute("Type") ) {
                m_corePropertiesFileName = e.attribute("Target");
            } else {
                kDebug() << "Unknown relationships element: " << e.attribute("Type") << " : " << e.attribute("Target") << endl;
            }
        }
        n = n.nextSibling();
    }

       if ( fixedRepresentationFileName.isEmpty() ) {
        // FixedRepresentation is a required part of the XPS document
        return false;
    }

    delete relDevice;


    const KZipFileEntry* fixedRepFile = static_cast<const KZipFileEntry *>(xpsArchive->directory()->entry( fixedRepresentationFileName ));

    QIODevice* fixedRepDevice = fixedRepFile->createDevice();

    QDomDocument fixedRepDom;
    if ( fixedRepDom.setContent( fixedRepDevice, true, &errMsg, &errLine, &errCol ) == false ) {
        // parse error
        kDebug() << "Could not parse Fixed Representation document: " << errMsg << " : "
                     << errLine << " : " << errCol << endl;
        return false;
    }

    n = fixedRepDom.documentElement().firstChild();
    while( !n.isNull() ) {
        QDomElement e = n.toElement();
        if( !e.isNull() ) {
            if (e.tagName() == "DocumentReference") {
                XpsDocument *doc = new XpsDocument( xpsArchive, e.attribute("Source") );
                for (int lv = 0; lv < doc->numPages(); ++lv) {
                    // our own copy of the pages list
                    m_pages.append( doc->page( lv ) );
                }
                m_documents.append(doc);
            } else {
                kDebug() << "Unhandled entry in FixedDocumentSequence" << e.tagName() << endl;
            }
        }
        n = n.nextSibling();
    }

    delete fixedRepDevice;

    return true;
}

const Okular::DocumentInfo * XpsFile::generateDocumentInfo()
{
    if ( m_docInfo )
        return m_docInfo;

    m_docInfo = new Okular::DocumentInfo();

    m_docInfo->set( "mimeType", "application/vnd.ms-xpsdocument" );

    if ( ! m_corePropertiesFileName.isEmpty() ) {
        const KZipFileEntry* corepropsFile = static_cast<const KZipFileEntry *>(xpsArchive->directory()->entry(m_corePropertiesFileName));

        QDomDocument corePropertiesDocumentDom;
        QString errMsg;
        int errLine, errCol;

        QIODevice *corepropsDevice = corepropsFile->createDevice();

        if ( corePropertiesDocumentDom.setContent( corepropsDevice, true, &errMsg, &errLine, &errCol ) == false ) {
            // parse error
            kDebug() << "Could not parse core properties (metadata) document: " << errMsg << " : "
                     << errLine << " : " << errCol << endl;
            // return whatever we have
            return m_docInfo;
        }

        QDomNode n = corePropertiesDocumentDom.documentElement().firstChild(); // the <coreProperties> level
        while( !n.isNull() ) {
            QDomElement e = n.toElement();
            if( !e.isNull() ) {
                if (e.tagName() == "title") {
                    m_docInfo->set( "title", e.text(), i18n("Title") );
                } else if (e.tagName() == "subject") {
                    m_docInfo->set( "subject", e.text(), i18n("Subject") );
                } else if (e.tagName() == "description") {
                    m_docInfo->set( "description", e.text(), i18n("Description") );
                } else if (e.tagName() == "creator") {
                    m_docInfo->set( "creator", e.text(), i18n("Author") );
                } else if (e.tagName() == "created") {
                    QDateTime createdDate = QDateTime::fromString( e.text(), "yyyy-MM-ddThh:mm:ssZ" );
                    m_docInfo->set( "creationDate", KGlobal::locale()->formatDateTime( createdDate, false, true ), i18n("Created" ) );
                } else if (e.tagName() == "modified") {
                    QDateTime modifiedDate = QDateTime::fromString( e.text(), "yyyy-MM-ddThh:mm:ssZ" );
                    m_docInfo->set( "modifiedDate", KGlobal::locale()->formatDateTime( modifiedDate, false, true ), i18n("Modified" ) );
                } else if (e.tagName() == "keywords") {
                    m_docInfo->set( "keywords", e.text(), i18n("Keywords") );
                } else {
                    kDebug() << "unhandled metadata tag: " << e.tagName() << " : " << e.text() << endl;
                }
            }
            n = n.nextSibling();
        }

        delete corepropsDevice;
    } else {
        kDebug() << "No core properties filename" << endl;
    }

    m_docInfo->set( "pages", QString::number(numPages()), i18n("Pages") );

    return m_docInfo;
}

bool XpsFile::closeDocument()
{

    if ( m_docInfo )
        delete m_docInfo;

    m_docInfo = 0;

    for (int i = 0; i < m_documents.size(); i++) {
        delete m_documents.at( i );
    }

    m_documents.clear();

    delete xpsArchive;

    return true;
}

int XpsFile::numPages() const
{
    return m_pages.size();
}

int XpsFile::numDocuments() const
{
    return m_documents.size();
}

XpsDocument* XpsFile::document(int documentNum) const
{
    return m_documents.at( documentNum );
}

XpsPage* XpsFile::page(int pageNum) const
{
    return m_pages.at( pageNum );
}

XpsGenerator::XpsGenerator()
  : Okular::Generator()
{
    m_xpsFile = new XpsFile;
}

XpsGenerator::~XpsGenerator()
{
    delete m_xpsFile;
}

bool XpsGenerator::loadDocument( const QString & fileName, QVector<Okular::Page*> & pagesVector )
{
    delete m_xpsFile;
    m_xpsFile = new XpsFile();

    m_xpsFile->loadDocument( fileName );
    pagesVector.resize( m_xpsFile->numPages() );

    int pagesVectorOffset = 0;

    for (int docNum = 0; docNum < m_xpsFile->numDocuments(); ++docNum )
    {
        XpsDocument *doc = m_xpsFile->document( docNum );
        for (int pageNum = 0; pageNum < doc->numPages(); ++pageNum )
        {
            QSize pageSize = doc->page( pageNum )->size();
            pagesVector[pagesVectorOffset] = new Okular::Page( pagesVectorOffset, pageSize.width(), pageSize.height(), Okular::Rotation0 );
            ++pagesVectorOffset;
        }
    }

    return true;
}

bool XpsGenerator::closeDocument()
{
    m_xpsFile->closeDocument();

    return true;
}

QImage XpsGenerator::image( Okular::PixmapRequest * request )
{
    QSize size( (int)request->width(), (int)request->height() );
    QImage image( size, QImage::Format_RGB32 );
    XpsPage *pageToRender = m_xpsFile->page( request->page()->number() );
    pageToRender->renderToImage( &image );
    return image;
}

const Okular::DocumentInfo * XpsGenerator::generateDocumentInfo()
{
    kDebug() << "generating document metadata" << endl;

    return m_xpsFile->generateDocumentInfo();
}

XpsRenderNode * XpsRenderNode::findChild( const QString &name )
{
    for (int i = 0; i < children.size(); i++) {
        if (children[i].name == name) {
            return &children[i];
        }
    }

    return NULL;
}


#include "generator_xps.moc"

