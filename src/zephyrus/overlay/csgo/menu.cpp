// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/overlay/csgo/menu.hpp>
#include <zephyrus/sdl2/sdl2.hpp>
#include <zephyrus/version.hpp>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <ranges>
#include <system_error>

#include <unistd.h>

namespace zephyrus::csgo::overlay {
namespace {
/// Mirrors `#caret`'s CSS animation (restored by `set_caret_animation`).
constexpr const char *blink_animation{"blink 0.55s alternate infinite"};

std::string format_number(float value, int decimals,
                          const std::string &suffix) {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals,
                  static_cast<double>(value));
    return std::string{buffer} + suffix;
}

std::string px(float value) {
    return std::to_string(static_cast<int>(std::lround(value))) + "px";
}

/// Escape remote/`user supplied text before it goes through SetInnerRML.
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

[[nodiscard]] bool valid_profile_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > 64 || name == "latest" ||
        name.starts_with('.')) {
        return false;
    }
    return std::ranges::all_of(name, [](const char character) {
        const auto value{static_cast<unsigned char>(character)};
        return std::isalnum(value) != 0 || character == '_' ||
               character == '-' || character == ' ';
    });
}

/// Human-readable name for the SDL scancodes used by the key bindings.
std::string key_name(const int scancode) {
    if (scancode == 0) {
        return "Unbound";
    }
    constexpr std::pair<int, const char *> names[]{
        {4, "A"},       {5, "B"},        {6, "C"},          {7, "D"},
        {8, "E"},       {9, "F"},        {10, "G"},         {11, "H"},
        {12, "I"},      {13, "J"},       {14, "K"},         {15, "L"},
        {16, "M"},      {17, "N"},       {18, "O"},         {19, "P"},
        {20, "Q"},      {21, "R"},       {22, "S"},         {23, "T"},
        {24, "U"},      {25, "V"},       {26, "W"},         {27, "X"},
        {28, "Y"},      {29, "Z"},       {30, "1"},         {31, "2"},
        {32, "3"},      {33, "4"},       {34, "5"},         {35, "6"},
        {36, "7"},      {37, "8"},       {38, "9"},         {39, "0"},
        {40, "Return"}, {41, "Escape"},  {42, "Backspace"}, {43, "Tab"},
        {44, "Space"},  {45, "-"},       {46, "="},         {47, "["},
        {48, "]"},      {49, "\\"},      {51, ";"},         {52, "'"},
        {53, "`"},      {54, ","},       {55, "."},         {56, "/"},
        {57, "CapsLock"}, {58, "F1"},    {59, "F2"},        {60, "F3"},
        {61, "F4"},     {62, "F5"},      {63, "F6"},        {64, "F7"},
        {65, "F8"},     {66, "F9"},      {67, "F10"},       {68, "F11"},
        {69, "F12"},    {73, "Insert"},  {74, "Home"},      {75, "PageUp"},
        {76, "Delete"}, {77, "End"},     {78, "PageDown"},  {79, "Right"},
        {80, "Left"},   {81, "Down"},    {82, "Up"},        {224, "LCtrl"},
        {225, "LShift"}, {226, "LAlt"},  {228, "RCtrl"},    {229, "RShift"},
        {230, "RAlt"},
    };
    for (const auto &[code, name] : names) {
        if (code == scancode) {
            return name;
        }
    }
    return "Key " + std::to_string(scancode);
}
} // namespace

menu::~menu() noexcept { shutdown(); }

void menu::initialize(Rml::ElementDocument &document) noexcept {
    document_ = &document;
    sidebar_ = document.GetElementById("sidebar");
    glider_ = document.GetElementById("glider");
    menu_root_ = document.GetElementById("menu");
    header_ = document.GetElementById("menu-header");
    tabs_ = document.GetElementById("tabs");
    content_ = document.GetElementById("content");
    content_scroll_ = document.GetElementById("content-scroll");
    resize_handle_ = document.GetElementById("resize-handle");
    config_list_ = document.GetElementById("config-list");
    config_input_ = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(
        document.GetElementById("in-config-name"));

    config_.set_path(config::document::default_path("csgo"));
    (void)config_.load();
    geometry saved{};
    if (const toml::table *menu_table{config::table_at(config_.root(), "menu")};
        menu_table != nullptr) {
        saved.x = static_cast<float>(config::read_float(*menu_table, "x", 160.0));
        saved.y = static_cast<float>(config::read_float(*menu_table, "y", 160.0));
        saved.width =
            static_cast<float>(config::read_float(*menu_table, "w", 900.0));
        saved.height =
            static_cast<float>(config::read_float(*menu_table, "h", 620.0));
    }
    apply_geometry(saved);

    // Top tabs.
    constexpr std::pair<std::string_view, std::string_view> tabs[]{
        {"tab-rage", "rage"},
        {"tab-visuals", "visuals"},
        {"tab-misc", "misc"},
        {"tab-config", "config"},
    };
    for (const auto &[id, group] : tabs) {
        if (Rml::Element *element{document.GetElementById(Rml::String{id})};
            element != nullptr) {
            click_actions_[element] = [this, group = std::string{group}] {
                select_tab(group);
            };
        }
    }

    // Sidebar entries: id `nav-<panel>` belongs to the group after `nav-`.
    constexpr std::pair<std::string_view, std::string_view> navs[]{
        {"nav-rage-main", "rage-main"},
        {"nav-rage-backtrack", "rage-backtrack"},
        {"nav-rage-antiaim", "rage-antiaim"},
        {"nav-rage-fakelag", "rage-fakelag"},
        {"nav-rage-resolver", "rage-resolver"},
        {"nav-rage-weapons", "rage-weapons"},
        {"nav-visuals-main", "visuals-main"},
        {"nav-visuals-esp", "visuals-esp"},
        {"nav-visuals-glow", "visuals-glow"},
        {"nav-misc-main", "misc-main"},
        {"nav-misc-movement", "misc-movement"},
        {"nav-config-main", "config-main"},
    };
    for (const auto &[id, panel] : navs) {
        Rml::Element *element{document.GetElementById(Rml::String{id})};
        if (element == nullptr) {
            continue;
        }
        nav_items_.emplace_back(element, std::string{panel});
        click_actions_[element] = [this, panel = std::string{panel}] {
            select_panel(panel);
        };
    }

    // Sliders: `sl-<name>` owns `fill-<name>` / `knob-<name>` / `val-<name>`.
    Rml::ElementList slider_elements;
    document_->QuerySelectorAll(slider_elements, ".slider");
    for (Rml::Element *element : slider_elements) {
        const Rml::String id{element->GetId()};
        if (!id.starts_with("sl-")) {
            continue;
        }
        const std::string name{id.c_str() + 3};
        auto binding{std::make_unique<slider_binding>()};
        binding->slider = element;
        binding->fill = document_->GetElementById(Rml::String{"fill-" + name});
        binding->knob = document_->GetElementById(Rml::String{"knob-" + name});
        binding->value_label =
            document_->GetElementById(Rml::String{"val-" + name});
        binding->min = element->GetAttribute<float>("data-min", 0.0F);
        binding->max = element->GetAttribute<float>("data-max", 1.0F);
        binding->decimals = element->GetAttribute<int>("data-decimals", 0);
        binding->suffix =
            element->GetAttribute<Rml::String>("data-suffix", "").c_str();
        if (binding->fill == nullptr || binding->knob == nullptr) {
            ZEPHYRUS_WARN("menu: incomplete slider {}", id);
            continue;
        }
        slider_lookup_[element] = binding.get();
        sliders_.push_back(std::move(binding));
    }

    // Checkboxes toggle their `checked` class locally.
    Rml::ElementList checkboxes;
    document_->QuerySelectorAll(checkboxes, ".checkbox");
    for (Rml::Element *element : checkboxes) {
        click_actions_[element] = [element] {
            element->SetClass("checked", !element->IsClassSet("checked"));
        };
    }

    // Combos: the head opens the popup, items select themselves.
    Rml::ElementList combos;
    document_->QuerySelectorAll(combos, ".combo");
    for (Rml::Element *element : combos) {
        click_actions_[element] = [this, element] { open_combo(element); };
    }
    Rml::ElementList items;
    document_->QuerySelectorAll(items, ".combo-item");
    for (Rml::Element *item : items) {
        Rml::Element *combo{item};
        while (combo != nullptr && !combo->IsClassSet("combo")) {
            combo = combo->GetParentNode();
        }
        if (combo == nullptr) {
            continue;
        }
        combo_lookup_[item] = combo;
        click_actions_[item] = [this, item, combo] {
            close_popups();
            if (Rml::Element *text{combo->QuerySelector(".text")};
                text != nullptr) {
                set_text(text, item->GetInnerRML().c_str());
            }
            int index{};
            bool found{};
            if (Rml::Element *popup{combo->QuerySelector(".combo-popup")};
                popup != nullptr) {
                Rml::ElementList siblings;
                popup->QuerySelectorAll(siblings, ".combo-item");
                for (Rml::Element *sibling : siblings) {
                    sibling->SetClass("selected", sibling == item);
                    if (sibling == item) {
                        found = true;
                    } else if (!found) {
                        ++index;
                    }
                }
            }
            if (found) {
                const auto handler{combo_handlers_.find(combo)};
                if (handler != combo_handlers_.end()) {
                    const Rml::String &id{combo->GetId()};
                    if (!id.empty()) {
                        store_int(id.c_str(), index);
                    }
                    if (handler->second) {
                        handler->second(index);
                    }
                }
            }
        };
    }

    // Config tab actions.
    bind_button("btn-config-save", [this] {
        config_save(config_name_value());
    });
    bind_button("btn-config-load", [this] {
        config_load(config_name_value());
    });
    bind_button("btn-config-delete", [this] {
        config_delete(config_name_value());
    });
    bind_button("btn-config-reset", [this] { config_reset(); });
    bind_button("btn-config-folder", [this] { open_config_folder(); });
    bind_button("btn-config-export", [this] { config_export(); });
    bind_button("btn-config-import", [this] { config_import(); });

    // Static shell data.
    if (Rml::Element *version{document_->GetElementById("val-build-version")};
        version != nullptr) {
        set_text(version, version::csgo_version());
    }
    refresh_config_list();

    document_->AddEventListener("click", this);
    document_->AddEventListener("mousedown", this);
    document_->AddEventListener("mousemove", this);
    document_->AddEventListener("mouseup", this);
    document_->AddEventListener("wheel", this);

    fps_window_ = std::chrono::steady_clock::now();
    select_tab(active_tab_);
    ZEPHYRUS_INFO("csgo menu controller ready");
}

void menu::shutdown() noexcept {
    if (document_ != nullptr) {
        document_->RemoveEventListener("click", this);
        document_->RemoveEventListener("mousedown", this);
        document_->RemoveEventListener("mousemove", this);
        document_->RemoveEventListener("mouseup", this);
        document_->RemoveEventListener("wheel", this);
    }
    save_geometry();
    (void)config_.save();
    document_ = nullptr;
    click_actions_.clear();
    nav_items_.clear();
    slider_lookup_.clear();
    sliders_.clear();
    combo_lookup_.clear();
    combo_handlers_.clear();
    keybinds_.clear();
    keybind_lookup_.clear();
    config_rows_.clear();
    menu_root_ = nullptr;
    header_ = nullptr;
    tabs_ = nullptr;
    sidebar_ = nullptr;
    glider_ = nullptr;
    content_ = nullptr;
    content_scroll_ = nullptr;
    resize_handle_ = nullptr;
    config_list_ = nullptr;
    config_input_ = nullptr;
    active_slider_ = nullptr;
    open_combo_ = nullptr;
    open_keybind_ = nullptr;
    capture_element_ = nullptr;
    capture_binding_ = nullptr;
    capture_handler_ = nullptr;
    capture_storage_.clear();
    dragging_ = false;
    resizing_ = false;
}

void menu::tick() noexcept {
    config_.tick();
    clamp_scroll();

    ++fps_frames_;
    const auto now{std::chrono::steady_clock::now()};
    const auto elapsed{now - fps_window_};
    if (elapsed >= std::chrono::milliseconds{500}) {
        if (fps_enabled_) {
            const double seconds{
                std::chrono::duration<double>(elapsed).count()};
            const int fps{seconds > 0.0
                              ? static_cast<int>(
                                    std::lround(static_cast<double>(fps_frames_) /
                                                seconds))
                              : 0};
            if (Rml::Element *label{document_ != nullptr
                                        ? document_->GetElementById("fps-label")
                                        : nullptr};
                label != nullptr) {
                set_text(label, std::to_string(fps) + " FPS");
            }
        }
        fps_frames_ = 0;
        fps_window_ = now;
    }
}

void menu::set_caret_animation(const bool enabled) noexcept {
    if (document_ == nullptr) {
        return;
    }
    if (Rml::Element *caret{document_->GetElementById("caret")};
        caret != nullptr) {
        caret->SetProperty("animation", enabled ? blink_animation
                                                : static_cast<const char *>("none"));
    }
}

void menu::set_element_visible(const std::string_view id,
                                const bool visible) noexcept {
    if (document_ == nullptr) {
        return;
    }
    if (Rml::Element *element{document_->GetElementById(Rml::String{id})};
        element != nullptr) {
        if (visible) {
            element->RemoveProperty("display");
        } else {
            element->SetProperty("display", "none");
        }
    }
}

void menu::set_style(const std::string_view selector,
                     const std::string_view property,
                     const std::string_view value) noexcept {
    if (document_ == nullptr) {
        return;
    }
    Rml::ElementList elements;
    document_->QuerySelectorAll(elements, Rml::String{selector});
    for (Rml::Element *element : elements) {
        if (value.empty()) {
            element->RemoveProperty(Rml::String{property});
        } else {
            element->SetProperty(Rml::String{property}, Rml::String{value});
        }
    }
    ZEPHYRUS_INFO("menu: style '{}' {} = '{}' on {} element(s)", selector,
                  property, value, elements.size());
}

void menu::set_viewport(const int width, const int height) noexcept {
    if (width > 0 && height > 0) {
        viewport_width_ = width;
        viewport_height_ = height;
        clamp_scroll();
    }
}

void menu::ProcessEvent(Rml::Event &event) noexcept {
    const Rml::String &type{event.GetType()};
    Rml::Element *target{event.GetTargetElement()};
    const int mouse_x{event.GetParameter<int>("mouse_x", 0)};
    const int mouse_y{event.GetParameter<int>("mouse_y", 0)};
    const int button{event.GetParameter<int>("button", 0)};

    if (type == "wheel") {
        scroll_by(event.GetParameter<float>("wheel_delta_y", 0.0F));
        return;
    }

    if (type == "mousedown") {
        // A press somewhere else cancels an armed key capture.
        if (capturing_key()) {
            bool inside{false};
            for (Rml::Element *element = target; element != nullptr;
                 element = element->GetParentNode()) {
                if (element == capture_element_) {
                    inside = true;
                    break;
                }
            }
            if (!inside) {
                (void)capture_key_event(41);
            }
        }

        // Resize from the bottom-right grip.
        if (button == 0 && resize_handle_ != nullptr) {
            for (Rml::Element *element = target; element != nullptr;
                 element = element->GetParentNode()) {
                if (element == resize_handle_) {
                    start_resize(mouse_x, mouse_y);
                    return;
                }
            }
        }

        // Key binds: right click opens the mode popup, left click captures.
        for (Rml::Element *element = target; element != nullptr;
             element = element->GetParentNode()) {
            const auto it{keybind_lookup_.find(element)};
            if (it != keybind_lookup_.end()) {
                if (button == 1) {
                    open_keybind_popup(*it->second);
                } else if (button == 0) {
                    start_listening(*it->second);
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
                update_slider_from_mouse(*active_slider_, mouse_x);
                return;
            }
        }

        // Drag the window from its header (the tab strip keeps its clicks).
        if (button == 0) {
            for (Rml::Element *element = target; element != nullptr;
                 element = element->GetParentNode()) {
                if (element == tabs_) {
                    return;
                }
                if (element == header_) {
                    start_drag(mouse_x, mouse_y);
                    return;
                }
            }
        }
        return;
    }

    if (type == "mousemove") {
        if (dragging_) {
            update_drag(mouse_x, mouse_y);
            return;
        }
        if (resizing_) {
            update_resize(mouse_x, mouse_y);
            return;
        }
        if (active_slider_ != nullptr) {
            update_slider_from_mouse(*active_slider_, mouse_x);
        }
        return;
    }

    if (type == "mouseup") {
        if (active_slider_ != nullptr) {
            active_slider_->slider->SetClass("dragging", false);
            active_slider_ = nullptr;
        }
        if (dragging_ || resizing_) {
            dragging_ = false;
            resizing_ = false;
            save_geometry();
        }
        return;
    }

    if (type != "click") {
        return;
    }

    for (Rml::Element *element = target; element != nullptr;
         element = element->GetParentNode()) {
        const auto it{click_actions_.find(element)};
        if (it != click_actions_.end()) {
            it->second();
            return;
        }
    }
    close_popups();
}

void menu::select_tab(std::string_view tab) noexcept {
    active_tab_ = std::string{tab};

    for (const std::string_view name : {"rage", "visuals", "misc", "config"}) {
        if (Rml::Element *element{
                document_->GetElementById("tab-" + Rml::String{name})};
            element != nullptr) {
            element->SetClass("active", name == active_tab_);
        }
    }

    for (const auto &[element, panel] : nav_items_) {
        // `panel` starts with its group name ("rage-main" -> group "rage").
        const std::string_view group{
            std::string_view{panel}.substr(0, panel.find('-'))};
        element->SetClass("hidden", group != active_tab_);
    }

    const std::string_view default_panel{tab == "rage"      ? "rage-main"
                                         : tab == "visuals" ? "visuals-main"
                                         : tab == "misc"    ? "misc-main"
                                                            : "config-main"};
    select_panel(default_panel);
}

void menu::select_panel(std::string_view panel) noexcept {
    active_panel_ = std::string{panel};

    for (const auto &[element, name] : nav_items_) {
        element->SetClass("active", name == active_panel_);
    }
    for (const auto &[element, name] : nav_items_) {
        if (Rml::Element *section{
                document_->GetElementById("panel-" + Rml::String{name})};
            section != nullptr) {
            section->SetClass("active", name == active_panel_);
        }
    }

    refresh_glider();
    reset_scroll();
}

void menu::start_drag(const int mouse_x, const int mouse_y) noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    dragging_ = true;
    drag_offset_x_ =
        static_cast<float>(mouse_x) - menu_root_->GetAbsoluteLeft();
    drag_offset_y_ =
        static_cast<float>(mouse_y) - menu_root_->GetAbsoluteTop();
}

void menu::update_drag(const int mouse_x, const int mouse_y) noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    // Keep (most of) the window on screen: at least `margin` pixels stay
    // visible on every side.
    constexpr float margin{56.0F};
    const float width{window_width_};
    const float min_x{-(width - margin)};
    const float max_x{std::max(static_cast<float>(viewport_width_) - margin,
                               min_x)};
    const float min_y{0.0F};
    const float max_y{std::max(static_cast<float>(viewport_height_) - 28.0F,
                               min_y)};
    const float x{std::clamp(static_cast<float>(mouse_x) - drag_offset_x_,
                             min_x, max_x)};
    const float y{std::clamp(static_cast<float>(mouse_y) - drag_offset_y_,
                             min_y, max_y)};
    // Re-anchor so the window follows the cursor again immediately after a
    // clamp.
    drag_offset_x_ = static_cast<float>(mouse_x) - x;
    drag_offset_y_ = static_cast<float>(mouse_y) - y;
    position_x_ = x;
    position_y_ = y;
    menu_root_->SetProperty("left", px(position_x_));
    menu_root_->SetProperty("top", px(position_y_));
}

void menu::start_resize(const int mouse_x, const int mouse_y) noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    resizing_ = true;
    resize_start_x_ = static_cast<float>(mouse_x);
    resize_start_y_ = static_cast<float>(mouse_y);
    resize_start_w_ = window_width_;
    resize_start_h_ = window_height_;
}

void menu::update_resize(const int mouse_x, const int mouse_y) noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    constexpr float min_w{720.0F};
    constexpr float min_h{480.0F};
    const float max_w{std::max(static_cast<float>(viewport_width_) - 16.0F,
                               min_w)};
    const float max_h{std::max(static_cast<float>(viewport_height_) - 16.0F,
                               min_h)};
    window_width_ = std::clamp(
        resize_start_w_ + (static_cast<float>(mouse_x) - resize_start_x_),
        min_w, max_w);
    window_height_ = std::clamp(
        resize_start_h_ + (static_cast<float>(mouse_y) - resize_start_y_),
        min_h, max_h);
    menu_root_->SetProperty("width", px(window_width_));
    menu_root_->SetProperty("height", px(window_height_));
    clamp_scroll();
}

void menu::save_geometry() noexcept {
    toml::table &menu_table{config::ensure_table(config_.root(), "menu")};
    menu_table.insert_or_assign("x", static_cast<double>(position_x_));
    menu_table.insert_or_assign("y", static_cast<double>(position_y_));
    menu_table.insert_or_assign("w", static_cast<double>(window_width_));
    menu_table.insert_or_assign("h", static_cast<double>(window_height_));
    config_.mark_dirty();
}

void menu::apply_geometry(const geometry &value) noexcept {
    if (menu_root_ == nullptr) {
        return;
    }
    position_x_ = value.x;
    position_y_ = value.y;
    window_width_ = value.width;
    window_height_ = value.height;
    menu_root_->SetProperty("left", px(position_x_));
    menu_root_->SetProperty("top", px(position_y_));
    menu_root_->SetProperty("width", px(window_width_));
    menu_root_->SetProperty("height", px(window_height_));
    clamp_scroll();
}

menu::geometry menu::window_geometry() const noexcept {
    return {position_x_, position_y_, window_width_, window_height_};
}

void menu::refresh_glider() noexcept {
    if (document_ == nullptr || sidebar_ == nullptr || glider_ == nullptr) {
        return;
    }
    Rml::Element *active{
        document_->GetElementById("nav-" + Rml::String{active_panel_})};
    if (active == nullptr || active->IsClassSet("hidden")) {
        glider_->SetProperty("display", "none");
        return;
    }
    glider_->SetProperty("display", "block");
    const float top{active->GetAbsoluteTop() - sidebar_->GetAbsoluteTop()};
    const float height{active->GetClientHeight()};
    if (height <= 0.0F) {
        return;
    }
    glider_->SetProperty("top", px(top));
    glider_->SetProperty("height", px(height));
}

void menu::set_slider_value(slider_binding &binding, float value) noexcept {
    value = std::clamp(value, binding.min, binding.max);
    const float t{binding.max > binding.min
                      ? (value - binding.min) / (binding.max - binding.min)
                      : 0.0F};
    const std::string percent{
        std::to_string(static_cast<int>(std::round(t * 100.0F))) + "%"};
    binding.fill->SetProperty("width", percent);
    binding.knob->SetProperty("left", percent);
    if (binding.value_label != nullptr) {
        set_text(binding.value_label,
                 format_number(value, binding.decimals, binding.suffix));
    }
}

void menu::update_slider_from_mouse(slider_binding &binding,
                                    const int mouse_x) noexcept {
    const float left{binding.slider->GetAbsoluteLeft()};
    const float width{binding.slider->GetClientWidth()};
    if (width <= 0.0F) {
        return;
    }
    const float t{
        std::clamp((static_cast<float>(mouse_x) - left) / width, 0.0F, 1.0F)};
    const float value{std::clamp(binding.min + t * (binding.max - binding.min),
                                 binding.min, binding.max)};
    set_slider_value(binding, value);
    if (binding.on_change) {
        binding.on_change(value);
    }
}

void menu::bind_toggle(const std::string_view id, const bool default_value,
                       std::function<void(bool)> handler) noexcept {
    bind_toggle(id, default_value, std::move(handler), id);
}

void menu::bind_toggle(const std::string_view id, const bool default_value,
                       std::function<void(bool)> handler,
                       const std::string_view storage_key) noexcept {
    if (document_ == nullptr) {
        return;
    }
    Rml::Element *element{document_->GetElementById(Rml::String{id})};
    if (element == nullptr) {
        ZEPHYRUS_WARN("menu: missing toggle {}", id);
        return;
    }
    const bool value{stored_bool(storage_key, default_value)};
    element->SetClass("checked", value);
    if (handler) {
        handler(value);
    }
    const std::string key{storage_key};
    click_actions_[element] = [this, element, key,
                               handler = std::move(handler)] {
        const bool checked{!element->IsClassSet("checked")};
        element->SetClass("checked", checked);
        store_bool(key, checked);
        if (handler) {
            handler(checked);
        }
    };
}

void menu::bind_slider(const std::string_view id, const float default_value,
                       std::function<void(float)> handler) noexcept {
    bind_slider(id, default_value, std::move(handler), id);
}

void menu::bind_slider(const std::string_view id, const float default_value,
                       std::function<void(float)> handler,
                       const std::string_view storage_key) noexcept {
    if (document_ == nullptr) {
        return;
    }
    Rml::Element *element{document_->GetElementById(Rml::String{id})};
    if (element == nullptr) {
        ZEPHYRUS_WARN("menu: missing slider {}", id);
        return;
    }
    const auto it{slider_lookup_.find(element)};
    if (it == slider_lookup_.end()) {
        return;
    }
    slider_binding *binding{it->second};
    const float value{
        static_cast<float>(stored_float(storage_key, default_value))};
    set_slider_value(*binding, value);
    if (handler) {
        handler(value);
    }
    const std::string key{storage_key};
    binding->on_change = [this, key,
                          handler = std::move(handler)](float current) {
        store_float(key, current);
        if (handler) {
            handler(current);
        }
    };
}

void menu::bind_combo(const std::string_view id, const int default_value,
                      std::function<void(int)> handler) noexcept {
    bind_combo(id, default_value, std::move(handler), id);
}

void menu::bind_combo(const std::string_view id, const int default_value,
                      std::function<void(int)> handler,
                      const std::string_view storage_key) noexcept {
    if (document_ == nullptr) {
        return;
    }
    Rml::Element *combo{document_->GetElementById(Rml::String{id})};
    if (combo == nullptr) {
        ZEPHYRUS_WARN("menu: missing combo {}", id);
        return;
    }
    const int value{static_cast<int>(stored_int(storage_key, default_value))};

    Rml::ElementList items;
    combo->QuerySelectorAll(items, ".combo-item");
    int index{};
    for (Rml::Element *item : items) {
        item->SetClass("selected", index == value);
        if (index == value) {
            if (Rml::Element *text{combo->QuerySelector(".text")};
                text != nullptr) {
                set_text(text, item->GetInnerRML().c_str());
            }
        }
        ++index;
    }

    if (handler) {
        handler(value);
    }
    const std::string key{storage_key};
    combo_handlers_[combo] = [this, key,
                              handler = std::move(handler)](int selected) {
        store_int(key, selected);
        if (handler) {
            handler(selected);
        }
    };
}

void menu::bind_key(const std::string_view id, keybind &binding,
                    const std::string_view storage_key) noexcept {
    if (document_ == nullptr) {
        return;
    }
    Rml::Element *element{document_->GetElementById(Rml::String{id})};
    if (element == nullptr) {
        ZEPHYRUS_WARN("menu: missing key binding {}", id);
        return;
    }
    auto view{std::make_unique<keybind_binding>()};
    view->bind = &binding;
    view->button = element;
    view->text = text_element_of(element);
    view->popup =
        document_->GetElementById(Rml::String{std::string{id} + "-popup"});
    if (view->text == nullptr) {
        ZEPHYRUS_WARN("menu: key binding {} has no text element", id);
        return;
    }

    const std::string key{storage_key.empty() ? std::string{id}
                                              : std::string{storage_key}};
    view->storage_key = key;
    if (const toml::table *features{
            config::table_at(config_.root(), "features")};
        features != nullptr) {
        // Raw scancode format (see `persist_keybind`); the shared
        // config::read_keybind would run the value through the SDL 1.2
        // key-name table and corrupt it.
        if (const toml::table *entry{config::table_at(*features, key)};
            entry != nullptr) {
            binding.key = static_cast<int>(
                config::read_int(*entry, "keycode", binding.key));
            binding.mode = keybind_mode_from_name(config::read_string(
                *entry, "mode", std::string{keybind_mode_name(binding.mode)}));
            binding.reset();
        }
    }

    keybind_binding *raw{view.get()};
    if (view->popup != nullptr) {
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
                click_actions_[item] = [this, raw, mode] {
                    set_keybind_mode(*raw, mode);
                };
            }
        }
        if (Rml::Element *item{
                document_->GetElementById(base + "-unset")};
            item != nullptr) {
            click_actions_[item] = [this, raw] {
                if (raw->bind != nullptr) {
                    raw->bind->key = 0;
                    raw->bind->reset();
                }
                update_keybind_text(*raw);
                persist_keybind(*raw);
                close_popups();
            };
        }
    }

    keybind_lookup_[element] = raw;
    keybinds_.push_back(std::move(view));
    update_keybind_text(*keybinds_.back());
}

void menu::bind_button(const std::string_view id,
                       std::function<void()> action) noexcept {
    if (document_ == nullptr) {
        return;
    }
    Rml::Element *element{document_->GetElementById(Rml::String{id})};
    if (element == nullptr) {
        ZEPHYRUS_WARN("menu: missing button {}", id);
        return;
    }
    click_actions_[element] = std::move(action);
}

void menu::start_listening(keybind_binding &binding) noexcept {
    close_popups();
    capture_binding_ = &binding;
    capture_element_ = binding.button;
    capture_handler_ = nullptr;
    capture_previous_key_ = binding.bind != nullptr ? binding.bind->key : 0;
    set_text(binding.text, "press a key...");
}

void menu::open_keybind_popup(keybind_binding &binding) noexcept {
    close_popups();
    if (binding.popup == nullptr) {
        return;
    }
    binding.popup->SetClass("open", true);
    open_keybind_ = &binding;
}

void menu::set_keybind_mode(keybind_binding &binding,
                            const keybind_mode mode) noexcept {
    if (binding.bind != nullptr) {
        binding.bind->mode = mode;
        binding.bind->reset();
    }
    update_keybind_text(binding);
    persist_keybind(binding);
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
        set_text(binding.text, "Unbound");
    } else {
        set_text(binding.text, key_name(binding.bind->key));
    }
}

void menu::persist_keybind(keybind_binding &binding) noexcept {
    if (binding.bind == nullptr) {
        return;
    }
    const std::string &key{binding.storage_key};
    if (key.empty()) {
        return;
    }
    // Store the raw SDL2 scancode only: the shared key-name helper is the
    // SDL 1.2 keysym table and would map scancodes to the wrong symbols.
    toml::table entry{};
    entry.insert("keycode", static_cast<std::int64_t>(binding.bind->key));
    entry.insert("mode",
                 std::string{keybind_mode_name(binding.bind->mode)});
    toml::table &features{config::ensure_table(config_.root(), "features")};
    features.insert_or_assign(key, std::move(entry));
    config_.mark_dirty();
}

bool menu::capture_key_event(const int scancode) noexcept {
    if (!capturing_key()) {
        return false;
    }
    if (scancode == 41) {
        // Escape cancels and restores the value from before capturing.
        if (capture_binding_ != nullptr && capture_binding_->bind != nullptr) {
            capture_binding_->bind->key = capture_previous_key_;
            update_keybind_text(*capture_binding_);
        }
        capture_binding_ = nullptr;
        capture_handler_ = nullptr;
        capture_element_ = nullptr;
        return true;
    }

    if (capture_binding_ != nullptr) {
        keybind *bind{capture_binding_->bind};
        if (bind != nullptr) {
            // A plain switch (Always, no key) becomes a Toggle once a key is
            // bound, otherwise pressing the new key would do nothing.
            if (bind->mode == keybind_mode::always && bind->key == 0) {
                bind->mode = keybind_mode::toggle;
            }
            if (bind->mode == keybind_mode::off) {
                bind->mode = keybind_mode::toggle;
            }
            bind->key = scancode;
            bind->reset();
        }
        update_keybind_text(*capture_binding_);
        persist_keybind(*capture_binding_);
    } else if (capture_handler_) {
        capture_handler_(scancode);
    }
    capture_binding_ = nullptr;
    capture_handler_ = nullptr;
    capture_element_ = nullptr;
    return true;
}

void menu::open_combo(Rml::Element *combo) noexcept {
    Rml::Element *popup{combo->QuerySelector(".combo-popup")};
    if (popup == nullptr) {
        return;
    }
    if (open_combo_ == popup) {
        close_popups();
        return;
    }
    close_popups();
    popup->SetClass("open", true);
    open_combo_ = popup;
}

void menu::close_popups() noexcept {
    if (open_combo_ != nullptr) {
        open_combo_->SetClass("open", false);
        open_combo_ = nullptr;
    }
    if (open_keybind_ != nullptr && open_keybind_->popup != nullptr) {
        open_keybind_->popup->SetClass("open", false);
        open_keybind_ = nullptr;
    }
    for (const auto &[item, combo] : combo_lookup_) {
        if (Rml::Element *popup{combo->QuerySelector(".combo-popup")};
            popup != nullptr) {
            popup->SetClass("open", false);
        }
    }
}

void menu::scroll_by(const float delta) noexcept {
    if (content_ == nullptr || content_scroll_ == nullptr) {
        return;
    }
    const float viewport{content_->GetClientHeight()};
    const float content{content_scroll_->GetClientHeight()};
    scroll_min_ = std::min(0.0F, viewport - content - 8.0F);
    scroll_offset_ =
        std::clamp(scroll_offset_ - delta * 72.0F, scroll_min_, 0.0F);
    content_scroll_->SetProperty(
        "top", px(scroll_offset_));
}

void menu::clamp_scroll() noexcept {
    if (content_ == nullptr || content_scroll_ == nullptr) {
        return;
    }
    const float viewport{content_->GetClientHeight()};
    const float content{content_scroll_->GetClientHeight()};
    const float minimum{std::min(0.0F, viewport - content - 8.0F)};
    if (scroll_min_ != minimum) {
        scroll_min_ = minimum;
    }
    if (scroll_offset_ < minimum || scroll_offset_ > 0.0F) {
        scroll_offset_ = std::clamp(scroll_offset_, minimum, 0.0F);
        content_scroll_->SetProperty("top", px(scroll_offset_));
    }
}

void menu::reset_scroll() noexcept {
    scroll_offset_ = 0.0F;
    scroll_min_ = 0.0F;
    if (content_scroll_ != nullptr) {
        content_scroll_->SetProperty("top", "0px");
    }
}

void menu::refresh_config_list() noexcept {
    if (document_ == nullptr || config_list_ == nullptr) {
        return;
    }
    // Drop the previous rows (and their click handlers).
    while (Rml::Element *child{config_list_->GetFirstChild()}) {
        click_actions_.erase(child);
        Rml::ElementPtr removed{config_list_->RemoveChild(child)};
    }
    config_rows_.clear();

    auto add_row = [this](const std::string &name,
                          const std::string &display) {
        Rml::ElementPtr row{document_->CreateElement("div")};
        row->SetClass("config-item", true);
        row->SetClass("selected", name == selected_config_);
        row->SetInnerRML(escape_rml(display));
        Rml::Element *raw{row.get()};
        click_actions_[raw] = [this, name] { select_config(name); };
        config_list_->AppendChild(std::move(row));
        config_rows_.emplace_back(raw, name);
    };
    add_row("latest", "latest (snapshot)");

    std::error_code error;
    const std::filesystem::path directory{config::document::profiles_dir("csgo")};
    std::vector<std::string> names;
    if (std::filesystem::is_directory(directory, error)) {
        for (const auto &entry :
             std::filesystem::directory_iterator{directory, error}) {
            if (!entry.is_regular_file(error)) {
                continue;
            }
            if (entry.path().extension() != ".toml") {
                continue;
            }
            const std::string stem{entry.path().stem().string()};
            if (valid_profile_name(stem)) {
                names.push_back(stem);
            }
        }
    }
    std::ranges::sort(names);
    for (const std::string &name : names) {
        add_row(name, name);
    }
}

void menu::select_config(std::string_view name) noexcept {
    selected_config_ = std::string{name};
    if (config_input_ != nullptr) {
        config_input_->SetValue(Rml::String{name});
    }
    for (auto &[row, row_name] : config_rows_) {
        row->SetClass("selected", row_name == name);
    }
}

std::string menu::config_name_value() const {
    if (config_input_ != nullptr) {
        return config_input_->GetValue().c_str();
    }
    return {};
}

std::vector<std::string> menu::config_names() const {
    std::vector<std::string> names{"latest"};
    std::error_code error;
    const std::filesystem::path directory{config::document::profiles_dir("csgo")};
    if (std::filesystem::is_directory(directory, error)) {
        for (const auto &entry :
             std::filesystem::directory_iterator{directory, error}) {
            if (!entry.is_regular_file(error)) {
                continue;
            }
            const std::string stem{entry.path().stem().string()};
            if (entry.path().extension() == ".toml" &&
                valid_profile_name(stem)) {
                names.push_back(stem);
            }
        }
    }
    std::ranges::sort(names);
    return names;
}

std::filesystem::path
menu::profile_path(const std::string_view name) const {
    return config::document::profiles_dir("csgo") / (std::string{name} + ".toml");
}

void menu::reload_from_config() noexcept {
    (void)config_.save();
    if (const toml::table *menu_table{config::table_at(config_.root(), "menu")};
        menu_table != nullptr) {
        geometry geometry_value{};
        geometry_value.x = static_cast<float>(
            config::read_float(*menu_table, "x", position_x_));
        geometry_value.y = static_cast<float>(
            config::read_float(*menu_table, "y", position_y_));
        geometry_value.width = static_cast<float>(
            config::read_float(*menu_table, "w", window_width_));
        geometry_value.height = static_cast<float>(
            config::read_float(*menu_table, "h", window_height_));
        apply_geometry(geometry_value);
    }
    refresh_config_list();
    if (on_config_reloaded) {
        on_config_reloaded();
    }
}

void menu::config_save(const std::string_view name) noexcept {
    if (!valid_profile_name(name)) {
        ZEPHYRUS_WARN("menu: invalid profile name '{}'", name);
        return;
    }
    config::document profile{};
    profile.set_path(profile_path(name));
    profile.replace_root(config_.root());
    if (!profile.save()) {
        ZEPHYRUS_WARN("menu: failed to save profile '{}'", name);
        return;
    }
    selected_config_ = std::string{name};
    refresh_config_list();
    ZEPHYRUS_INFO("menu: profile '{}' saved", name);
}

void menu::config_load(const std::string_view name) noexcept {
    if (!valid_profile_name(name)) {
        ZEPHYRUS_WARN("menu: invalid profile name '{}'", name);
        return;
    }
    const std::filesystem::path path{profile_path(name)};
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        ZEPHYRUS_WARN("menu: profile '{}' does not exist", name);
        return;
    }
    try {
        toml::table root{toml::parse_file(path.string())};
        config_.replace_root(std::move(root));
        selected_config_ = std::string{name};
        reload_from_config();
        ZEPHYRUS_INFO("menu: profile '{}' loaded", name);
    } catch (const toml::parse_error &failure) {
        ZEPHYRUS_WARN("menu: cannot parse profile '{}': {}", name,
                      std::string{failure.description()});
    }
}

void menu::config_delete(const std::string_view name) noexcept {
    if (!valid_profile_name(name)) {
        ZEPHYRUS_WARN("menu: invalid profile name '{}'", name);
        return;
    }
    std::error_code error;
    if (!std::filesystem::remove(profile_path(name), error) || error) {
        ZEPHYRUS_WARN("menu: cannot delete profile '{}'", name);
        return;
    }
    if (selected_config_ == name) {
        selected_config_.clear();
    }
    refresh_config_list();
    ZEPHYRUS_INFO("menu: profile '{}' deleted", name);
}

void menu::config_reset() noexcept {
    config_.replace_root(toml::table{});
    selected_config_.clear();
    apply_geometry(geometry{});
    reload_from_config();
    ZEPHYRUS_INFO("menu: configuration reset to defaults");
}

void menu::config_export() noexcept {
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr || api->set_clipboard_text == nullptr) {
        ZEPHYRUS_WARN("menu: clipboard unavailable");
        return;
    }
    const std::string text{config_.dump()};
    api->set_clipboard_text(text.c_str());
    ZEPHYRUS_INFO("menu: configuration exported to clipboard ({} bytes)",
                  text.size());
}

void menu::config_import() noexcept {
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr || api->get_clipboard_text == nullptr ||
        api->free_memory == nullptr) {
        ZEPHYRUS_WARN("menu: clipboard unavailable");
        return;
    }
    char *raw{api->get_clipboard_text()};
    if (raw == nullptr) {
        ZEPHYRUS_WARN("menu: clipboard is empty");
        return;
    }
    const std::string text{raw};
    api->free_memory(raw);
    try {
        toml::table root{toml::parse(text)};
        config_.replace_root(std::move(root));
        selected_config_.clear();
        reload_from_config();
        ZEPHYRUS_INFO("menu: configuration imported from clipboard");
    } catch (const toml::parse_error &failure) {
        ZEPHYRUS_WARN("menu: clipboard is not a valid profile: {}",
                      std::string{failure.description()});
    }
}

void menu::open_config_folder() noexcept {
    std::error_code error;
    const std::filesystem::path directory{config::document::profiles_dir("csgo")};
    std::filesystem::create_directories(directory, error);
    if (error) {
        ZEPHYRUS_WARN("menu: cannot create {}", directory.string());
        return;
    }
    // Detached helper process; the game process must not be blocked.
    const pid_t pid{fork()};
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", directory.c_str(),
               static_cast<char *>(nullptr));
        _exit(1);
    }
    ZEPHYRUS_INFO("menu: opening {}", directory.string());
}

void menu::set_text(Rml::Element *element, std::string_view text) noexcept {
    element->SetInnerRML(Rml::String{text});
}

Rml::Element *menu::text_element_of(Rml::Element *element) noexcept {
    if (element == nullptr || document_ == nullptr) {
        return nullptr;
    }
    const Rml::String id{element->GetId()};
    if (!id.empty()) {
        if (Rml::Element *text{
                document_->GetElementById(Rml::String{id + "-text"})};
            text != nullptr) {
            return text;
        }
    }
    return element->QuerySelector(".text");
}

void menu::store_bool(const std::string_view id, const bool value) noexcept {
    toml::table &features{config::ensure_table(config_.root(), "features")};
    features.insert_or_assign(std::string{id}, value);
    config_.mark_dirty();
}

void menu::store_float(const std::string_view id, const double value) noexcept {
    toml::table &features{config::ensure_table(config_.root(), "features")};
    features.insert_or_assign(std::string{id}, value);
    config_.mark_dirty();
}

void menu::store_int(const std::string_view id,
                     const std::int64_t value) noexcept {
    toml::table &features{config::ensure_table(config_.root(), "features")};
    features.insert_or_assign(std::string{id}, value);
    config_.mark_dirty();
}

bool menu::stored_bool(const std::string_view id,
                       const bool fallback) noexcept {
    const toml::table *features{config::table_at(config_.root(), "features")};
    return features != nullptr ? config::read_bool(*features, id, fallback)
                               : fallback;
}

double menu::stored_float(const std::string_view id,
                          const double fallback) noexcept {
    const toml::table *features{config::table_at(config_.root(), "features")};
    return features != nullptr ? config::read_float(*features, id, fallback)
                               : fallback;
}

std::int64_t menu::stored_int(const std::string_view id,
                              const std::int64_t fallback) noexcept {
    const toml::table *features{config::table_at(config_.root(), "features")};
    return features != nullptr ? config::read_int(*features, id, fallback)
                               : fallback;
}
} // namespace zephyrus::csgo::overlay
