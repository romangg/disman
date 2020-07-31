/*************************************************************************************
 *  Copyright 2014 by Sebastian Kügler <sebas@kde.org>                               *
 *                                                                                   *
 *  This program is free software; you can redistribute it and/or                    *
 *  modify it under the terms of the GNU General Public License                      *
 *  as published by the Free Software Foundation; either version 2                   *
 *  of the License, or (at your option) any later version.                           *
 *                                                                                   *
 *  This program is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 *  GNU General Public License for more details.                                     *
 *                                                                                   *
 *  You should have received a copy of the GNU General Public License                *
 *  along with this program; if not, write to the Free Software                      *
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
 *************************************************************************************/
#include "testpnp.h"

#include <QCommandLineParser>
#include <QGuiApplication>

using namespace Disman;

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    QCommandLineOption input
        = QCommandLineOption(QStringList() << QStringLiteral("m") << QStringLiteral("monitor"),
                             QStringLiteral("Keep running monitoring for changes"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption(input);
    parser.process(app);

    new TestPnp(parser.isSet(input));
    return app.exec();
}
