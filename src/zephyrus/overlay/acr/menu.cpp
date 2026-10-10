// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/overlay/acr/menu.hpp>

#include <zephyrus/acr/game.hpp>
#include <zephyrus/acr/offsets.hpp>
#include <zephyrus/acr/weapons.hpp>
#include <zephyrus/log.hpp>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementUtilities.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace zephyrus::overlay {
namespace {
std::string escape_rml(std::string_view text) {
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

std::string format_number(float value, int decimals, std::string_view suffix) {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals,
                  static_cast<double>(value));
    return std::string{buffer} + std::string{suffix};
}

Rml::Element *find_ancestor(
    Rml::Element *start,
    const std::unordered_map<Rml::Element *, std::function<void()>> &actions,
    std::function<void()> *out_action = nullptr) {
    for (Rml::Element *element = start; element != nullptr;
         element = element->GetParentNode()) {
        const auto it{actions.find(element)};
        if (it != actions.end()) {
            if (out_action != nullptr) {
                *out_action = it->second;
            }
            return element;
        }
    }
    return nullptr;
}
} // namespace

menu::~menu() = default;

void menu::initialize(Rml::ElementDocument &document, const acr::game *game,
                      features::esp *esp, features::rage *rage,
                      features::thirdperson *thirdperson,
                      features::movement *movement,
                      features::exploits *exploits, keybind *menu_key,
                      std::function<void()> spawn_bot,
                      config_actions config) noexcept {
    document_ = &document;
    game_ = game;
    esp_ = esp;
    rage_ = rage;
    thirdperson_ = thirdperson;
    movement_ = movement;
    exploits_ = exploits;
    menu_key_ = menu_key;
    spawn_bot_ = std::move(spawn_bot);
    config_ = std::move(config);
    config_name_input_ = document.GetElementById("in-config-name");
    config_list_ = document.GetElementById("config-list");

    sidebar_ = document.GetElementById("sidebar");
    glider_ = document.GetElementById("glider");

    for (const auto &[id, group] :
         std::initializer_list<std::pair<std::string_view, std::string_view>>{
             {"nav-aim-main", "aim"},
             {"nav-aim-trigger", "aim"},
             {"nav-visuals-esp", "visuals"},
             {"nav-visuals-camera", "visuals"},
             {"nav-visuals-status", "visuals"},
             {"nav-exp-movement", "exploits"},
             {"nav-exp-teleport", "exploits"},
             {"nav-exp-actions", "exploits"},
             {"nav-exp-packets", "exploits"},
             {"nav-misc-general", "misc"}}) {
        if (Rml::Element *element{document.GetElementById(Rml::String{id})};
            element != nullptr) {
            nav_items_.emplace_back(element, std::string{group});
        }
    }

    edit_profile_ = rage_->load_profile(edit_index_);

    const auto aim_change{[this] { store_edit(); }};
    const auto checkbox{[this](std::string_view id, bool *value,
                               std::function<void()> on_change = {}) {
        bind_checkbox(id, value, std::move(on_change));
    }};

    // Aimbot (per weapon profile).
    checkbox("cb-aim-enabled", &edit_profile_.enabled, aim_change);
    checkbox("cb-aim-silent", &edit_profile_.silent_aim, aim_change);
    checkbox("cb-aim-autofire", &edit_profile_.autofire, aim_change);
    checkbox("cb-aim-visible", &edit_profile_.require_visible, aim_change);
    checkbox("cb-aim-team", &edit_profile_.team_check, aim_change);
    checkbox("cb-aim-magnet", &edit_profile_.head_magnet, aim_change);
    checkbox("cb-aim-norecoil", &edit_profile_.no_recoil, aim_change);
    checkbox("cb-aim-autoscope", &edit_profile_.auto_scope, aim_change);
    checkbox("cb-aim-autostop", &edit_profile_.auto_stop, aim_change);
    checkbox("cb-aim-between", &edit_profile_.between_shots, aim_change);
    // Triggerbot (same profile).
    checkbox("cb-trig-enabled", &edit_profile_.triggerbot, aim_change);
    checkbox("cb-trig-magnet", &edit_profile_.head_magnet, aim_change);
    checkbox("cb-trig-autofire", &edit_profile_.autofire, aim_change);
    // ESP (global).
    checkbox("cb-esp-enabled", &esp_->options().enabled);
    checkbox("cb-esp-name", &esp_->options().draw_name);
    checkbox("cb-esp-health", &esp_->options().draw_health);
    checkbox("cb-esp-distance", &esp_->options().draw_distance);
    checkbox("cb-esp-skeleton", &esp_->options().draw_skeleton);
    checkbox("cb-esp-weapon", &esp_->options().draw_weapon);
    checkbox("cb-esp-glow", &esp_->options().glow);
    checkbox("cb-esp-visible-only", &esp_->options().visible_only);
    checkbox("cb-esp-teammates", &esp_->options().show_teammates);
    if (thirdperson_ != nullptr) {
        checkbox("cb-third-enabled", &thirdperson_enabled_, [this] {
            thirdperson_->set_enabled(thirdperson_enabled_);
        });
        bind_slider("sl-third-distance", "fill-third-distance",
                    "knob-third-distance", "val-third-distance", 1.0F,
                    features::thirdperson::max_distance, 0, "m",
                    [this](float value) {
                        thirdperson_->set_distance(value);
                        thirdperson_distance_ui_ =
                            static_cast<int>(value);
                    });
        bind_keybind("kb-third", "kb-third-text", {}, &thirdperson_->toggle_key);
    }

    bind_slider(
        "sl-aim-fov", "fill-aim-fov", "knob-aim-fov", "val-aim-fov", 1.0F,
        180.0F, 0, "\xc2\xb0", [this](float value) {
            edit_profile_.fov = value;
            store_edit();
        });
    bind_slider("sl-aim-distance", "fill-aim-distance", "knob-aim-distance",
                "val-aim-distance", 4.0F, 512.0F, 0, "m", [this](float value) {
                    edit_profile_.max_distance = value * 4.0F;
                    store_edit();
                });
    bind_slider("sl-aim-hitchance", "fill-aim-hitchance",
                "knob-aim-hitchance", "val-aim-hitchance", 0.0F, 100.0F, 0,
                "%", [this](float value) {
                    edit_profile_.hitchance = static_cast<int>(value);
                    store_edit();
                });

    bind_combo("co-aim-priority", "co-aim-priority-popup",
               "co-aim-priority-text",
               {"co-aim-priority-0", "co-aim-priority-1", "co-aim-priority-2"},
               {"FOV", "Distance", "Health"}, &priority_ui_, [this](int value) {
                   priority_ui_ = value;
                   edit_profile_.priority =
                       static_cast<features::rage_priority>(value);
                   store_edit();
               });
    bind_combo("co-aim-hitbox", "co-aim-hitbox-popup", "co-aim-hitbox-text",
               {"co-aim-hitbox-0", "co-aim-hitbox-1", "co-aim-hitbox-2",
                "co-aim-hitbox-3", "co-aim-hitbox-4"},
               {"Head", "Chest", "Stomach", "Legs", "Lethal"}, &hitbox_ui_,
               [this](int value) {
                   hitbox_ui_ = value;
                   edit_profile_.hitbox =
                       static_cast<features::rage_hitbox>(value);
                   store_edit();
               });
    {
        std::vector<std::string> ids{"co-aim-profile-0"};
        std::vector<std::string> labels{"All weapons"};
        for (int gun{}; gun < acr::gun_count; ++gun) {
            ids.push_back("co-aim-profile-" + std::to_string(gun + 1));
            labels.emplace_back(acr::weapon_name(gun));
        }
        std::vector<std::string_view> id_views;
        id_views.reserve(ids.size());
        for (const std::string &id : ids) {
            id_views.push_back(id);
        }
        bind_combo("co-aim-profile", "co-aim-profile-popup",
                   "co-aim-profile-text", id_views, std::move(labels),
                   &profile_ui_index_, [this](int value) {
                       edit_index_ = value == 0 ? features::rage::all_weapons
                                                : value - 1;
                       edit_profile_ = rage_->load_profile(edit_index_);
                       refresh();
                   });
    }

    bind_keybind("kb-aim", "kb-aim-text", "kb-aim-popup",
                 &rage_->toggle_key);
    bind_keybind("kb-trigger", "kb-trigger-text", "kb-trigger-popup",
                 &rage_->trigger_key);
    bind_keybind("kb-esp", "kb-esp-text", "kb-esp-popup",
                 &esp_->options().toggle_key);
    bind_keybind("kb-menu", "kb-menu-text", {}, menu_key_);

    bind_combo("co-esp-box", "co-esp-box-popup", "co-esp-box-text",
               {"co-esp-box-0", "co-esp-box-1", "co-esp-box-2", "co-esp-box-3",
                "co-esp-box-4"},
               {"None", "2D", "2D Corners", "3D", "3D Corners"},
               reinterpret_cast<int *>(&esp_->options().box),
               [this](int value) {
                   esp_->options().box = static_cast<features::esp_box>(value);
               });
    bind_combo("co-esp-snapline", "co-esp-snapline-popup",
               "co-esp-snapline-text",
               {"co-esp-snapline-0", "co-esp-snapline-1", "co-esp-snapline-2",
                "co-esp-snapline-3"},
               {"Off", "Bottom", "Top", "Crosshair"},
               reinterpret_cast<int *>(&esp_->options().snapline),
               [this](int value) {
                   esp_->options().snapline =
                       static_cast<features::esp_snapline>(value);
               });

    checkbox("cb-aim-skip-prot", &edit_profile_.skip_protected, aim_change);
    checkbox("cb-aim-magnet-all", &edit_profile_.head_magnet_all, aim_change);
    bind_slider("sl-aim-magnet", "fill-aim-magnet", "knob-aim-magnet",
                "val-aim-magnet", 0.0F, 1.95F, 2, "", [this](float value) {
                    edit_profile_.head_magnet_strength = value;
                    store_edit();
                });

    checkbox("cb-esp-spawnprot", &esp_->options().show_spawn_protection);
    if (exploits_ != nullptr) {
        checkbox("cb-esp-tracers", &exploits_->options().tracers);
        checkbox("cb-esp-tracer-own", &exploits_->options().tracer_own);
        bind_slider("sl-esp-tracer-time", "fill-esp-tracer-time",
                    "knob-esp-tracer-time", "val-esp-tracer-time", 0.2F,
                    5.0F, 1, "s", [this](float value) {
                        exploits_->options().tracer_seconds = value;
                    });
    }
    if (movement_ != nullptr) {
        bind_keybind("kb-mov", "kb-mov-text", "kb-mov-popup",
                     &movement_->toggle_key);
        checkbox("cb-mov-speedhack", &movement_->options().speedhack);
        bind_slider("sl-mov-speed", "fill-mov-speed", "knob-mov-speed",
                    "val-mov-speed", 1.0F, 3.0F, 2, "x",
                    [this](float value) {
                        movement_->options().speedhack_multiplier = value;
                    });
        checkbox("cb-mov-bhop", &movement_->options().bhop);
        checkbox("cb-mov-bhop-hold", &movement_->options().bhop_hold_jump);
        checkbox("cb-mov-air", &movement_->options().air_control);
        checkbox("cb-mov-air-stop", &movement_->options().air_control_stop);
        checkbox("cb-mov-air-boost", &movement_->options().air_control_boost);
    }
    if (exploits_ != nullptr) {
        bind_keybind("kb-tp-forward", "kb-tp-forward-text",
                     "kb-tp-forward-popup", &exploits_->blink_key);
        bind_click("btn-tp-forward",
                   [this] { exploits_->trigger_blink(0); });
        bind_keybind("kb-tp-crosshair", "kb-tp-crosshair-text",
                     "kb-tp-crosshair-popup", &exploits_->blink_crosshair_key);
        bind_click("btn-tp-crosshair",
                   [this] { exploits_->trigger_blink(1); });
        bind_keybind("kb-tp-target", "kb-tp-target-text", "kb-tp-target-popup",
                     &exploits_->blink_target_key);
        bind_click("btn-tp-target", [this] { exploits_->trigger_blink(2); });
        bind_slider("sl-tp-distance", "fill-tp-distance", "knob-tp-distance",
                    "val-tp-distance", 1.0F, 64.0F, 0, "m",
                    [this](float value) {
                        exploits_->options().blink_distance_m = value;
                    });
        checkbox("cb-tp-fade", &exploits_->options().blink_fade);
        checkbox("cb-tp-velocity", &exploits_->options().blink_keep_velocity);

        bind_keybind("kb-exp-knife", "kb-exp-knife-text",
                     "kb-exp-knife-popup", &exploits_->knife_key);
        bind_click("btn-exp-knife", [this] { exploits_->trigger_knife(); });
        checkbox("cb-exp-knife-recycle",
                 &exploits_->options().knife_recycle);
        bind_slider("sl-exp-knife-spacing", "fill-exp-knife-spacing",
                    "knob-exp-knife-spacing", "val-exp-knife-spacing", 300.0F,
                    2000.0F, 0, "ms", [this](float value) {
                        exploits_->options().knife_spacing_ms =
                            static_cast<int>(value);
                    });
        bind_keybind("kb-exp-nade", "kb-exp-nade-text", "kb-exp-nade-popup",
                     &exploits_->nade_key);
        bind_click("btn-exp-nade", [this] { exploits_->trigger_nade(); });
        checkbox("cb-exp-nade-crosshair",
                 &exploits_->options().nade_at_crosshair);
        bind_slider("sl-exp-nade-spacing", "fill-exp-nade-spacing",
                    "knob-exp-nade-spacing", "val-exp-nade-spacing", 600.0F,
                    3000.0F, 0, "ms", [this](float value) {
                        exploits_->options().nade_spacing_ms =
                            static_cast<int>(value);
                    });

        checkbox("cb-pkt-scope", &exploits_->options().force_scope);
        checkbox("cb-pkt-crouch", &exploits_->options().force_crouch);
        checkbox("cb-pkt-velocity", &exploits_->options().zero_velocity);
        checkbox("cb-pkt-onfloor", &exploits_->options().force_onfloor);
    }

    bind_click("btn-config-save", [this] {
        if (config_.save) {
            config_.save(config_name_value());
        }
    });
    bind_click("btn-config-load", [this] {
        if (config_.load) {
            config_.load(config_name_value());
        }
    });
    bind_click("btn-config-delete", [this] {
        if (config_.remove) {
            config_.remove(config_name_value());
        }
    });
    bind_click("btn-config-folder", [this] {
        if (config_.open_folder) {
            config_.open_folder();
        }
    });
    bind_click("btn-config-reset", [this] {
        if (config_.reset) {
            config_.reset();
        }
    });
    bind_click("btn-config-export", [this] {
        if (config_.export_clipboard) {
            config_.export_clipboard();
        }
    });
    bind_click("btn-config-import", [this] {
        if (config_.import_clipboard) {
            config_.import_clipboard();
        }
    });

    bind_click("tab-aim", [this] { select_tab("aim"); });
    bind_click("tab-visuals", [this] { select_tab("visuals"); });
    bind_click("tab-exploits", [this] { select_tab("exploits"); });
    bind_click("tab-misc", [this] { select_tab("misc"); });

    bind_click("nav-aim-main", [this] { select_panel("aim-main"); });
    bind_click("nav-aim-trigger", [this] { select_panel("aim-trigger"); });
    bind_click("nav-visuals-esp", [this] { select_panel("visuals-esp"); });
    bind_click("nav-visuals-camera", [this] {
        select_panel("visuals-camera");
    });
    bind_click("nav-visuals-status", [this] {
        select_panel("visuals-status");
    });
    bind_click("nav-misc-general", [this] { select_panel("misc-general"); });
    bind_click("nav-exp-movement", [this] {
        select_panel("exp-movement");
    });
    bind_click("nav-exp-teleport", [this] {
        select_panel("exp-teleport");
    });
    bind_click("nav-exp-actions", [this] { select_panel("exp-actions"); });
    bind_click("nav-exp-packets", [this] { select_panel("exp-packets"); });

    bind_click("btn-spawn-bot", [this] {
        if (spawn_bot_) {
            spawn_bot_();
        }
    });

    // Sliders answer to press and drag.
    for (const auto &binding : sliders_) {
        binding->slider->AddEventListener("mousedown", this);
    }
    document.AddEventListener("mousedown", this);
    document.AddEventListener("mousemove", this);
    document.AddEventListener("mouseup", this);
    document.AddEventListener("click", this);
    document.AddEventListener("mousescroll", this);
    menu_root_ = document.GetElementById("menu");
    header_ = document.GetElementById("menu-header");
    tabs_ = document.GetElementById("tabs");
    drag_handle_ = document.GetElementById("wordmark");
    resize_handle_ = document.GetElementById("resize-handle");
    content_area_ = document.GetElementById("content");
    scroll_content_ = document.GetElementById("content-scroll");

    select_tab(active_tab_);
    refresh();

    ZEPHYRUS_INFO("menu controller initialized");
}

void menu::shutdown() noexcept {
    document_ = nullptr;
    game_ = nullptr;
    esp_ = nullptr;
    rage_ = nullptr;
    thirdperson_ = nullptr;
    movement_ = nullptr;
    exploits_ = nullptr;
    config_ = {};
    config_name_input_ = nullptr;
    config_list_ = nullptr;
    config_rows_.clear();
    sidebar_ = nullptr;
    glider_ = nullptr;
    click_actions_.clear();
    sliders_.clear();
    slider_lookup_.clear();
    combos_.clear();
    combo_lookup_.clear();
    checkbox_bindings_.clear();
    text_cache_.clear();
    active_slider_ = nullptr;
    open_combo_ = nullptr;
}

void menu::bind_checkbox(std::string_view id, bool *value,
                         std::function<void()> on_change) noexcept {
    Rml::Element *element{document_->GetElementById(Rml::String{id})};
    if (element == nullptr || value == nullptr) {
        ZEPHYRUS_WARN("menu: missing checkbox {}", id);
        return;
    }
    checkbox_bindings_.emplace_back(element, value);
    click_actions_[element] = [this, element, value,
                               change = std::move(on_change),
                               name = std::string{id}] {
        *value = !*value;
        element->SetClass("checked", *value);
        if (change) {
            change();
        }
        ZEPHYRUS_TRACE("menu: {} = {}", name, *value);
    };
}

void menu::bind_slider(std::string_view id, std::string_view fill_id,
                       std::string_view knob_id, std::string_view value_id,
                       float min, float max, int decimals, std::string suffix,
                       std::function<void(float)> apply) noexcept {
    auto binding{std::make_unique<slider_binding>()};
    binding->slider = document_->GetElementById(Rml::String{id});
    binding->fill = document_->GetElementById(Rml::String{fill_id});
    binding->knob = document_->GetElementById(Rml::String{knob_id});
    binding->value_label = document_->GetElementById(Rml::String{value_id});
    binding->min = min;
    binding->max = max;
    binding->decimals = decimals;
    binding->suffix = std::move(suffix);
    binding->apply = std::move(apply);

    if (binding->slider == nullptr || binding->fill == nullptr ||
        binding->knob == nullptr) {
        ZEPHYRUS_WARN("menu: missing slider {}", id);
        return;
    }
    slider_lookup_[binding->slider] = binding.get();
    sliders_.push_back(std::move(binding));
}

void menu::bind_combo(std::string_view id, std::string_view popup_id,
                      std::string_view text_id,
                      const std::vector<std::string_view> &item_ids,
                      std::vector<std::string> labels, int *value,
                      std::function<void(int)> apply) noexcept {
    auto binding{std::make_unique<combo_binding>()};
    binding->combo = document_->GetElementById(Rml::String{id});
    binding->popup = document_->GetElementById(Rml::String{popup_id});
    binding->text = document_->GetElementById(Rml::String{text_id});
    binding->labels = std::move(labels);
    binding->apply = std::move(apply);
    binding->value = value;

    if (binding->combo == nullptr || binding->popup == nullptr ||
        binding->text == nullptr) {
        ZEPHYRUS_WARN("menu: missing combo {}", id);
        return;
    }

    combo_binding *view{binding.get()};
    for (std::size_t index{}; index < item_ids.size(); ++index) {
        Rml::Element *item{
            document_->GetElementById(Rml::String{item_ids[index]})};
        if (item == nullptr) {
            continue;
        }
        binding->items.push_back(item);
        click_actions_[item] = [this, view, index] {
            if (view->value != nullptr) {
                *view->value = static_cast<int>(index);
            }
            if (view->apply) {
                view->apply(static_cast<int>(index));
            }
            for (std::size_t i{}; i < view->items.size(); ++i) {
                view->items[i]->SetClass("selected", i == index);
            }
            if (index < view->labels.size()) {
                set_text(view->text, view->labels[index]);
            }
            close_popups();
        };
    }

    combo_lookup_[binding->combo] = view;
    click_actions_[binding->combo] = [this, view] {
        const bool was_open{open_combo_ == view};
        close_popups();
        if (!was_open) {
            position_popup(view->popup, view->combo);
            view->popup->SetClass("open", true);
            open_combo_ = view;
        }
        ZEPHYRUS_TRACE("menu: combo toggled (was_open={}, now_open={})",
                       was_open, open_combo_ == view);
    };

    combos_.push_back(std::move(binding));
}

void menu::bind_keybind(std::string_view id, std::string_view text_id,
                        std::string_view popup_id, keybind *bind) noexcept {
    auto binding{std::make_unique<keybind_binding>()};
    binding->bind = bind;
    binding->button = document_->GetElementById(Rml::String{id});
    binding->text = document_->GetElementById(Rml::String{text_id});
    if (!popup_id.empty()) {
        binding->popup = document_->GetElementById(Rml::String{popup_id});
    }
    if (binding->button == nullptr || binding->text == nullptr) {
        ZEPHYRUS_WARN("menu: missing keybind {}", id);
        return;
    }

    keybind_binding *view{binding.get()};
    if (binding->popup != nullptr) {
        const std::string base{id};
        const struct {
            const char *suffix;
            keybind_mode mode;
        } modes[]{
            {"-off", keybind_mode::off},
            {"-always", keybind_mode::always},
            {"-hold", keybind_mode::hold},
            {"-toggle", keybind_mode::toggle},
        };
        for (const auto &[suffix, mode] : modes) {
            if (Rml::Element *item{
                    document_->GetElementById(base + suffix)};
                item != nullptr) {
                click_actions_[item] = [this, view, mode] {
                    set_keybind_mode(*view, mode);
                };
            }
        }
        if (Rml::Element *item{
                document_->GetElementById(base + "-unset")};
            item != nullptr) {
            click_actions_[item] = [this, view] {
                if (view->bind != nullptr) {
                    view->bind->key = 0;
                    view->bind->reset();
                }
                update_keybind_text(*view);
                close_popups();
            };
        }
    }

    keybind_lookup_[binding->button] = view;
    keybinds_.push_back(std::move(binding));
    update_keybind_text(*keybinds_.back());
}

void menu::start_listening(keybind_binding &binding) noexcept {
    close_popups();
    listening_ = &binding;
    set_text(binding.text, "...");
    binding.button->SetClass("listening", true);
}

void menu::capture_key(int code) noexcept {
    if (listening_ == nullptr || listening_->bind == nullptr) {
        listening_ = nullptr;
        return;
    }
    // A plain switch (Always, no key) becomes a Toggle once a key is bound,
    // otherwise pressing the new key would not change anything.
    if (listening_->bind->mode == keybind_mode::always &&
        listening_->bind->key == 0) {
        listening_->bind->mode = keybind_mode::toggle;
    }
    listening_->bind->key = code;
    listening_->bind->mode = listening_->bind->mode == keybind_mode::off
                                 ? keybind_mode::toggle
                                 : listening_->bind->mode;
    listening_->bind->reset();
    listening_->button->SetClass("listening", false);
    update_keybind_text(*listening_);
    listening_ = nullptr;
    ZEPHYRUS_TRACE("menu: keybind captured key={}", code);
}

void menu::set_keybind_mode(keybind_binding &binding,
                            keybind_mode mode) noexcept {
    if (binding.bind != nullptr) {
        binding.bind->mode = mode;
        binding.bind->reset();
    }
    update_keybind_text(binding);
    close_popups();
}

void menu::update_keybind_text(keybind_binding &binding) noexcept {
    if (binding.bind == nullptr) {
        return;
    }
    switch (binding.bind->mode) {
    case keybind_mode::off:
        set_text(binding.text, "Off");
        return;
    case keybind_mode::always:
        set_text(binding.text, "On");
        return;
    case keybind_mode::hold:
    case keybind_mode::toggle:
        break;
    }
    if (binding.bind->key == 0) {
        set_text(binding.text, "Bind");
    } else {
        set_text(binding.text, key_name(binding.bind->key));
    }
}

void menu::refresh_keybinds() noexcept {
    for (const auto &binding : keybinds_) {
        update_keybind_text(*binding);
    }
}

void menu::bind_click(std::string_view id,
                      std::function<void()> action) noexcept {
    Rml::Element *element{document_->GetElementById(Rml::String{id})};
    if (element == nullptr) {
        ZEPHYRUS_WARN("menu: missing element {}", id);
        return;
    }
    click_actions_[element] = std::move(action);
}

void menu::select_tab(std::string_view tab) noexcept {
    active_tab_ = std::string{tab};

    for (const std::string_view name :
         {"aim", "visuals", "exploits", "misc"}) {
        if (Rml::Element *element{
                document_->GetElementById("tab-" + Rml::String{name})};
            element != nullptr) {
            element->SetClass("active", name == active_tab_);
        }
    }

    // Sidebar entries belong to exactly one tab.
    for (const auto &[element, group] : nav_items_) {
        element->SetClass("hidden", group != active_tab_);
    }

    const std::string_view default_panel{
        tab == "aim"        ? "aim-main"
        : tab == "visuals"  ? "visuals-esp"
        : tab == "exploits" ? "exp-movement"
                            : "misc-general"};
    select_panel(default_panel);
}

void menu::select_panel(std::string_view panel) noexcept {
    active_panel_ = std::string{panel};

    for (const std::string_view name :
         {"aim-main", "aim-trigger", "visuals-esp", "visuals-camera",
          "visuals-status", "exp-movement", "exp-teleport", "exp-actions",
          "exp-packets", "misc-general"}) {
        if (Rml::Element *element{
                document_->GetElementById("panel-" + Rml::String{name})};
            element != nullptr) {
            element->SetClass("active", name == active_panel_);
        }
        if (Rml::Element *element{
                document_->GetElementById("nav-" + Rml::String{name})};
            element != nullptr) {
            element->SetClass("active", name == active_panel_);
        }
    }

    refresh_glider();
    animate_active_panel();
    refresh_content_width();
    reset_scroll();
}

void menu::animate_active_panel() noexcept {
    if (document_ == nullptr) {
        return;
    }
    Rml::Element *panel{
        document_->GetElementById("panel-" + Rml::String{active_panel_})};
    if (panel == nullptr) {
        return;
    }
    Rml::ElementList sections;
    panel->GetElementsByTagName(sections, "section");
    int index{};
    for (Rml::Element *section : sections) {
        const std::string delay{
            std::to_string(static_cast<double>(index) * 0.05)};
        section->SetProperty("animation", "section-in 0.32s quintic-out " +
                                              delay + "s");
        ++index;
    }
}

void menu::refresh_glider() noexcept {
    if (document_ == nullptr || sidebar_ == nullptr || glider_ == nullptr) {
        return;
    }
    Rml::Element *active{
        document_->GetElementById("nav-" + Rml::String{active_panel_})};
    if (active == nullptr || active->IsClassSet("hidden")) {
        return;
    }
    const float top{active->GetAbsoluteTop() - sidebar_->GetAbsoluteTop()};
    const float height{active->GetClientHeight()};
    if (height <= 0.0F) {
        return;
    }
    const float scale{dp_scale()};
    glider_->SetProperty(
        "top", std::to_string(static_cast<int>(std::lround(top * scale))) +
                   "dp");
    glider_->SetProperty(
        "height", std::to_string(static_cast<int>(std::lround(height * scale))) +
                      "dp");
}

void menu::update_slider_from_mouse(slider_binding &binding,
                                    int mouse_x) noexcept {
    const float left{binding.slider->GetAbsoluteLeft()};
    const float width{binding.slider->GetClientWidth()};
    if (width <= 0.0F) {
        return;
    }
    float t{(static_cast<float>(mouse_x) - left) / width};
    t = std::clamp(t, 0.0F, 1.0F);
    float value{binding.min + t * (binding.max - binding.min)};
    if (binding.decimals == 0) {
        value = std::round(value);
    }
    set_slider_value(binding, value);
    if (binding.apply) {
        binding.apply(value);
    }
}

void menu::set_slider_value(slider_binding &binding, float value) noexcept {
    value = std::clamp(value, binding.min, binding.max);
    const float t{(value - binding.min) / (binding.max - binding.min)};
    const std::string percent{
        std::to_string(static_cast<int>(std::round(t * 100.0F))) + "%"};
    binding.fill->SetProperty("width", percent);
    binding.knob->SetProperty("left", percent);
    if (binding.value_label != nullptr) {
        set_text(binding.value_label,
                 format_number(value, binding.decimals, binding.suffix));
    }
}

void menu::close_popups() noexcept {
    if (open_combo_ != nullptr) {
        open_combo_->popup->SetClass("open", false);
        open_combo_ = nullptr;
    }
    if (open_keybind_ != nullptr && open_keybind_->popup != nullptr) {
        open_keybind_->popup->SetClass("open", false);
        open_keybind_ = nullptr;
    }
}

void menu::set_text(Rml::Element *element, std::string_view text) noexcept {
    if (element != nullptr) {
        element->SetInnerRML(escape_rml(text));
    }
}

std::string menu::config_name_value() const noexcept {
    if (config_name_input_ == nullptr) {
        return {};
    }
    return config_name_input_->GetAttribute<Rml::String>("value", "");
}

void menu::refresh_config_list() noexcept {
    if (config_list_ == nullptr || !config_.list) {
        return;
    }
    for (const auto &[element, name] : config_rows_) {
        (void)name;
        click_actions_.erase(element);
        config_list_->RemoveChild(element);
    }
    config_rows_.clear();

    const std::string selected{config_name_value()};
    const std::vector<std::string> entries{config_.list ? config_.list()
                                                        : std::vector<std::string>{}};
    // The list has a fixed height; show the newest handful. Extra profiles
    // stay reachable by typing their name.
    constexpr std::size_t max_rows{4};
    std::size_t shown{};
    for (const std::string &name : entries) {
        if (shown++ >= max_rows) {
            break;
        }
        Rml::ElementPtr element{document_->CreateElement("div")};
        if (element == nullptr) {
            break;
        }
        element->SetClass("config-item", true);
        element->SetInnerRML(
            escape_rml(name == "latest" ? "latest (snapshot)" : name));
        Rml::Element *raw{element.get()};
        config_list_->AppendChild(std::move(element));
        config_rows_.emplace_back(raw, name);
        click_actions_[raw] = [this, name] { select_config_row(name); };
        if (name == selected) {
            raw->SetClass("selected", true);
        }
    }
}

void menu::select_config_row(std::string_view name) noexcept {
    const std::string value{name};
    if (config_name_input_ != nullptr) {
        config_name_input_->SetAttribute("value", Rml::String{value});
    }
    for (const auto &[element, row_name] : config_rows_) {
        element->SetClass("selected", row_name == value);
    }
}

void menu::set_cached(std::string_view id, std::string_view text) noexcept {
    const std::string key{id};
    const std::string value{text};
    const auto it{text_cache_.find(key)};
    if (it != text_cache_.end() && it->second == value) {
        return;
    }
    text_cache_[key] = value;
    set_text(document_->GetElementById(key), value);
}

void menu::set_cached_number(std::string_view id, int value,
                             std::string_view suffix) noexcept {
    float &display{number_display_[std::string{id}]};
    const float target{static_cast<float>(value)};
    if (std::abs(display - target) < 0.6F) {
        display = target;
    } else {
        // Ease toward the target like Analyst's AnimatedNumber.
        display += (target - display) * 0.35F;
    }
    set_cached(id, std::to_string(static_cast<int>(std::lround(display))) +
                       std::string{suffix});
}

void menu::store_edit() noexcept {
    if (rage_ == nullptr) {
        return;
    }
    rage_->store_profile(edit_index_, edit_profile_);
}

void menu::refresh() noexcept {
    if (document_ == nullptr) {
        return;
    }

    if (rage_ != nullptr) {
        edit_profile_ = rage_->load_profile(edit_index_);
        profile_ui_index_ =
            edit_index_ == features::rage::all_weapons ? 0 : edit_index_ + 1;
        priority_ui_ = static_cast<int>(edit_profile_.priority);
        hitbox_ui_ = static_cast<int>(edit_profile_.hitbox);
    }
    if (thirdperson_ != nullptr) {
        thirdperson_enabled_ = thirdperson_->enabled();
        thirdperson_distance_ui_ =
            static_cast<int>(thirdperson_->distance());
    }

    for (const auto &[element, value] : checkbox_bindings_) {
        element->SetClass("checked", value != nullptr && *value);
    }

    // Sliders: read current option values back into their visuals.
    for (const auto &binding : sliders_) {
        const Rml::String &id{binding->slider->GetId()};
        if (id == "sl-aim-fov" && rage_ != nullptr) {
            set_slider_value(*binding, edit_profile_.fov);
        } else if (id == "sl-aim-distance" && rage_ != nullptr) {
            set_slider_value(*binding, edit_profile_.max_distance / 4.0F);
        } else if (id == "sl-aim-hitchance" && rage_ != nullptr) {
            set_slider_value(*binding,
                             static_cast<float>(edit_profile_.hitchance));
        } else if (id == "sl-aim-magnet" && rage_ != nullptr) {
            set_slider_value(*binding, edit_profile_.head_magnet_strength);
        } else if (id == "sl-third-distance" && thirdperson_ != nullptr) {
            set_slider_value(*binding,
                             static_cast<float>(thirdperson_distance_ui_));
        } else if (id == "sl-esp-tracer-time" && exploits_ != nullptr) {
            set_slider_value(*binding, exploits_->options().tracer_seconds);
        } else if (id == "sl-mov-speed" && movement_ != nullptr) {
            set_slider_value(*binding,
                             movement_->options().speedhack_multiplier);
        } else if (id == "sl-tp-distance" && exploits_ != nullptr) {
            set_slider_value(*binding, exploits_->options().blink_distance_m);
        } else if (id == "sl-exp-knife-spacing" && exploits_ != nullptr) {
            set_slider_value(
                *binding, static_cast<float>(
                              exploits_->options().knife_spacing_ms));
        } else if (id == "sl-exp-nade-spacing" && exploits_ != nullptr) {
            set_slider_value(
                *binding,
                static_cast<float>(exploits_->options().nade_spacing_ms));
        }
    }

    for (const auto &binding : combos_) {
        const int index{binding->value != nullptr ? *binding->value : 0};
        for (std::size_t i{}; i < binding->items.size(); ++i) {
            binding->items[i]->SetClass("selected",
                                        static_cast<int>(i) == index);
        }
        if (index >= 0 &&
            static_cast<std::size_t>(index) < binding->labels.size()) {
            set_text(binding->text, binding->labels[index]);
        }
    }

    refresh_keybinds();
    refresh_config_list();
}

void menu::sync(int fps, bool /*overlay_visible*/) noexcept {
    if (document_ == nullptr) {
        return;
    }

    // A window resize can leave a dragged window partly outside; pull it back.
    if (menu_moved_ && menu_root_ != nullptr) {
        const float view_w{static_cast<float>(viewport_width_)};
        const float view_h{static_cast<float>(viewport_height_)};
        const float width{menu_root_->GetClientWidth()};
        if (view_w > 32.0F && view_h > 32.0F) {
            const float margin{56.0F};
            const float min_x{-(width - margin)};
            const float max_x{std::max(view_w - margin, min_x)};
            const float max_y{std::max(view_h - margin, 0.0F)};
            const float x{
                std::clamp(menu_root_->GetAbsoluteLeft(), min_x, max_x)};
            const float y{
                std::clamp(menu_root_->GetAbsoluteTop(), 0.0F, max_y)};
            if (std::abs(x - menu_root_->GetAbsoluteLeft()) > 1.0F ||
                std::abs(y - menu_root_->GetAbsoluteTop()) > 1.0F) {
                const float scale{dp_scale()};
                menu_root_->SetProperty(
                    "left", std::to_string(static_cast<int>(
                                          std::lround(x * scale))) +
                                "dp");
                menu_root_->SetProperty(
                    "top", std::to_string(static_cast<int>(
                                         std::lround(y * scale))) +
                               "dp");
            }
        }
    }
    materialize_position();

    // A window resize changes every dp based size; re-flow the content tree
    // explicitly, RmlUi keeps the previous layout otherwise. The same call
    // fixes up the width after a resize: it reads the layout produced by the
    // previous update, so it converges within a frame or two.
    if (viewport_width_ != sync_view_w_ || viewport_height_ != sync_view_h_) {
        sync_view_w_ = viewport_width_;
        sync_view_h_ = viewport_height_;
        content_width_dp_ = 0;
    }
    refresh_content_width();

    // The profile list mirrors the directory; refresh it while visible so
    // externally added/removed files show up (OAE polls every 0.1 s).
    if (active_panel_ == "misc-general" &&
        (++config_refresh_counter_ % 30) == 1) {
        refresh_config_list();
    }

    if ((++sync_counter_ % 15) != 1) {
        return;
    }

    set_cached("fps-label", std::to_string(fps) + " FPS");
    refresh_glider();

    if (game_ == nullptr) {
        return;
    }
    const std::vector<acr::player> players{game_->players()};
    set_cached_number("val-players", static_cast<int>(players.size()));
    set_cached_number("val-bots", game_->bot_count());

    if (esp_ != nullptr) {
        set_cached_number("val-drawn",
                          static_cast<int>(esp_->last_drawn()));
    }

    const acr::player local{game_->local_player()};
    if (local.valid()) {
        set_cached_number("val-ping", local.ping(), " ms");
        set_cached("val-weapon", std::to_string(local.weapon_type()));
    }

    if (rage_ != nullptr && rage_->has_target()) {
        set_cached("val-target",
                   "#" + std::to_string(rage_->target().clientnum()));
    } else {
        set_cached("val-target", "none");
    }
    set_cached("val-bots-general", std::to_string(game_->bot_count()));
}

void menu::open_keybind_popup(keybind_binding &binding) noexcept {
    close_popups();
    if (binding.popup == nullptr) {
        start_listening(binding);
        return;
    }
    position_popup(binding.popup, binding.button);
    binding.popup->SetClass("open", true);
    open_keybind_ = &binding;
}

void menu::materialize_position() noexcept {
    if (menu_moved_ || menu_root_ == nullptr || document_ == nullptr) {
        return;
    }
    const float view_w{static_cast<float>(viewport_width_)};
    const float view_h{static_cast<float>(viewport_height_)};
    if (view_w < 64.0F || view_h < 64.0F) {
        return;
    }
    // Place the window at absolute document coordinates computed from the
    // viewport. The stylesheet must not use percentage centering: RmlUi
    // resolves the percentage against the element's own size whenever that
    // size changes and keeps applying the stylesheet margin to the children's
    // containing block, displacing every child by half the resize delta.
    const float width{menu_root_->GetClientWidth()};
    const float height{menu_root_->GetClientHeight()};
    const float x{std::max((view_w - width) * 0.5F, 0.0F)};
    const float y{std::max((view_h - height) * 0.5F, 0.0F)};
    if (std::abs(x - menu_root_->GetAbsoluteLeft()) < 0.5F &&
        std::abs(y - menu_root_->GetAbsoluteTop()) < 0.5F) {
        menu_moved_ = true; // Already in place (a previous frame placed it).
        return;
    }
    const float scale{dp_scale()};
    menu_root_->SetProperty(
        "left", std::to_string(static_cast<int>(std::lround(x * scale))) + "dp");
    menu_root_->SetProperty(
        "top", std::to_string(static_cast<int>(std::lround(y * scale))) + "dp");
    menu_root_->SetProperty("margin-left", "0dp");
    menu_root_->SetProperty("margin-top", "0dp");
    menu_moved_ = true;
    ZEPHYRUS_TRACE("menu: placed at ({:.0f},{:.0f}) size {:.0f}x{:.0f} "
                   "view {:.0f}x{:.0f}",
                   static_cast<double>(x), static_cast<double>(y),
                   static_cast<double>(width), static_cast<double>(height),
                   static_cast<double>(view_w), static_cast<double>(view_h));
}

menu::geometry menu::window_geometry() const noexcept {
    geometry result{};
    // Before the first materialize the element still sits at (0,0) and its
    // layout may not have converged; only a user-set (dragged/resized/loaded)
    // geometry is persisted.
    if (menu_root_ == nullptr || !geometry_user_set_) {
        return result;
    }
    const float scale{dp_scale()};
    result.x = menu_root_->GetAbsoluteLeft() * scale;
    result.y = menu_root_->GetAbsoluteTop() * scale;
    result.width = menu_root_->GetClientWidth() * scale;
    result.height = menu_root_->GetClientHeight() * scale;
    result.valid = result.width >= 600.0F && result.height >= 400.0F;
    return result;
}

void menu::apply_geometry(const geometry &value) noexcept {
    if (menu_root_ == nullptr || !value.valid || value.width < 600.0F ||
        value.height < 400.0F) {
        return;
    }
    const float width{std::clamp(value.width, 600.0F, 4096.0F)};
    const float height{std::clamp(value.height, 400.0F, 2160.0F)};
    const auto dp{[](float number) {
        return std::to_string(static_cast<int>(std::lround(number))) + "dp";
    }};
    menu_root_->SetProperty("left", dp(value.x));
    menu_root_->SetProperty("top", dp(value.y));
    menu_root_->SetProperty("width", dp(width));
    menu_root_->SetProperty("height", dp(height));
    menu_root_->SetProperty("margin-left", "0dp");
    menu_root_->SetProperty("margin-top", "0dp");
    menu_moved_ = true;
    geometry_user_set_ = true;
    drag_applied_x_dp_ = static_cast<int>(std::lround(value.x));
    drag_applied_y_dp_ = static_cast<int>(std::lround(value.y));
    resize_applied_w_dp_ = static_cast<int>(std::lround(width));
    resize_applied_h_dp_ = static_cast<int>(std::lround(height));
    ZEPHYRUS_TRACE("menu: geometry applied {:.0f},{:.0f} {:.0f}x{:.0f}",
                   static_cast<double>(value.x), static_cast<double>(value.y),
                   static_cast<double>(width), static_cast<double>(height));
}

void menu::reset_window_layout() noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    menu_root_->SetProperty("width", "900dp");
    menu_root_->SetProperty("height", "620dp");
    menu_root_->RemoveProperty("left");
    menu_root_->RemoveProperty("top");
    menu_root_->RemoveProperty("margin-left");
    menu_root_->RemoveProperty("margin-top");
    menu_moved_ = false;
    geometry_user_set_ = false;
    drag_applied_x_dp_ = 0;
    drag_applied_y_dp_ = 0;
    resize_applied_w_dp_ = 0;
    resize_applied_h_dp_ = 0;
    ZEPHYRUS_INFO("menu: layout reset to defaults");
}

void menu::start_drag() noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    materialize_position();
    geometry_user_set_ = true;
    dragging_ = true;
    drag_offset_x_ =
        static_cast<float>(cursor_x_) - menu_root_->GetAbsoluteLeft();
    drag_offset_y_ =
        static_cast<float>(cursor_y_) - menu_root_->GetAbsoluteTop();
}

void menu::start_resize() noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    ZEPHYRUS_TRACE("menu: resize start ({:.0f}x{:.0f}) cursor=({},{})",
                   static_cast<double>(menu_root_->GetClientWidth()),
                   static_cast<double>(menu_root_->GetClientHeight()),
                   cursor_x_, cursor_y_);
    materialize_position();
    geometry_user_set_ = true;
    resizing_ = true;
    resize_start_w_ = menu_root_->GetClientWidth();
    resize_start_h_ = menu_root_->GetClientHeight();
    resize_start_x_ = static_cast<float>(cursor_x_);
    resize_start_y_ = static_cast<float>(cursor_y_);
}

void menu::update_resize() noexcept {
    if (!resizing_ || menu_root_ == nullptr || document_ == nullptr) {
        return;
    }
    const float scale{dp_scale()};
    const float view_w{static_cast<float>(viewport_width_)};
    const float view_h{static_cast<float>(viewport_height_)};
    float width{resize_start_w_ +
                (static_cast<float>(cursor_x_) - resize_start_x_)};
    float height{resize_start_h_ +
                 (static_cast<float>(cursor_y_) - resize_start_y_)};
    const float min_w{720.0F / scale};
    const float min_h{480.0F / scale};
    // Keep the clamp bounds ordered even when the viewport is tiny.
    const float max_w{std::max(view_w - 24.0F, min_w)};
    const float max_h{std::max(view_h - 24.0F, min_h)};
    width = std::clamp(width, min_w, max_w);
    height = std::clamp(height, min_h, max_h);
    static std::size_t dbg{};
    if ((++dbg % 20) == 0) {
        ZEPHYRUS_TRACE("menu: resize {:.0f}x{:.0f} view={:.0f}x{:.0f}",
                       static_cast<double>(width),
                       static_cast<double>(height),
                       static_cast<double>(view_w),
                       static_cast<double>(view_h));
    }
    const int width_dp{static_cast<int>(std::lround(width * scale))};
    const int height_dp{static_cast<int>(std::lround(height * scale))};
    if (width_dp != resize_applied_w_dp_) {
        resize_applied_w_dp_ = width_dp;
        menu_root_->SetProperty("width", std::to_string(width_dp) + "dp");
    }
    if (height_dp != resize_applied_h_dp_) {
        resize_applied_h_dp_ = height_dp;
        menu_root_->SetProperty("height", std::to_string(height_dp) + "dp");
    }
    refresh_content_width();
}

void menu::refresh_content_width() noexcept {
    if (content_area_ == nullptr || scroll_content_ == nullptr) {
        return;
    }
    // GetClientWidth covers padding, the scroll container needs the content
    // box width. Only touch the property when it actually changed, redundant
    // writes re-layout the whole tree every frame.
    const float width{
        content_area_->GetBox().GetSize(Rml::BoxArea::Content).x};
    if (width <= 1.0F) {
        return;
    }
    const int value{static_cast<int>(std::lround(width * dp_scale()))};
    if (value == content_width_dp_) {
        return;
    }
    content_width_dp_ = value;
    scroll_content_->SetProperty("width", std::to_string(value) + "dp");
}

void menu::update_drag() noexcept {
    if (!dragging_ || menu_root_ == nullptr || document_ == nullptr) {
        return;
    }
    float x{static_cast<float>(cursor_x_) - drag_offset_x_};
    float y{static_cast<float>(cursor_y_) - drag_offset_y_};
    const float view_w{static_cast<float>(viewport_width_)};
    const float view_h{static_cast<float>(viewport_height_)};
    const float width{menu_root_->GetClientWidth()};
    const float margin{56.0F};
    const float min_x{-(width - margin)};
    const float max_x{std::max(view_w - margin, min_x)};
    const float max_y{std::max(view_h - margin, 0.0F)};
    x = std::clamp(x, min_x, max_x);
    y = std::clamp(y, 0.0F, max_y);
    // Re-anchor after clamping: once the window is pinned at a screen edge,
    // moving the cursor back must move the window again immediately instead
    // of first retracing the overshoot.
    drag_offset_x_ = static_cast<float>(cursor_x_) - x;
    drag_offset_y_ = static_cast<float>(cursor_y_) - y;
    const float scale{dp_scale()};
    const int x_dp{static_cast<int>(std::lround(x * scale))};
    const int y_dp{static_cast<int>(std::lround(y * scale))};
    // Redundant inline writes re-dirty the style sheet on every mouse move.
    if (x_dp != drag_applied_x_dp_) {
        drag_applied_x_dp_ = x_dp;
        menu_root_->SetProperty("left", std::to_string(x_dp) + "dp");
    }
    if (y_dp != drag_applied_y_dp_) {
        drag_applied_y_dp_ = y_dp;
        menu_root_->SetProperty("top", std::to_string(y_dp) + "dp");
    }
}

float menu::dp_scale() const noexcept {
    if (menu_root_ == nullptr) {
        return 1.0F;
    }
    const float ratio{
        Rml::ElementUtilities::GetDensityIndependentPixelRatio(menu_root_)};
    return ratio > 0.01F ? 1.0F / ratio : 1.0F;
}

void menu::scroll_by(float delta) noexcept {
    if (scroll_content_ == nullptr || content_area_ == nullptr) {
        return;
    }
    const float viewport{content_area_->GetClientHeight()};
    const float content{scroll_content_->GetClientHeight()};
    scroll_min_ = std::min(0.0F, viewport - content - 8.0F);
    scroll_offset_ =
        std::clamp(scroll_offset_ - delta * 72.0F, scroll_min_, 0.0F);
    scroll_content_->SetProperty(
        "top",
        std::to_string(static_cast<int>(
            std::lround(scroll_offset_ * dp_scale()))) +
            "dp");
}

void menu::reset_scroll() noexcept {
    scroll_offset_ = 0.0F;
    if (scroll_content_ != nullptr) {
        scroll_content_->SetProperty("top", "0dp");
    }
}

void menu::position_popup(Rml::Element *popup, Rml::Element *anchor) noexcept {
    if (popup == nullptr || anchor == nullptr || menu_root_ == nullptr) {
        return;
    }
    // Popups inside the scrolling content would be clipped by it; make them
    // children of the menu root and place them under their anchor.
    if (popup->GetParentNode() != menu_root_) {
        if (Rml::Element *old_parent{popup->GetParentNode()};
            old_parent != nullptr) {
            Rml::ElementPtr moved{old_parent->RemoveChild(popup)};
            if (moved != nullptr) {
                menu_root_->AppendChild(std::move(moved));
            }
        }
    }
    const float scale{dp_scale()};
    const int left{static_cast<int>(std::lround(
        (anchor->GetAbsoluteLeft() - menu_root_->GetAbsoluteLeft()) * scale))};
    const int top{static_cast<int>(std::lround(
        (anchor->GetAbsoluteTop() - menu_root_->GetAbsoluteTop() +
         anchor->GetClientHeight()) *
        scale))};
    popup->SetProperty("left", std::to_string(left) + "dp");
    popup->SetProperty("top", std::to_string(top) + "dp");
    popup->SetProperty(
        "width",
        std::to_string(static_cast<int>(
            std::lround(anchor->GetClientWidth() * scale))) +
            "dp");
}

void menu::ProcessEvent(Rml::Event &event) noexcept {
    const Rml::String &type{event.GetType()};
    Rml::Element *target{event.GetTargetElement()};

    if (type == "mousedown") {
        const int button{event.GetParameter<int>("button", 0)};

        // Resize from the bottom-right grip.
        if (button == 0 && resize_handle_ != nullptr) {
            for (Rml::Element *element = target; element != nullptr;
                 element = element->GetParentNode()) {
                if (element == resize_handle_) {
                    start_resize();
                    return;
                }
            }
        }

        // Keybind buttons: right click opens the mode popup. Left click is
        // handled in `click` so the popup is not closed in the same gesture.
        for (Rml::Element *element = target; element != nullptr;
             element = element->GetParentNode()) {
            const auto it{keybind_lookup_.find(element)};
            if (it != keybind_lookup_.end()) {
                if (button == 1) {
                    open_keybind_popup(*it->second);
                }
                return;
            }
        }

        // Sliders answer to press and drag.
        for (Rml::Element *element = target; element != nullptr;
             element = element->GetParentNode()) {
            const auto it{slider_lookup_.find(element)};
            if (it != slider_lookup_.end()) {
                active_slider_ = it->second;
                active_slider_->slider->SetClass("dragging", true);
                update_slider_from_mouse(
                    *active_slider_,
                    event.GetParameter<int>("mouse_x", 0));
                return;
            }
        }

        // Drag the window from its header (the tab strip keeps its clicks).
        if (button == 0 && header_ != nullptr) {
            bool on_header{false};
            bool on_tabs{false};
            for (Rml::Element *element = target; element != nullptr;
                 element = element->GetParentNode()) {
                on_header = on_header || element == header_;
                on_tabs = on_tabs || element == tabs_;
            }
            if (on_header && !on_tabs) {
                start_drag();
                return;
            }
        }
        return;
    }

    if (type == "mousemove") {
        if (resizing_) {
            update_resize();
            return;
        }
        if (dragging_) {
            update_drag();
            return;
        }
        if (active_slider_ != nullptr) {
            update_slider_from_mouse(
                *active_slider_, event.GetParameter<int>("mouse_x", 0));
        }
        return;
    }

    if (type == "mouseup") {
        if (resizing_ && menu_root_ != nullptr) {
            ZEPHYRUS_TRACE(
                "menu: resize end menu=({:.0f},{:.0f}) {:.0f}x{:.0f} header_h={:.0f} sidebar_w={:.0f} content_h={:.0f}",
                static_cast<double>(menu_root_->GetAbsoluteLeft()),
                static_cast<double>(menu_root_->GetAbsoluteTop()),
                static_cast<double>(menu_root_->GetClientWidth()),
                static_cast<double>(menu_root_->GetClientHeight()),
                static_cast<double>(header_ != nullptr ? header_->GetClientHeight() : -1.0F),
                static_cast<double>(document_ != nullptr
                                        ? (document_->GetElementById("sidebar")
                                               ? document_->GetElementById("sidebar")->GetClientWidth()
                                               : -1.0F)
                                        : -1.0F),
                static_cast<double>(content_area_ != nullptr ? content_area_->GetClientHeight() : -1.0F));
            Rml::Element *sidebar_el{
                document_ != nullptr ? document_->GetElementById("sidebar") : nullptr};
            ZEPHYRUS_TRACE(
                "menu: geom header=({:.0f},{:.0f}) sidebar=({:.0f},{:.0f}) content=({:.0f},{:.0f})",
                static_cast<double>(header_ != nullptr ? header_->GetAbsoluteLeft() : -1.0F),
                static_cast<double>(header_ != nullptr ? header_->GetAbsoluteTop() : -1.0F),
                static_cast<double>(sidebar_el != nullptr ? sidebar_el->GetAbsoluteLeft() : -1.0F),
                static_cast<double>(sidebar_el != nullptr ? sidebar_el->GetAbsoluteTop() : -1.0F),
                static_cast<double>(content_area_ != nullptr ? content_area_->GetAbsoluteLeft() : -1.0F),
                static_cast<double>(content_area_ != nullptr ? content_area_->GetAbsoluteTop() : -1.0F));
        }
        resizing_ = false;
        dragging_ = false;
        if (active_slider_ != nullptr) {
            active_slider_->slider->SetClass("dragging", false);
        }
        active_slider_ = nullptr;
        return;
    }

    if (type == "mousescroll") {
        scroll_by(event.GetParameter<float>("wheel_delta_y", 0.0F));
        return;
    }

    if (type != "click") {
        return;
    }

    // Left click on a keybind starts listening for a new key.
    for (Rml::Element *element = target; element != nullptr;
         element = element->GetParentNode()) {
        const auto it{keybind_lookup_.find(element)};
        if (it != keybind_lookup_.end()) {
            start_listening(*it->second);
            return;
        }
    }

    // Clicks outside a combo close any open dropdown, combo clicks toggle.
    bool inside_combo{false};
    for (Rml::Element *element = target; element != nullptr;
         element = element->GetParentNode()) {
        if (combo_lookup_.contains(element)) {
            inside_combo = true;
            break;
        }
    }
    if (!inside_combo) {
        close_popups();
    }

    std::function<void()> action;
    if (find_ancestor(target, click_actions_, &action) != nullptr && action) {
        action();
    }
}
} // namespace zephyrus::overlay
