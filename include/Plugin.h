#pragma once

#include "version.h"

#include <string_view>

#define PLUGIN_MODNAME "Passive Weapon Enchantment Recharging"
#define PLUGIN_AUTHOR "Zebrina"

using namespace std::literals;

namespace Plugin
{
    inline constexpr auto VERSION = plugin_version::make(1, 1, 0, 0);
    inline constexpr auto INTERNALNAME = "PassiveWeaponEnchantmentRecharging"sv;
    inline constexpr auto CONFIGFILE = "OBSE\\Plugins\\PassiveWeaponEnchantmentRecharging.toml"sv;
}
