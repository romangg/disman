/*************************************************************************************
 *  Copyright 2014 Sebastian Kügler <sebas@kde.org>                                  *
 *                                                                                   *
 *  This library is free software; you can redistribute it and/or                    *
 *  modify it under the terms of the GNU Lesser General Public                       *
 *  License as published by the Free Software Foundation; either                     *
 *  version 2.1 of the License, or (at your option) any later version.               *
 *                                                                                   *
 *  This library is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU                *
 *  Lesser General Public License for more details.                                  *
 *                                                                                   *
 *  You should have received a copy of the GNU Lesser General Public                 *
 *  License along with this library; if not, write to the Free Software              *
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA       *
 *************************************************************************************/
#include "qscreenscreen.h"

#include "qscreenbackend.h"
#include "qscreenoutput.h"

#include <configmonitor.h>
#include <mode.h>

#include <QGuiApplication>
#include <QScreen>

using namespace Disman;

QScreenScreen::QScreenScreen(QScreenConfig* config)
    : QObject(config)
{
}

QScreenScreen::~QScreenScreen()
{
}

ScreenPtr QScreenScreen::toDismanScreen() const
{
    Disman::ScreenPtr dismanScreen(new Disman::Screen);
    updateDismanScreen(dismanScreen);
    return dismanScreen;
}

void QScreenScreen::updateDismanScreen(ScreenPtr& screen) const
{
    if (!screen) {
        return;
    }

    auto primary = QGuiApplication::primaryScreen();

    if (primary) {
        QSize _s = primary->availableVirtualGeometry().size();

        screen->set_current_size(_s);
        screen->set_id(1);
        screen->set_max_size(_s);
        screen->set_min_size(_s);
        screen->set_current_size(_s);
        screen->set_max_outputs_count(QGuiApplication::screens().count());
    }
}
