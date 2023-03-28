/*************************************************************************
Copyright © 2014-2015 Sebastian Kügler <sebas@kde.org>
Copyright © 2019-2020 Roman Gilg <subdiff@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
**************************************************************************/
#include "waylandoutput.h"

#include "utils.h"
#include "wayland_interface.h"
#include "wayland_logging.h"

#include <QRectF>
#include <Wrapland/Client/wlr_output_configuration_v1.h>
#include <mode.h>

using namespace Disman;
namespace Wl = Wrapland::Client;

const std::map<Wl::WlrOutputHeadV1::Transform, Output::Rotation> s_rotationMap
    = {{Wl::WlrOutputHeadV1::Transform::Normal, Output::None},
       {Wl::WlrOutputHeadV1::Transform::Rotated90, Output::Right},
       {Wl::WlrOutputHeadV1::Transform::Rotated180, Output::Inverted},
       {Wl::WlrOutputHeadV1::Transform::Rotated270, Output::Left},
       {Wl::WlrOutputHeadV1::Transform::Flipped, Output::None},
       {Wl::WlrOutputHeadV1::Transform::Flipped90, Output::Right},
       {Wl::WlrOutputHeadV1::Transform::Flipped180, Output::Inverted},
       {Wl::WlrOutputHeadV1::Transform::Flipped270, Output::Left}};

Output::Rotation toDismanRotation(const Wl::WlrOutputHeadV1::Transform transform)
{
    auto it = s_rotationMap.find(transform);
    assert(it != s_rotationMap.end());
    return it->second;
}

Wl::WlrOutputHeadV1::Transform toWraplandTransform(const Output::Rotation rotation)
{
    for (auto const& [key, val] : s_rotationMap) {
        if (val == rotation) {
            return key;
        }
    }
    assert(false);
    return Wl::WlrOutputHeadV1::Transform::Normal;
}

WaylandOutput::WaylandOutput(quint32 id,
                             Wrapland::Client::WlrOutputHeadV1* head,
                             WaylandInterface* parent)
    : m_id(id)
    , m_head(head)
{
    connect(m_head, &Wl::WlrOutputHeadV1::removed, this, &WaylandOutput::removed);

    auto manager = parent->outputManager();
    connect(manager, &Wl::WlrOutputManagerV1::done, this, [this, manager]() {
        disconnect(manager, &Wl::WlrOutputManagerV1::done, this, nullptr);
        connect(m_head, &Wl::WlrOutputHeadV1::changed, this, &WaylandOutput::changed);
        Q_EMIT dataReceived();
    });
}

quint32 WaylandOutput::id() const
{
    return m_id;
}

bool WaylandOutput::enabled() const
{
    return m_head != nullptr;
}

bool portraitMode(Wrapland::Client::WlrOutputHeadV1* head)
{
    auto transform = head->transform();
    return transform == Wl::WlrOutputHeadV1::Transform::Rotated90
        || transform == Wl::WlrOutputHeadV1::Transform::Rotated270
        || transform == Wl::WlrOutputHeadV1::Transform::Flipped90
        || transform == Wl::WlrOutputHeadV1::Transform::Flipped270;
}

QRectF WaylandOutput::geometry() const
{
    auto const current_mode = m_head->currentMode();
    if (!current_mode) {
        return QRectF();
    }
    auto modeSize = current_mode->size();

    // Rotate and scale.
    if (portraitMode(m_head)) {
        modeSize.transpose();
    }

    modeSize = modeSize / m_head->scale();

    return QRectF(m_head->position(), modeSize);
}

Wrapland::Client::WlrOutputHeadV1* WaylandOutput::outputHead() const
{
    return m_head;
}

QString modeName(const Wl::WlrOutputModeV1* mode)
{
    return QString::number(mode->size().width()) + QLatin1Char('x')
        + QString::number(mode->size().height()) + QLatin1Char('@')
        + QString::number(qRound(mode->refresh() / 1000.0));
}

OutputPtr WaylandOutput::toDismanOutput()
{
    OutputPtr output(new Output());
    output->set_id(m_id);
    updateDismanOutput(output);
    return output;
}

void WaylandOutput::updateDismanOutput(OutputPtr& output)
{
    // Initialize primary output
    output->set_enabled(m_head->enabled());
    output->set_name(m_head->name().toStdString());
    output->set_description(m_head->description().toStdString());
    output->set_hash(hash().toStdString());
    output->set_physical_size(m_head->physicalSize());
    output->set_position(m_head->position());
    output->set_rotation(toDismanRotation(m_head->transform()));

    ModeMap modeList;
    std::vector<std::string> preferredModeIds;
    m_modeIdMap.clear();

    auto current_head_mode = m_head->currentMode();
    ModePtr current_mode;

    int modeCounter = 0;
    auto const modes = m_head->modes();
    for (auto const& wlMode : qAsConst(modes)) {
        auto const modeId = std::to_string(++modeCounter);

        ModePtr mode(new Mode());

        mode->set_id(modeId);

        // Wrapland gives the refresh rate as int in mHz.
        mode->set_refresh(wlMode->refresh());
        mode->set_size(wlMode->size());
        mode->set_name(modeName(wlMode).toStdString());

        if (wlMode->preferred()) {
            preferredModeIds.push_back(modeId);
        }
        if (current_head_mode == wlMode) {
            current_mode = mode;
        }

        // Update the Disman => Wrapland mode id translation map.
        m_modeIdMap.insert({modeId, wlMode});

        // Add to the modelist which gets set on the output.
        modeList[modeId] = mode;
    }

    output->set_preferred_modes(preferredModeIds);
    output->set_modes(modeList);

    if (current_head_mode) {
        if (!current_mode) {
            qCWarning(DISMAN_WAYLAND) << "Could not find the current mode in:";
            for (auto const& [key, mode] : modeList) {
                qCWarning(DISMAN_WAYLAND) << "  " << mode;
            }
        } else {
            output->set_mode(current_mode);
            output->set_resolution(current_mode->size());
            if (!output->set_refresh_rate(current_mode->refresh())) {
                qCWarning(DISMAN_WAYLAND) << "Failed setting the current mode:" << current_mode;
            }
        }
    }

    output->set_scale(m_head->scale());
    output->setType(Utils::guessOutputType(m_head->name(), m_head->name()));
}

bool WaylandOutput::setWlConfig(Wl::WlrOutputConfigurationV1* wlConfig,
                                const Disman::OutputPtr& output)
{
    bool changed = false;

    // enabled?
    if (m_head->enabled() != output->enabled()) {
        changed = true;
    }

    // In any case set the enabled state to initialize the output's native handle.
    wlConfig->setEnabled(m_head, output->enabled());

    if (!output->enabled()) {
        // A disabled head can not be configured in any way.
        return changed;
    }

    // position
    if (m_head->position() != output->position()) {
        changed = true;
        wlConfig->setPosition(m_head, output->position().toPoint());
    }

    // scale
    if (!qFuzzyCompare(m_head->scale(), output->scale())) {
        changed = true;
        wlConfig->setScale(m_head, output->scale());
    }

    // rotation
    if (toDismanRotation(m_head->transform()) != output->rotation()) {
        changed = true;
        wlConfig->setTransform(m_head, toWraplandTransform(output->rotation()));
    }

    // mode
    if (auto mode_id = output->auto_mode()->id(); m_modeIdMap.find(mode_id) != m_modeIdMap.end()) {
        auto newMode = m_modeIdMap.at(mode_id);
        if (newMode != m_head->currentMode()) {
            changed = true;
            wlConfig->setMode(m_head, newMode);
        }
    } else {
        qCWarning(DISMAN_WAYLAND) << "Invalid Disman mode:" << mode_id.c_str()
                                  << "\n  -> available were:";
        for (auto const& [key, value] : m_modeIdMap) {
            qCWarning(DISMAN_WAYLAND).nospace() << value << ": " << key.c_str();
        }
    }

    return changed;
}

QString WaylandOutput::hash() const
{
    assert(m_head);
    if (!m_head->model().isEmpty()) {
        return QStringLiteral("%1:%2:%3:%4")
            .arg(m_head->make(), m_head->model(), m_head->serialNumber(), m_head->name());
    }
    return m_head->description();
}

QDebug operator<<(QDebug dbg, const WaylandOutput* output)
{
    dbg << "WaylandOutput(Id:" << output->id() << ", Name:"
        << QString(output->outputHead()->name() + QLatin1Char(' ')
                   + output->outputHead()->description())
        << ")";
    return dbg;
}
