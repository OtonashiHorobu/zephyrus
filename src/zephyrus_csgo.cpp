// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/asset/ui.hpp>
#include <zephyrus/csgo/constants.hpp>
#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/hooks/client_mode_hooks.hpp>
#include <zephyrus/csgo/interfaces.hpp>
#include <zephyrus/features/csgo/animations.hpp>
#include <zephyrus/features/csgo/anti_aim.hpp>
#include <zephyrus/features/csgo/backtrack.hpp>
#include <zephyrus/features/csgo/esp.hpp>
#include <zephyrus/features/csgo/fake_lag.hpp>
#include <zephyrus/features/csgo/movement.hpp>
#include <zephyrus/features/csgo/rage.hpp>
#include <zephyrus/features/csgo/resolver.hpp>
#include <zephyrus/features/csgo/visuals.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>
#include <zephyrus/overlay/csgo/overlay.hpp>
#include <zephyrus/platform/detection.hpp>
#include <zephyrus/sdl2/sdl2.hpp>
#include <zephyrus/version.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <boost/di.hpp>
#include <fmt/format.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#if ZEPHYRUS_OS_LINUX
namespace {
using csgo_overlay = zephyrus::csgo::overlay::overlay;

zephyrus::zephyrus *app{};
csgo_overlay *overlay{};
std::atomic<bool> overlay_stop{};
std::thread overlay_installer;

std::optional<zephyrus::csgo::game> game;
std::unique_ptr<zephyrus::csgo::hooks::client_mode_hooks> client_mode_hooks;
std::optional<zephyrus::csgo::features::movement> movement;
std::optional<zephyrus::csgo::features::visuals> visuals;
std::optional<zephyrus::csgo::features::rage> rage;
std::optional<zephyrus::csgo::features::anti_aim> anti_aim;
std::optional<zephyrus::csgo::features::fake_lag> fake_lag;
std::optional<zephyrus::csgo::features::backtrack> backtrack;
std::optional<zephyrus::csgo::features::esp> esp;
std::optional<zephyrus::csgo::features::resolver> resolver;
std::optional<zephyrus::csgo::features::animations> animations;
bool game_hooks_installed{};
bool menu_refreshed{};
std::mutex game_mutex;

/// Manual anti-aim directions and the slow-walk key: live keybinds so the
/// menu can rebind them and pick Off/Always/Hold/Toggle.
zephyrus::keybind manual_left_bind{6, zephyrus::keybind_mode::always};   // C
zephyrus::keybind manual_right_bind{27, zephyrus::keybind_mode::always}; // X
zephyrus::keybind manual_back_bind{29, zephyrus::keybind_mode::always};  // Z
zephyrus::keybind manual_forward_bind{25,
                                      zephyrus::keybind_mode::always}; // V
std::array<bool, 512> key_state{};

/// Feature key bindings: Off / Always / Hold / Toggle, persisted by the menu
/// under `[features]` and fed from the overlay's raw key events.
zephyrus::keybind rage_key{};
zephyrus::keybind anti_aim_key{};
zephyrus::keybind panic_key{};
zephyrus::keybind slow_walk_bind{226, zephyrus::keybind_mode::hold};

[[nodiscard]] bool key_down(const int scancode) noexcept {
    if (scancode <= 0 || scancode >= static_cast<int>(key_state.size())) {
        return false;
    }
    return key_state[static_cast<std::size_t>(scancode)];
}

/// Emergency switch: disable every feature and close the menu. Deliberately
/// not an unload: the callback runs inside a hook where `dlclose` is unsafe.
void panic_disable_features() noexcept {
    if (rage) {
        rage->options().enabled = false;
    }
    if (anti_aim) {
        anti_aim->options().enabled = false;
    }
    if (fake_lag) {
        fake_lag->options().enabled = false;
    }
    if (backtrack) {
        backtrack->options().enabled = false;
    }
    if (esp) {
        esp->options().enabled = false;
    }
    if (movement) {
        movement->options().bhop = false;
        movement->options().auto_strafe = false;
        movement->options().slow_walk = false;
    }
    if (visuals) {
        visuals->options().no_flash = false;
        visuals->options().fov_changer = false;
    }
    if (overlay != nullptr) {
        overlay->close();
    }
    ZEPHYRUS_WARN("panic: all features disabled, menu closed");
}

/// Scripted testing/tuning channel: newline separated commands read from
/// `$ZEPHYRUS_CSGO_CMD` (default `$XDG_RUNTIME_DIR/zephyrus-cmd`). The file is
/// removed after processing, like the ACR channel.
[[nodiscard]] std::string command_path() {
    if (const char *command{std::getenv("ZEPHYRUS_CSGO_CMD")};
        command != nullptr && command[0] != '\0') {
        return command;
    }
    if (const char *runtime{std::getenv("XDG_RUNTIME_DIR")};
        runtime != nullptr && runtime[0] != '\0') {
        return std::string{runtime} + "/zephyrus-cmd";
    }
    return "/tmp/zephyrus-cmd";
}

[[nodiscard]] bool parse_on_off(std::string_view value,
                                bool fallback) noexcept {
    if (value == "on" || value == "1" || value == "true") {
        return true;
    }
    if (value == "off" || value == "0" || value == "false") {
        return false;
    }
    return fallback;
}

/// Escape remote text before it goes through `SetInnerRML`.
[[nodiscard]] std::string escape_rml(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

void log_feature_state() noexcept {
    if (rage) {
        ZEPHYRUS_INFO(
            "rage: enabled={} per_weapon={} default(hc={} dmg={} fov={})",
            rage->options().enabled, rage->options().per_weapon,
            rage->options().defaults.hitchance,
            rage->options().defaults.min_damage, rage->options().defaults.fov);
    }
    if (anti_aim) {
        ZEPHYRUS_INFO(
            "aa: enabled={} pitch={} yaw={} desync={}/{} lby={}",
            anti_aim->options().enabled, anti_aim->options().pitch_mode,
            anti_aim->options().yaw_mode, anti_aim->options().desync,
            anti_aim->options().desync_mode, anti_aim->options().lby_mode);
    }
    if (fake_lag) {
        ZEPHYRUS_INFO("fakelag: enabled={} mode={} limit={} choked={}",
                      fake_lag->options().enabled, fake_lag->options().mode,
                      fake_lag->options().limit, fake_lag->choked());
    }
    if (backtrack) {
        ZEPHYRUS_INFO("backtrack: enabled={} limit={}ms",
                      backtrack->options().enabled,
                      backtrack->options().time_limit);
    }
    if (esp) {
        ZEPHYRUS_INFO("esp: enabled={}", esp->options().enabled);
    }
    if (visuals) {
        ZEPHYRUS_INFO("visuals: noflash={} fov={}",
                      visuals->options().no_flash, visuals->options().fov);
    }
    if (game.has_value()) {
        ZEPHYRUS_INFO("patterns: {} missing", game->resolved().missing);
    }
}

void log_players() noexcept {
    if (!game.has_value()) {
        return;
    }
    const zephyrus::csgo::entity local{game->local_player()};
    for (int index{1}; index <= game->max_clients(); ++index) {
        const zephyrus::csgo::entity player{game->get_entity(index)};
        if (!player.valid()) {
            continue;
        }
        // `player_info_t` is 0x158 bytes, the name starts at +16 (verified in
        // the engine's GetPlayerInfo).
        std::array<std::byte, 0x158> info{};
        const bool has_info{
            zephyrus::csgo::virtual_call<8, bool>(game->engine(), index,
                                                  info.data())};
        const char *name{
            has_info ? reinterpret_cast<const char *>(info.data() + 16) : ""};
        const zephyrus::csgo::anim_state_view state{player.anim_state()};
        ZEPHYRUS_INFO(
            "player {}: alive={} dormant={} team={} hp={} origin=({:.0f},"
            "{:.0f},{:.0f}) eye_yaw={:.1f} state={:#x} state_eye={:.1f} "
            "foot={:.1f} move={:.1f} layers={} info={} name='{}'",
            index, player.is_alive(), player.is_dormant(), player.team_number(),
            player.health(), player.abs_origin().x, player.abs_origin().y,
            player.abs_origin().z, player.eye_angles().y,
            reinterpret_cast<std::uintptr_t>(state.raw()),
            state.valid() ? state.eye_yaw() : 0.0F, state.foot_yaw(),
            state.move_yaw(), player.animation_layer_count(), has_info, name);
        if (const void *raw{state.raw()}; raw != nullptr) {
            std::array<float, 9> fields{};
            constexpr std::ptrdiff_t offsets[]{0x20, 0x24, 0x28, 0x2C, 0x30,
                                               0x34, 0x50, 0x6C, 0x84};
            for (std::size_t i{}; i < fields.size(); ++i) {
                std::memcpy(&fields[i],
                            static_cast<const std::byte *>(raw) + offsets[i],
                            sizeof(float));
            }
            ZEPHYRUS_INFO(
                "  animstate raw: 20={:.2f} 24={:.2f} 28={:.2f} 2C={:.2f} "
                "30={:.2f} 34={:.2f} 50={:.2f} 6C={:.2f} 84={:.2f}",
                fields[0], fields[1], fields[2], fields[3], fields[4],
                fields[5], fields[6], fields[7], fields[8]);
        }
        (void)local;
    }
}

void handle_command(const std::string_view line) noexcept {
    const auto space{line.find(' ')};
    const std::string_view name{line.substr(0, space)};
    const std::string_view value{space == std::string_view::npos
                                     ? std::string_view{}
                                     : line.substr(space + 1)};

    if (name == "rage" && rage) {
        if (!value.empty()) {
            rage->options().enabled =
                parse_on_off(value, rage->options().enabled);
        }
        ZEPHYRUS_INFO("rage {}", rage->options().enabled ? "on" : "off");
    } else if (name == "aa" && anti_aim) {
        if (!value.empty()) {
            anti_aim->options().enabled =
                parse_on_off(value, anti_aim->options().enabled);
        }
        ZEPHYRUS_INFO("aa {}", anti_aim->options().enabled ? "on" : "off");
    } else if (name == "desync" && anti_aim) {
        if (!value.empty()) {
            anti_aim->options().desync =
                parse_on_off(value, anti_aim->options().desync);
        }
        ZEPHYRUS_INFO("desync {}", anti_aim->options().desync ? "on" : "off");
    } else if (name == "fl" && fake_lag) {
        if (!value.empty()) {
            fake_lag->options().enabled =
                parse_on_off(value, fake_lag->options().enabled);
        }
        ZEPHYRUS_INFO("fakelag {}", fake_lag->options().enabled ? "on" : "off");
    } else if (name == "bt" && backtrack) {
        if (!value.empty()) {
            backtrack->options().enabled =
                parse_on_off(value, backtrack->options().enabled);
        }
        ZEPHYRUS_INFO("backtrack {}",
                      backtrack->options().enabled ? "on" : "off");
    } else if (name == "esp" && esp) {
        if (!value.empty()) {
            esp->options().enabled =
                parse_on_off(value, esp->options().enabled);
        }
        ZEPHYRUS_INFO("esp {}", esp->options().enabled ? "on" : "off");
    } else if (name == "autowall" && rage) {
        if (!value.empty()) {
            rage->options().defaults.autowall =
                parse_on_off(value, rage->options().defaults.autowall);
        }
        ZEPHYRUS_INFO("autowall {}",
                      rage->options().defaults.autowall ? "on" : "off");
    } else if (name == "fov" && visuals) {
        if (!value.empty()) {
            visuals->options().fov_changer =
                parse_on_off(value, visuals->options().fov_changer);
            if (const float parsed{
                    std::strtof(std::string{value}.c_str(), nullptr)};
                parsed > 0.0F) {
                visuals->options().fov = parsed;
            }
        }
        ZEPHYRUS_INFO("fov {}", visuals->options().fov_changer
                                    ? visuals->options().fov
                                    : 0.0F);
    } else if (name == "bhop" && movement) {
        if (!value.empty()) {
            movement->options().bhop =
                parse_on_off(value, movement->options().bhop);
        }
        ZEPHYRUS_INFO("bhop {}", movement->options().bhop ? "on" : "off");
    } else if (name == "ui") {
        if (overlay == nullptr) {
            ZEPHYRUS_WARN("ui: overlay not ready");
        } else {
            const auto space2{value.find(' ')};
            const std::string_view action{value.substr(0, space2)};
            const std::string_view argument{
                space2 == std::string_view::npos
                    ? std::string_view{}
                    : value.substr(space2 + 1)};
            if (action == "open") {
                if (!overlay->visible()) {
                    overlay->toggle();
                }
            } else if (action == "close") {
                overlay->close();
            } else if (action == "toggle") {
                overlay->toggle();
            } else if (action == "tab" && !argument.empty()) {
                overlay->menu_controller().show_tab(argument);
            } else if (action == "panel" && !argument.empty()) {
                overlay->menu_controller().show_panel(argument);
            } else if (action == "hide" && !argument.empty()) {
                overlay->menu_controller().set_element_visible(argument, false);
            } else if (action == "show" && !argument.empty()) {
                overlay->menu_controller().set_element_visible(argument, true);
            } else {
                ZEPHYRUS_WARN("unknown ui action '{}'", value);
            }
        }
    } else if (name == "config") {
        if (overlay == nullptr) {
            ZEPHYRUS_WARN("config: overlay not ready");
        } else {
            auto &controller{overlay->menu_controller()};
            const auto space2{value.find(' ')};
            const std::string_view action{value.substr(0, space2)};
            std::string argument{
                space2 == std::string_view::npos
                    ? std::string{}
                    : std::string{value.substr(space2 + 1)}};
            if (argument.empty()) {
                argument = controller.config_name_value();
            }
            if (action == "save") {
                controller.config_save(argument);
            } else if (action == "load") {
                controller.config_load(argument);
            } else if (action == "delete") {
                controller.config_delete(argument);
            } else if (action == "reset") {
                controller.config_reset();
            } else if (action == "export") {
                controller.config_export();
            } else if (action == "import") {
                controller.config_import();
            } else if (action == "folder") {
                controller.open_config_folder();
            } else if (action == "list") {
                for (const std::string &entry : controller.config_names()) {
                    ZEPHYRUS_INFO("config profile: {}", entry);
                }
            } else {
                ZEPHYRUS_WARN("unknown config action '{}'", value);
            }
        }
    } else if (name == "perf") {
        if (overlay == nullptr) {
            ZEPHYRUS_WARN("perf: overlay not ready");
        } else {
            const auto space2{value.find(' ')};
            const std::string_view what{value.substr(0, space2)};
            const std::string_view state{
                space2 == std::string_view::npos
                    ? std::string_view{}
                    : value.substr(space2 + 1)};
            const bool enabled{state != "off"};
            if (what == "fps") {
                overlay->menu_controller().set_fps_updates(enabled);
                ZEPHYRUS_INFO("perf fps {}", enabled ? "on" : "off");
            } else if (what == "anim") {
                overlay->menu_controller().set_caret_animation(enabled);
                ZEPHYRUS_INFO("perf anim {}", enabled ? "on" : "off");
            } else {
                ZEPHYRUS_WARN("unknown perf action '{}'", value);
            }
        }
    } else if (name == "style") {
        if (overlay == nullptr) {
            ZEPHYRUS_WARN("style: overlay not ready");
        } else {
            // style <selector> <property> <value...>
            const auto first{value.find(' ')};
            const std::string_view selector{value.substr(0, first)};
            const std::string_view rest{
                first == std::string_view::npos
                    ? std::string_view{}
                    : value.substr(first + 1)};
            const auto second{rest.find(' ')};
            const std::string_view property{rest.substr(0, second)};
            const std::string_view setting{
                second == std::string_view::npos
                    ? std::string_view{}
                    : rest.substr(second + 1)};
            if (selector.empty() || property.empty()) {
                ZEPHYRUS_WARN("style: usage: style <selector> <property> "
                              "<value|->");
            } else {
                overlay->menu_controller().set_style(
                    selector, property, setting == "-" ? "" : setting);
            }
        }
    } else if (name == "dump") {
        log_feature_state();
    } else if (name == "players") {
        log_players();
    } else if (name == "netvar") {
        if (!value.empty()) {
            const std::uint32_t offset{
                zephyrus::csgo::netvars::get(zephyrus::csgo::fnv1a(value))};
            ZEPHYRUS_INFO("netvar {} = {:#x}", value, offset);
        }
    } else if (name == "trace" && game.has_value()) {
        const zephyrus::csgo::entity local{game->local_player()};
        if (!local.valid()) {
            ZEPHYRUS_WARN("trace: no local player");
        } else {
            const zephyrus::csgo::Vector eye{local.eye_position()};
            const zephyrus::csgo::Vector forward{
                zephyrus::csgo::Vector::forward_from_angle(
                    local.eye_angles())};
            const zephyrus::csgo::trace result{game->trace_ray(
                eye, eye + forward * 2048.0F, 0x4600400BU,
                static_cast<zephyrus::csgo::entity_pod *>(nullptr))};
            const int contents{game->point_contents(eye, 0x4600400BU)};
            ZEPHYRUS_INFO(
                "trace: fraction={:.3f} start_solid={} all_solid={} "
                "hitgroup={} entity={:#x} end=({:.1f},{:.1f},{:.1f}) "
                "contents={:#x}",
                static_cast<double>(result.fraction), result.start_solid,
                result.all_solid, result.hitgroup,
                reinterpret_cast<std::uintptr_t>(result.entity),
                static_cast<double>(result.endpos.x),
                static_cast<double>(result.endpos.y),
                static_cast<double>(result.endpos.z), contents);
        }
    } else {
        ZEPHYRUS_WARN("unknown command '{}'", line);
    }
}

void poll_commands() noexcept {
    const std::string path{command_path()};
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return;
    }
    std::ifstream stream{path};
    if (!stream) {
        return;
    }
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            ZEPHYRUS_INFO("command '{}'", line);
            handle_command(line);
        }
    }
    stream.close();
    std::filesystem::remove(path, error);
}

void report(std::string_view stage, std::string_view error) noexcept;

/// Resolve the client interfaces and install the game hooks. Retried by the
/// installer thread until the engine has loaded its modules. Also called from
/// the game thread (`on_ready`) when the background installer already gave up.
void initialize_game() noexcept {
    std::lock_guard lock{game_mutex};
    if (game.has_value()) {
        return;
    }

    auto instance{zephyrus::csgo::game::create()};
    if (!instance) {
        static std::string last_error;
        if (instance.error() != last_error) {
            last_error = instance.error();
            ZEPHYRUS_WARN("game not ready: {}", last_error);
        }
        return;
    }
    game = *instance;
    ZEPHYRUS_INFO("game resolved: client={:#x} entity_list={:#x} "
                  "client_mode={:#x} input={:#x} global_vars={:#x}",
                  reinterpret_cast<std::uintptr_t>(game->client()),
                  reinterpret_cast<std::uintptr_t>(game->entity_list()),
                  reinterpret_cast<std::uintptr_t>(game->client_mode()),
                  reinterpret_cast<std::uintptr_t>(game->input()),
                  reinterpret_cast<std::uintptr_t>(game->global_vars()));

    movement.emplace();
    visuals.emplace();
    rage.emplace();
    anti_aim.emplace();
    fake_lag.emplace();
    backtrack.emplace();
    esp.emplace();
    resolver.emplace();
    animations.emplace();
    if (rage.has_value()) {
        rage->set_resolver(&*resolver);
        rage->set_backtrack(&*backtrack);
        rage->set_animations(&*animations);
    }
    if (backtrack.has_value()) {
        backtrack->set_animations(&*animations);
    }
}

/// Install the game hooks from the game thread (first present). Inline
/// patching while the game may be executing the target functions from
/// another thread tears their code and crashes the process.
/// Returns true once the hooks are live; failures keep `client_mode_hooks`
/// empty and are retried by the caller.
[[nodiscard]] bool install_game_hooks() noexcept {
    if (game_hooks_installed) {
        return true;
    }
    if (!game.has_value() || !movement || !visuals) {
        return false;
    }
    client_mode_hooks =
        std::make_unique<zephyrus::csgo::hooks::client_mode_hooks>();
    client_mode_hooks->override_view = [](zephyrus::csgo::view_setup &setup) {
        if (visuals) {
            visuals->override_view(setup);
        }
    };
    client_mode_hooks->create_move = [](zephyrus::csgo::user_cmd &cmd,
                                        bool original_send) {
        if (!game.has_value()) {
            return original_send;
        }
        static int command_tick{};
        if ((++command_tick % 15) == 1) {
            poll_commands();
        }
        const float angle_backup{cmd.viewangles.y};
        const zephyrus::csgo::Vector original_angles{cmd.viewangles};
        if (visuals) {
            visuals->apply_no_flash(*game);
        }
        if (movement) {
            movement->options().slow_walk =
                slow_walk_bind.mode != zephyrus::keybind_mode::off;
            movement->set_slow_walk_active(slow_walk_bind.active());
            movement->create_move(*game, cmd);
        }

        // Ragebot first: it must see the player's real view (the anti-aim
        // would otherwise rotate the FOV base) and it sets the fire buttons
        // that fake lag and the anti-aim react to.
        if (rage && rage_key.active()) {
            rage->run(*game, cmd);
        }

        bool send{original_send};
        if (fake_lag) {
            send = fake_lag->run(*game, cmd, send);
        }
        if (anti_aim && anti_aim_key.active()) {
            anti_aim->set_manual_keys(
                key_down(manual_left_bind.key),
                key_down(manual_right_bind.key),
                key_down(manual_back_bind.key),
                key_down(manual_forward_bind.key));
            anti_aim->run(*game, cmd, send, original_angles);
        }

        // Keep the world-space movement direction after the angle rewrites.
        if (movement) {
            movement->fix_movement(cmd, angle_backup);
        }

        return send;
    };
    client_mode_hooks->frame_stage_notify = [](int stage) {
        if (!game.has_value()) {
            return;
        }
        if (animations) {
            animations->update(*game, stage);
        }
    };
    if (auto result{
            client_mode_hooks->install(game->client_mode(), game->client())};
        !result) {
        static bool reported{};
        if (!reported) {
            reported = true;
            report("client mode hooks", result.error());
        }
        client_mode_hooks.reset();
        return false;
    }
    game_hooks_installed = true;
    return true;
}

/// The game must be started with `-insecure`. Without it VAC-secured servers
/// are reachable and the library refuses to install anything: this port is
/// for community/HvH (insecure) servers only.
[[nodiscard]] bool insecure_launch() noexcept {
    std::ifstream file{"/proc/self/cmdline", std::ios::binary};
    if (!file) {
        return false;
    }

    const std::string cmdline{std::istreambuf_iterator<char>{file},
                              std::istreambuf_iterator<char>{}};
    std::string_view args{cmdline};
    while (!args.empty()) {
        const auto separator{args.find('\0')};
        const std::string_view argument{separator == std::string_view::npos
                                            ? args
                                            : args.substr(0, separator)};
        if (argument == "-insecure") {
            return true;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        args.remove_prefix(separator + 1);
    }
    return false;
}

void report(std::string_view stage, std::string_view error) noexcept {
    fmt::println("[zephyrus-csgo] {}: {}", stage, error);
    ZEPHYRUS_ERROR("{}: {}", stage, error);
}

void probe_game_version() noexcept {
    std::ifstream file{"csgo/steam.inf"};
    if (!file) {
        ZEPHYRUS_WARN("csgo/steam.inf not found (cwd is not the game root?)");
        return;
    }

    const auto trimmed{[](std::string_view value) {
        while (!value.empty() &&
               (value.back() == '\r' || value.back() == ' ')) {
            value.remove_suffix(1);
        }
        return value;
    }};

    std::string line;
    while (std::getline(file, line)) {
        if (line.starts_with("PatchVersion=")) {
            const std::string_view value{trimmed(line.c_str() + 13)};
            if (value != zephyrus::csgo::target_patch_version) {
                ZEPHYRUS_WARN(
                    "game patch {} != supported {}; patterns may not resolve",
                    value, zephyrus::csgo::target_patch_version);
            } else {
                ZEPHYRUS_INFO("game patch {}", value);
            }
        } else if (line.starts_with("ClientVersion=")) {
            const std::string_view value{trimmed(line.c_str() + 14)};
            if (value != zephyrus::csgo::target_client_version) {
                ZEPHYRUS_WARN("client version {} != supported {}", value,
                              zephyrus::csgo::target_client_version);
            } else {
                ZEPHYRUS_INFO("client version {}", value);
            }
        }
    }
}

void probe_module(std::string_view name) noexcept {
    auto module{zephyrus::hook::find_module(name)};
    if (!module) {
        ZEPHYRUS_WARN("module {}: {}", name, module.error());
        return;
    }
    ZEPHYRUS_INFO("module {}: base={:#x} size={:#x}", module->path,
                  reinterpret_cast<std::uintptr_t>(module->base), module->size);
}

void probe_interfaces() noexcept {
    struct interface_check {
        std::string_view module;
        std::string_view name;
    };
    constexpr interface_check checks[]{
        {zephyrus::csgo::engine_module, "VEngineClient014"},
        {zephyrus::csgo::engine_module, "VEngineCvar007"},
        {zephyrus::csgo::engine_module, "VModelInfoClient004"},
        {zephyrus::csgo::client_module, "VClient018"},
        {zephyrus::csgo::client_module, "VClientEntityList003"},
    };

    for (const auto &check : checks) {
        auto module{zephyrus::hook::find_module(check.module)};
        if (!module) {
            ZEPHYRUS_WARN("interface {} skipped: {}", check.name,
                          module.error());
            continue;
        }
        auto interface{zephyrus::csgo::create_interface(*module, check.name)};
        if (!interface) {
            ZEPHYRUS_WARN("interface {}: {}", check.name, interface.error());
            continue;
        }
        ZEPHYRUS_INFO("interface {} = {:#x}", check.name,
                      reinterpret_cast<std::uintptr_t>(*interface));
    }
}

void probe_patterns() noexcept {
    struct pattern_check {
        std::string_view module;
        std::string_view name;
        std::string_view pattern;
    };
    // Only the finders that survived the RE audit (see patterns.cpp); a
    // failure disables the corresponding feature instead of aborting.
    constexpr pattern_check checks[]{
        {zephyrus::csgo::client_module, "localPlayer", "83 FF FF 48 8B 05"},
    };

    for (const auto &check : checks) {
        auto module{zephyrus::hook::find_module(check.module)};
        if (!module) {
            ZEPHYRUS_WARN("pattern {} skipped: {}", check.name, module.error());
            continue;
        }
        auto match{zephyrus::hook::find_pattern(*module, check.pattern)};
        if (!match) {
            ZEPHYRUS_WARN("pattern {}: {}", check.name, match.error());
            continue;
        }
        const auto offset{reinterpret_cast<std::uintptr_t>(*match) -
                          reinterpret_cast<std::uintptr_t>(module->base)};
        ZEPHYRUS_INFO("pattern {} @ +{:#x}", check.name, offset);
    }
}

void probe() noexcept {
    probe_game_version();
    probe_module(zephyrus::csgo::client_module);
    probe_module(zephyrus::csgo::engine_module);
    probe_module(zephyrus::csgo::sdl_module);
    probe_interfaces();
    probe_patterns();
}

/// SDL2 is loaded by the engine long after our constructor runs, so the
/// overlay cannot be installed synchronously. Poll for the library, then
/// install the menu hooks (the pointer-slot swap is a single atomic pointer
/// store, safe from this thread; the inline fallback would not be, but the
/// host's FF 25 thunks always provide a slot).
void install_overlay_when_ready() noexcept {
    bool logged_wait{};
    bool logged_video_wait{};
    for (int attempt{}; attempt < 900 && !overlay_stop.load(); ++attempt) {
        if (app == nullptr) {
            return;
        }

        // The game modules load before SDL video is up; retry both sides.
        initialize_game();
        if (overlay != nullptr && game.has_value()) {
            return;
        }

        auto sdl{zephyrus::sdl2::initialize()};
        if (!sdl) {
            if (!logged_wait) {
                logged_wait = true;
                ZEPHYRUS_INFO("overlay: waiting for SDL2 ({})", sdl.error());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // Install only after SDL_Init has run. Before that the exported
        // thunks still point at bootstrap stubs and SDL rewrites its jump
        // slots during initialization, which discards early patches.
        const zephyrus::sdl2::api *api{*sdl};
        if (api->was_init(SDL_INIT_VIDEO) == 0) {
            if (!logged_video_wait) {
                logged_video_wait = true;
                ZEPHYRUS_INFO("overlay: waiting for the SDL video subsystem");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // Give SDL a moment to finish populating the jump slots.
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (overlay == nullptr) {
            auto *candidate{new csgo_overlay{}};
            // Menu bindings: every control persists under `[features]` in the
            // CS:GO profile and drives the live feature object.
            candidate->menu_setup = [candidate](
                                        zephyrus::csgo::overlay::menu &menu) {
                menu.on_config_reloaded = [candidate] {
                    candidate->refresh_menu_bindings();
                };
                // Global keys: menu visibility and the panic switch.
                menu.bind_key("kb-menu", candidate->menu_key(), "kb-menu");
                menu.bind_key("kb-panic", panic_key, "kb-panic");
                if (rage.has_value()) {
                    auto &defaults{rage->options().defaults};
                    menu.bind_toggle("cb-rage-enabled", rage->options().enabled,
                                     [](bool v) {
                                         if (rage)
                                             rage->options().enabled = v;
                                     });
                    menu.bind_toggle(
                        "cb-rage-silent", defaults.silent, [](bool v) {
                            if (rage)
                                rage->options().defaults.silent = v;
                        });
                    menu.bind_toggle(
                        "cb-rage-autofire", defaults.auto_fire, [](bool v) {
                            if (rage)
                                rage->options().defaults.auto_fire = v;
                        });
                    menu.bind_toggle(
                        "cb-rage-autoscope", defaults.auto_scope, [](bool v) {
                            if (rage)
                                rage->options().defaults.auto_scope = v;
                        });
                    menu.bind_toggle(
                        "cb-rage-autostop", defaults.autostop, [](bool v) {
                            if (rage)
                                rage->options().defaults.autostop = v;
                        });
                    menu.bind_toggle("cb-rage-norecoil",
                                     rage->options().no_recoil, [](bool v) {
                                         if (rage)
                                             rage->options().no_recoil = v;
                                     });
                    menu.bind_toggle("cb-rage-visible",
                                     rage->options().visible_only, [](bool v) {
                                         if (rage)
                                             rage->options().visible_only = v;
                                     });
                    menu.bind_toggle("cb-rage-team",
                                     !rage->options().friendly_fire,
                                     [](bool v) {
                                         if (rage)
                                             rage->options().friendly_fire =
                                                 !v;
                                     });
                    menu.bind_toggle(
                        "cb-rage-autowall", defaults.autowall, [](bool v) {
                            if (rage)
                                rage->options().defaults.autowall = v;
                        });
                    menu.bind_toggle("cb-rage-perweapon",
                                     rage->options().per_weapon, [](bool v) {
                                         if (rage)
                                             rage->options().per_weapon = v;
                                     });
                    menu.bind_combo("co-rage-priority", defaults.priority,
                                    [](int v) {
                                        if (rage)
                                            rage->options().defaults.priority =
                                                std::clamp(v, 0, 3);
                                    });
                    menu.bind_combo("co-rage-hitbox", defaults.hitbox,
                                    [](int v) {
                                        if (rage)
                                            rage->options().defaults.hitbox =
                                                std::clamp(v, 0, 7);
                                    });
                    menu.bind_slider("sl-rage-fov", defaults.fov, [](float v) {
                        if (rage)
                            rage->options().defaults.fov = v;
                    });
                    menu.bind_slider(
                        "sl-rage-mindmg",
                        static_cast<float>(defaults.min_damage), [](float v) {
                            if (rage)
                                rage->options().defaults.min_damage =
                                    static_cast<int>(v);
                        });
                    menu.bind_slider(
                        "sl-rage-hitchance",
                        static_cast<float>(defaults.hitchance), [](float v) {
                            if (rage)
                                rage->options().defaults.hitchance =
                                    static_cast<int>(v);
                        });
                    if (backtrack.has_value()) {
                        menu.bind_toggle(
                            "cb-rage-backtrack", backtrack->options().enabled,
                            [](bool v) {
                                if (backtrack)
                                    backtrack->options().enabled = v;
                            });
                    }
                    menu.bind_key("kb-rage", rage_key, "kb-rage");

                    // Weapons page: the element ids are shared, the storage
                    // keys follow the selected category.
                    auto bind_category = [&menu](int category_index) {
                        if (!rage.has_value()) {
                            return;
                        }
                        using zephyrus::csgo::features::weapon_category;
                        const auto category{static_cast<weapon_category>(
                            std::clamp(category_index, 0, 6))};
                        auto &profile{rage->category_profile(category)};
                        const std::string prefix{
                            "wcat" + std::to_string(category_index) + "-"};
                        const auto key =
                            [&prefix](const std::string_view name) {
                                return prefix + std::string{name};
                            };
                        menu.bind_toggle(
                            "w-enabled", profile.enabled,
                            [&profile](bool v) { profile.enabled = v; },
                            key("enabled"));
                        menu.bind_combo(
                            "w-hitbox", profile.hitbox,
                            [&profile](int v) {
                                profile.hitbox = std::clamp(v, 0, 7);
                            },
                            key("hitbox"));
                        menu.bind_combo(
                            "w-priority", profile.priority,
                            [&profile](int v) {
                                profile.priority = std::clamp(v, 0, 3);
                            },
                            key("priority"));
                        menu.bind_slider(
                            "sl-w-fov", profile.fov,
                            [&profile](float v) { profile.fov = v; },
                            key("fov"));
                        menu.bind_slider(
                            "sl-w-hitchance",
                            static_cast<float>(profile.hitchance),
                            [&profile](float v) {
                                profile.hitchance = static_cast<int>(v);
                            },
                            key("hitchance"));
                        menu.bind_slider(
                            "sl-w-mindmg",
                            static_cast<float>(profile.min_damage),
                            [&profile](float v) {
                                profile.min_damage = static_cast<int>(v);
                            },
                            key("mindmg"));
                        menu.bind_toggle(
                            "w-silent", profile.silent,
                            [&profile](bool v) { profile.silent = v; },
                            key("silent"));
                        menu.bind_toggle(
                            "w-autofire", profile.auto_fire,
                            [&profile](bool v) { profile.auto_fire = v; },
                            key("autofire"));
                        menu.bind_toggle(
                            "w-autoscope", profile.auto_scope,
                            [&profile](bool v) { profile.auto_scope = v; },
                            key("autoscope"));
                        menu.bind_toggle(
                            "w-autostop", profile.autostop,
                            [&profile](bool v) { profile.autostop = v; },
                            key("autostop"));
                        menu.bind_toggle(
                            "w-autowall", profile.autowall,
                            [&profile](bool v) { profile.autowall = v; },
                            key("autowall"));
                    };
                    menu.bind_combo("co-wcat", 2, [bind_category](int v) {
                        bind_category(v);
                    });
                }
                if (anti_aim.has_value()) {
                    auto &options{anti_aim->options()};
                    menu.bind_toggle("cb-aa-enabled", options.enabled,
                                     [](bool v) {
                                         if (anti_aim)
                                             anti_aim->options().enabled = v;
                                     });
                    menu.bind_combo("co-aa-pitch", options.pitch_mode,
                                    [](int v) {
                                        if (anti_aim)
                                            anti_aim->options().pitch_mode =
                                                std::clamp(v, 0, 5);
                                    });
                    menu.bind_combo("co-aa-yaw", options.yaw_mode, [](int v) {
                        if (anti_aim)
                            anti_aim->options().yaw_mode = std::clamp(v, 0, 7);
                    });
                    menu.bind_slider("sl-aa-yawadd", options.yaw_add,
                                     [](float v) {
                                         if (anti_aim)
                                             anti_aim->options().yaw_add = v;
                                     });
                    menu.bind_slider(
                        "sl-aa-jitter", options.jitter_range, [](float v) {
                            if (anti_aim)
                                anti_aim->options().jitter_range = v;
                        });
                    menu.bind_slider("sl-aa-spin", options.spin_speed,
                                     [](float v) {
                                         if (anti_aim)
                                             anti_aim->options().spin_speed = v;
                                     });
                    menu.bind_toggle(
                        "cb-aa-attargets", options.auto_direction, [](bool v) {
                            if (anti_aim)
                                anti_aim->options().auto_direction = v;
                        });
                    menu.bind_toggle(
                        "cb-aa-onshot", options.disable_on_shot, [](bool v) {
                            if (anti_aim)
                                anti_aim->options().disable_on_shot = v;
                        });
                    menu.bind_toggle("cb-fa-enabled", options.desync,
                                     [](bool v) {
                                         if (anti_aim)
                                             anti_aim->options().desync = v;
                                     });
                    menu.bind_combo("co-fa-mode", options.desync_mode,
                                    [](int v) {
                                        if (anti_aim)
                                            anti_aim->options().desync_mode =
                                                std::clamp(v, 0, 3);
                                    });
                    menu.bind_combo("co-fa-side", options.desync_side,
                                    [](int v) {
                                        if (anti_aim)
                                            anti_aim->options().desync_side =
                                                std::clamp(v, 0, 2);
                                    });
                    menu.bind_slider(
                        "sl-fa-angle", options.desync_angle, [](float v) {
                            if (anti_aim)
                                anti_aim->options().desync_angle = v;
                        });
                    menu.bind_combo("co-fa-lby", options.lby_mode, [](int v) {
                        if (anti_aim)
                            anti_aim->options().lby_mode = std::clamp(v, 0, 2);
                    });
                    menu.bind_key("kb-aa", anti_aim_key, "kb-aa");
                    menu.bind_key("kb-man-left", manual_left_bind, "kb-man-left");
                    menu.bind_key("kb-man-right", manual_right_bind,
                                  "kb-man-right");
                    menu.bind_key("kb-man-back", manual_back_bind,
                                  "kb-man-back");
                    menu.bind_key("kb-man-forward", manual_forward_bind,
                                  "kb-man-forward");
                }
                if (fake_lag.has_value()) {
                    const auto &defaults{fake_lag->options()};
                    menu.bind_toggle("cb-fl-enabled", defaults.enabled,
                                     [](bool v) {
                                         if (fake_lag)
                                             fake_lag->options().enabled = v;
                                     });
                    menu.bind_toggle("cb-fl-onshot", defaults.on_shot,
                                     [](bool v) {
                                         if (fake_lag)
                                             fake_lag->options().on_shot = v;
                                     });
                    menu.bind_combo("co-fl-mode", defaults.mode, [](int v) {
                        if (fake_lag)
                            fake_lag->options().mode = v > 2 ? 0 : v;
                    });
                    menu.bind_slider(
                        "sl-fl-limit", static_cast<float>(defaults.limit),
                        [](float v) {
                            if (fake_lag)
                                fake_lag->options().limit = static_cast<int>(v);
                        });
                }
                if (backtrack.has_value()) {
                    const auto &defaults{backtrack->options()};
                    menu.bind_toggle("cb-bt-enabled", defaults.enabled,
                                     [](bool v) {
                                         if (backtrack)
                                             backtrack->options().enabled = v;
                                     });
                    menu.bind_slider("sl-bt-limit",
                                     static_cast<float>(defaults.time_limit),
                                     [](float v) {
                                         if (backtrack)
                                             backtrack->options().time_limit =
                                                 static_cast<int>(v);
                                     });
                }
                if (movement.has_value()) {
                    const auto &defaults{movement->options()};
                    menu.bind_toggle("cb-mv-bhop", defaults.bhop, [](bool v) {
                        if (movement)
                            movement->options().bhop = v;
                    });
                    menu.bind_toggle(
                        "cb-mv-autostrafe", defaults.auto_strafe, [](bool v) {
                            if (movement)
                                movement->options().auto_strafe = v;
                        });
                    menu.bind_key("kb-mv-slowwalk", slow_walk_bind,
                                  "kb-mv-slowwalk");
                }
                if (visuals.has_value()) {
                    const auto &defaults{visuals->options()};
                    menu.bind_toggle("cb-vis-noflash", defaults.no_flash,
                                     [](bool v) {
                                         if (visuals)
                                             visuals->options().no_flash = v;
                                     });
                }
                if (resolver.has_value()) {
                    const auto &defaults{resolver->options()};
                    menu.bind_toggle("cb-res-enabled", defaults.enabled,
                                     [](bool v) {
                                         if (resolver)
                                             resolver->options().enabled = v;
                                     });
                    menu.bind_combo("co-res-mode", defaults.mode, [](int v) {
                        if (resolver)
                            resolver->options().mode = v > 1 ? 0 : v;
                    });
                    menu.bind_combo("co-res-side", defaults.side, [](int v) {
                        if (resolver)
                            resolver->options().side = v > 2 ? 0 : v;
                    });
                }
                if (esp.has_value()) {
                    const auto &defaults{esp->options()};
                    menu.bind_toggle("cb-esp-enabled", defaults.enabled,
                                     [](bool v) {
                                         if (esp)
                                             esp->options().enabled = v;
                                     });
                    menu.bind_toggle("cb-esp-box", defaults.box, [](bool v) {
                        if (esp)
                            esp->options().box = v;
                    });
                    menu.bind_toggle("cb-esp-name", defaults.name, [](bool v) {
                        if (esp)
                            esp->options().name = v;
                    });
                    menu.bind_toggle("cb-esp-health", defaults.health,
                                     [](bool v) {
                                         if (esp)
                                             esp->options().health = v;
                                     });
                    menu.bind_toggle("cb-esp-weapon", defaults.weapon,
                                     [](bool v) {
                                         if (esp)
                                             esp->options().weapon = v;
                                     });
                    menu.bind_toggle("cb-esp-distance", defaults.distance,
                                     [](bool v) {
                                         if (esp)
                                             esp->options().distance = v;
                                     });
                    menu.bind_toggle("cb-esp-snapline", defaults.snapline,
                                     [](bool v) {
                                         if (esp)
                                             esp->options().snapline = v;
                                     });
                    menu.bind_toggle("cb-esp-team", defaults.team, [](bool v) {
                        if (esp)
                            esp->options().team = v;
                    });
                    menu.bind_toggle("cb-esp-visible", defaults.visible_only,
                                     [](bool v) {
                                         if (esp)
                                             esp->options().visible_only = v;
                                     });
                }
            };

            candidate->esp_enabled = [] {
                if (!esp.has_value() || !esp->options().enabled ||
                    !game.has_value()) {
                    return false;
                }
                // Skip the whole overlay frame while the local player is
                // dead/spectating: nothing to project and the render path
                // stays quiet across respawns.
                const zephyrus::csgo::entity local{game->local_player()};
                return local.valid() && local.is_alive();
            };
            candidate->esp_draw = [](Rml::Element &layer) {
                constexpr float unset{std::numeric_limits<float>::quiet_NaN()};
                struct row {
                    Rml::Element *box{};
                    Rml::Element *health_bg{};
                    Rml::Element *health_fill{};
                    Rml::Element *name{};
                    Rml::Element *weapon{};
                    Rml::Element *distance{};
                    Rml::Element *snapline{};
                    // Cached widget state: writing the same property again
                    // still dirties RmlUi layout, which was the top CPU
                    // consumer while the menu is open. Only changes are
                    // written.
                    bool box_shown{};
                    float box_left{unset};
                    float box_top{unset};
                    float box_width{unset};
                    float box_height{unset};
                    bool box_visible{};
                    bool box_team{};
                    bool box_enemy{};
                    bool health_shown{};
                    bool fill_shown{};
                    float health_left{unset};
                    float health_top{unset};
                    float health_height{unset};
                    float fill_height{unset};
                    bool name_shown{};
                    float name_left{unset};
                    float name_top{unset};
                    std::string name_text;
                    bool weapon_shown{};
                    float weapon_left{unset};
                    float weapon_top{unset};
                    std::string weapon_text;
                    bool distance_shown{};
                    float distance_left{unset};
                    float distance_top{unset};
                    std::string distance_text;
                    bool snapline_shown{};
                    float snapline_left{unset};
                    float snapline_top{unset};
                    float snapline_width{unset};
                    std::string snapline_transform;
                };
                static std::vector<row> rows;
                static Rml::ElementDocument *rows_document{};

                const auto set_display = [](bool &cached, Rml::Element *element,
                                            const bool visible) {
                    if (element == nullptr || cached == visible) {
                        return;
                    }
                    cached = visible;
                    element->SetProperty("display",
                                         visible ? "block" : "none");
                };
                const auto set_value = [](float &cached, Rml::Element *element,
                                          const char *property,
                                          const float value) {
                    if (element == nullptr) {
                        return;
                    }
                    const float rounded{
                        static_cast<float>(std::lround(value))};
                    if (cached == rounded) {
                        return;
                    }
                    cached = rounded;
                    char buffer[32]{};
                    std::snprintf(buffer, sizeof(buffer), "%dpx",
                                  static_cast<int>(rounded));
                    element->SetProperty(property, buffer);
                };
                const auto set_text_value = [](std::string &cached,
                                               Rml::Element *element,
                                               const std::string_view text) {
                    if (element == nullptr || cached == text) {
                        return;
                    }
                    cached.assign(text);
                    element->SetInnerRML(escape_rml(text));
                };
                const auto set_class = [](bool &cached, Rml::Element *element,
                                          const char *name, const bool value) {
                    if (element == nullptr || cached == value) {
                        return;
                    }
                    cached = value;
                    element->SetClass(name, value);
                };
                const auto hide_all = [&set_display] {
                    for (row &entry : rows) {
                        set_display(entry.box_shown, entry.box, false);
                        set_display(entry.health_shown, entry.health_bg, false);
                        set_display(entry.fill_shown, entry.health_fill, false);
                        set_display(entry.name_shown, entry.name, false);
                        set_display(entry.weapon_shown, entry.weapon, false);
                        set_display(entry.distance_shown, entry.distance,
                                    false);
                        set_display(entry.snapline_shown, entry.snapline,
                                    false);
                    }
                };

                // The document is recreated when the overlay is re-initialized;
                // stale rows would point into freed memory.
                Rml::ElementDocument *document{layer.GetOwnerDocument()};
                if (rows_document != document) {
                    rows.clear();
                    rows_document = document;
                }

                if (!game.has_value() || !esp.has_value() ||
                    !esp->options().enabled) {
                    hide_all();
                    return;
                }
                const zephyrus::csgo::entity local{game->local_player()};
                if (!local.valid() || !local.is_alive()) {
                    hide_all();
                    return;
                }

                const auto make_element = [](Rml::Element &parent,
                                             std::string_view classes) {
                    Rml::ElementDocument *owner{parent.GetOwnerDocument()};
                    if (owner == nullptr) {
                        return static_cast<Rml::Element *>(nullptr);
                    }
                    Rml::ElementPtr element{owner->CreateElement("div")};
                    if (element == nullptr) {
                        return static_cast<Rml::Element *>(nullptr);
                    }
                    for (std::size_t begin{};;) {
                        const std::size_t end{classes.find(' ', begin)};
                        const std::string_view part{
                            classes.substr(begin, end - begin)};
                        if (!part.empty()) {
                            element->SetClass(Rml::String{part}, true);
                        }
                        if (end == std::string_view::npos) {
                            break;
                        }
                        begin = end + 1;
                    }
                    element->SetProperty("display", "none");
                    Rml::Element *raw{element.get()};
                    parent.AppendChild(std::move(element));
                    return raw;
                };

                if (rows.empty()) {
                    rows.reserve(65);
                    for (int index{}; index < 65; ++index) {
                        row entry{};
                        entry.box = make_element(layer, "esp-box");
                        entry.health_bg = make_element(layer, "esp-health-bg");
                        if (entry.health_bg != nullptr) {
                            entry.health_fill = make_element(*entry.health_bg,
                                                             "esp-health-fill");
                        }
                        entry.name = make_element(layer, "esp-label");
                        entry.weapon =
                            make_element(layer, "esp-label esp-weapon");
                        entry.distance =
                            make_element(layer, "esp-label esp-distance");
                        entry.snapline = make_element(layer, "esp-snapline");
                        rows.push_back(entry);
                    }
                }

                const Rml::Vector2i dimensions{
                    layer.GetContext()->GetDimensions()};
                const auto drawables{
                    esp->collect(*game, dimensions.x, dimensions.y)};

                std::size_t used{};
                for (const auto &drawable : drawables) {
                    if (used >= rows.size()) {
                        break;
                    }
                    row &entry{rows[used++]};
                    const float width{drawable.right - drawable.left};
                    const float height{drawable.bottom - drawable.top};
                    const auto &options{esp->options()};

                    set_display(entry.box_shown, entry.box, options.box);
                    set_value(entry.box_left, entry.box, "left",
                              drawable.left);
                    set_value(entry.box_top, entry.box, "top", drawable.top);
                    set_value(entry.box_width, entry.box, "width", width);
                    set_value(entry.box_height, entry.box, "height", height);
                    set_class(entry.box_visible, entry.box, "visible",
                              drawable.visible);
                    set_class(entry.box_team, entry.box, "team",
                              drawable.teammate);
                    set_class(entry.box_enemy, entry.box, "enemy",
                              !drawable.teammate);

                    set_display(entry.health_shown, entry.health_bg,
                                options.health);
                    set_value(entry.health_left, entry.health_bg, "left",
                              drawable.left - 6.0F);
                    set_value(entry.health_top, entry.health_bg, "top",
                              drawable.top);
                    set_value(entry.health_height, entry.health_bg, "height",
                              height);
                    set_display(entry.fill_shown, entry.health_fill,
                                options.health);
                    set_value(entry.fill_height, entry.health_fill, "height",
                              height * drawable.health);

                    set_display(entry.name_shown, entry.name,
                                options.name && !drawable.name.empty());
                    set_value(entry.name_left, entry.name, "left",
                              drawable.left);
                    set_value(entry.name_top, entry.name, "top",
                              drawable.top - 16.0F);
                    // Player names are remote data: never feed them to the
                    // RML parser unescaped (set_text_value escapes).
                    set_text_value(entry.name_text, entry.name, drawable.name);

                    set_display(entry.weapon_shown, entry.weapon,
                                options.weapon && !drawable.weapon.empty());
                    set_value(entry.weapon_left, entry.weapon, "left",
                              drawable.left);
                    set_value(entry.weapon_top, entry.weapon, "top",
                              drawable.bottom + 2.0F);
                    set_text_value(entry.weapon_text, entry.weapon,
                                   drawable.weapon);

                    set_display(entry.distance_shown, entry.distance,
                                options.distance);
                    set_value(entry.distance_left, entry.distance, "left",
                              drawable.left);
                    set_value(entry.distance_top, entry.distance, "top",
                              drawable.bottom + 16.0F);
                    char distance_buffer[32]{};
                    std::snprintf(distance_buffer, sizeof(distance_buffer),
                                  "%.0fm",
                                  static_cast<double>(drawable.distance));
                    set_text_value(entry.distance_text, entry.distance,
                                   distance_buffer);

                    set_display(entry.snapline_shown, entry.snapline,
                                options.snapline);
                    const float x0{static_cast<float>(dimensions.x) * 0.5F};
                    const float y0{static_cast<float>(dimensions.y)};
                    const float x1{(drawable.left + drawable.right) * 0.5F};
                    const float y1{drawable.bottom};
                    const float dx{x1 - x0};
                    const float dy{y1 - y0};
                    set_value(entry.snapline_left, entry.snapline, "left",
                              x0);
                    set_value(entry.snapline_top, entry.snapline, "top", y0);
                    set_value(entry.snapline_width, entry.snapline, "width",
                              std::sqrt(dx * dx + dy * dy));
                    char transform[48]{};
                    std::snprintf(
                        transform, sizeof(transform), "rotate(%ddeg)",
                        static_cast<int>(std::lround(
                            std::atan2(dy, dx) / std::numbers::pi_v<float> *
                            180.0F)));
                    if (entry.snapline != nullptr &&
                        entry.snapline_transform != transform) {
                        entry.snapline_transform = transform;
                        entry.snapline->SetProperty("transform", transform);
                    }
                }

                for (; used < rows.size(); ++used) {
                    row &entry{rows[used]};
                    set_display(entry.box_shown, entry.box, false);
                    set_display(entry.health_shown, entry.health_bg, false);
                    set_display(entry.fill_shown, entry.health_fill, false);
                    set_display(entry.name_shown, entry.name, false);
                    set_display(entry.weapon_shown, entry.weapon, false);
                    set_display(entry.distance_shown, entry.distance, false);
                    set_display(entry.snapline_shown, entry.snapline, false);
                }
            };
            // Retried from the game thread every frame until everything is
            // installed (on_ready clears itself on success). `initialize_game`
            // runs here as well: the background installer may have timed out
            // while the player was still in the main menu, and the engine only
            // creates its ClientMode once a map loads.
            candidate->on_ready = [candidate] {
                if (!game.has_value()) {
                    static int resolve_tick{};
                    if ((++resolve_tick % 15) == 1) {
                        initialize_game();
                    }
                    if (!game.has_value()) {
                        return;
                    }
                }
                if (!menu_refreshed) {
                    // The menu document may have been created before the
                    // feature objects existed (main menu -> map load); bind
                    // the controls now that they are available.
                    candidate->refresh_menu_bindings();
                    menu_refreshed = true;
                }
                if (!game_hooks_installed && !install_game_hooks()) {
                    // Retry next frame (failures are logged once).
                    return;
                }
                candidate->on_ready = nullptr;
            };
            candidate->key_event = [](int scancode, bool down) {
                if (scancode > 0 &&
                    scancode < static_cast<int>(key_state.size())) {
                    key_state[static_cast<std::size_t>(scancode)] = down;
                }
                rage_key.on_key(scancode, down);
                anti_aim_key.on_key(scancode, down);
                slow_walk_bind.on_key(scancode, down);
                if (panic_key.on_key(scancode, down) && down &&
                    panic_key.consume_pressed()) {
                    panic_disable_features();
                }
            };
            if (auto result{candidate->initialize(app->rmlui_manager())};
                !result) {
                report("overlay", result.error());
                delete candidate;
                return;
            }
            overlay = candidate;
        }

        if (game.has_value()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (!overlay_stop.load()) {
        if (overlay == nullptr) {
            ZEPHYRUS_WARN("overlay: SDL2 never appeared, overlay disabled");
        } else {
            // The game/ClientMode did not exist yet; `on_ready` on the game
            // thread keeps resolving and installing from here.
            ZEPHYRUS_INFO("overlay: background installer finished; game "
                          "resolution continues from the overlay thread");
        }
    }
}
} // namespace

/// Entry point for GDB injection / `dlopen`:
///   call (void*)zephyrus_csgo_init()
/// The ELF constructor below calls it when the library is preloaded or
/// `dlopen`ed, so plain LD_PRELOAD works.
extern "C" __attribute__((visibility("default"))) void
zephyrus_csgo_init() noexcept {
    if (app != nullptr) {
        return;
    }

    if (!insecure_launch()) {
        fmt::println("[zephyrus-csgo] refusing to initialize: the game was "
                     "not launched with -insecure; no hooks installed");
        return;
    }

    // A previous shutdown stopped the installer; re-arm it for a reload.
    overlay_stop = false;

    app = boost::di::make_injector(
              boost::di::bind<>.to(cmrc::zephyrus::asset::ui::get_filesystem()),
              boost::di::bind<>.to(zephyrus::backend_mode::injected))
              .create<zephyrus::zephyrus *>();
    if (app == nullptr) {
        fmt::println("[zephyrus-csgo] failed to create the runtime");
        return;
    }

    if (auto result{app->init()}; !result) {
        report("init", result.error());
    }

    ZEPHYRUS_INFO("zephyrus-csgo {} starting (legacy CS:GO linux64)",
                  zephyrus::version::csgo_version());
    probe();

    overlay_installer = std::thread{install_overlay_when_ready};
}

extern "C" __attribute__((visibility("default"))) void
zephyrus_csgo_shutdown() noexcept {
    if (app == nullptr) {
        return;
    }

    // Unloading from inside one of our own detours would free objects that
    // are still on the stack. Refuse; the caller can retry from game code.
    if (zephyrus::hook::active_detour_depth() > 0) {
        fmt::println("[zephyrus-csgo] unload refused: called from inside a "
                     "hook callback, retry from a safe point");
        ZEPHYRUS_WARN("unload refused: called from inside a hook callback");
        return;
    }

    ZEPHYRUS_INFO("unloading zephyrus-csgo");

    overlay_stop = true;
    if (overlay_installer.joinable()) {
        overlay_installer.join();
    }
    game_hooks_installed = false;
    menu_refreshed = false;

    // Restore the SDL2 pointer slots before the RmlUi runtime goes away.
    delete overlay;
    overlay = nullptr;

    // Restore the game hooks while the game code is still mapped.
    if (client_mode_hooks != nullptr) {
        client_mode_hooks->uninstall();
        client_mode_hooks.reset();
    }
    game.reset();
    movement.reset();
    visuals.reset();
    rage.reset();
    anti_aim.reset();
    fake_lag.reset();
    backtrack.reset();
    esp.reset();
    resolver.reset();
    animations.reset();

    app->shutdown();
    delete app;
    app = nullptr;
}

/// Test/automation hook: re-run the module/interface/pattern probe and write
/// the results to the log.
extern "C" __attribute__((visibility("default"))) void
zephyrus_csgo_probe() noexcept {
    if (app == nullptr) {
        fmt::println("[zephyrus-csgo] not initialized");
        return;
    }
    probe();
}

/// Test/automation hook: toggle the overlay without a keyboard event.
extern "C" __attribute__((visibility("default"))) void
zephyrus_csgo_toggle_overlay() noexcept {
    if (overlay != nullptr) {
        overlay->toggle();
    }
}

/// Test/automation: world FOV override (0 restores the default).
extern "C" __attribute__((visibility("default"))) void
zephyrus_csgo_set_fov(int fov) noexcept {
    if (visuals) {
        visuals->options().fov_changer = fov > 0;
        visuals->options().fov = static_cast<float>(fov);
        ZEPHYRUS_INFO("fov changer {}", fov > 0 ? fov : 0);
    }
}

/// Test/automation: bunny hop master switch.
extern "C" __attribute__((visibility("default"))) void
zephyrus_csgo_set_bhop(int enabled) noexcept {
    if (movement) {
        movement->options().bhop = enabled != 0;
        ZEPHYRUS_INFO("bhop {}", enabled != 0 ? "on" : "off");
    }
}

__attribute__((constructor)) void zephyrus_csgo_ctor() noexcept {
    zephyrus_csgo_init();
}

__attribute__((destructor)) void zephyrus_csgo_dtor() noexcept {
    zephyrus_csgo_shutdown();
}
#endif
