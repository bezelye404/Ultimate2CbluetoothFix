#include "PadFormat.h"
#include <cstddef>

// Not defined by the dinput.h of every SDK; the flag lets a data format entry be skipped when the object is absent.
#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL 0x80000000
#endif

namespace Ultimate2CFixer {

namespace {

constexpr int kAxisSlots = 6;

struct UsageSlot {
    WORD page;
    WORD usage;
};

// Order matches PadRaw: lX, lY, lZ, lRz, brake, gas.
constexpr UsageSlot kSlots[kAxisSlots] = {
    { 0x01, 0x30 },  // X
    { 0x01, 0x31 },  // Y
    { 0x01, 0x32 },  // Z
    { 0x01, 0x35 },  // Rz
    { 0x02, 0xC5 },  // Brake
    { 0x02, 0xC4 },  // Accelerator
};

struct Found {
    DWORD axisType[kAxisSlots] = {};
    bool hasAxis[kAxisSlots] = {};
    DWORD povType = 0;
    bool hasPov = false;
};

BOOL CALLBACK EnumObjectsCallback(LPCDIDEVICEOBJECTINSTANCEW obj, LPVOID ref) {
    auto* found = reinterpret_cast<Found*>(ref);

    if (obj->dwType & DIDFT_AXIS) {
        for (int i = 0; i < kAxisSlots; ++i) {
            if (!found->hasAxis[i] && obj->wUsagePage == kSlots[i].page && obj->wUsage == kSlots[i].usage) {
                found->axisType[i] = obj->dwType;
                found->hasAxis[i] = true;
                break;
            }
        }
    } else if ((obj->dwType & DIDFT_POV) && !found->hasPov) {
        found->povType = obj->dwType;
        found->hasPov = true;
    }
    return DIENUM_CONTINUE;
}

} // namespace

bool SetPadDataFormat(IDirectInputDevice8W* device) {
    Found found;
    device->EnumObjects(EnumObjectsCallback, &found, DIDFT_AXIS | DIDFT_POV);
    for (int i = 0; i < kAxisSlots; ++i) {
        if (!found.hasAxis[i]) return false;
    }

    DIOBJECTDATAFORMAT objects[kAxisSlots + 1 + 16] = {};
    DWORD count = 0;

    for (int i = 0; i < kAxisSlots; ++i) {
        objects[count].pguid = nullptr;
        objects[count].dwOfs = static_cast<DWORD>(offsetof(PadRaw, lX) + i * sizeof(LONG));
        objects[count].dwType = DIDFT_AXIS | DIDFT_MAKEINSTANCE(DIDFT_GETINSTANCE(found.axisType[i]));
        ++count;
    }

    objects[count].pguid = nullptr;
    objects[count].dwOfs = static_cast<DWORD>(offsetof(PadRaw, pov));
    objects[count].dwType = found.hasPov
        ? (DIDFT_POV | DIDFT_MAKEINSTANCE(DIDFT_GETINSTANCE(found.povType)))
        : (DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL);
    ++count;

    for (int b = 0; b < 16; ++b) {
        objects[count].pguid = nullptr;
        objects[count].dwOfs = static_cast<DWORD>(offsetof(PadRaw, buttons) + b);
        objects[count].dwType = DIDFT_BUTTON | DIDFT_MAKEINSTANCE(b) | DIDFT_OPTIONAL;
        ++count;
    }

    DIDATAFORMAT format = {};
    format.dwSize = sizeof(DIDATAFORMAT);
    format.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
    format.dwFlags = DIDF_ABSAXIS;
    format.dwDataSize = sizeof(PadRaw);
    format.dwNumObjs = count;
    format.rgodf = objects;

    return SUCCEEDED(device->SetDataFormat(&format));
}

} // namespace Ultimate2CFixer
