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
#include <utility>
#include "display_frame.h"

// Minimal Win32 presentation window: shows the latest decoded frame in the client area, either at the
// largest integer scale that fits (pixel-exact) or at the largest size of the picture's proportions
// that fits; centred, with black borders.
// Messages are pumped by the caller at each presentation; closing the window sets closed(). The
// window does not pace the run (host/frame_pacer.h does). The window's size is a multiple of the game's
// picture (640 x 448) or the whole screen without a border; both, the scaling and a short status
// text in the corner (the frame rate display) can change while it is open.
class ViewerWindow {
public:
    enum class Scaling {Integer,Stretch};
    explicit ViewerWindow(Scaling scaling=Scaling::Integer):scaling_(scaling) {}
    ~ViewerWindow() {if(hwnd_)DestroyWindow(hwnd_);}
    void present(const DisplayFrame& frame,unsigned flip) {
        present(frame,"BT2 Recomp - frame "+std::to_string(flip)+(frame.unwritten?" (unwritten pixels)":""));
    }
    void present(const DisplayFrame& frame,const std::string& title) {
        if(closed_)return;
        frame_=frame;
        if(!hwnd_)create();
        if(title!=title_) {title_=title;SetWindowTextA(hwnd_,title.c_str());}
        InvalidateRect(hwnd_,nullptr,FALSE);UpdateWindow(hwnd_);
        pump();
    }
    void setScaling(Scaling scaling) {scaling_=scaling;}
    void setStatus(std::string text) {status_=std::move(text);} // Drawn in the top left corner; empty for none.
    // The largest multiple of 640 x 448 that fits the screen the window is on (the main screen
    // before it exists).
    unsigned largestScale() const {
        MONITORINFO monitor{};monitor.cbSize=sizeof monitor;
        LONG width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
        if(hwnd_ && GetMonitorInfoA(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&monitor)) {width=monitor.rcMonitor.right-monitor.rcMonitor.left;height=monitor.rcMonitor.bottom-monitor.rcMonitor.top;}
        return unsigned(std::max<LONG>(1,std::min(width/640,height/448)));
    }
    // The window's size as a multiple of 640 x 448, or the whole screen without a border.
    void setLayout(unsigned scale,bool fullscreen) {
        if(scale==scale_ && fullscreen==fullscreen_)return;
        scale_=scale;fullscreen_=fullscreen;
        if(hwnd_)applyLayout();
    }
    // The client area in pixels (640 x 448 times the scale before the window exists).
    std::pair<unsigned,unsigned> clientSize() const {
        RECT client{0,0,LONG(640*scale_),LONG(448*scale_)};
        if(hwnd_)GetClientRect(hwnd_,&client);
        return {unsigned(std::max<LONG>(client.right,1)),unsigned(std::max<LONG>(client.bottom,1))};
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
    HWND handle() const {return hwnd_;} // For dialogs that belong to the window.
private:
    Scaling scaling_;HWND hwnd_=nullptr;bool closed_=false;DisplayFrame frame_;
    unsigned scale_=2;bool fullscreen_=false;std::string title_,status_;
    void applyLayout() {
        if(fullscreen_) {
            MONITORINFO monitor{};monitor.cbSize=sizeof monitor;GetMonitorInfoA(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&monitor);
            SetWindowLongPtrA(hwnd_,GWL_STYLE,WS_POPUP|WS_VISIBLE);
            SetWindowPos(hwnd_,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,
                monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_FRAMECHANGED);
        } else {
            RECT r{0,0,LONG(640*scale_),LONG(448*scale_)};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
            SetWindowLongPtrA(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);
            RECT at;GetWindowRect(hwnd_,&at);
            SetWindowPos(hwnd_,HWND_TOP,std::max<LONG>(at.left,0),std::max<LONG>(at.top,0),r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED);
        }
    }
    void create() {
        SetProcessDPIAware(); // One picture pixel is a whole number of screen pixels; the system does not rescale the window.
        WNDCLASSA wc{};wc.lpfnWndProc=&ViewerWindow::proc;wc.hInstance=GetModuleHandleA(nullptr);
        wc.lpszClassName="BT2RecompViewer";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        RegisterClassA(&wc);
        RECT r{0,0,LONG(640*scale_),LONG(448*scale_)};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        hwnd_=CreateWindowA(wc.lpszClassName,"BT2 Recomp",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
            r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,this);
        if(!hwnd_)throw std::runtime_error("Viewer window creation failed");
        SetWindowLongPtrA(hwnd_,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(this));
        if(fullscreen_)applyLayout();
    }
    void paint(HDC dc) {
        RECT client;GetClientRect(hwnd_,&client);
        if(frame_.bgra.empty()) {FillRect(dc,&client,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));return;}
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=LONG(frame_.width);
        bi.bmiHeader.biHeight=-LONG(frame_.height);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        // The picture keeps its shape in every window: whole multiples of its size, or the largest
        // size of the same proportions that fits; the rest of the window is black.
        int w,h;
        if(scaling_==Scaling::Integer) {
            const int scale=std::max(1,int(std::min(client.right/LONG(frame_.width),client.bottom/LONG(frame_.height))));
            w=int(frame_.width)*scale;h=int(frame_.height)*scale;
        } else if(std::int64_t(client.right)*frame_.height<=std::int64_t(client.bottom)*frame_.width) {
            w=std::max(1,int(client.right));h=std::max(1,int(std::int64_t(client.right)*frame_.height/frame_.width));
        } else {
            h=std::max(1,int(client.bottom));w=std::max(1,int(std::int64_t(client.bottom)*frame_.width/frame_.height));
        }
        const int x=(client.right-w)/2,y=(client.bottom-h)/2;
        // Black only the borders around the frame: clearing the whole client area first made every
        // presented frame flash black briefly (flicker).
        const auto black=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        const RECT borders[4]={{0,0,client.right,y},{0,y+h,client.right,client.bottom},{0,y,x,y+h},{x+w,y,client.right,y+h}};
        for(const auto& border:borders)if(border.right>border.left && border.bottom>border.top)FillRect(dc,&border,black);
        SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,x,y,w,h,0,0,int(frame_.width),int(frame_.height),frame_.bgra.data(),&bi,DIB_RGB_COLORS,SRCCOPY);
        if(!status_.empty()) {
            SetBkMode(dc,OPAQUE);SetBkColor(dc,RGB(0,0,0));SetTextColor(dc,RGB(255,255,0));
            TextOutA(dc,x+6,y+4,status_.c_str(),int(status_.size()));
        }
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
