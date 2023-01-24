// Copyright 2021-2023 FLECS Technologies GmbH
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

#include "impl/apps_impl.h"

#include <cpr/cpr.h>

#include <fstream>

#include "api/api.h"
#include "common/app/manifest/manifest.h"
#include "modules/factory/factory.h"
#include "modules/jobs/jobs.h"
#include "modules/manifests/manifests.h"
#include "modules/marketplace/marketplace.h"
#include "util/fs/fs.h"
#include "util/json/json.h"
#include "util/process/process.h"
#include "util/string/string_utils.h"

using std::operator""s;

namespace FLECS {
namespace impl {

static auto acquire_download_token(std::string_view license_key) //
    -> std::string
{
    const auto mp_api = dynamic_cast<const module_marketplace_t*>(api::query_module("mp").get());
    if (!mp_api) {
        return "";
    }

    const auto wc_user_token = mp_api->token();

    auto post_json = json_t{};
    post_json["wc_user_token"] = wc_user_token;
    post_json["license_key"] = license_key;

#ifndef NDEBUG
    const auto url = cpr::Url{"https://marketplace.flecs.tech:8443/api/v1/app/download"};
#else
    const auto url = cpr::Url{"https://marketplace.flecs.tech/api/v1/app/download"};
#endif // NDEBUG

    const auto http_res = cpr::Post(
        url,
        cpr::Header{{"content-type", "application/json"}},
        cpr::Body{post_json.dump()});

    if (http_res.status_code != 200) {
        return "";
    }

    const auto response_json = parse_json(http_res.text);
    if (!is_valid_json(response_json)) {
        return "";
    }

    const auto success = response_json["success"].get<bool>();
    const auto user_token = response_json["user_token"].get<std::string>();
    const auto access_token = response_json["access_token"]["token"].get<std::string>();
    const auto uuid = response_json["access_token"]["uuid"].get<std::string>();

    if (!success || user_token.empty() || access_token.empty() || uuid.empty()) {
        return "";
    }

    return stringify_delim(';', user_token, access_token, uuid);
}

static auto expire_download_token(const std::string& user_token, const std::string& access_token) //
    -> bool
{
    auto post_json = json_t{};
    post_json["user_token"] = user_token;
    post_json["access_token"] = access_token;

#ifndef NDEBUG
    const auto url = cpr::Url{"https://marketplace.flecs.tech/api/v1/app/finish"};
#else
    const auto url = cpr::Url{"https://marketplace.flecs.tech/api/v1/app/finish"};
#endif // NDEBUG

    const auto http_res = cpr::Post(
        url,
        cpr::Header{{"content-type", "application/json"}},
        cpr::Body{post_json.dump()});

    if (http_res.status_code != 200) {
        return false;
    }

    const auto response_json = parse_json(http_res.text);
    if (!is_valid_json(response_json)) {
        return false;
    }

    return response_json["success"].get<bool>();
}

module_apps_t::module_apps_t(FLECS::module_apps_t* parent)
    : _parent{parent}
    , _installed_apps{}
    , _installed_apps_mutex{}
    , _jobs_api{}
{}

module_apps_t::~module_apps_t()
{}

auto module_apps_t::do_init() //
    -> void
{
    _jobs_api = std::dynamic_pointer_cast<FLECS::module_jobs_t>(api::query_module("jobs"));
    _manifests_api =
        std::dynamic_pointer_cast<FLECS::module_manifests_t>(api::query_module("manifests"));
    _manifests_api->base_path("/var/lib/flecs/manifests/");
}

auto module_apps_t::do_load(fs::path base_path) //
    -> crow::response
{
    auto response = json_t{};
    auto res = crow::status::OK;

    base_path /= "apps.json";
    auto json_file = std::ifstream{base_path.c_str()};
    if (json_file.good()) {
        auto apps_json = parse_json(json_file);
        try {
            _installed_apps = apps_json.get<decltype(_installed_apps)>();
            response["additionalInfo"] = "Loaded apps from "s + base_path.string();
        } catch (const std::exception& ex) {
            _installed_apps.clear();
            res = crow::status::INTERNAL_SERVER_ERROR;
            response["additionalInfo"] =
                "Could not load apps from "s + base_path.string() + ": "s + ex.what();
        }
    }

    return {res, "json", response.dump()};
}

auto module_apps_t::do_save(fs::path base_path) const //
    -> crow::response
{
    auto response = json_t{};

    auto ec = std::error_code{};
    fs::create_directories(base_path, ec);
    if (ec) {
        response["additionalInfo"] =
            "Could not save apps to "s + base_path.string() + ": " + std::to_string(ec.value());
        return {crow::status::INTERNAL_SERVER_ERROR, "json", response.dump()};
    }

    base_path /= "apps.json";
    auto apps_json = std::ofstream{base_path.c_str(), std::ios_base::out | std::ios_base::trunc};
    apps_json << json_t(_installed_apps);
    if (!apps_json) {
        response["additionalInfo"] = "Could not save apps to "s + base_path.string();
        return {crow::status::INTERNAL_SERVER_ERROR, "json", response.dump()};
    }

    response["additionalInfo"] = "Saved apps to "s + base_path.string();
    return {crow::status::OK, "json", response.dump()};
}

auto module_apps_t::do_list(std::string_view app_name, std::string_view version) const //
    -> crow::response
{
    auto response = json_t::array();

    for (decltype(auto) app : _installed_apps) {
        const auto apps_match = app_name.empty() || (app_name == app.first.name());
        const auto versions_match =
            app_name.empty() || version.empty() || (version == app.first.version());
        if (apps_match && versions_match) {
            response.push_back(app.second);
        }
    }

    return {crow::status::OK, "json", response.dump()};
}

auto module_apps_t::queue_install_from_marketplace(
    std::string app_name, std::string version, std::string license_key) //
    -> crow::response
{
    auto job = job_t{};
    job.callable = std::bind(
        &module_apps_t::do_install_from_marketplace,
        this,
        std::move(app_name),
        std::move(version),
        std::move(license_key),
        std::placeholders::_1);

    auto job_id = _jobs_api->append(std::move(job));
    return crow::response{
        crow::status::ACCEPTED,
        "json",
        "{\"jobId\":" + std::to_string(job_id) + "}"};
}

auto module_apps_t::do_install_from_marketplace(
    std::string app_name,
    std::string version,
    std::string license_key,
    job_progress_t& progress) //
    -> void
{
    const auto app_key = app_key_t{std::move(app_name), std::move(version)};

    {
        auto lock = progress.lock();

        progress.desc(
            "Installing app "s + app_key.name().data() + " (" + app_key.version().data() + ")");
        progress.num_steps(6);

        progress.current_step()._desc = "Downloading manifest";
        progress.current_step()._num = 1;
        progress.current_step()._unit = {};
        progress.current_step()._units_done = {};
        progress.current_step()._units_total = {};
        progress.current_step()._rate = {};
    }

    // Download App manifest and forward to manifest installation, if download successful
    const auto [manifest, _] = _manifests_api->add_from_marketplace(app_key);
    if (auto p = manifest.lock()) {
        return do_install_impl(std::move(p), license_key, progress);
    }

    auto lock = progress.lock();
    progress.result().code = -1;
    progress.result().message = "Could not download manifest";
    return;
}

auto module_apps_t::queue_sideload(std::string manifest_string, std::string license_key) //
    -> crow::response
{
    auto job = job_t{};
    job.callable = std::bind(
        &module_apps_t::do_sideload,
        this,
        std::move(manifest_string),
        std::move(license_key),
        std::placeholders::_1);

    auto job_id = _jobs_api->append(std::move(job));
    return crow::response{
        crow::status::ACCEPTED,
        "json",
        "{\"jobId\":" + std::to_string(job_id) + "}"};
}

auto module_apps_t::do_sideload(
    std::string manifest_string, std::string license_key, job_progress_t& progress) //
    -> void
{
    const auto [manifest, _] = _manifests_api->add_from_string(manifest_string);
    // Step 1: Validate transferred manifest
    if (auto p = manifest.lock()) {
        // Step 2: Forward to manifest installation
        return do_install_impl(std::move(p), license_key, progress);
    }

    auto lock = progress.lock();
    progress.result().code = -1;
    progress.result().message = "Could not parse manifest";
    return;
}

auto module_apps_t::do_install_impl(
    std::shared_ptr<app_manifest_t> manifest,
    std::string_view license_key,
    job_progress_t& progress) //
    -> void
{
    {
        auto lock = progress.lock();

        progress.current_step()._desc = "Loading manifest";
        progress.current_step()._num++;
    }

    // Step 1: Create app from manifest
    auto tmp = app_t{app_key_t{manifest->app(), manifest->version()}, manifest};
    if (!tmp.key().is_valid()) {
        auto lock = progress.lock();
        progress.result().code = -1;
        progress.result().message = "Could not open app manifest";
        return;
    }
    tmp.desired(app_status_e::Installed);
    tmp.status(app_status_e::ManifestDownloaded);

    // Step 2: Determine current App status to decide where to continue
    auto it = _installed_apps.find(tmp.key());
    if (it == _installed_apps.end()) {
        it = _installed_apps.emplace(tmp.key(), std::move(tmp)).first;
    }

    auto& app = it->second;
    switch (app.status()) {
        case app_status_e::ManifestDownloaded: {
            {
                auto lock = progress.lock();

                progress.current_step()._desc = "Acquiring download token";
                progress.current_step()._num++;
            }

            // Step 3: Acquire download token for App
            app.download_token(acquire_download_token(license_key));

            if (app.download_token().empty()) {
                auto lock = progress.lock();
                progress.result().message = "Could not acquire download token";
            } else {
                app.status(app_status_e::TokenAcquired);
            }
            [[fallthrough]];
        }
        case app_status_e::TokenAcquired: {
            // Step 4: Pull Docker image for this App
            auto docker_login_process = process_t{};
            auto docker_pull_process = process_t{};
            auto docker_logout_process = process_t{};
            const auto args = split(app.download_token(), ';');

            if (args.size() == 3) {
                {
                    auto lock = progress.lock();

                    progress.current_step()._desc = "Logging in";
                    progress.current_step()._num++;
                }
                auto login_attempts = 3;
                while (login_attempts-- > 0) {
                    docker_login_process = process_t{};
                    docker_login_process
                        .spawnp("docker", "login", "--username", "flecs", "--password", args[1]);
                    docker_login_process.wait(true, true);
                    if (docker_login_process.exit_code() == 0) {
                        break;
                    }
                }
            }

            if (docker_login_process.exit_code() != 0) {
                auto lock = progress.lock();
                progress.result().code = -1;
                progress.result().message = docker_login_process.stderr();
                _parent->save();
                return;
            }

            {
                auto lock = progress.lock();

                progress.current_step()._desc = "Downloading";
                progress.current_step()._num++;
            }
            auto pull_attempts = 3;
            while (pull_attempts-- > 0) {
                docker_pull_process = process_t{};
                docker_pull_process.spawnp("docker", "pull", manifest->image_with_tag());
                docker_pull_process.wait(true, true);
                if (docker_pull_process.exit_code() == 0) {
                    break;
                }
            }

            docker_logout_process.spawnp("docker", "logout");
            docker_logout_process.wait(true, true);

            if (docker_pull_process.exit_code() != 0) {
                auto lock = progress.lock();
                progress.result().code = -1;
                progress.result().message = docker_pull_process.stderr();
                _parent->save();
                return;
            }
            app.status(app_status_e::ImageDownloaded);
            [[fallthrough]];
        }
        case app_status_e::ImageDownloaded: {
            {
                auto lock = progress.lock();

                progress.current_step()._desc = "Expiring download token";
                progress.current_step()._num++;
            }

            // Step 5: Expire download token
            const auto args = split(app.download_token(), ';');
            if (args.size() == 3) {
                const auto success = expire_download_token(args[0], args[2]);
                if (success) {
                    app.download_token("");
                    app.status(app_status_e::Installed);
                } else {
                    auto lock = progress.lock();
                    progress.result().message = "Could not expire download token";
                }
            } else {
                app.download_token("");
                app.status(app_status_e::Installed);
            }
            break;
        }
        default: {
        }
    }

    // Final step: Persist successful installation into db
    _parent->save();

    {
        auto lock = progress.lock();

        progress.result().code = 0;
    }
}

auto module_apps_t::queue_uninstall(std::string app_name, std::string version, bool force) //
    -> crow::response
{
    if (!is_app_installed(app_name, version)) {
        auto response = json_t{};
        response["additionalInfo"] =
            "Cannot uninstall "s + app_name + " (" + version + "), which is not installed";
        return {crow::status::BAD_REQUEST, "json", response.dump()};
    }

    auto job = job_t{};
    job.callable = std::bind(
        &module_apps_t::do_uninstall,
        this,
        std::move(app_name),
        std::move(version),
        std::move(force),
        std::placeholders::_1);

    auto job_id = _jobs_api->append(std::move(job));
    return crow::response{
        crow::status::ACCEPTED,
        "json",
        "{\"jobId\":" + std::to_string(job_id) + "}"};
}

auto module_apps_t::do_uninstall(
    std::string app_name, std::string version, bool force, job_progress_t& progress) //
    -> void
{
    const auto app_key = app_key_t{std::move(app_name), std::move(version)};

    // Step 1: Ensure App is actually installed
    if (!is_app_installed(app_name, version)) {
        auto lock = progress.lock();
        progress.result().code = -1;
        progress.result().message =
            "Cannot uninstall "s + app_name + " (" + version + "), which is not installed";
        return;
    }

    // Step 2: Load App manifest
    auto& app = _installed_apps.find(app_key)->second;
    auto manifest = app.manifest();

    // Step 2a: Prevent removal of system apps
    if (cxx20::contains(manifest->category(), "system") && !force) {
        auto lock = progress.lock();
        progress.result().code = -1;
        progress.result().message = "Not uninstalling system app "s + app.key().name().data() +
                                    "(" + app.key().version().data() + ")";
        return;
    }

    app.desired(app_status_e::NotInstalled);

#if 0
    // Step 3: Stop and delete all instances of the App
    const auto instance_ids = _deployment->instance_ids(app_name, version);
    for (const auto& instance_id : instance_ids) {
        do_delete_instance(instance_id, app_name, version, response);
    }
#endif // 0

    // Step 4: Remove Docker image of the App
    const auto image = manifest->image_with_tag();
    auto docker_process = process_t{};
    docker_process.spawnp("docker", "rmi", "-f", image);
    docker_process.wait(false, true);
    if (docker_process.exit_code() != 0) {
        std::fprintf(
            stderr,
            "Warning: Could not remove image %s of app %s (%s)\n",
            image.c_str(),
            app_name.c_str(),
            version.c_str());
    }

    // Step 5: Persist removal of App into db
    _installed_apps.erase(app_key);
    _parent->save();

    // Step 6: Remove App manifest
    _manifests_api->erase(app_key);
}

auto module_apps_t::queue_export_app(std::string app_name, std::string version) const //
    -> crow::response
{
    auto job = job_t{};
    job.callable =
        std::bind(&module_apps_t::do_export_app, this, app_name, version, std::placeholders::_1);

    auto job_id = _jobs_api->append(std::move(job));
    return crow::response{
        crow::status::ACCEPTED,
        "json",
        "{\"jobId\":" + std::to_string(job_id) + "}"};
}

auto module_apps_t::do_export_app(
    std::string app_name, std::string version, job_progress_t& progress) const //
    -> void
{
    // Step 1: Ensure App is actually installed
    if (!is_app_installed(app_name, version)) {
        auto lock = progress.lock();
        progress.result().code = -1;
        progress.result().message =
            "Cannot export "s + app_name + " (" + version + "), which is not installed";
        return;
    }

    // Step 2: Load App manifest
    auto& app = _installed_apps.find(app_key_t{app_name, version})->second;
    auto manifest = app.manifest();

    // Step 3: Create export directory
    auto ec = std::error_code{};
    fs::create_directories("/var/lib/flecs/export-tmp/apps/");
    if (ec) {
        auto lock = progress.lock();
        progress.result().code = -1;
        progress.result().message =
            "Could not create export directory for "s + app_name + " (" + version + ")";
        return;
    }

    // Step 4: Export image
    auto docker_process = process_t{};
    const auto filename = std::string{"/var/lib/flecs/export-tmp/apps/"}
                              .append(app.key().name())
                              .append("_")
                              .append(app.key().version())
                              .append(".tar");
    docker_process.spawnp("docker", "save", "--output", filename, manifest->image_with_tag());
    docker_process.wait(false, true);
    if (docker_process.exit_code() != 0) {
        auto lock = progress.lock();
        progress.result().code = -1;
        progress.result().message = docker_process.stderr();
        return;
    }

    // Step 5: Copy manifest
    const auto manifest_src = fs::path{"/var/lib/flecs/apps"} / app_name / version / "manifest.yml";
    const auto manifest_dst =
        fs::path{"/var/lib/flecs/export-tmp/apps/"} / (app_name + "_" + version + ".yml");
    fs::copy_file(manifest_src, manifest_dst, ec);
    if (ec) {
        auto lock = progress.lock();
        progress.result().code = -1;
        progress.result().message =
            "Could not copy app manifest for "s + app_name + " (" + version + ")";
        return;
    }
}

auto module_apps_t::has_app(std::string_view app_name, std::string_view version) const noexcept //
    -> bool
{
    return _installed_apps.count(app_key_t{std::string{app_name}, std::string{version}});
}

auto module_apps_t::is_app_installed(
    std::string_view app_name, std::string_view version) const noexcept //
    -> bool
{
    if (!has_app(app_name, version)) {
        return false;
    }

    return _installed_apps.at(app_key_t{std::string{app_name}, std::string{version}}).status() ==
           app_status_e::Installed;
}

} // namespace impl
} // namespace FLECS
