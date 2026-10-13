// Draws a menu screen into a frame (docs/plans/launcher/LAUNCHER_PLAN.md, Style), in the manner of
// the console's system menu: on the first screen a near-black haze with a ring of glowing points
// turning beside the list; on the screens below it a violet haze with a slowly turning fan of
// translucent bars around a dark sphere. The list has no bar: the chosen entry is bright cyan, the
// others grey; a screen's title is yellow; the bottom line names the buttons with their symbols.
// The in-game menu is shown on the first screen's dark haze alone, without the moving part.
// Everything is drawn by this code with a system font; no artwork of the console's menu is used.
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
#include <cmath>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include "host/display_frame.h"
#include "host/menu_model.h"

class MenuRenderer {
public:
    static constexpr unsigned visible_entries=7;
    ~MenuRenderer() {release();}
    MenuRenderer()=default;
    MenuRenderer(const MenuRenderer&)=delete;
    MenuRenderer& operator=(const MenuRenderer&)=delete;

    // `seconds` moves the background; `root` chooses the first screen's look; `prompt` replaces the
    // note (waiting for a key); `backdrop` is set for the in-game menu (its picture is not shown).
    DisplayFrame draw(const MenuScreen& screen,unsigned width,unsigned height,double seconds,bool root,const std::string& prompt={},
                      const DisplayFrame* backdrop=nullptr) {
        prepare(width,height);
        const bool over_game=backdrop && backdrop->width;
        if(over_game)std::copy(haze_[0].begin(),haze_[0].end(),bits_); // The in-game menu: the plain dark haze, the game hidden.
        else {
            std::copy(haze_[root?0:1].begin(),haze_[root?0:1].end(),bits_);
            if(root)ring(seconds);else fan(seconds);
        }
        SetBkMode(dc_,TRANSPARENT);
        const int w=int(width),h=int(height),unit=h/24;
        if(!root || over_game)clock(unit);
        // The list: beside the ring on the first screen, otherwise in the right half under the title.
        const unsigned count=unsigned(screen.entries.size());
        const unsigned first=count<=visible_entries?0:std::min(screen.highlighted>visible_entries/2?screen.highlighted-visible_entries/2:0u,count-visible_entries);
        const unsigned shown=std::min(count,visible_entries);
        const int row=unit*2-unit/3,left=root && !over_game?w*48/100:w*36/100,right=w*95/100;
        const int top=h/2-int(shown)*row/2+(root && !over_game?0:unit);
        text(screen.title,left,top-row*2,title_,root && !over_game?RGB(150,150,160):RGB(238,218,64));
        for(unsigned n=first;n<first+shown;++n) {
            const auto& entry=screen.entries[n];const int y=top+int(n-first)*row;
            const bool on=n==screen.highlighted;
            const double pulse=0.5+0.5*std::sin(seconds*4.0);
            const COLORREF colour=!entry.enabled?RGB(84,84,92):on?RGB(BYTE(96+40*pulse),BYTE(196+30*pulse),255):RGB(150,150,158);
            // The value keeps its place at the right; the label gets what is left and is cut short if it must be.
            const std::string value=entry.value?fit(entry.value(),entry_,(right-left)*6/10):std::string();
            const int value_width=value.empty()?0:extent(value,entry_)+unit;
            text(fit(entry.label,entry_,right-left-value_width),left,y,entry_,colour);
            if(!value.empty())textRight(value,right,y,entry_,!entry.enabled?RGB(84,84,92):on?RGB(236,236,240):RGB(120,120,128));
        }
        const int arrows=left-unit;
        if(first>0)triangle(arrows,top+unit/2,unit/3,true,RGB(90,150,255));
        if(first+shown<count)triangle(arrows,top+int(shown)*row-unit,unit/3,false,RGB(90,150,255));
        const std::string& note=!prompt.empty()?prompt:screen.entries[screen.highlighted].note;
        if(!note.empty())textCentred(fit(note,note_,w*92/100),w/2,h-unit*5,note_,!prompt.empty()?RGB(255,225,120):RGB(176,176,188));
        // The bottom line: a symbol and what it does.
        int x=w*34/100;const int y=h-unit*3;
        x=hint(Symbol::Cross,"Enter",x,y,unit);
        x=hint(Symbol::Circle,"Back",x,y,unit);
        hint(Symbol::Arrows,"Change",x,y,unit);
        DisplayFrame frame;frame.width=width;frame.height=height;frame.bgra.assign(bits_,bits_+std::size_t(width)*height);
        return frame;
    }

private:
    enum class Symbol {Cross,Circle,Arrows};
    HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;std::uint32_t* bits_=nullptr;unsigned width_=0,height_=0;
    HFONT title_=nullptr,entry_=nullptr,note_=nullptr;
    std::vector<std::uint32_t> haze_[2]; // The still part of the two backgrounds.

    void release() {
        for(HFONT font:{title_,entry_,note_})if(font)DeleteObject(font);
        if(dc_) {SelectObject(dc_,old_);DeleteDC(dc_);}
        if(bitmap_)DeleteObject(bitmap_);
        dc_=nullptr;bitmap_=nullptr;title_=entry_=note_=nullptr;bits_=nullptr;
    }
    void prepare(unsigned width,unsigned height) {
        if(width==width_ && height==height_ && dc_)return;
        release();
        width_=width;height_=height;
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=LONG(width);bi.bmiHeader.biHeight=-LONG(height);
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        dc_=CreateCompatibleDC(nullptr);void* bits=nullptr;
        bitmap_=CreateDIBSection(dc_,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!dc_ || !bitmap_)throw std::runtime_error("Menu drawing surface could not be created");
        bits_=static_cast<std::uint32_t*>(bits);old_=SelectObject(dc_,bitmap_);
        const auto font=[&](int size,int weight) {return CreateFontA(-size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Arial");};
        const int unit=int(height)/24;
        title_=font(unit*9/8,FW_SEMIBOLD);entry_=font(unit*9/8,FW_SEMIBOLD);note_=font(std::max(unit*3/4,11),FW_SEMIBOLD);
        // The hazes: darkest at the edges, a little lighter where the moving part sits, with fine grain.
        for(unsigned kind=0;kind<2;++kind) {
            haze_[kind].resize(std::size_t(width)*height);
            std::uint32_t seed=12345u+kind;
            const double cx=kind?0.30:0.30,cy=0.5;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                const double dx=(double(x)/width-cx)*double(width)/height,dy=double(y)/height-cy;
                const double glow=std::max(0.0,1.0-std::sqrt(dx*dx+dy*dy)*1.25);
                seed=seed*1664525u+1013904223u;const double grain=double((seed>>24)&7);
                double r,g,b;
                if(kind==0) {r=10+16*glow+grain;g=10+17*glow+grain;b=13+24*glow+grain;}
                else {r=30+52*glow+grain;g=22+40*glow+grain;b=58+96*glow+grain;}
                haze_[kind][std::size_t(y)*width+x]=(std::uint32_t(r)<<16)|(std::uint32_t(g)<<8)|std::uint32_t(b);
            }
        }
    }
    // Text with a soft dark edge below and to the right.
    void text(const std::string& s,int x,int y,HFONT font,COLORREF colour) {
        SelectObject(dc_,font);const int offset=std::max(int(height_)/400,1);
        SetTextColor(dc_,RGB(0,0,0));TextOutA(dc_,x+offset,y+offset,s.c_str(),int(s.size()));
        SetTextColor(dc_,colour);TextOutA(dc_,x,y,s.c_str(),int(s.size()));
    }
    int extent(const std::string& s,HFONT font) {SelectObject(dc_,font);SIZE size{};GetTextExtentPoint32A(dc_,s.c_str(),int(s.size()),&size);return size.cx;}
    // `s`, or its beginning with three dots, within `width` pixels.
    std::string fit(std::string s,HFONT font,int width) {
        if(extent(s,font)<=width)return s;
        while(s.size()>1 && extent(s+"...",font)>width)s.pop_back();
        return s+"...";
    }
    void textRight(const std::string& s,int right,int y,HFONT font,COLORREF colour) {text(s,right-extent(s,font),y,font,colour);}
    void textCentred(const std::string& s,int centre,int y,HFONT font,COLORREF colour) {text(s,centre-extent(s,font)/2,y,font,colour);}
    void clock(int unit) {
        const std::time_t now=std::time(nullptr);std::tm local{};localtime_s(&local,&now);
        char date[32],time[32];std::strftime(date,sizeof date,"%m/%d/%Y",&local);std::strftime(time,sizeof time,"%I:%M:%S %p",&local);
        text(date,int(width_)*7/100,unit,note_,RGB(200,200,214));
        textRight(time,int(width_)*93/100,unit,note_,RGB(200,200,214));
    }
    void triangle(int cx,int cy,int size,bool up,COLORREF colour) {
        const POINT points[3]={{cx-size,cy+(up?size:-size)},{cx+size,cy+(up?size:-size)},{cx,cy+(up?-size:size)}};
        const HBRUSH brush=CreateSolidBrush(colour);const HGDIOBJ old_brush=SelectObject(dc_,brush),old_pen=SelectObject(dc_,GetStockObject(NULL_PEN));
        Polygon(dc_,points,3);
        SelectObject(dc_,old_brush);SelectObject(dc_,old_pen);DeleteObject(brush);
    }
    // A button's symbol and its label; returns where the next one starts.
    int hint(Symbol symbol,const char* label,int x,int y,int unit) {
        const int size=unit*2/5,cy=y+unit/2;
        const COLORREF colour=symbol==Symbol::Cross?RGB(124,170,240):symbol==Symbol::Circle?RGB(255,96,96):RGB(200,200,214);
        const HPEN pen=CreatePen(PS_SOLID,std::max(unit/9,2),colour);const HGDIOBJ old_pen=SelectObject(dc_,pen),old_brush=SelectObject(dc_,GetStockObject(NULL_BRUSH));
        if(symbol==Symbol::Cross) {MoveToEx(dc_,x-size,cy-size,nullptr);LineTo(dc_,x+size,cy+size);MoveToEx(dc_,x+size,cy-size,nullptr);LineTo(dc_,x-size,cy+size);}
        else if(symbol==Symbol::Circle)Ellipse(dc_,x-size,cy-size,x+size,cy+size);
        SelectObject(dc_,old_pen);SelectObject(dc_,old_brush);DeleteObject(pen);
        if(symbol==Symbol::Arrows) {triangle(x-size/2,cy,size*2/3,true,colour);triangle(x+size,cy,size*2/3,false,colour);}
        text(label,x+unit,y-unit/8,note_,RGB(214,214,224));
        return x+unit*2+extent(label,note_)+unit;
    }
    static std::uint32_t mix(std::uint32_t under,std::uint32_t over,double alpha) {
        const auto channel=[&](unsigned shift) {
            const double a=double((under>>shift)&255),b=double((over>>shift)&255);
            return std::uint32_t(a+(b-a)*alpha)<<shift;
        };
        return channel(0)|channel(8)|channel(16);
    }
    // Light added to a pixel: red, green and blue amounts, each stopping at white.
    static std::uint32_t lit(std::uint32_t p,double r,double g,double b) {
        const auto channel=[&](unsigned shift,double add) {return std::uint32_t(std::min(255.0,double((p>>shift)&255)+add))<<shift;};
        return channel(16,r)|channel(8,g)|channel(0,b);
    }
    void glow(double cx,double cy,double radius,double strength) {
        const int x0=std::max(int(cx-radius),0),x1=std::min(int(cx+radius)+1,int(width_)),y0=std::max(int(cy-radius),0),y1=std::min(int(cy+radius)+1,int(height_));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
            const double d=std::hypot(x-cx,y-cy)/radius;
            if(d>=1.0)continue;
            const double fall=(1.0-d)*(1.0-d),core=d<0.22?1.0:0.0;
            auto& p=bits_[std::size_t(y)*width_+x];
            p=lit(p,strength*(150*fall+105*core),strength*(185*fall+70*core),strength*(255*fall));
        }
    }
    // The first screen: seven points of light on a tilted ring that turns slowly, the far ones dimmer.
    void ring(double seconds) {
        const double cx=width_*0.30,cy=height_*0.50,radius=height_*0.15,tilt=0.35;
        glow(cx,cy,radius*1.7,0.09); // The faint body the points circle.
        for(unsigned n=0;n<7;++n) {
            const double angle=seconds*0.35+n*(6.283185307/7.0)+0.30*std::sin(seconds*0.21+n);
            // A circle seen at a slant (narrower across than tall), leaning to one side.
            const double across=std::cos(angle)*radius*0.62,along=std::sin(angle)*radius;
            const double x=cx+across*std::cos(tilt)-along*std::sin(tilt),y=cy+across*std::sin(tilt)+along*std::cos(tilt);
            const double depth=std::cos(angle+0.9); // Towards the viewer when positive.
            glow(x,y,height_*(0.036+0.012*depth),0.66+0.34*depth);
        }
    }
    // The other screens: a fan of translucent bars turning around a dark sphere.
    void fan(double seconds) {
        const double cx=width_*0.30,cy=height_*0.50,inner=height_*0.085,outer=height_*0.62;
        const double turn=seconds*0.08,slice=6.283185307/12.0;
        const int x0=std::max(int(cx-outer),0),x1=std::min(int(cx+outer)+1,int(width_)),y0=std::max(int(cy-outer),0),y1=std::min(int(cy+outer)+1,int(height_));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
            const double dx=x-cx,dy=y-cy,r=std::sqrt(dx*dx+dy*dy);
            auto& p=bits_[std::size_t(y)*width_+x];
            if(r<inner) {
                // The sphere: dark, with a rim of light on the upper left.
                const double rim=std::max(0.0,(-dx-dy)/(inner*1.4))*(r/inner)*(r/inner);
                p=mix(p,0x00060612u,0.92);p=lit(p,70*rim,80*rim,130*rim);
                continue;
            }
            if(r>=outer)continue;
            double a=std::fmod(std::atan2(dy,dx)+turn+62.83185307,slice)/slice; // 0..1 across one bar and its gap.
            if(a>0.42)continue;
            const double edge=std::min(a,0.42-a)/0.21,fade=1.0-(r-inner)/(outer-inner);
            const double alpha=0.34*fade*std::min(1.0,edge*3.0);
            p=mix(p,0x004656D8u,alpha);
        }
    }
};
#endif
