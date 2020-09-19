/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "backend_impl.h"

#include "device.h"
#include "filer_controller.h"
#include "generator.h"
#include "logging.h"

namespace Disman
{

BackendImpl::BackendImpl()
    : Backend()
    , m_device{new Device}
    , m_filer_controller{new Filer_controller(m_device.get())}
{
    connect(m_device.get(), &Device::lid_open_changed, this, &BackendImpl::load_lid_config);
}

BackendImpl::~BackendImpl() = default;

void BackendImpl::init([[maybe_unused]] QVariantMap const& arguments)
{
    // noop, maybe overridden in individual backends.
}

Filer_controller* BackendImpl::filer_controller() const
{
    return m_filer_controller.get();
}

Disman::ConfigPtr BackendImpl::config() const
{
    m_config_initialized = true;

    auto config = std::make_shared<Config>();

    // We update from the windowing system first so the controller knows about the current
    // configuration and then update one more time so the windowing system can override values
    // it provides itself.
    update_config(config);
    filer_controller()->read(config);
    update_config(config);

    return config;
}

void BackendImpl::set_config(Disman::ConfigPtr const& config)
{
    if (!config) {
        return;
    }
    set_config_impl(config);
}

bool BackendImpl::handle_config_change()
{
    auto cfg = config();

    if (!m_config || m_config->hash() != cfg->hash()) {
        qCDebug(DISMAN_BACKEND) << "Config with new output pattern received:" << cfg;

        if (cfg->cause() == Config::Cause::unknown) {
            qCDebug(DISMAN_BACKEND)
                << "Config received that is unknown. Creating an optimized config now.";
            Generator generator(cfg);
            generator.optimize();
            cfg = generator.config();
        } else {
            // We set the windowing system to our saved values. They were overriden before so
            // re-read them.
            m_filer_controller->read(cfg);
        }

        m_config = cfg;

        if (set_config_impl(cfg)) {
            qCDebug(DISMAN_BACKEND) << "Config for new output pattern sent.";
            return false;
        }
    }

    Q_EMIT config_changed(cfg);
    return true;
}

void BackendImpl::load_lid_config()
{
    if (!m_config_initialized) {
        qCWarning(DISMAN_BACKEND) << "Lid open state changed but first config has not yet been "
                                     "initialized. Doing nothing.";
        return;
    }
    auto cfg = config();

    if (m_device->lid_open()) {
        // The lid has been opnened. Try to load the open lid file.
        if (!m_filer_controller->load_lid_file(cfg)) {
            return;
        }
        qCDebug(DISMAN_BACKEND) << "Loaded lid-open file on lid being opened.";
    } else {
        // The lid has been closed, we write the current config as open-lid-config and then generate
        // an optimized one with the embedded display disabled that gets applied.
        Generator generator(cfg);
        qCDebug(DISMAN_BACKEND) << "Lid closed, trying to disable embedded display.";

        if (!generator.disable_embedded()) {
            // Alternative config could not be generated.
            qCWarning(DISMAN_BACKEND) << "Embedded display could not be disabled.";
            return;
        }
        if (m_filer_controller->save_lid_file(cfg)) {
            qCWarning(DISMAN_BACKEND) << "Failed to save open-lid file.";
            return;
        }
        cfg = generator.config();
    }

    set_config_impl(cfg);
}

}
