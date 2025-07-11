#include "OBSE/OBSE.h"
#include "RE/Oblivion.h"
#include <REL//REL.h>

#include <BGSScriptExtenderPluginTools.h>
#include <windows_lean_and_mean.h>

#include "Plugin.h"

#pragma comment(lib, "commonlibob64.lib")
#pragma comment(lib, "BGSScriptExtenderPluginTools.lib")

using namespace RE;

using UpdateFunction = uintptr_t(*)(PlayerCharacter*);
//using RemoveExtraDataFunction = void (*)(ExtraDataList* extraList, uint8_t type);
//using UpdateWeaponChargePercentFunction = void (*)(bool, float percent);

static UpdateFunction HookedFunction;
//static UpdateWeaponChargePercentFunction UpdateWeaponChargePercent;
//static RemoveExtraDataFunction RemoveExtraData;
//static GameCalendar* gGameCalendar;

static bool gPercentRecharge = false;
static float gRechargeValuePerDay = 3200.0f;
static uint64_t gRechargeInterval = 1000;
static bool gRechargeFollowerWeapons = false;

static void UpdateWeaponChargePercent(float percent)
{
    REL::Relocation<void(*)(bool, float)> func{ REL::ID(405125) };
    func(true, percent);
}

static void ActorRechargeWeapons(TESObjectREFR* actor, float value)
{
    if (!actor)
        return;

    ExtraContainerChanges* containerChanges = actor->extra.GetExtraData<ExtraContainerChanges>();
    if (!containerChanges)
        return;

    BSSimpleList<InventoryEntryData*>* inventoryList = containerChanges->GetObjectList();
    if (!inventoryList)
        return;

    for (auto entry : *inventoryList)
    {
        if (entry->extraData == nullptr || entry->object->GetFormType() != FormType::Weapon)
            continue;

        TESObjectWEAP* weapon = static_cast<TESObjectWEAP*>(entry->object);
        if (weapon->formEnchanting == nullptr)
            continue;

        for (auto weaponExtra : *(entry->extraData))
        {
            ExtraCharge* extraCharge = reinterpret_cast<ExtraCharge*>(weaponExtra->GetExtraData(ExtraDataType::Charge));
            if (!extraCharge)
                continue;

            if (gPercentRecharge)
                extraCharge->charge += value * (float)weapon->amountOfEnchantment;
            else
                extraCharge->charge += value;

            if (extraCharge->charge >= weapon->amountOfEnchantment)
                weaponExtra->RemoveExtra(ExtraDataType::Charge);

            if (weaponExtra->HasExtraData(ExtraDataType::Worn))
                UpdateWeaponChargePercent(std::min(1.0f, extraCharge->charge / (float)weapon->amountOfEnchantment));
        }
    }
}

static bool ActorIsFollowingPlayer(Actor* actor)
{
    if (actor->currentProcess == nullptr)
        return false;

    HighProcess* process = RE::oblivion_cast<HighProcess*, BaseProcess>(actor->currentProcess);
    if (!process)
        return false;

    TESPackage* currentPackage = process->GetCurrentPackage();
    if (!currentPackage)
        return false;

    constexpr const uint8_t PACKAGE_TYPE_FOLLOW = 1;
    constexpr const uint32_t DEFENSIVE_COMBAT_FLAG = 0x00400000;

    return currentPackage->procedureType == PACKAGE_PROCEDURE_TYPE::kFollowWithoutEscort &&
           currentPackage->packData.packType == PACKAGE_TYPE_FOLLOW &&
           (currentPackage->packData.packFlags & DEFENSIVE_COMBAT_FLAG) == DEFENSIVE_COMBAT_FLAG &&
           currentPackage->packTarget->target == PlayerCharacter::GetSingleton();
}

static uintptr_t RechargeHook(PlayerCharacter* player)
{
    static uint64_t lastRechargeTime = 0;
    static float lastRechargeGameTime = 0.0f;
    static TESObjectCELL* lastPlayerCell = nullptr;

    uintptr_t returnValue = HookedFunction(player);

    if (player->parentCell)
    {
        uint64_t time = GetTickCount64();
        if (time - lastRechargeTime >= gRechargeInterval)
        {
            float gameTime = GameCalendar::GetSingleton()->GetCurrentGameTime();
            if (gameTime > lastRechargeGameTime)
            {
                float gameTimeElapsed = gameTime - lastRechargeGameTime;
                if (gameTimeElapsed < 1.04)
                {
                    float rechargeValue = gameTimeElapsed * gRechargeValuePerDay;
                    if (rechargeValue > 0.0f)
                    {
                        ActorRechargeWeapons(player, rechargeValue);

                        if (gRechargeFollowerWeapons)
                        {
                            for (auto ref : player->parentCell->listReferences)
                            {
                                if (ref->GetFormType() != FormType::ActorCharacter)
                                    continue;

                                Character* character = static_cast<Character*>(ref);
                                if (character->GetActorType() != ACTOR_TYPE::kNPC)
                                    continue;

                                if (ActorIsFollowingPlayer(character))
                                    ActorRechargeWeapons(ref, rechargeValue);
                            }
                        }
                    }
                }
            }

            lastRechargeTime = time;
            lastRechargeGameTime = gameTime;
        }
    }

    return returnValue;
}

static bool ApplyPatch()
{
    reverse_engineering::info exe;
    if (!exe.read_process())
        return false;

    namespace re = reverse_engineering;

#if 0
    constexpr re::signature gameCalendar
    {
        // OblivionRemastered-Win64-Shipping.exe+68373F2 - 48 8D 0D BF1CC802     - lea rcx,[OblivionRemastered-Win64-Shipping.exe+94B90B8] { (1DC8AA85830) }
        0x48, 0x8D, 0x0D, re::any_byte, re::any_byte, re::any_byte, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+68373F9 - 0B FB                 - or edi,ebx
        0x0B, 0xFB,
        // OblivionRemastered-Win64-Shipping.exe+68373FB - C1 E7 09              - shl edi,09 { 9 }
        0xC1, 0xE7, 0x09,
        // OblivionRemastered-Win64-Shipping.exe+68373FE - E8 5DB7F1FF           - call OblivionRemastered-Win64-Shipping.exe+6752B60
        0xE8, re::any_byte, re::any_byte, re::any_byte, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+6837403 - 48 8B 5C 24 30        - mov rbx,[rsp+30]
        0x48, 0x8B, 0x5C, 0x24, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+6837408 - 0FBE C8               - movsx ecx,al
        0x0F, 0xBE, 0xC8,
        // OblivionRemastered-Win64-Shipping.exe+683740B - 0B CF                 - or ecx,edi
        0x0B, 0xCF,
        // OblivionRemastered-Win64-Shipping.exe+683740D - 89 4E 24              - mov [rsi+24],ecx
        0x89, 0x4E, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+6837410 - 48 8B 74 24 38        - mov rsi,[rsp+38]
        0x48, 0x8B, 0x74, 0x24, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+6837415 - 48 83 C4 20           - add rsp,20 { 32 }
        0x48, 0x83, 0xC4, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+6837419 - 5F                    - pop rdi
        0x5F,
        // OblivionRemastered-Win64-Shipping.exe+683741A - C3                    - ret  
        0xC3,
    };

    exe.find_signature("GameCalendar", gameCalendar);
    if (!gameCalendar)
        return false;

    gGameCalendar = (GameCalendar*)gameCalendar.get_4byte_displacement("GameCalendar pointer", 3);

    constexpr re::signature removeExtraData
    {
        // OblivionRemastered-Win64-Shipping.exe+67DC41F - 0F2F C6               - comiss xmm0,xmm6
        0x0F, 0x2F, 0xC6,
        // OblivionRemastered-Win64-Shipping.exe+67DC422 - 0F82 B2000000         - jb OblivionRemastered-Win64-Shipping.exe+67DC4DA
        0x0F, 0x82, re::any_byte, re::any_byte, 0x00, 0x00,
        // OblivionRemastered-Win64-Shipping.exe+67DC428 - B2 2E                 - mov dl,2E { 46 }
        0xB2, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+67DC42A - 48 8B CF              - mov rcx,rdi
        0x48, 0x8B, 0xCF,
        // OblivionRemastered-Win64-Shipping.exe+67DC42D - E8 8E9EE6FF           - call OblivionRemastered-Win64-Shipping.exe+66462C0
        0xE8, re::any_byte, re::any_byte, re::any_byte, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+67DC432 - 48 83 7F 08 00        - cmp qword ptr [rdi+08],00 { 0 }
        0x48, 0x83, 0x7F, re::any_byte, 0x00,
        // OblivionRemastered-Win64-Shipping.exe+67DC437 - 0F85 D0000000         - jne OblivionRemastered-Win64-Shipping.exe+67DC50D
        0x0F, 0x85, re::any_byte, re::any_byte, 0x00, 0x00,
        // OblivionRemastered-Win64-Shipping.exe+67DC43D - 48 3B FB              - cmp rdi,rbx
        0x48, 0x3B, 0xFB,
    };

    exe.find_signature("RemoveExtraData", removeExtraData);
    if (!removeExtraData)
        return false;

    RemoveExtraData = (RemoveExtraDataFunction)removeExtraData.get_4byte_displacement("RemoveExtraData function", 15);

    constexpr re::signature updateWeaponChargePercent
    {
        // OblivionRemastered-Win64-Shipping.exe+661C42F - F3 41 0F5E F0         - divss xmm6,xmm8
        0xF3, 0x41, 0x0F, 0x5E, 0xF0,
        // OblivionRemastered-Win64-Shipping.exe+661C434 - B1 01                 - mov cl,01 { 1 }
        0xB1, 0x01,
        // OblivionRemastered-Win64-Shipping.exe+661C436 - 0F28 CE               - movaps xmm1,xmm6
        0x0F, 0x28, 0xCE,
        // OblivionRemastered-Win64-Shipping.exe+661C439 - E8 C2A3F7FF           - call OblivionRemastered-Win64-Shipping.exe+6596800
        0xE8, re::any_byte, re::any_byte, re::any_byte, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+661C43E - F3 0F10 0D DA459C02   - movss xmm1,[OblivionRemastered-Win64-Shipping.exe+8FE0A20] { (0.10) }
        0xF3, 0x0F, 0x10, 0x0D, re::any_byte, re::any_byte, re::any_byte, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+661C446 - F3 41 0F5E F8         - divss xmm7,xmm8
        0xF3, 0x41, 0x0F, 0x5E, 0xF8,
        // OblivionRemastered-Win64-Shipping.exe+661C44B - 0F2F F9               - comiss xmm7,xmm1
        0x0F, 0x2F, 0xF9,
    };

    exe.find_signature("UpdateWeaponChargePercent", updateWeaponChargePercent);
    if (!updateWeaponChargePercent)
        return false;

    UpdateWeaponChargePercent = (UpdateWeaponChargePercentFunction)updateWeaponChargePercent.get_4byte_displacement("UpdateWeaponChargePercent function", 11);
#endif

    constexpr re::signature noMenuUpdate
    {
        // OblivionRemastered-Win64-Shipping.exe+68F9E65 - 77 04                 - ja OblivionRemastered-Win64-Shipping.exe+68F9E6B
        0x77, 0x04,
        // OblivionRemastered-Win64-Shipping.exe+68F9E67 - 33 DB                 - xor ebx,ebx
        0x33, 0xDB,
        // OblivionRemastered-Win64-Shipping.exe+68F9E69 - EB 0A                 - jmp OblivionRemastered-Win64-Shipping.exe+68F9E75
        0xEB, 0x0A,
        // OblivionRemastered-Win64-Shipping.exe+68F9E6B - 48 8B 88 38010000     - mov rcx,[rax+00000138]
        0x48, 0x8B, 0x88, re::any_byte, 0x01, 0x00, 0x00,
        // OblivionRemastered-Win64-Shipping.exe+68F9E72 - 48 8B 19              - mov rbx,[rcx]
        0x48, 0x8B, 0x19,
        // OblivionRemastered-Win64-Shipping.exe+68F9E75 - 48 8B 0D 0CB5B502     - mov rcx,[OblivionRemastered-Win64-Shipping.exe+9455388] { (1D296979A00) }
        0x48, 0x8B, 0x0D, re::any_byte, re::any_byte, re::any_byte, re::any_byte,
        // OblivionRemastered-Win64-Shipping.exe+68F9E7C - E8 2FD5C0FA           - call OblivionRemastered-Win64-Shipping.exe+15073B0
        0xE8, re::any_byte, re::any_byte, re::any_byte, re::any_byte,
    };

    exe.find_signature("update", noMenuUpdate);
    if (!noMenuUpdate)
        return false;

    HookedFunction = (UpdateFunction)REL::GetTrampoline().write_call5(noMenuUpdate.get_address() + 23, (uintptr_t)RechargeHook);

    return HookedFunction;
}

OBSE_PLUGIN_VERSION = []()
    {
        OBSE::PluginVersionData v{};

        v.PluginVersion(Plugin::VERSION);
        v.PluginName(Plugin::MODNAME);
        v.AuthorName(Plugin::AUTHOR);
        v.UsesAddressLibrary(true);
        v.HasNoStructUse(true);
        //v.UsesUpdatedStructs();
        //v.CompatibleVersions({ OBSE::Version{ 0, 0, 0, 1 } });

        return v;
    }();

OBSE_PLUGIN_LOAD(const OBSE::LoadInterface* obse)
{
    if (!plugin_log::initialize(Plugin::INTERNALNAME))
        return false;

    OBSE::InitInfo info{};
    info.log = false;
    info.trampoline = true;
    info.trampolineSize = 14;
    OBSE::Init(obse, info);

    OBSE::TrampolineInterface* trampolineInterface = static_cast<OBSE::TrampolineInterface*>(obse->QueryInterface(OBSE::LoadInterface::kTrampoline));
    if (!trampolineInterface)
    {
        plugin_log::err("Failed to query OBSE trampoline interface."sv);
        return false;
    }

    constexpr const std::string_view configSection{ "Settings"sv };

    plugin_configuration config{ Plugin::CONFIGFILE };

    gPercentRecharge = config.get(configSection, "bPercentRecharge"sv, false);
    gRechargeValuePerDay = config.get(configSection, "fRechargeValuePerDay"sv, 3200.0f);
    gRechargeInterval = config.get(configSection, "iRechargeInterval"sv, 1000);
    gRechargeFollowerWeapons = config.get(configSection, "bRechargeFollowerWeapons", false);

    if (gRechargeValuePerDay <= 0.0f)
    {
        plugin_log::err("fRechargeValuePerDay cannot be less or equal to zero."sv);
        return false;
    }

    if (gPercentRecharge)
        plugin_log::info("Percent based recharge enabled. Recharge percent (over a day) is {:.2f}%"sv, gRechargeValuePerDay * 100.0f);

    bool success = ApplyPatch();
    if (success)
        plugin_log::info("Successful plugin load! Have fun! :D"sv);

    return success;
}
