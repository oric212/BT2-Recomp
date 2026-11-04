#include "host/upnp_port.h"
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <natupnp.h>
#include <oleauto.h>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"oleaut32.lib")

namespace {
std::wstring wide(const std::string& text) {return std::wstring(text.begin(),text.end());} // Addresses: ASCII.
std::string narrow(const BSTR text) {std::string out;if(text)for(const wchar_t* c=text;*c;++c)out+=char(*c);return out;}
struct Bstr {
    BSTR value;
    explicit Bstr(const std::wstring& text):value(SysAllocString(text.c_str())) {}
    ~Bstr() {SysFreeString(value);}
};
template<class T> struct Com {
    T* p=nullptr;
    ~Com() {reset();}
    void reset() {if(p)p->Release();p=nullptr;}
    T** out() {return &p;}
};
}

UpnpPort::UpnpPort(std::uint16_t port):port_(port),thread_([this]{run();}) {}

UpnpPort::~UpnpPort() {
    {std::lock_guard guard(lock_);stop_=true;}
    stop_signal_.notify_all();
    thread_.join();
}

UpnpPort::State UpnpPort::state() const {std::lock_guard guard(lock_);return state_;}
std::string UpnpPort::externalAddress() const {std::lock_guard guard(lock_);return external_;}
std::string UpnpPort::failure() const {std::lock_guard guard(lock_);return failure_;}

std::string UpnpPort::localAddress() {
    // A UDP socket "connected" to an internet address sends nothing; it only picks the local address a packet would leave from.
    WSADATA data;
    if(WSAStartup(MAKEWORD(2,2),&data))return {};
    std::string address;
    const SOCKET s=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(s!=INVALID_SOCKET) {
        sockaddr_in to{};to.sin_family=AF_INET;to.sin_port=htons(53);inet_pton(AF_INET,"8.8.8.8",&to.sin_addr);
        sockaddr_in local{};int size=sizeof local;
        if(!::connect(s,reinterpret_cast<sockaddr*>(&to),sizeof to) && !getsockname(s,reinterpret_cast<sockaddr*>(&local),&size)) {
            char text[INET_ADDRSTRLEN]{};
            if(inet_ntop(AF_INET,&local.sin_addr,text,sizeof text) && std::string(text)!="0.0.0.0")address=text;
        }
        closesocket(s);
    }
    WSACleanup();
    return address;
}

void UpnpPort::run() {
    const bool com=SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    const auto fail=[&](const std::string& why) {std::lock_guard guard(lock_);state_=State::Failed;failure_=why;};
    Com<IUPnPNAT> nat;Com<IStaticPortMappingCollection> mappings;
    bool added=false;
    const std::string local=localAddress();
    if(!com)fail("Windows' UPnP did not start");
    else if(local.empty())fail("no network address");
    else if(FAILED(CoCreateInstance(__uuidof(UPnPNAT),nullptr,CLSCTX_ALL,__uuidof(IUPnPNAT),reinterpret_cast<void**>(nat.out()))) || !nat.p)
        fail("Windows' UPnP is missing");
    else if(FAILED(nat.p->get_StaticPortMappingCollection(mappings.out())) || !mappings.p)
        fail("no answer from the router");
    else {
        Com<IStaticPortMapping> mapping;
        const Bstr udp(L"UDP"),client(wide(local)),description(L"BT2 Recomp online battle");
        if(FAILED(mappings.p->Add(port_,udp.value,port_,client.value,VARIANT_TRUE,description.value,mapping.out())) || !mapping.p)
            fail("the router refused (another PC may use the port)");
        else {
            added=true;
            BSTR external=nullptr;
            const bool known=SUCCEEDED(mapping.p->get_ExternalIPAddress(&external)) && external;
            std::lock_guard guard(lock_);
            state_=State::Opened;external_=known?narrow(external):std::string();
            if(external)SysFreeString(external);
        }
    }
    {
        std::unique_lock guard(lock_);
        stop_signal_.wait(guard,[this]{return stop_;});
    }
    if(added) {const Bstr udp(L"UDP");mappings.p->Remove(port_,udp.value);}
    mappings.reset();nat.reset(); // Released before COM ends on this thread.
    if(com)CoUninitialize();
}
