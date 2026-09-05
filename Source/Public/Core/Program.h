// Copyright BattleDash. All Rights Reserved.

#pragma once

#include <Core/Server.h>
#include <SDK/TypeInfo.h>
#include <API/APIService.h>
#include <Hook/Func.h>

#include <Windows.h>

#define OFFSET_GLOBAL_SETTINGS_MANAGER 0x1453E0508

namespace Kyber
{
TL_DECLARE_FUNC(0x14119B8C0, __int64, Settings_Settings, __int64 settingsManager, __int64 typeInfo);

__int64 ClientStateChangeHk(__int64 a1, ClientState currentClientState, ClientState lastClientState);

class Program
{
public:
    Program(HMODULE module);
    ~Program();

    DWORD WINAPI InitializationThread();
    void InitializeGameHooks();

    template<typename T>
    T* GetSettingsObject(const __int64 typeInfoOffset)
    { 
        return reinterpret_cast<T*>(
            Settings_Settings(*reinterpret_cast<__int64*>(OFFSET_GLOBAL_SETTINGS_MANAGER), typeInfoOffset)); 
    }

    HMODULE m_module;
    APIService* m_api;
    Server* m_server;
    ClientState m_clientState = ClientState_None;
    bool m_joining;
};

template<class T>
class Settings
{
public:
    Settings(const __int64 typeInfoOffset)
    { 
        m_settings = g_program->GetSettingsObject<T>(typeInfoOffset);
    }

    inline T* operator->()
    {
        return m_settings;
    }

    inline const T* operator->() const
    {
        return m_settings;
    }

    inline operator T*()
    {
        return m_settings;
    }

    inline operator const T*() const
    {
        return m_settings;
    }

private:
    T* m_settings;
};

} // namespace Kyber

extern Kyber::Program* g_program;