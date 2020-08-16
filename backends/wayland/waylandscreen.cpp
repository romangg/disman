/*************************************************************************************
 *  Copyright 2014-2015 Sebastian Kügler <sebas@kde.org>                             *
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
#include "waylandscreen.h"

#include "waylandoutput.h"

#include <mode.h>

#include <QRect>

using namespace Disman;

WaylandScreen::WaylandScreen(QObject* parent)
    : QObject(parent)
    , m_outputCount(0)
{
}

ScreenPtr WaylandScreen::toDismanScreen(Disman::ConfigPtr& parent) const
{
    Q_UNUSED(parent);

    Disman::ScreenPtr dismanScreen(new Disman::Screen);
    updateDismanScreen(dismanScreen);
    return dismanScreen;
}

void WaylandScreen::setOutputs(const QList<WaylandOutput*>& outputs)
{
    m_outputCount = outputs.count();

    QRect r;
    for (const auto* out : outputs) {
        if (out->enabled()) {
            r |= out->geometry().toRect();
        }
    }
    m_size = r.size();
}

void WaylandScreen::updateDismanScreen(Disman::ScreenPtr& screen) const
{
    screen->setMinSize(QSize(0, 0));

    // 64000^2 should be enough for everyone.
    screen->setMaxSize(QSize(64000, 64000));

    screen->setCurrentSize(m_size);
    screen->setMaxActiveOutputsCount(m_outputCount);
}
