/*************************************************************************************
 *  Copyright (C) 2012 by Alejandro Fiestas Olivares <afiestas@kde.org>              *
 *  Copyright (C) 2014 by Daniel Vrátil <dvratil@redhat.com>                         *
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
#include "output.h"
#include "abstractbackend.h"
#include "backendmanager_p.h"
#include "disman_debug.h"
#include "edid.h"
#include "mode.h"

#include <QCryptographicHash>
#include <QRect>
#include <QScopedPointer>
#include <QStringList>

using namespace Disman;

class Q_DECL_HIDDEN Output::Private
{
public:
    Private()
        : id(0)
        , type(Unknown)
        , replicationSource(0)
        , rotation(None)
        , scale(1.0)
        , connected(false)
        , enabled(false)
        , primary(false)
        , edid(nullptr)
    {
    }

    Private(const Private& other)
        : id(other.id)
        , name(other.name)
        , type(other.type)
        , icon(other.icon)
        , clones(other.clones)
        , replicationSource(other.replicationSource)
        , currentModeId(other.currentModeId)
        , preferredMode(other.preferredMode)
        , preferredModes(other.preferredModes)
        , sizeMm(other.sizeMm)
        , position(other.position)
        , rotation(other.rotation)
        , scale(other.scale)
        , connected(other.connected)
        , enabled(other.enabled)
        , primary(other.primary)
        , followPreferredMode(other.followPreferredMode)
    {
        Q_FOREACH (const ModePtr& otherMode, other.modeList) {
            modeList.insert(otherMode->id(), otherMode->clone());
        }
        if (other.edid) {
            edid.reset(other.edid->clone());
        }
    }

    QString biggestMode(const ModeList& modes) const;
    bool compareModeList(const ModeList& before, const ModeList& after);

    int id;
    QString name;
    Type type;
    QString icon;
    ModeList modeList;
    QList<int> clones;
    int replicationSource;
    QString currentModeId;
    QString preferredMode;
    QStringList preferredModes;
    QSize sizeMm;
    QPointF position;
    Rotation rotation;
    qreal scale;
    bool connected;
    bool enabled;
    bool primary;
    bool followPreferredMode = false;

    QScopedPointer<Edid> edid;
};

bool Output::Private::compareModeList(const ModeList& before, const ModeList& after)
{
    if (before.count() != after.count()) {
        return false;
    }

    for (auto itb = before.constBegin(); itb != before.constEnd(); ++itb) {
        auto ita = after.constFind(itb.key());
        if (ita == after.constEnd()) {
            return false;
        }
        const auto& mb = itb.value();
        const auto& ma = ita.value();
        if (mb->id() != ma->id()) {
            return false;
        }
        if (mb->size() != ma->size()) {
            return false;
        }
        if (!qFuzzyCompare(mb->refreshRate(), ma->refreshRate())) {
            return false;
        }
        if (mb->name() != ma->name()) {
            return false;
        }
    }
    // They're the same
    return true;
}

QString Output::Private::biggestMode(const ModeList& modes) const
{
    int area, total = 0;
    Disman::ModePtr biggest;
    Q_FOREACH (const Disman::ModePtr& mode, modes) {
        area = mode->size().width() * mode->size().height();
        if (area < total) {
            continue;
        }
        if (area == total && mode->refreshRate() < biggest->refreshRate()) {
            continue;
        }
        if (area == total && mode->refreshRate() > biggest->refreshRate()) {
            biggest = mode;
            continue;
        }

        total = area;
        biggest = mode;
    }

    if (!biggest) {
        return QString();
    }

    return biggest->id();
}

Output::Output()
    : QObject(nullptr)
    , d(new Private())
{
}

Output::Output(Output::Private* dd)
    : QObject()
    , d(dd)
{
}

Output::~Output()
{
    delete d;
}

OutputPtr Output::clone() const
{
    return OutputPtr(new Output(new Private(*d)));
}

int Output::id() const
{
    return d->id;
}

void Output::setId(int id)
{
    if (d->id == id) {
        return;
    }

    d->id = id;

    Q_EMIT outputChanged();
}

QString Output::name() const
{
    return d->name;
}

void Output::setName(const QString& name)
{
    if (d->name == name) {
        return;
    }

    d->name = name;

    Q_EMIT outputChanged();
}

// TODO KF6: remove this deprecated method
QString Output::hash() const
{
    if (edid() && edid()->isValid()) {
        return edid()->hash();
    }
    return name();
}

QString Output::hashMd5() const
{
    if (edid() && edid()->isValid()) {
        return edid()->hash();
    }
    const auto hash = QCryptographicHash::hash(name().toLatin1(), QCryptographicHash::Md5);
    return QString::fromLatin1(hash.toHex());
}

Output::Type Output::type() const
{
    return d->type;
}

void Output::setType(Type type)
{
    if (d->type == type) {
        return;
    }

    d->type = type;

    Q_EMIT outputChanged();
}

QString Output::icon() const
{
    return d->icon;
}

void Output::setIcon(const QString& icon)
{
    if (d->icon == icon) {
        return;
    }

    d->icon = icon;

    Q_EMIT outputChanged();
}

ModePtr Output::mode(const QString& id) const
{
    if (!d->modeList.contains(id)) {
        return ModePtr();
    }

    return d->modeList[id];
}

ModeList Output::modes() const
{
    return d->modeList;
}

void Output::setModes(const ModeList& modes)
{
    bool changed = !d->compareModeList(d->modeList, modes);
    d->modeList = modes;
    if (changed) {
        emit modesChanged();
        emit outputChanged();
    }
}

QString Output::currentModeId() const
{
    return d->currentModeId;
}

void Output::setCurrentModeId(const QString& modeId)
{
    if (d->currentModeId == modeId) {
        return;
    }

    d->currentModeId = modeId;

    Q_EMIT currentModeIdChanged();
}

ModePtr Output::currentMode() const
{
    return d->modeList.value(d->currentModeId);
}

void Output::setPreferredModes(const QStringList& modes)
{
    d->preferredMode = QString();
    d->preferredModes = modes;
}

QStringList Output::preferredModes() const
{
    return d->preferredModes;
}

QString Output::preferredModeId() const
{
    if (!d->preferredMode.isEmpty()) {
        return d->preferredMode;
    }
    if (d->preferredModes.isEmpty()) {
        return d->biggestMode(modes());
    }

    int area, total = 0;
    Disman::ModePtr biggest;
    Disman::ModePtr candidateMode;
    Q_FOREACH (const QString& modeId, d->preferredModes) {
        candidateMode = mode(modeId);
        area = candidateMode->size().width() * candidateMode->size().height();
        if (area < total) {
            continue;
        }
        if (area == total && biggest && candidateMode->refreshRate() < biggest->refreshRate()) {
            continue;
        }
        if (area == total && biggest && candidateMode->refreshRate() > biggest->refreshRate()) {
            biggest = candidateMode;
            continue;
        }

        total = area;
        biggest = candidateMode;
    }

    Q_ASSERT_X(biggest, "preferredModeId", "biggest mode must exist");

    d->preferredMode = biggest->id();
    return d->preferredMode;
}

ModePtr Output::preferredMode() const
{
    return d->modeList.value(preferredModeId());
}

void Output::setPosition(const QPointF& position)
{
    if (d->position == position) {
        return;
    }

    d->position = position;
    Q_EMIT geometryChanged();
}

// TODO KF6: make the Rotation enum an enum class and align values with Wayland transformation
// property
Output::Rotation Output::rotation() const
{
    return d->rotation;
}

void Output::setRotation(Output::Rotation rotation)
{
    if (d->rotation == rotation) {
        return;
    }

    d->rotation = rotation;
    Q_EMIT geometryChanged();
}

double Output::scale() const
{
    return d->scale;
}

void Output::setScale(double scale)
{
    if (qFuzzyCompare(d->scale, scale)) {
        return;
    }
    d->scale = scale;
    Q_EMIT geometryChanged();
}

QRectF Output::geometry() const
{
    auto geo = QRectF(d->position, QSizeF());

    auto size = enforcedModeSize();
    if (!size.isValid()) {
        return geo;
    }
    if (!isHorizontal()) {
        size = size.transposed();
    }

    geo.setSize(size / d->scale);
    return geo;
}

QPointF Output::position() const
{
    return d->position;
}

bool Output::isConnected() const
{
    return d->connected;
}

void Output::setConnected(bool connected)
{
    if (d->connected == connected) {
        return;
    }

    d->connected = connected;

    Q_EMIT isConnectedChanged();
}

bool Output::isEnabled() const
{
    return d->enabled;
}

void Output::setEnabled(bool enabled)
{
    if (d->enabled == enabled) {
        return;
    }

    d->enabled = enabled;

    Q_EMIT isEnabledChanged();
}

bool Output::isPrimary() const
{
    return d->primary;
}

void Output::setPrimary(bool primary)
{
    if (d->primary == primary) {
        return;
    }

    d->primary = primary;

    Q_EMIT isPrimaryChanged();
}

QList<int> Output::clones() const
{
    return d->clones;
}

void Output::setClones(const QList<int>& outputlist)
{
    if (d->clones == outputlist) {
        return;
    }

    d->clones = outputlist;

    Q_EMIT clonesChanged();
}

int Output::replicationSource() const
{
    return d->replicationSource;
}

void Output::setReplicationSource(int source)
{
    if (d->replicationSource == source) {
        return;
    }

    d->replicationSource = source;

    Q_EMIT replicationSourceChanged();
}

void Output::setEdid(const QByteArray& rawData)
{
    Q_ASSERT(d->edid.isNull());
    d->edid.reset(new Edid(rawData));
}

Edid* Output::edid() const
{
    return d->edid.data();
}

QSize Output::sizeMm() const
{
    return d->sizeMm;
}

void Output::setSizeMm(const QSize& size)
{
    d->sizeMm = size;
}

bool Disman::Output::followPreferredMode() const
{
    return d->followPreferredMode;
}

void Disman::Output::setFollowPreferredMode(bool follow)
{
    if (follow != d->followPreferredMode) {
        d->followPreferredMode = follow;
        Q_EMIT followPreferredModeChanged(follow);
    }
}

bool Output::isPositionable() const
{
    return isConnected() && isEnabled() && !replicationSource();
}

QSize Output::enforcedModeSize() const
{
    if (const auto mode = currentMode()) {
        return mode->size();
    } else if (const auto mode = preferredMode()) {
        return mode->size();
    } else if (d->modeList.count() > 0) {
        return d->modeList.first()->size();
    }
    return QSize();
}

void Output::apply(const OutputPtr& other)
{
    typedef void (Disman::Output::*ChangeSignal)();
    QList<ChangeSignal> changes;

    // We block all signals, and emit them only after we have set up everything
    // This is necessary in order to prevent clients from accessing inconsistent
    // outputs from intermediate change signals
    const bool keepBlocked = signalsBlocked();
    blockSignals(true);
    if (d->name != other->d->name) {
        changes << &Output::outputChanged;
        setName(other->d->name);
    }
    if (d->type != other->d->type) {
        changes << &Output::outputChanged;
        setType(other->d->type);
    }
    if (d->icon != other->d->icon) {
        changes << &Output::outputChanged;
        setIcon(other->d->icon);
    }
    if (d->position != other->d->position) {
        changes << &Output::geometryChanged;
        setPosition(other->geometry().topLeft());
    }
    if (d->rotation != other->d->rotation) {
        changes << &Output::rotationChanged;
        setRotation(other->d->rotation);
    }
    if (!qFuzzyCompare(d->scale, other->d->scale)) {
        changes << &Output::geometryChanged;
        setScale(other->d->scale);
    }
    if (d->currentModeId != other->d->currentModeId) {
        changes << &Output::currentModeIdChanged;
        setCurrentModeId(other->d->currentModeId);
    }
    if (d->connected != other->d->connected) {
        changes << &Output::isConnectedChanged;
        setConnected(other->d->connected);
    }
    if (d->enabled != other->d->enabled) {
        changes << &Output::isEnabledChanged;
        setEnabled(other->d->enabled);
    }
    if (d->primary != other->d->primary) {
        changes << &Output::isPrimaryChanged;
        setPrimary(other->d->primary);
    }
    if (d->clones != other->d->clones) {
        changes << &Output::clonesChanged;
        setClones(other->d->clones);
        ;
    }
    if (d->replicationSource != other->d->replicationSource) {
        changes << &Output::replicationSourceChanged;
        setReplicationSource(other->d->replicationSource);
        ;
    }
    if (!d->compareModeList(d->modeList, other->d->modeList)) {
        changes << &Output::outputChanged;
        changes << &Output::modesChanged;
    }

    setPreferredModes(other->d->preferredModes);
    ModeList modes;
    Q_FOREACH (const ModePtr& otherMode, other->modes()) {
        modes.insert(otherMode->id(), otherMode->clone());
    }
    setModes(modes);

    // Non-notifyable changes
    if (other->d->edid) {
        d->edid.reset(other->d->edid->clone());
    }

    blockSignals(keepBlocked);

    while (!changes.isEmpty()) {
        const ChangeSignal& sig = changes.first();
        Q_EMIT(this->*sig)();
        changes.removeAll(sig);
    }
}

QDebug operator<<(QDebug dbg, const Disman::OutputPtr& output)
{
    if (output) {
        dbg << "Disman::Output(" << output->id() << " " << output->name()
            << (output->isConnected() ? "connected" : "disconnected")
            << (output->isEnabled() ? "enabled" : "disabled")
            << (output->isPrimary() ? "primary" : "") << "geometry:" << output->geometry()
            << "scale:" << output->scale() << "modeId:" << output->currentModeId()
            << "clone:" << (output->clones().isEmpty() ? "no" : "yes")
            << "followPreferredMode:" << output->followPreferredMode() << ")";
    } else {
        dbg << "Disman::Output(NULL)";
    }
    return dbg;
}
