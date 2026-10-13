// The system's folder chooser, for the two places where the player points the program at a folder
// outside its own (disc images, a memory card folder). It is a mouse and keyboard dialog: the one
// part of the launcher a controller cannot operate.
#pragma once
#include "host/utf8.h"
#include <shlobj.h>

// The folder the player chose as UTF-8, or empty when the dialog was cancelled.
inline std::string chooseFolder(HWND owner,const wchar_t* title) {
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    BROWSEINFOW info{};
    info.hwndOwner=owner;info.lpszTitle=title;info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    std::string chosen;
    if(const PIDLIST_ABSOLUTE item=SHBrowseForFolderW(&info)) {
        wchar_t path[MAX_PATH];
        if(SHGetPathFromIDListW(item,path))chosen=utf8(std::filesystem::path(path));
        CoTaskMemFree(item);
    }
    if(SUCCEEDED(com))CoUninitialize();
    return chosen;
}
