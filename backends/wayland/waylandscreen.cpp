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

#include "waylandbackend.h"
#include "waylandconfig.h"
#include "waylandscreen.h"
#include "waylandoutput.h"

#include <configmonitor.h>
#include <mode.h>

// #include <QtCore/QFile>
// #include <QtCore/qplugin.h>
// #include <QtCore/QRect>
// #include <QAbstractEventDispatcher>

using namespace KScreen;

WaylandScreen::WaylandScreen(WaylandConfig *config)
    : QObject(config)
    , m_size(1600, 900)
    , m_outputCount(1) // FIXME

{
}

WaylandScreen::~WaylandScreen()
{
}

Screen* WaylandScreen::toKScreenScreen(Config* parent) const
{
    KScreen::Screen *kscreenScreen = new KScreen::Screen(parent);
    updateKScreenScreen(kscreenScreen);
    return kscreenScreen;
}

void WaylandScreen::setOutputs(const QList<WaylandOutput*> outputs)
{
    int width = 1600;
    int height = 900;
//     foreach (auto o, outputs) {
//         width += o->pixelSize().width();
//         qDebug() << "WW: " << width;
//     }
}


void WaylandScreen::updateKScreenScreen(Screen* screen) const
{
//     auto primary = QGuiApplication::primaryScreen();
//     QSize _s = primary->availableVirtualGeometry().size();
//
//     screen->setCurrentSize(_s);
//     screen->setId(1);
//     screen->setMaxSize(_s);
//     screen->setMinSize(_s);
//     screen->setCurrentSize(_s);
//     screen->setMaxActiveOutputsCount(QGuiApplication::screens().count());
    screen->setMinSize(m_size);
    screen->setMaxSize(m_size);
    screen->setCurrentSize(m_size);
    screen->setMaxActiveOutputsCount(m_outputCount); // FIXME
}

#include "waylandscreen.moc"

