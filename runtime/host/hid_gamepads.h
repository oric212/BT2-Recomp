// Gamepads read directly over HID (Win32, USB or Bluetooth, no wrapper needed): the DualSense, the
// DualShock 4 (playstation_report.h) and generic HID gamepads such as 8BitDo pads in D-input or Switch
// mode (generic_gamepad.h). Xbox pads are XInput's and are left out here: XInput devices (an "IG_" path)
// and Microsoft's pads over Bluetooth (vendor 045E), so no pad counts twice.
// A scanning thread looks for new pads every second; each pad is read on its own thread, because HID
// reports arrive when the controller sends them. Pads are listed in the order they were found.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
}
#include "host/generic_gamepad.h"
#include "host/playstation_report.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <vector>

// One open pad and the thread reading it; the thread ends when the pad is gone or at shutdown.
class HidPadReader {
public:
    enum class Kind {DualSense,DualShock4,Generic};
    // `device` is open; the reader owns it and its preparsed data.
    HidPadReader(HANDLE device,Kind kind,bool bluetooth,unsigned report_bytes,PHIDP_PREPARSED_DATA parsed,GenericLayout layout)
        :device_(device),kind_(kind),bluetooth_(bluetooth),report_bytes_(report_bytes),parsed_(parsed),layout_(layout),
         values_(kind==Kind::Generic && parsed?findValues(parsed):Values{}),thread_([this]{run();}) {}
    HidPadReader(const HidPadReader&)=delete;
    HidPadReader& operator=(const HidPadReader&)=delete;
    ~HidPadReader() {
        stop_=true;CancelIoEx(device_,nullptr);
        thread_.join();
        CloseHandle(device_);
        if(parsed_)HidD_FreePreparsedData(parsed_);
    }
    bool finished() const {return finished_;}
    const char* name() const {return kind_==Kind::DualSense?"DualSense":kind_==Kind::DualShock4?"DualShock 4":"HID gamepad";}
    GamepadState state() {std::lock_guard lock(mutex_);return state_;}
    // The buttons held in any report since the last call (each report arrives on this reader's thread).
    std::uint32_t takeSeen() {std::lock_guard lock(mutex_);const auto seen=seen_|state_.buttons;seen_=0;return seen;}
    // BT2_PAD_TIMING=1 (a diagnostic): how old the newest report is when the game reads it, and whether reports waited
    // in Windows' queue before the reader took them (each read that returns at once was waiting).
    struct Timing {unsigned reports=0,waited=0;double longest_gap_ms=0;};
    Timing timing() {std::lock_guard lock(mutex_);return timing_;}
    std::chrono::steady_clock::time_point received() {std::lock_guard lock(mutex_);return received_;}

private:
    // The axes and the hat a generic pad declares, with their ranges.
    struct ValueRange {bool present=false;long min=0,max=0;unsigned bits=0;};
    static constexpr USAGE first_axis=0x30; // X, Y, Z, Rx, Ry, Rz, then ..., hat switch 0x39.
    using Values=std::array<ValueRange,10>;
    static Values findValues(PHIDP_PREPARSED_DATA parsed) {
        Values values{};
        HIDP_CAPS caps{};if(HidP_GetCaps(parsed,&caps)!=HIDP_STATUS_SUCCESS || !caps.NumberInputValueCaps)return values;
        std::vector<HIDP_VALUE_CAPS> list(caps.NumberInputValueCaps);USHORT count=caps.NumberInputValueCaps;
        if(HidP_GetValueCaps(HidP_Input,list.data(),&count,parsed)!=HIDP_STATUS_SUCCESS)return values;
        for(USHORT n=0;n<count;++n) {
            const auto& v=list[n];
            if(v.UsagePage!=0x01)continue;
            const USAGE low=v.IsRange?v.Range.UsageMin:v.NotRange.Usage,high=v.IsRange?v.Range.UsageMax:v.NotRange.Usage;
            for(unsigned u=std::max<unsigned>(low,first_axis);u<=high && u<first_axis+values.size();++u) {
                auto& range=values[u-first_axis];
                if(range.present)continue;
                range.present=true;range.bits=v.BitSize;range.min=v.LogicalMin;range.max=v.LogicalMax;
                // A range written with a sign it should not have (an 8-bit maximum of 255 read as -1): unsigned over the bits.
                if(range.max<range.min) {range.min=0;range.max=range.bits>=31?0x7FFFFFFF:long((1ul<<range.bits)-1);}
            }
        }
        return values;
    }
    std::optional<GamepadState> parseGeneric(std::span<std::uint8_t> report) {
        GenericReport out;
        std::array<USAGE,64> usages{};ULONG count=ULONG(usages.size());
        const auto status=HidP_GetUsages(HidP_Input,0x09,0,usages.data(),&count,parsed_,reinterpret_cast<PCHAR>(report.data()),ULONG(report.size()));
        if(status==HIDP_STATUS_INCOMPATIBLE_REPORT_ID)return std::nullopt; // Another report of the pad's (not its input state).
        if(status==HIDP_STATUS_SUCCESS)for(ULONG n=0;n<count;++n)if(usages[n]>=1 && usages[n]<=32)out.buttons|=1u<<(usages[n]-1);
        const auto value=[&](USAGE usage)->std::optional<GenericValue> {
            const auto& range=values_[usage-first_axis];
            if(!range.present)return std::nullopt;
            ULONG raw=0;
            if(HidP_GetUsageValue(HidP_Input,0x01,0,usage,&raw,parsed_,reinterpret_cast<PCHAR>(report.data()),ULONG(report.size()))!=HIDP_STATUS_SUCCESS)return std::nullopt;
            long v=long(raw);
            if(range.min<0 && range.bits>0 && range.bits<32 && raw>>(range.bits-1)&1)v=long(raw|~((1ul<<range.bits)-1)); // Signed.
            return GenericValue{v,range.min,range.max};
        };
        out.x=value(0x30);out.y=value(0x31);out.z=value(0x32);out.rx=value(0x33);out.ry=value(0x34);out.rz=value(0x35);out.hat=value(0x39);
        return decodeGenericReport(layout_,out);
    }
    std::optional<GamepadState> parse(std::span<std::uint8_t> report) {
        switch(kind_) {
        case Kind::DualSense:return parseDualSenseReport(report,bluetooth_);
        case Kind::DualShock4:return parseDualShock4Report(report);
        default:return parseGeneric(report);
        }
    }
    void run() {
        std::vector<std::uint8_t> report(report_bytes_);
        auto last=std::chrono::steady_clock::now();
        while(!stop_) {
            DWORD read=0;
            const auto asked=std::chrono::steady_clock::now();
            if(!ReadFile(device_,report.data(),DWORD(report.size()),&read,nullptr))break; // Unplugged, or cancelled at shutdown.
            const auto now=std::chrono::steady_clock::now();
            if(const auto parsed=parse(std::span<std::uint8_t>(report.data(),read))) {
                std::lock_guard lock(mutex_);state_=*parsed;received_=now;seen_|=parsed->buttons;
                ++timing_.reports;
                if(now-asked<std::chrono::microseconds(500))++timing_.waited;
                timing_.longest_gap_ms=std::max(timing_.longest_gap_ms,std::chrono::duration<double,std::milli>(now-last).count());
                last=now;
            }
        }
        {std::lock_guard lock(mutex_);state_={};}
        finished_=true;
    }
    HANDLE device_;Kind kind_;bool bluetooth_;unsigned report_bytes_;PHIDP_PREPARSED_DATA parsed_;GenericLayout layout_;
    Values values_;
    std::mutex mutex_;GamepadState state_;std::uint32_t seen_=0;
    std::chrono::steady_clock::time_point received_{};Timing timing_;
    std::atomic<bool> stop_{false},finished_{false};
    std::thread thread_; // Last member: it starts in the constructor and uses the others.
};

class HidGamepads {
public:
    HidGamepads():thread_([this]{scan();}) {}
    HidGamepads(const HidGamepads&)=delete;
    HidGamepads& operator=(const HidGamepads&)=delete;
    ~HidGamepads() {
        stop_=true;thread_.join();
        std::lock_guard lock(mutex_);pads_.clear();
    }
    // An open pad; it is connected once its reader has had a report.
    struct Pad {std::shared_ptr<HidPadReader> reader;};
    std::vector<Pad> connected() {
        std::lock_guard lock(mutex_);
        std::vector<Pad> out;
        for(const auto& p:pads_)if(!p.reader->finished())out.push_back(p);
        return out;
    }
    // Every reader's name and timing so far (BT2_PAD_TIMING).
    std::vector<Pad> all() {std::lock_guard lock(mutex_);return {pads_.begin(),pads_.end()};}

private:
    struct Entry : Pad {std::string path;};
    void scan() {
        while(!stop_) {
            {
                // Pads that are gone are let go, so they are found again when they come back.
                std::lock_guard lock(mutex_);
                pads_.remove_if([this](const Entry& e){if(!e.reader->finished())return false;gone_.push_back(e.reader);return true;});
            }
            gone_.clear(); // Joins their threads (already ended) outside the lock.
            find();
            for(unsigned n=0;n<10 && !stop_;++n)std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Look again in a second.
        }
    }
    void find() {
        GUID hid;HidD_GetHidGuid(&hid);
        const HDEVINFO set=SetupDiGetClassDevsA(&hid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
        if(set==INVALID_HANDLE_VALUE)return;
        SP_DEVICE_INTERFACE_DATA item{};item.cbSize=sizeof item;
        std::vector<std::string> paths;
        for(DWORD n=0;!stop_ && SetupDiEnumDeviceInterfaces(set,nullptr,&hid,n,&item);++n) {
            DWORD needed=0;SetupDiGetDeviceInterfaceDetailA(set,&item,nullptr,0,&needed,nullptr);
            std::vector<char> storage(needed);
            auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(storage.data());detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
            if(!needed || !SetupDiGetDeviceInterfaceDetailA(set,&item,detail,needed,nullptr,nullptr))continue;
            paths.push_back(upperCase(detail->DevicePath));
        }
        SetupDiDestroyDeviceInfoList(set);
        // Keyboards that also declare a gamepad (a Keychron keyboard does) are not pads: their USB IDs, from the
        // keyboard interface's path ("...\KBD").
        std::set<std::string> keyboards;
        for(const auto& path:paths)if(path.ends_with("\\KBD") && !usbId(path).empty())keyboards.insert(usbId(path));
        for(const auto& path:paths) {
            if(stop_ || skipped_.contains(path))continue;
            {
                std::lock_guard lock(mutex_);
                if(std::any_of(pads_.begin(),pads_.end(),[&](const Entry& e){return e.path==path;}))continue;
            }
            if(keyboards.contains(usbId(path))) {skipped_.insert(path);continue;}
            open(path);
        }
    }
    static std::string upperCase(std::string text) {for(auto& c:text)c=char(std::toupper(static_cast<unsigned char>(c)));return text;}
    // "VID_xxxx&PID_yyyy" in a USB device's path; empty for others.
    static std::string usbId(const std::string& path) {
        const auto at=path.find("VID_");
        return at==std::string::npos || path.size()<at+17?std::string():path.substr(at,17);
    }
    // Opens a pad and starts its reader; a device that is not a pad is not opened again. `path` is in upper case.
    void open(const std::string& path) {
        if(path.find("IG_")!=std::string::npos) {skipped_.insert(path);return;} // An XInput pad.
        HANDLE device=CreateFileA(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        if(device==INVALID_HANDLE_VALUE)device=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        if(device==INVALID_HANDLE_VALUE) {
            if(GetLastError()==ERROR_ACCESS_DENIED)skipped_.insert(path); // Windows keeps keyboards and mice to itself.
            return;
        }
        HIDD_ATTRIBUTES attributes{};attributes.Size=sizeof attributes;
        PHIDP_PREPARSED_DATA parsed=nullptr;HIDP_CAPS caps{};
        if(!HidD_GetAttributes(device,&attributes) || !HidD_GetPreparsedData(device,&parsed)) {CloseHandle(device);return;}
        if(HidP_GetCaps(parsed,&caps)!=HIDP_STATUS_SUCCESS) {HidD_FreePreparsedData(parsed);CloseHandle(device);return;}
        const auto vendor=attributes.VendorID,product=attributes.ProductID;
        const bool dualsense=vendor==0x054C && (product==0x0CE6 || product==0x0DF2);           // The DualSense and the Edge.
        const bool dualshock4=vendor==0x054C && (product==0x05C4 || product==0x09CC || product==0x0BA0); // Two models, the wireless adapter.
        const bool gamepad=caps.UsagePage==0x01 && (caps.Usage==0x04 || caps.Usage==0x05);   // A joystick or a gamepad.
        HidPadReader::Kind kind=HidPadReader::Kind::Generic;
        if(dualsense)kind=HidPadReader::Kind::DualSense;
        else if(dualshock4)kind=HidPadReader::Kind::DualShock4;
        else if(!gamepad || vendor==0x045E) {HidD_FreePreparsedData(parsed);CloseHandle(device);skipped_.insert(path);return;}
        const unsigned report_bytes=caps.InputReportByteLength;
        if(kind!=HidPadReader::Kind::Generic && report_bytes<10) {HidD_FreePreparsedData(parsed);CloseHandle(device);skipped_.insert(path);return;}
        const bool bluetooth=report_bytes>64;
        // Over Bluetooth Sony's controllers send their full report only after a feature report was read: the
        // DualSense's 0x05, the DualShock 4's 0x02 (its calibration).
        if(bluetooth && kind!=HidPadReader::Kind::Generic) {
            std::vector<std::uint8_t> feature(caps.FeatureReportByteLength?caps.FeatureReportByteLength:64);
            feature[0]=kind==HidPadReader::Kind::DualSense?0x05:0x02;
            HidD_GetFeature(device,feature.data(),ULONG(feature.size()));
        }
        if(kind!=HidPadReader::Kind::Generic) {HidD_FreePreparsedData(parsed);parsed=nullptr;}
        auto reader=std::make_shared<HidPadReader>(device,kind,bluetooth,report_bytes,parsed,genericLayout(vendor,product));
        std::lock_guard lock(mutex_);
        Entry e;e.reader=std::move(reader);e.path=path;
        pads_.push_back(std::move(e));
    }
    std::mutex mutex_;std::list<Entry> pads_;
    std::set<std::string> skipped_;                  // Scanning thread only.
    std::vector<std::shared_ptr<HidPadReader>> gone_; // Scanning thread only.
    std::atomic<bool> stop_{false};
    std::thread thread_; // Last member: it starts in the constructor and uses the others.
};
#endif
