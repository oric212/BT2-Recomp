#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include "display_frame.h"

// Minimal Win32 presentation window: shows the latest decoded frame in the client area, either at the
// largest integer scale that fits (centred, black borders; pixel-exact) or stretched to fill.
// Messages are pumped by the caller at each presentation; closing the window sets closed(). The
// window does not pace the run (host/frame_pacer.h does).
class ViewerWindow {
public:
    enum class Scaling {Integer,Stretch};
    explicit ViewerWindow(Scaling scaling=Scaling::Integer):scaling_(scaling) {}
    ~ViewerWindow() {if(hwnd_)DestroyWindow(hwnd_);}
    void present(const DisplayFrame& frame,unsigned flip) {
        if(closed_)return;
        frame_=frame;
        if(!hwnd_)create(frame.width,frame.height);
        SetWindowTextA(hwnd_,("BT2 Recomp - frame "+std::to_string(flip)+(frame.unwritten?" (unwritten pixels)":"")).c_str());
        InvalidateRect(hwnd_,nullptr,FALSE);UpdateWindow(hwnd_);
        pump();
    }
    void pump() {
        MSG msg;
        while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageA(&msg);}
    }
    void waitUntilClosed() {
        MSG msg;
        while(hwnd_ && !closed_ && GetMessageA(&msg,nullptr,0,0)>0) {TranslateMessage(&msg);DispatchMessageA(&msg);}
    }
    bool closed() const {return closed_;}
    bool focused() const {return hwnd_ && GetForegroundWindow()==hwnd_;} // Input is taken only then.
    bool opened() const {return hwnd_!=nullptr;}
private:
    Scaling scaling_;HWND hwnd_=nullptr;bool closed_=false;DisplayFrame frame_;
    void create(unsigned width,unsigned height) {
        WNDCLASSA wc{};wc.lpfnWndProc=&ViewerWindow::proc;wc.hInstance=GetModuleHandleA(nullptr);
        wc.lpszClassName="BT2RecompViewer";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        RegisterClassA(&wc);
        RECT r{0,0,LONG(width*2),LONG(height*2)};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        hwnd_=CreateWindowA(wc.lpszClassName,"BT2 Recomp",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
            r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,this);
        if(!hwnd_)throw std::runtime_error("Viewer window creation failed");
        SetWindowLongPtrA(hwnd_,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(this));
    }
    void paint(HDC dc) {
        RECT client;GetClientRect(hwnd_,&client);
        if(frame_.bgra.empty()) {FillRect(dc,&client,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));return;}
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=LONG(frame_.width);
        bi.bmiHeader.biHeight=-LONG(frame_.height);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        int x=0,y=0,w=client.right,h=client.bottom;
        if(scaling_==Scaling::Integer) {
            const int scale=std::max(1,int(std::min(client.right/LONG(frame_.width),client.bottom/LONG(frame_.height))));
            w=int(frame_.width)*scale;h=int(frame_.height)*scale;x=(client.right-w)/2;y=(client.bottom-h)/2;
            // Black only the borders around the frame: clearing the whole client area first made every
            // presented frame flash black briefly (flicker).
            const auto black=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            const RECT borders[4]={{0,0,client.right,y},{0,y+h,client.right,client.bottom},{0,y,x,y+h},{x+w,y,client.right,y+h}};
            for(const auto& border:borders)if(border.right>border.left && border.bottom>border.top)FillRect(dc,&border,black);
        }
        SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,x,y,w,h,0,0,int(frame_.width),int(frame_.height),frame_.bgra.data(),&bi,DIB_RGB_COLORS,SRCCOPY);
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<ViewerWindow*>(GetWindowLongPtrA(hwnd,GWLP_USERDATA));
        switch(message) {
        case WM_PAINT: {PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);if(self)self->paint(dc);EndPaint(hwnd,&ps);return 0;}
        case WM_ERASEBKGND: return 1;
        case WM_CLOSE: if(self)self->closed_=true;DestroyWindow(hwnd);return 0;
        case WM_DESTROY: if(self)self->hwnd_=nullptr;return 0;
        default: return DefWindowProcA(hwnd,message,w,l);
        }
    }
};
#endif
