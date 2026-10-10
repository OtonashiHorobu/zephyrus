// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/features/acr/thirdperson.hpp>

#include <zephyrus/acr/symbols.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>

#include <algorithm>
#include <cmath>

namespace zephyrus::features {
std::expected<void, std::string>
thirdperson::initialize(const acr::game & /*game*/) noexcept {
    auto symbol{hook::resolve(acr::symbols::thirdperson)};
    if (!symbol) {
        return std::unexpected{"missing thirdperson variable: " +
                               symbol.error()};
    }
    thirdperson_ = reinterpret_cast<int *>(*symbol);
    last_ = std::chrono::steady_clock::now();
    return {};
}

void thirdperson::shutdown() noexcept {
    if (thirdperson_ != nullptr) {
        *thirdperson_ = 0;
    }
    thirdperson_ = nullptr;
}

void thirdperson::set_distance(float distance) noexcept {
    target_distance_ = std::clamp(distance, 1.0F, max_distance);
}

void thirdperson::update() noexcept {
    if (thirdperson_ == nullptr) {
        return;
    }

    const auto now{std::chrono::steady_clock::now()};
    const float dt{
        std::clamp(std::chrono::duration<float>(now - last_).count(), 0.0F,
                   0.1F)};
    last_ = now;

    const float target{enabled_ ? target_distance_ : 0.0F};

    // Exponential ease; the game only cares whether the int is non-zero, so
    // snap to at least 1 while pulling back.
    current_ += (target - current_) * std::min(1.0F, dt * 9.0F);
    if (enabled_ && current_ < 1.0F) {
        current_ = 1.0F;
    }

    if (!enabled_ && current_ < 1.2F) {
        if (*thirdperson_ != 0) {
            *thirdperson_ = 0;
        }
        current_ = 0.0F;
        return;
    }

    const int value{std::clamp(static_cast<int>(std::lround(current_)), 1,
                               static_cast<int>(max_distance))};
    if (*thirdperson_ != value) {
        *thirdperson_ = value;
    }
}
} // namespace zephyrus::features
