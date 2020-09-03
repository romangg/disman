/*
    SPDX-FileCopyrightText: 2014-2016 by Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "doctor.h"

#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRectF>

#include "backendmanager_p.h"
#include "config.h"
#include "configoperation.h"
#include "getconfigoperation.h"
#include "log.h"
#include "setconfigoperation.h"

Q_LOGGING_CATEGORY(DISMAN_CTL, "disman.ctl")

static QTextStream cout(stdout);
static QTextStream cerr(stderr);

const static QString green = QStringLiteral("\033[01;32m");
const static QString red = QStringLiteral("\033[01;31m");
const static QString yellow = QStringLiteral("\033[01;33m");
const static QString blue = QStringLiteral("\033[01;34m");
const static QString bold = QStringLiteral("\033[01;39m");
const static QString cr = QStringLiteral("\033[0;0m");

namespace Disman::ConfigSerializer
{
// Exported private symbol in configserializer_p.h in Disman
extern QJsonObject serializeConfig(const Disman::ConfigPtr& config);
}

namespace Disman::Ctl
{

Doctor::Doctor(QCommandLineParser* parser, QObject* parent)
    : QObject(parent)
    , m_config(nullptr)
    , m_parser(parser)
    , m_changed(false)
{
    if (m_parser->optionNames().isEmpty() && m_parser->positionalArguments().isEmpty()) {
        // When dismanctl was launched without any parameter show help and quit.
        m_parser->showHelp(1);
    }
    if (m_parser->isSet(QStringLiteral("info"))) {
        showBackends();
    }
    if (parser->isSet(QStringLiteral("json")) || parser->isSet(QStringLiteral("outputs"))
        || !m_parser->positionalArguments().isEmpty()) {

        Disman::GetConfigOperation* op = new Disman::GetConfigOperation();
        connect(op,
                &Disman::GetConfigOperation::finished,
                this,
                [this](Disman::ConfigOperation* op) { configReceived(op); });
        return;
    }

    if (m_parser->isSet(QStringLiteral("log"))) {

        const QString logmsg = m_parser->value(QStringLiteral("log"));
        if (!Log::instance()->enabled()) {
            qCWarning(DISMAN_CTL)
                << "Logging is disabled, unset DISMAN_LOGGING in your environment.";
        } else {
            Log::log(logmsg);
        }
    }
    // We need to kick the event loop, otherwise .quit() hangs
    QTimer::singleShot(0, qApp->quit);
}

void Doctor::showBackends() const
{
    cout << "Environment: " << Qt::endl;
    auto env_disman_backend = (qgetenv("DISMAN_BACKEND").isEmpty())
        ? QStringLiteral("[not set]")
        : QString::fromUtf8(qgetenv("DISMAN_BACKEND"));
    cout << "  * DISMAN_BACKEND       : " << env_disman_backend << Qt::endl;
    auto env_disman_backend_inprocess = (qgetenv("DISMAN_IN_PROCESS").isEmpty())
        ? QStringLiteral("[not set]")
        : QString::fromUtf8(qgetenv("DISMAN_IN_PROCESS"));
    cout << "  * DISMAN_IN_PROCESS    : " << env_disman_backend_inprocess << Qt::endl;
    auto env_disman_logging = (qgetenv("DISMAN_LOGGING").isEmpty())
        ? QStringLiteral("[not set]")
        : QString::fromUtf8(qgetenv("DISMAN_LOGGING"));
    cout << "  * DISMAN_LOGGING       : " << env_disman_logging << Qt::endl;

    cout << "Logging to               : "
         << (Log::instance()->enabled() ? Log::instance()->logFile()
                                        : QStringLiteral("[logging disabled]"))
         << Qt::endl;
    auto backends = BackendManager::instance()->listBackends();
    auto preferred = BackendManager::instance()->preferred_backend();
    cout << "Preferred Disman backend : " << green << preferred.fileName() << cr << Qt::endl;
    cout << "Available Disman backends:" << Qt::endl;
    for (auto const file_info : backends) {
        auto c = blue;
        if (preferred == file_info) {
            c = green;
        }
        cout << "  * " << c << file_info.fileName() << cr << ": " << file_info.absoluteFilePath()
             << Qt::endl;
    }
    cout << Qt::endl;
}

void Doctor::parsePositionalArgs()
{
    for (auto const& op : m_parser->positionalArguments()) {
        auto ops = op.split(QLatin1Char('.'));
        if (ops.count() > 2) {
            bool ok;
            int output_id = -1;
            if (ops[0] == QLatin1String("output")) {
                for (auto const& output : m_config->outputs()) {
                    if (output->name() == ops[1].toStdString()) {
                        output_id = output->id();
                    }
                }
                if (output_id == -1) {
                    output_id = ops[1].toInt(&ok);
                    if (!ok) {
                        cerr << "Unable to parse output id: " << ops[1] << Qt::endl;
                        qApp->exit(3);
                        return;
                    }
                }
                if (ops.count() == 3 && ops[2] == QLatin1String("enable")) {
                    if (!setEnabled(output_id, true)) {
                        qApp->exit(1);
                        return;
                    };
                } else if (ops.count() == 3 && ops[2] == QLatin1String("disable")) {
                    if (!setEnabled(output_id, false)) {
                        qApp->exit(1);
                        return;
                    };
                } else if (ops.count() == 4 && ops[2] == QLatin1String("mode")) {
                    QString mode_id = ops[3];
                    // set mode
                    if (!setMode(output_id, mode_id)) {
                        qApp->exit(9);
                        return;
                    }
                    qCDebug(DISMAN_CTL) << "Output" << output_id << "set mode" << mode_id;

                } else if (ops.count() == 4 && ops[2] == QLatin1String("position")) {
                    QStringList _pos = ops[3].split(QLatin1Char(','));
                    if (_pos.count() != 2) {
                        qCWarning(DISMAN_CTL) << "Invalid position:" << ops[3];
                        qApp->exit(5);
                        return;
                    }
                    int x = _pos[0].toInt(&ok);
                    int y = _pos[1].toInt(&ok);
                    if (!ok) {
                        cerr << "Unable to parse position: " << ops[3] << Qt::endl;
                        qApp->exit(5);
                        return;
                    }

                    QPoint p(x, y);
                    qCDebug(DISMAN_CTL) << "Output position" << p;
                    if (!setPosition(output_id, p)) {
                        qApp->exit(1);
                        return;
                    }
                } else if ((ops.count() == 4 || ops.count() == 5)
                           && ops[2] == QLatin1String("scale")) {
                    // be lenient about . vs. comma as separator
                    qreal scale = ops[3].replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
                    if (ops.count() == 5) {
                        const QString dbl = ops[3] + QLatin1String(".") + ops[4];
                        scale = dbl.toDouble(&ok);
                    };
                    // set scale
                    if (!ok || qFuzzyCompare(scale, 0.0) || !setScale(output_id, scale)) {
                        qCDebug(DISMAN_CTL)
                            << "Could not set scale " << scale << " to output " << output_id;
                        qApp->exit(9);
                        return;
                    }
                } else if ((ops.count() == 4)
                           && (ops[2] == QLatin1String("orientation")
                               || ops[2] == QStringLiteral("rotation"))) {
                    const QString _rotation = ops[3].toLower();
                    bool ok = false;
                    const QHash<QString, Disman::Output::Rotation> rotationMap(
                        {{QStringLiteral("none"), Disman::Output::None},
                         {QStringLiteral("normal"), Disman::Output::None},
                         {QStringLiteral("left"), Disman::Output::Left},
                         {QStringLiteral("right"), Disman::Output::Right},
                         {QStringLiteral("inverted"), Disman::Output::Inverted}});
                    Disman::Output::Rotation rot = Disman::Output::None;
                    // set orientation
                    if (rotationMap.contains(_rotation)) {
                        ok = true;
                        rot = rotationMap[_rotation];
                    }
                    if (!ok || !setRotation(output_id, rot)) {
                        qCDebug(DISMAN_CTL) << "Could not set orientation " << _rotation
                                            << " to output " << output_id;
                        qApp->exit(9);
                        return;
                    }
                } else {
                    cerr << "Unable to parse arguments: " << op << Qt::endl;
                    qApp->exit(2);
                    return;
                }
            }
        }
    }
}

void Doctor::configReceived(Disman::ConfigOperation* op)
{
    m_config = op->config();

    if (m_parser->isSet(QStringLiteral("json"))) {
        showJson();
        qApp->quit();
    }
    if (m_parser->isSet(QStringLiteral("outputs"))) {
        showOutputs();
        qApp->quit();
    }

    parsePositionalArgs();

    if (m_changed) {
        applyConfig();
        m_changed = false;
    }
}

void Doctor::showOutputs() const
{
    if (!m_config) {
        qCWarning(DISMAN_CTL) << "Invalid config.";
        return;
    }

    QHash<Disman::Output::Type, QString> typeString;
    typeString[Disman::Output::Unknown] = QStringLiteral("Unknown");
    typeString[Disman::Output::VGA] = QStringLiteral("VGA");
    typeString[Disman::Output::DVI] = QStringLiteral("DVI");
    typeString[Disman::Output::DVII] = QStringLiteral("DVII");
    typeString[Disman::Output::DVIA] = QStringLiteral("DVIA");
    typeString[Disman::Output::DVID] = QStringLiteral("DVID");
    typeString[Disman::Output::HDMI] = QStringLiteral("HDMI");
    typeString[Disman::Output::Panel] = QStringLiteral("Panel");
    typeString[Disman::Output::TV] = QStringLiteral("TV");
    typeString[Disman::Output::TVComposite] = QStringLiteral("TVComposite");
    typeString[Disman::Output::TVSVideo] = QStringLiteral("TVSVideo");
    typeString[Disman::Output::TVComponent] = QStringLiteral("TVComponent");
    typeString[Disman::Output::TVSCART] = QStringLiteral("TVSCART");
    typeString[Disman::Output::TVC4] = QStringLiteral("TVC4");
    typeString[Disman::Output::DisplayPort] = QStringLiteral("DisplayPort");

    for (auto const& output : m_config->outputs()) {
        cout << green << "Output: " << cr << output->id() << " " << output->name().c_str();
        cout << " "
             << (output->isEnabled() ? green + QLatin1String("enabled")
                                     : red + QLatin1String("disabled"));
        cout << " " << (output->isPrimary() ? green + QLatin1String("primary") : QString());
        auto _type = typeString[output->type()];
        cout << " " << yellow << (_type.isEmpty() ? QStringLiteral("UnmappedOutputType") : _type);
        cout << blue << " Modes: " << cr;
        for (auto const& mode : output->modes()) {
            auto name = QStringLiteral("%1x%2@%3")
                            .arg(QString::number(mode->size().width()),
                                 QString::number(mode->size().height()),
                                 QString::number(qRound(mode->refreshRate())));
            if (mode == output->auto_mode()) {
                name = green + name + QLatin1Char('*') + cr;
            }
            if (mode == output->preferred_mode()) {
                name = name + QLatin1Char('!');
            }
            cout << mode->id() << ":" << name << " ";
        }
        const auto g = output->geometry();
        cout << yellow << "Geometry: " << cr << g.x() << "," << g.y() << " " << g.width() << "x"
             << g.height() << " ";
        cout << yellow << "Scale: " << cr << output->scale() << " ";
        cout << yellow << "Rotation: " << cr << output->rotation() << " ";
        if (output->isPrimary()) {
            cout << blue << "primary";
        }
        cout << cr << Qt::endl;
    }
}

void Doctor::showJson() const
{
    QJsonDocument doc(Disman::ConfigSerializer::serializeConfig(m_config));
    cout << doc.toJson(QJsonDocument::Indented);
}

bool Doctor::setEnabled(int id, bool enabled = true)
{
    if (!m_config) {
        qCWarning(DISMAN_CTL) << "Invalid config.";
        return false;
    }

    for (auto const& output : m_config->outputs()) {
        if (output->id() == id) {
            cout << (enabled ? "Enabling " : "Disabling ") << "output " << id << Qt::endl;
            output->setEnabled(enabled);
            m_changed = true;
            return true;
        }
    }
    cerr << "Output with id " << id << " not found." << Qt::endl;
    qApp->exit(8);
    return false;
}

bool Doctor::setPosition(int id, const QPoint& pos)
{
    if (!m_config) {
        qCWarning(DISMAN_CTL) << "Invalid config.";
        return false;
    }

    for (auto const& output : m_config->outputs()) {
        if (output->id() == id) {
            qCDebug(DISMAN_CTL) << "Set output position" << pos;
            output->setPosition(pos);
            m_changed = true;
            return true;
        }
    }
    cout << "Output with id " << id << " not found." << Qt::endl;
    return false;
}

bool Doctor::setMode(int id, const QString& mode_id)
{
    if (!m_config) {
        qCWarning(DISMAN_CTL) << "Invalid config.";
        return false;
    }

    for (auto& output : m_config->outputs()) {
        if (output->id() == id) {
            // find mode
            for (auto const& mode : output->modes()) {
                auto name = QStringLiteral("%1x%2@%3")
                                .arg(QString::number(mode->size().width()),
                                     QString::number(mode->size().height()),
                                     QString::number(qRound(mode->refreshRate())));
                if (mode->id() == mode_id || name == mode_id) {
                    qCDebug(DISMAN_CTL) << "Taddaaa! Found mode" << mode->id() << name;
                    output->set_mode(mode);
                    m_changed = true;
                    return true;
                }
            }
        }
    }
    cout << "Output mode " << mode_id << " not found." << Qt::endl;
    return false;
}

bool Doctor::setScale(int id, qreal scale)
{
    if (!m_config) {
        qCWarning(DISMAN_CTL) << "Invalid config.";
        return false;
    }

    for (auto& output : m_config->outputs()) {
        if (output->id() == id) {
            output->setScale(scale);
            m_changed = true;
            return true;
        }
    }
    cout << "Output scale " << id << " invalid." << Qt::endl;
    return false;
}

bool Doctor::setRotation(int id, Disman::Output::Rotation rot)
{
    if (!m_config) {
        qCWarning(DISMAN_CTL) << "Invalid config.";
        return false;
    }

    for (auto& output : m_config->outputs()) {
        if (output->id() == id) {
            output->setRotation(rot);
            m_changed = true;
            return true;
        }
    }
    cout << "Output rotation " << id << " invalid." << Qt::endl;
    return false;
}

void Doctor::applyConfig()
{
    if (!m_changed) {
        return;
    }
    auto setop = new SetConfigOperation(m_config, this);
    setop->exec();
    qCDebug(DISMAN_CTL) << "setop exec returned" << m_config;
    qApp->exit(0);
}

}
