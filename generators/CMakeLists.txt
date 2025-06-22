include (MacroLogFeature)

set(LIBSPECTRE_MINIMUM_VERSION "0.2")

macro_optional_find_package(PopplerQt4)
macro_log_feature(POPPLER_QT4_FOUND "Poppler-Qt4" "A PDF rendering library" "http://poppler.freedesktop.org" FALSE "0.60.1" "Support for PDF files in okular.")

macro_optional_find_package(LibSpectre)
macro_log_feature(LIBSPECTRE_FOUND "libspectre" "A PostScript rendering library" "http://libspectre.freedesktop.org/wiki/" FALSE "${LIBSPECTRE_MINIMUM_VERSION}" "Support for PS files in okular.")

find_package(Exiv2 0.21)
set_package_properties(Exiv2 PROPERTIES DESCRIPTION "Image Tag reader"
                       URL "http://www.exiv2.org" TYPE OPTIONAL
                       PURPOSE "Support image files in okular.")

macro_optional_find_package(CHM)
macro_log_feature(CHM_FOUND "CHM" "A library for dealing with Microsoft ITSS/CHM format files" "http://www.jedrea.com/chmlib" FALSE "" "Support CHM files in okular.")

macro_optional_find_package(DjVuLibre)
macro_log_feature(DJVULIBRE_FOUND "DjVuLibre" "A library for dealing with DjVu formatted files" "http://djvulibre.djvuzone.org" FALSE "3.5.17" "Support for DjVu files in okular.")

macro_optional_find_package(TIFF)
macro_log_feature(TIFF_FOUND "libTIFF" "A library for reading and writing TIFF formatted files," "http://www.remotesensing.org/libtiff" FALSE "" "Support for TIFF files in okular.")

macro_optional_find_package(Freetype)
macro_log_feature(FREETYPE_FOUND "FreeType" "A font rendering engine" "http://www.freetype.org" FALSE "" "Provides freetype font support in the okular DVI generator.")

macro_optional_find_package(EPub)
macro_log_feature(EPUB_FOUND "libepub" "A library for reading EPub documents" "http://sourceforge.net/projects/ebook-tools" FALSE "" "Support for EPub documents in Okular.")

# let's enable the generators properly configured

if(POPPLER_QT4_FOUND)
  add_subdirectory(poppler)
endif(POPPLER_QT4_FOUND)

if(LIBSPECTRE_FOUND)
  add_subdirectory(spectre)
endif(LIBSPECTRE_FOUND)

if(EXIV2_FOUND)
  add_subdirectory( kimgio )
endif(EXIV2_FOUND)

if(CHM_FOUND)
  add_subdirectory( chm )
endif(CHM_FOUND)

if(DJVULIBRE_FOUND)
  add_subdirectory(djvu)
endif(DJVULIBRE_FOUND)

add_subdirectory(dvi)

if(TIFF_FOUND)
  add_subdirectory(tiff)
endif(TIFF_FOUND)

add_subdirectory(xps)

add_subdirectory(ooo)

add_subdirectory(fictionbook)

add_subdirectory(comicbook)

add_subdirectory(fax)

if(EPUB_FOUND)
  add_subdirectory(epub)
endif(EPUB_FOUND)

add_subdirectory(txt)
