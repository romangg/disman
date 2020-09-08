/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "backend_impl.h"

namespace Disman
{

BackendImpl::BackendImpl()
    : Backend()
{
}

void BackendImpl::init([[maybe_unused]] QVariantMap const& arguments)
{
}

Disman::ConfigPtr BackendImpl::config() const
{
    return config_impl();
}

void BackendImpl::set_config(Disman::ConfigPtr const& config)
{
    if (!config) {
        return;
    }
    set_config_impl(config);
}

}
