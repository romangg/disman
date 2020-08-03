/*************************************************************************************
 *  Copyright 2015 by Sebastian Kügler <sebas@kde.org>                           *
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
#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QObject>
#include <QSignalSpy>
#include <QtTest>

#include "../src/backendmanager_p.h"
#include "../src/config.h"
#include "../src/configmonitor.h"
#include "../src/edid.h"
#include "../src/getconfigoperation.h"
#include "../src/mode.h"
#include "../src/output.h"
#include "../src/setconfigoperation.h"

Q_LOGGING_CATEGORY(DISMAN, "disman")

using namespace Disman;

class TestInProcess : public QObject
{
    Q_OBJECT

public:
    explicit TestInProcess(QObject* parent = nullptr);

private Q_SLOTS:
    void initTestCase();

    void init();
    void cleanup();

    void loadConfig();

    void testCreateJob();
    void testModeSwitching();
    void testBackendCaching();

    void testConfigApply();
    void testConfigMonitor();

private:
    ConfigPtr m_config;
    bool m_backendServiceInstalled = false;
};

TestInProcess::TestInProcess(QObject* parent)
    : QObject(parent)
    , m_config(nullptr)
{
}

void TestInProcess::initTestCase()
{
    m_backendServiceInstalled = true;

    const QString dismanServiceName = QStringLiteral("Disman");
    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    if (!bus->isServiceRegistered(dismanServiceName)) {
        auto reply = bus->startService(dismanServiceName);
        if (!reply.isValid()) {
            qDebug() << "D-Bus service Disman could not be started, skipping out-of-process tests";
            m_backendServiceInstalled = false;
        }
    }
}

void TestInProcess::init()
{
    qputenv("DISMAN_LOGGING", "false");
    // Make sure we do everything in-process
    qputenv("DISMAN_BACKEND_INPROCESS", "1");
    // Use Fake backend with one of the json configs
    qputenv("DISMAN_BACKEND", "Fake");
    qputenv("DISMAN_BACKEND_ARGS", "TEST_DATA=" TEST_DATA "multipleoutput.json");

    Disman::BackendManager::instance()->shutdownBackend();
}

void TestInProcess::cleanup()
{
    Disman::BackendManager::instance()->shutdownBackend();
}

void TestInProcess::loadConfig()
{
    qputenv("DISMAN_BACKEND_INPROCESS", "1");
    BackendManager::instance()->setMethod(BackendManager::InProcess);

    auto* op = new GetConfigOperation();
    QVERIFY(op->exec());
    m_config = op->config();
    QVERIFY(m_config);
    QVERIFY(m_config->isValid());
}

void TestInProcess::testModeSwitching()
{
    Disman::BackendManager::instance()->shutdownBackend();
    BackendManager::instance()->setMethod(BackendManager::InProcess);
    // Load QScreen backend in-process
    qDebug() << "TT qscreen in-process";
    qputenv("DISMAN_BACKEND", "QScreen");
    auto op = new GetConfigOperation();
    QVERIFY(op->exec());
    auto oc = op->config();
    QVERIFY(oc != nullptr);
    QVERIFY(oc->isValid());

    qDebug() << "TT fake in-process";
    // Load the Fake backend in-process
    qputenv("DISMAN_BACKEND", "Fake");
    auto ip = new GetConfigOperation();
    QVERIFY(ip->exec());
    auto ic = ip->config();
    QVERIFY(ic != nullptr);
    QVERIFY(ic->isValid());
    QVERIFY(ic->outputs().count());

    Disman::ConfigPtr xc(nullptr);
    if (m_backendServiceInstalled) {
        qDebug() << "TT xrandr out-of-process";
        // Load the xrandr backend out-of-process
        qputenv("DISMAN_BACKEND", "QScreen");
        qputenv("DISMAN_BACKEND_INPROCESS", "0");
        BackendManager::instance()->setMethod(BackendManager::OutOfProcess);
        auto xp = new GetConfigOperation();
        QCOMPARE(BackendManager::instance()->method(), BackendManager::OutOfProcess);
        QVERIFY(xp->exec());
        xc = xp->config();
        QVERIFY(xc != nullptr);
        QVERIFY(xc->isValid());
        QVERIFY(xc->outputs().count());
    }

    qDebug() << "TT fake in-process";

    qputenv("DISMAN_BACKEND_INPROCESS", "1");
    BackendManager::instance()->setMethod(BackendManager::InProcess);
    // Load the Fake backend in-process
    qputenv("DISMAN_BACKEND", "Fake");
    auto fp = new GetConfigOperation();
    QCOMPARE(BackendManager::instance()->method(), BackendManager::InProcess);
    QVERIFY(fp->exec());
    auto fc = fp->config();
    QVERIFY(fc != nullptr);
    QVERIFY(fc->isValid());
    QVERIFY(fc->outputs().count());

    QVERIFY(oc->isValid());
    QVERIFY(ic->isValid());
    if (xc) {
        QVERIFY(xc->isValid());
    }
    QVERIFY(fc->isValid());
}

void TestInProcess::testBackendCaching()
{
    Disman::BackendManager::instance()->shutdownBackend();
    qputenv("DISMAN_BACKEND", "Fake");
    QElapsedTimer t;
    BackendManager::instance()->setMethod(BackendManager::InProcess);
    QCOMPARE(BackendManager::instance()->method(), BackendManager::InProcess);
    int t_cold;
    int t_warm;

    {
        t.start();
        auto cp = new GetConfigOperation();
        cp->exec();
        auto cc = cp->config();
        t_cold = t.nsecsElapsed();
        QVERIFY(cc != nullptr);
        QVERIFY(cc->isValid());
        QVERIFY(cc->outputs().count());
    }
    {
        // Disman::BackendManager::instance()->shutdownBackend();
        QCOMPARE(BackendManager::instance()->method(), BackendManager::InProcess);
        t.start();
        auto cp = new GetConfigOperation();
        cp->exec();
        auto cc = cp->config();
        t_warm = t.nsecsElapsed();
        QVERIFY(cc != nullptr);
        QVERIFY(cc->isValid());
        QVERIFY(cc->outputs().count());
    }
    {
        auto cp = new GetConfigOperation();
        QCOMPARE(BackendManager::instance()->method(), BackendManager::InProcess);
        cp->exec();
        auto cc = cp->config();
        QVERIFY(cc != nullptr);
        QVERIFY(cc->isValid());
        QVERIFY(cc->outputs().count());
    }
    // Check if all our configs are still valid after the backend is gone
    Disman::BackendManager::instance()->shutdownBackend();

    if (m_backendServiceInstalled) {
        // qputenv("DISMAN_BACKEND", "QScreen");
        qputenv("DISMAN_BACKEND_INPROCESS", "0");
        BackendManager::instance()->setMethod(BackendManager::OutOfProcess);
        QCOMPARE(BackendManager::instance()->method(), BackendManager::OutOfProcess);
        int t_x_cold;

        {
            t.start();
            auto xp = new GetConfigOperation();
            xp->exec();
            t_x_cold = t.nsecsElapsed();
            auto xc = xp->config();
            QVERIFY(xc != nullptr);
        }
        t.start();
        auto xp = new GetConfigOperation();
        xp->exec();
        int t_x_warm = t.nsecsElapsed();
        auto xc = xp->config();
        QVERIFY(xc != nullptr);

        // Make sure in-process is faster
        QVERIFY(t_cold > t_warm);
        QVERIFY(t_x_cold > t_x_warm);
        QVERIFY(t_x_cold > t_cold);
        return;
        qDebug() << "ip  speedup for cached access:" << (qreal)((qreal)t_cold / (qreal)t_warm);
        qDebug() << "oop speedup for cached access:" << (qreal)((qreal)t_x_cold / (qreal)t_x_warm);
        qDebug() << "out-of vs. in-process speedup:" << (qreal)((qreal)t_x_warm / (qreal)t_warm);
        qDebug() << "cold oop:   " << ((qreal)t_x_cold / 1000000);
        qDebug() << "cached oop: " << ((qreal)t_x_warm / 1000000);
        qDebug() << "cold in process:   " << ((qreal)t_cold / 1000000);
        qDebug() << "cached in process: " << ((qreal)t_warm / 1000000);
    }
}

void TestInProcess::testCreateJob()
{
    Disman::BackendManager::instance()->shutdownBackend();
    {
        BackendManager::instance()->setMethod(BackendManager::InProcess);
        auto op = new GetConfigOperation();
        auto _op = qobject_cast<GetConfigOperation*>(op);
        QVERIFY(_op != nullptr);
        QCOMPARE(BackendManager::instance()->method(), BackendManager::InProcess);
        QVERIFY(op->exec());
        auto cc = op->config();
        QVERIFY(cc != nullptr);
        QVERIFY(cc->isValid());
    }
    if (m_backendServiceInstalled) {
        BackendManager::instance()->setMethod(BackendManager::OutOfProcess);
        auto op = new GetConfigOperation();
        auto _op = qobject_cast<GetConfigOperation*>(op);
        QVERIFY(_op != nullptr);
        QCOMPARE(BackendManager::instance()->method(), BackendManager::OutOfProcess);
        QVERIFY(op->exec());
        auto cc = op->config();
        QVERIFY(cc != nullptr);
        QVERIFY(cc->isValid());
    }
    Disman::BackendManager::instance()->shutdownBackend();
    BackendManager::instance()->setMethod(BackendManager::InProcess);
}

void TestInProcess::testConfigApply()
{
    qputenv("DISMAN_BACKEND", "Fake");
    Disman::BackendManager::instance()->shutdownBackend();
    BackendManager::instance()->setMethod(BackendManager::InProcess);
    auto op = new GetConfigOperation();
    op->exec();
    auto config = op->config();
    //     qDebug() << "op:" << config->outputs().count();
    auto output = config->outputs().first();
    //     qDebug() << "res:" << output->geometry();
    //     qDebug() << "modes:" << output->modes();
    auto m0 = output->modes().first();
    // qDebug() << "m0:" << m0->id() << m0;
    output->set_mode(m0);
    QVERIFY(Config::canBeApplied(config));

    // expected to fail, SetConfigOperation is out-of-process only
    auto setop = new SetConfigOperation(config);
    QVERIFY(!setop->hasError());
    QVERIFY(setop->exec());

    QVERIFY(!setop->hasError());
}

void TestInProcess::testConfigMonitor()
{
    qputenv("DISMAN_BACKEND", "Fake");

    Disman::BackendManager::instance()->shutdownBackend();
    BackendManager::instance()->setMethod(BackendManager::InProcess);
    auto op = new GetConfigOperation();
    op->exec();
    auto config = op->config();
    //     qDebug() << "op:" << config->outputs().count();
    auto output = config->outputs().first();
    //     qDebug() << "res:" << output->geometry();
    //     qDebug() << "modes:" << output->modes();
    auto m0 = output->modes().first();
    // qDebug() << "m0:" << m0->id() << m0;
    output->set_mode(m0);
    QVERIFY(Config::canBeApplied(config));

    QSignalSpy monitorSpy(ConfigMonitor::instance(), &ConfigMonitor::configurationChanged);
    qDebug() << "MOnitorspy connencted.";
    ConfigMonitor::instance()->addConfig(config);

    auto setop = new SetConfigOperation(config);
    QVERIFY(!setop->hasError());
    // do not cal setop->exec(), this must not block as the signalspy already blocks
    QVERIFY(monitorSpy.wait(500));
}

QTEST_GUILESS_MAIN(TestInProcess)

#include "testinprocess.moc"
