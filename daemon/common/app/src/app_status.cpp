// Copyright 2021-2022 FLECS Technologies GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "app_status.h"

#include <algorithm>
#include <array>
#include <tuple>

namespace FLECS {

auto to_string_view(app_status_e app_status) //
    -> std::string_view
{
    const auto strings = std::array<std::tuple<app_status_e, std::string_view>, 7>{{
        {app_status_e::NotInstalled, "not installed"},
        {app_status_e::ManifestDownloaded, "manifest downloaded"},
        {app_status_e::TokenAcquired, "token acquired"},
        {app_status_e::ImageDownloaded, "image downloaded"},
        {app_status_e::Installed, "installed"},
        {app_status_e::Removed, "removed"},
        {app_status_e::Purged, "purged"},
    }};

    const auto it = std::find_if(
        strings.cbegin(),
        strings.cend(),
        [&app_status](const std::tuple<app_status_e, std::string_view>& elem) {
            return std::get<0>(elem) == app_status;
        });

    return it == strings.cend() ? "unknown" : std::get<1>(*it);
}

auto to_string(app_status_e app_status) //
    -> std::string
{
    return std::string{to_string_view(app_status)};
}

auto app_status_from_string(std::string_view str) //
    -> app_status_e
{
    const auto status = std::array<std::tuple<std::string_view, app_status_e>, 7>{{
        {"not installed", app_status_e::NotInstalled},
        {"manifest downloaded", app_status_e::ManifestDownloaded},
        {"token acquired", app_status_e::TokenAcquired},
        {"image downloaded", app_status_e::ImageDownloaded},
        {"installed", app_status_e::Installed},
        {"removed", app_status_e::Removed},
        {"purged", app_status_e::Purged},
    }};

    const auto it = std::find_if(
        status.cbegin(),
        status.cend(),
        [&str](const std::tuple<std::string_view, app_status_e>& elem) {
            return std::get<0>(elem) == str;
        });

    return it == status.cend() ? app_status_e::Unknown : std::get<1>(*it);
}

} // namespace FLECS
