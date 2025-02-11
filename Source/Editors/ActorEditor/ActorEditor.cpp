﻿// ActorEditor.cpp : Определяет точку входа для приложения.
//
#include "stdafx.h"
#include "resources\splash.h"

XREPROPS_API extern bool bIsActorEditor;
ECORE_API extern bool bIsLevelEditor;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    bIsActorEditor = true;
    bIsLevelEditor = false;
    if (strstr(GetCommandLine(), "-nosplash") == nullptr)
    {
        constexpr bool topmost = false;
        splash::show(topmost);
    }
    splash::update_progress(1);

    if (!IsDebuggerPresent())
        Debug._initialize(false);

    const char* FSName = "fs.ltx";
    {
        if (strstr(GetCommandLine(), "-soc_14") || strstr(GetCommandLine(), "-soc_10004"))
        {
            FSName = "fs_soc.ltx";
        }
        else if (strstr(GetCommandLine(), "-soc"))
        {
            FSName = "fs_soc.ltx";
        }
        else if (strstr(GetCommandLine(), "-cs"))
        {
            FSName = "fs_cs.ltx";
        }
    }
    splash::update_progress(5);
    Core._initialize("Actor_Editor", ELogCallback, 1, FSName, true);

    splash::update_progress(25);
    ATools = xr_new<CActorTools>();
    Tools  = ATools;

    splash::update_progress(9);
    UI = xr_new<CActorMain>();
    UI->RegisterCommands();

    splash::update_progress(15);
    UIMainForm* MainForm = xr_new<UIMainForm>();
    ::MainForm           = MainForm;
    UI->Push(MainForm, false);

    splash::update_progress(22);
    GameMaterialLibraryEditors->Load();
    splash::update_progress(22);

    splash::update_progress(1);
    while (MainForm->Frame()) {}

    GameMaterialLibraryEditors->Unload();
    xr_delete(MainForm);
    Core._destroy();
    splash::hide();
    return 0;
}
