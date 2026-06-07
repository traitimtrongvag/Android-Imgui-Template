#ifndef ANDROID_MOD_MENU_MACROS_H
#define ANDROID_MOD_MENU_MACROS_H
#include <KittyMemory/KittyMemory.h>
#include <KittyMemory/MemoryPatch.h>
#include <KittyMemory/KittyScanner.h>
#include <KittyMemory/KittyUtils.h>
#include "../And64InlineHook/And64InlineHook.hpp"
#include "AnAn/Tools/Dobby/dobby.h"

void hook(void *offset, void* ptr, void **orig)
{
    DobbyHook(offset, ptr, orig);
}

#define HOOK(offset, ptr, orig) \
    DobbyHook((void*)getAbsoluteAddress(TARGET_LIB, offset), \
              (void*)ptr, \
              (void**)&orig)
        
#define HOOK_LIB(lib, offset, ptr, orig) hook((void *)getAbsoluteAddress(TARGET_LIB, string2Offset(oxorany(offset))), (void *)ptr, (void **)&orig)
#define HOOK_NO_ORIG(offset, ptr) DobbyHook((void *)getAbsoluteAddress(TARGET_LIB, oxorany(offset)), (void *)ptr, NULL)
#define HOOK_LIB_NO_ORIG(lib, offset, ptr) hook((void *)getAbsoluteAddress(TARGET_LIB, string2Offset(oxorany(offset))), (void *)ptr, NULL)

#define HOOKSYM(sym, ptr, org) DobbyHook(dlsym(dlopen(TARGET_LIB, 4), OBFUSCATE(sym)), (void *)ptr, (void **)&org)
#define HOOKSYM_LIB(lib, sym, ptr, org) DobbyHook(dlsym(dlopen(OBFUSCATE(TARGET_LIB), 4), OBFUSCATE(sym)), (void *)ptr, (void **)&org)

#define HOOKSYM_NO_ORIG(sym, ptr)  DobbyHook(dlsym(dlopen(TARGET_LIB, 4), OBFUSCATE(sym)), (void *)ptr, NULL)
#define HOOKSYM_LIB_NO_ORIG(lib, sym, ptr) DobbyHook(dlsym(dlopen(OBFUSCATE(TARGET_LIB), 4), OBFUSCATE(sym)), (void *)ptr, NULL)


std::vector<MemoryPatch> memoryPatches;
std::vector<uint64_t> offsetVector;

// Patching a offset without switch.
// Modified patchOffset function
void patchOffset(const char* libName, uintptr_t offset, std::string hex, bool isOn) {
    KittyMemory::ProcMap map;
    MemoryPatch patch = MemoryPatch::createWithHex(map, offset, hex);

    if (std::find(offsetVector.begin(), offsetVector.end(), offset) != offsetVector.end()) {
        std::vector<uint64_t>::iterator itr = std::find(offsetVector.begin(), offsetVector.end(), offset);
        patch = memoryPatches[std::distance(offsetVector.begin(), itr)];
    } else {
        memoryPatches.push_back(patch);
        offsetVector.push_back(offset);
    }

    if (!patch.isValid()) return;
    if (isOn) {
        patch.Modify();
    } else {
        patch.Restore();
    }
}

void patchOffsetSym(uintptr_t absolute_address, std::string hexBytes, bool isOn) {
    MemoryPatch patch = MemoryPatch::createWithHex(absolute_address, hexBytes);

    if (std::find(offsetVector.begin(), offsetVector.end(), absolute_address) != offsetVector.end()) {
        std::vector<uint64_t>::iterator itr = std::find(offsetVector.begin(), offsetVector.end(), absolute_address);
        patch = memoryPatches[std::distance(offsetVector.begin(), itr)];
    } else {
        memoryPatches.push_back(patch);
        offsetVector.push_back(absolute_address);
    }

    if (!patch.isValid()) return;
    if (isOn) {
        patch.Modify();
    } else {
        patch.Restore();
    }
}

#define PATCH(offset, hex) patchOffset(TARGET_LIB, offset, OBFUSCATE(hex), true)
#define PATCH_LIB(lib, offset, hex) patchOffset((TARGET_LIB), offset, OBFUSCATE(hex), true)

#define PATCH_SYM(sym, hex) patchOffset(dlsym(dlopen(TARGET_LIB, 4), OBFUSCATE(sym)), OBFUSCATE(hex), true)
#define PATCH_LIB_SYM(lib, sym, hex) patchOffset(dlsym(dlopen(lib, 4), OBFUSCATE(sym)), OBFUSCATE(hex), true)

#define PATCH_SWITCH(offset, hex, boolean) patchOffset(TARGET_LIB, offset, OBFUSCATE(hex), boolean)
#define PATCH_LIB_SWITCH(lib, offset, hex, boolean) patchOffset((TARGET_LIB), offset, OBFUSCATE(hex), boolean)

#define PATCH_SYM_SWITCH(sym, hex, boolean) patchOffsetSym((uintptr_t)dlsym(dlopen(TARGET_LIB, 4), OBFUSCATE(sym)), OBFUSCATE(hex), boolean)
#define PATCH_LIB_SYM_SWITCH(lib, sym, hex, boolean) patchOffsetSym((uintptr_t)dlsym(dlopen(lib, 4), OBFUSCATE(sym)), OBFUSCATE(hex), boolean)

#define RESTORE(offset) patchOffset(TARGET_LIB, offset, "", false)
#define RESTORE_LIB(lib, offset) patchOffset(OBFUSCATE(TARGET_LIB), offset, "", false)

#define RESTORE_SYM(sym) patchOffsetSym((uintptr_t)dlsym(dlopen(TARGET_LIB, 4), OBFUSCATE(sym)), "", false)
#define RESTORE_LIB_SYM(lib, sym) patchOffsetSym((uintptr_t)dlsym(dlopen(lib, 4), OBFUSCATE(sym)), "", false)

#define HOOKAU_LOG(dll, ns, claz, method, args, org, rep) \
    do { \
        void* _offset = GetMethodOffset(oxorany(dll), oxorany(ns), oxorany(claz), oxorany(method), args); \
        writeLog("HOOK", "[HOOKAU] %s | ns='%s' | class='%s' | method='%s' | args=%d => offset=%p (%s)", \
            dll, ns, claz, method, args, _offset, (_offset != nullptr) ? "OK" : "FAIL"); \
        if (_offset) Tools::Hook((void*)(uintptr_t)_offset, (void*)org, (void**)&rep); \
    } while(0)

#define HOOK_DIRECT(offset, org, rep) \
    do { \
        void* _addr = (void*)getAbsoluteAddress(TARGET_LIB, offset); \
        if (_addr) Tools::Hook(_addr, (void*)org, (void**)&rep); \
    } while(0)
    
#define HOOKAU(dll, ns, claz, method, args, org, rep) Tools::Hook((void *) (uintptr_t)GetMethodOffset(oxorany(dll), oxorany(ns), oxorany(claz), oxorany(method), args), (void *)org, (void **)&rep)
#define HOOKAU_NO_ORIG(dll, ns, claz, method, args, org, rep) Tools::Hook((void *) (uintptr_t)GetMethodOffset(oxorany(dll), oxorany(ns), oxorany(claz), oxorany(method), args), (void *)org, NULL)

#define PATCHAU(dll, ns, claz, method, args, hex) MemoryPatch::createWithHex((uintptr_t)GetMethodOffset(OBFUSCATE(dll), OBFUSCATE(ns), OBFUSCATE(claz) , OBFUSCATE(method), args), OBFUSCATE(hex)).Modify();

#endif //ANDROID_MOD_MENU_MACROS_H
