// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <string_view>

namespace zephyrus::acr::symbols {
// Client functions hooked or called by zephyrus.
inline constexpr std::string_view keypress{"_Z8keypressibi6SDLMod"};
inline constexpr std::string_view mousemove{"_Z9mousemoveii"};
inline constexpr std::string_view inputgrab{"_Z9inputgrabb"};
inline constexpr std::string_view shoot{"_Z5shootP9playerentR3vec"};
inline constexpr std::string_view sendshoot{"_ZN6weapon9sendshootERK3vec"};
inline constexpr std::string_view sendposition{"_Z12sendpositionP9playerent"};
inline constexpr std::string_view moveplayer{"_Z10moveplayerP7physentib"};
inline constexpr std::string_view moveplayer_step{"_Z10moveplayerP7physentibi"};
inline constexpr std::string_view addshotline{
    "_Z11addshotlineP9playerent3vecRKS1_i"};

// Game functions used through dlsym.
inline constexpr std::string_view raycubelos{"_Z10raycubelosRK3vecS1_f"};
inline constexpr std::string_view raycube{"_Z7raycubeRK3vecS1_RS_"};
inline constexpr std::string_view entinmap{"_Z8entinmapP7physent"};
inline constexpr std::string_view addmsg{"_Z6addmsgiPKcz"};
inline constexpr std::string_view c2sinfo{"_Z7c2sinfob"};
inline constexpr std::string_view vecfromyawpitch{"_Z15vecfromyawpitchffiiR3vec"};
inline constexpr std::string_view world_to_screen{
    "_Z13worldtoscreenRK3vecR4vec2i"};
inline constexpr std::string_view draw_text{"_Z9draw_textPKciiiiiiii"};
inline constexpr std::string_view execute{"_Z7executePKc"};
inline constexpr std::string_view add_bot{"_Z5addaiv"};
inline constexpr std::string_view bot_count{"_Z7numbotsv"};

// Exported game globals.
inline constexpr std::string_view player1{"player1"};
inline constexpr std::string_view players{"players"};
inline constexpr std::string_view camera1{"camera1"};
inline constexpr std::string_view worldhit{"worldhit"};
inline constexpr std::string_view worldhitzone{"worldhitzone"};
inline constexpr std::string_view worldpos{"worldpos"};
inline constexpr std::string_view camdir{"camdir"};
inline constexpr std::string_view gamemode{"gamemode"};
inline constexpr std::string_view mutators{"mutators"};
inline constexpr std::string_view guns{"guns"};
inline constexpr std::string_view muls{"muls"};
inline constexpr std::string_view thirdperson{"thirdperson"};
inline constexpr std::string_view effective_damage{
    "_Z15effectiveDamageifbb"};
inline constexpr std::string_view lastmillis{"lastmillis"};
inline constexpr std::string_view setscope{"_Z8setscopeb"};
inline constexpr std::string_view matrixstack{"_ZN9vertmodel11matrixstackE"};
inline constexpr std::string_view worldhitpos{"worldhitpos"};
inline constexpr std::string_view gametype{"gametype"};
} // namespace zephyrus::acr::symbols
