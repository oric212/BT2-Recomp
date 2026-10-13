// The system's folder chooser, for the two places where the player points the program at a folder
// outside its own (disc images, a memory card folder). It is a mouse and keyboard dialog: the one
// part of the launcher a controller cannot operate.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <string>

// The folder the player chose, or empty when the dialog was cancelled.
inline std::string chooseFolder(HWND owner,const char* title) {
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    BROWSEINFOA info{};
    info.hwndOwner=owner;info.lpszTitle=title;info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    std::string chosen;
    if(const PIDLIST_ABSOLUTE item=SHBrowseForFolderA(&info)) {
        char path[MAX_PATH];
        if(SHGetPathFromIDListA(item,path))chosen=path;
        CoTaskMemFree(item);
    }
    if(SUCCEEDED(com))CoUninitialize();
    return chosen;
}
