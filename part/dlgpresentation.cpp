/***************************************************************************
 *   Copyright (C) 2006,2008 by Pino Toscano <pino@kde.org>                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "dlgpresentation.h"

#include "widgetdrawingtools.h"

#include <KColorButton>
#include <KLocalizedString>
#include <KPluralHandlingSpinBox>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QScreen>

#include "settings.h"

DlgPresentation::DlgPresentation(QWidget *parent)
    : QWidget(parent)
{
    QFormLayout *layout = new QFormLayout(this);

    // BEGIN Navigation section
    // Checkbox and spinbox: advance automatically, interval
    QCheckBox *advanceAutomatically = new QCheckBox(this);
    advanceAutomatically->setText(i18nc("@option:check Config dialog, presentation page", "Advance automatically"));
    advanceAutomatically->setObjectName(QStringLiteral("kcfg_SlidesAdvance"));
    layout->addRow(QString(), advanceAutomatically);

    KPluralHandlingSpinBox *advanceTime = new KPluralHandlingSpinBox(this);
    advanceTime->setSuffix(ki18ncp("Advance every %1 seconds", " second", " seconds"));
    advanceTime->setObjectName(QStringLiteral("kcfg_SlidesAdvanceTime"));
    layout->addRow(i18nc("@label:spinbox Config dialog, presentation page", "Advance every:"), advanceTime);

    advanceAutomatically->setChecked(false);
    advanceTime->setEnabled(false);
    connect(advanceAutomatically, &QCheckBox::toggled, advanceTime, &KPluralHandlingSpinBox::setEnabled);

    // Checkbox: Loop after last page
    QCheckBox *loopAfterLastPage = new QCheckBox(this);
    loopAfterLastPage->setText(i18nc("@option:check Config dialog, presentation page", "Loop after last page"));
    loopAfterLastPage->setObjectName(QStringLiteral("kcfg_SlidesLoop"));
    layout->addRow(QString(), loopAfterLastPage);

    // Combobox: Touch navigation
    QComboBox *tapNavigation = new QComboBox(this);
    tapNavigation->addItem(i18nc("@item:inlistbox Config dialog, presentation page, tap navigation", "Tap left/right side to go back/forward"));
    tapNavigation->addItem(i18nc("@item:inlistbox Config dialog, presentation page, tap navigation", "Tap anywhere to go forward"));
    tapNavigation->addItem(i18nc("@item:inlistbox Config dialog, presentation page, tap navigation", "Disabled"));
    tapNavigation->setObjectName(QStringLiteral("kcfg_SlidesTapNavigation"));
    layout->addRow(i18nc("@label:listbox Config dialog, presentation page, tap navigation", "Touch navigation:"), tapNavigation);
    // END Navigation section

    layout->addRow(new QLabel(this));

    // BEGIN Appearance section
    // Color button: Background color
    KColorButton *backgroundColor = new KColorButton(this);
    backgroundColor->setObjectName(QStringLiteral("kcfg_SlidesBackgroundColor"));
    layout->addRow(i18nc("@label:chooser Config dialog, presentation page", "Background color:"), backgroundColor);

    // Combobox: Cursor visibility
    QComboBox *cursorVisibility = new QComboBox(this);
    cursorVisibility->addItem(i18nc("@item:inlistbox Config dialog, presentation page, cursor visibility", "Hidden after delay"));
    cursorVisibility->addItem(i18nc("@item:inlistbox Config dialog, presentation page, cursor visibility", "Always visible"));
    cursorVisibility->addItem(i18nc("@item:inlistbox Config dialog, presentation page, cursor visibility", "Always hidden"));
    cursorVisibility->setObjectName(QStringLiteral("kcfg_SlidesCursor"));
    layout->addRow(i18nc("@label:listbox Config dialog, presentation page, cursor visibility", "Mouse cursor:"), cursorVisibility);

    // Checkbox: Show progress indicator
    QCheckBox *showProgressIndicator = new QCheckBox(this);
    showProgressIndicator->setText(i18nc("@option:check Config dialog, presentation page", "Show progress indicator"));
    showProgressIndicator->setObjectName(QStringLiteral("kcfg_SlidesShowProgress"));
    layout->addRow(QString(), showProgressIndicator);

    // Checkbox: Show summary page
    QCheckBox *showSummaryPage = new QCheckBox(this);
    showSummaryPage->setText(i18nc("@option:check Config dialog, presentation page", "Show summary page"));
    showSummaryPage->setObjectName(QStringLiteral("kcfg_SlidesShowSummary"));
    layout->addRow(QString(), showSummaryPage);
    // END Appearance section

    layout->addRow(new QLabel(this));

    // BEGIN Transitions section
    // Checkbox: Enable transitions
    QCheckBox *enableTransitions = new QCheckBox(this);
    enableTransitions->setText(i18nc("@option:check Config dialog, presentation page, transitions", "Enable transitions"));
    enableTransitions->setObjectName(QStringLiteral("kcfg_SlidesTransitionsEnabled"));
    layout->addRow(QString(), enableTransitions);

    // Combobox: Default transition
    QComboBox *defaultTransition = new QComboBox(this);
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Blinds vertical"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Blinds horizontal"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Box in"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Box out"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Dissolve"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Fade"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Glitter down"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Glitter right"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Glitter right-down"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Random transition"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Replace"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Split horizontal in"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Split horizontal out"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Split vertical in"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Split vertical out"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Wipe down"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Wipe right"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Wipe left"));
    defaultTransition->addItem(i18nc("@item:inlistbox Config dialog, presentation page, transitions", "Wipe up"));
    defaultTransition->setObjectName(QStringLiteral("kcfg_SlidesTransition"));
    layout->addRow(i18nc("@label:listbox Config dialog, presentation page, transitions", "Default transition:"), defaultTransition);
    defaultTransition->setEnabled(Okular::Settings::slidesTransitionsEnabled());
    connect(enableTransitions, &QCheckBox::toggled, defaultTransition, &QComboBox::setEnabled);
    // END Transitions section

    layout->addRow(new QLabel(this));

    // BEGIN Placement section
    // Combobox: Preferred screen
    PreferredScreenSelector *preferredScreen = new PreferredScreenSelector(this);
    preferredScreen->setObjectName(QStringLiteral("kcfg_SlidesScreen"));
    layout->addRow(i18nc("@label:listbox Config dialog, presentation page, preferred screen", "Preferred screen:"), preferredScreen);
    // END Placement section

    layout->addRow(new QLabel(this));

    // BEGIN Drawing tools section: WidgetDrawingTools manages drawing tools.
    QLabel *toolsLabel = new QLabel(this);
    toolsLabel->setText(i18nc("@label Config dialog, presentation page, heading line for drawing tool manager", "<h3>Drawing Tools</h3>"));
    layout->addRow(toolsLabel);

    WidgetDrawingTools *kcfg_DrawingTools = new WidgetDrawingTools(this);
    kcfg_DrawingTools->setObjectName(QStringLiteral("kcfg_DrawingTools"));

    layout->addRow(kcfg_DrawingTools);
    // END Drawing tools section
}

PreferredScreenSelector::PreferredScreenSelector(QWidget *parent)
    : QComboBox(parent)
    , m_disconnectedScreenNumber(k_noDisconnectedScreenNumber)
{
    // Populate list:
    static_assert(k_specialScreenCount == 2, "Special screens unknown to PreferredScreenSelector constructor.");
    addItem(i18nc("@item:inlistbox Config dialog, presentation page, preferred screen", "Current Screen"));
    addItem(i18nc("@item:inlistbox Config dialog, presentation page, preferred screen", "Default Screen"));

    const QList<QScreen *> screens = qApp->screens();
    for (int screenNumber = 0; screenNumber < screens.count(); ++screenNumber) {
        QScreen *screen = screens.at(screenNumber);
        addItem(i18nc("@item:inlistbox Config dialog, presentation page, preferred screen. %1 is the screen number (0, 1, ...). %2 is the screen manufacturer name. %3 is the screen model name. %4 is the screen name like DVI-0",
                      "Screen %1 (%2 %3 %4)",
                      screenNumber,
                      screen->manufacturer(),
                      screen->model(),
                      screen->name()));
    }

    // If a disconnected screen is configured, it will be appended last:
    m_disconnectedScreenIndex = count();

    // KConfigWidgets setup:
    setProperty("kcfg_property", QByteArray("preferredScreen"));
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) { emit preferredScreenChanged(index - k_specialScreenCount); });
}

void PreferredScreenSelector::setPreferredScreen(int newScreen)
{
    // Check whether the new screen is not in the list of connected screens:
    if (newScreen >= m_disconnectedScreenIndex - k_specialScreenCount) {
        if (m_disconnectedScreenNumber == k_noDisconnectedScreenNumber) {
            addItem(QString());
        }
        setItemText(m_disconnectedScreenIndex, i18nc("@item:inlistbox Config dialog, presentation page, preferred screen. %1 is the screen number (0, 1, ...), hopefully not 0.", "Screen %1 (disconnected)", newScreen));
        setCurrentIndex(m_disconnectedScreenIndex);
        m_disconnectedScreenNumber = newScreen;
        return;
    }

    setCurrentIndex(newScreen + k_specialScreenCount);

    // screenChanged() is emitted through currentIndexChanged().
}

int PreferredScreenSelector::preferredScreen() const
{
    if (currentIndex() == m_disconnectedScreenIndex) {
        return m_disconnectedScreenNumber;
    } else {
        return currentIndex() - k_specialScreenCount;
    }
}
