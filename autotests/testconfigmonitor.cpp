/*
 * Copyright (C) 2014  Daniel Vratil <dvratil@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <QObject>
#include <QSignalSpy>
#include <QtTest>

#include "../src/backendmanager_p.h"
#include "../src/config.h"
#include "../src/configmonitor.h"
#include "../src/configoperation.h"
#include "../src/getconfigoperation.h"
#include "../src/output.h"
#include "../src/setconfigoperation.h"
#include <QSignalSpy>

#include "fakebackendinterface.h"

class TestConfigMonitor : public QObject
{
    Q_OBJECT
public:
    TestConfigMonitor()
    {
    }

    Disman::ConfigPtr getConfig()
    {
        auto op = new Disman::GetConfigOperation();
        if (!op->exec()) {
            qWarning("Failed to retrieve backend: %s", qPrintable(op->errorString()));
            return Disman::ConfigPtr();
        }

        return op->config();
    }

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("DISMAN_LOGGING", "false");
        qputenv("DISMAN_BACKEND", "Fake");
        // This particular test is only useful for out of process operation, so enforce that
        qputenv("DISMAN_BACKEND_INPROCESS", "0");
        Disman::BackendManager::instance()->shutdownBackend();
    }

    void cleanupTestCase()
    {
        Disman::BackendManager::instance()->shutdownBackend();
    }

    void testChangeNotifyInProcess()
    {
        qputenv("DISMAN_BACKEND_INPROCESS", "1");
        Disman::BackendManager::instance()->shutdownBackend();
        Disman::BackendManager::instance()->setMethod(Disman::BackendManager::InProcess);
        // json file for the fake backend
        qputenv("DISMAN_BACKEND_ARGS", "TEST_DATA=" TEST_DATA "singleoutput.json");

        // Prepare monitor
        Disman::ConfigMonitor* monitor = Disman::ConfigMonitor::instance();
        QSignalSpy spy(monitor, SIGNAL(configurationChanged()));

        // Get config and monitor it for changes
        Disman::ConfigPtr config = getConfig();
        monitor->addConfig(config);

        auto output = config->outputs().first();

        output->setEnabled(false);
        auto setop = new Disman::SetConfigOperation(config);
        QVERIFY(!setop->hasError());
        setop->exec();
        QTRY_VERIFY(!spy.isEmpty());

        QCOMPARE(spy.size(), 1);
        QCOMPARE(config->output(1)->isEnabled(), false);

        output->setEnabled(false);
        auto setop2 = new Disman::SetConfigOperation(config);
        QVERIFY(!setop2->hasError());
        setop2->exec();
        QTRY_VERIFY(!spy.isEmpty());
        QCOMPARE(spy.size(), 2);
    }
};

QTEST_MAIN(TestConfigMonitor)

#include "testconfigmonitor.moc"
