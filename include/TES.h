#pragma once

#include "obse64\GameForms.h"

class TESGlobal : public TESForm
{
public:
    virtual ~TESGlobal();

    u32 unk38[5];
    float value;
};

static_assert(offsetof(TESGlobal, value) == 0x44);

class GameCalendar
{
public:

    [[nodiscard]] float GetGameTime() const
    {
        return gameDaysPassed->value + (gameHour->value / 24.0f);
    }

    TESGlobal* gameYear;
    TESGlobal* gameWeek;
    TESGlobal* gameDay;
    TESGlobal* gameHour;
    TESGlobal* gameDaysPassed;
    TESGlobal* timeScale;
};

class BSExtraCharge : public BSExtraData
{
public:
    enum { kID = kExtraData_Charge };

    virtual ~BSExtraCharge();

    float charge;
};

static_assert(offsetof(BSExtraCharge, charge) == 0x18);
