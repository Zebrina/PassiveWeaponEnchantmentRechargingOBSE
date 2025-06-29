#pragma once

#include "OBSE/OBSE.h"

#include <string_view>

using namespace std::literals;

namespace Plugin
{
    inline constexpr auto MODNAME = "Passive Weapon Enchantment Recharging"sv;
    inline constexpr auto AUTHOR = "Zebrina"sv;
    inline constexpr auto VERSION = OBSE::Version{ 1, 1, 1, 0 };
    inline constexpr auto INTERNALNAME = "PassiveWeaponEnchantmentRecharging"sv;
    inline constexpr auto CONFIGFILE = "OBSE\\Plugins\\PassiveWeaponEnchantmentRecharging.toml"sv;
}
