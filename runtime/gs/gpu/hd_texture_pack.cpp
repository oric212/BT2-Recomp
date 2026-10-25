#include "gs/gpu/hd_texture_pack.h"
#include "gs/gpu/hd_texture_image.h"
#include "device.hpp"
#include "thread_id.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <stdexcept>

// `done`: loaded, or refused (`none`).
struct HdTexturePack::Entry {Image image;std::filesystem::path path;std::uint64_t bytes=0,used=0;bool none=false,done=false;};

namespace {
std::uint32_t u32(const std::vector<std::uint8_t>& d,std::size_t at) {std::uint32_t v;std::memcpy(&v,d.data()+at,4);return v;}
bool isDds(const std::filesystem::path& p) {
    auto e=p.extension().string();std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return char(std::tolower(c));});
    return e==".dds";
}
// The pack's names below `root`, in path order, so the first of a duplicated name is always the same one.
std::vector<std::filesystem::path> ddsFiles(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    if(!std::filesystem::is_directory(root))return files;
    for(const auto& e:std::filesystem::recursive_directory_iterator(root))if(e.is_regular_file() && isDds(e.path()))files.push_back(e.path());
    std::sort(files.begin(),files.end());
    return files;
}
}

std::optional<HdTexturePack::Dds> HdTexturePack::readDds(const std::vector<std::uint8_t>& file) {
    if(file.size()<128 || std::memcmp(file.data(),"DDS ",4))return std::nullopt;
    Dds dds;dds.height=u32(file,12);dds.width=u32(file,16);
    const std::uint32_t mips=std::max<std::uint32_t>(1,u32(file,28));
    const std::uint32_t fourcc=u32(file,84);
    std::size_t at=128;
    auto code=[](const char* c){std::uint32_t v;std::memcpy(&v,c,4);return v;};
    if(fourcc==code("DX10")) {
        if(file.size()<148)return std::nullopt;
        switch(u32(file,128)) { // DXGI format; sRGB variants are read as UNORM.
        case 71:case 72:dds.format=VK_FORMAT_BC1_RGBA_UNORM_BLOCK;dds.block_bytes=8;break;
        case 74:case 75:dds.format=VK_FORMAT_BC2_UNORM_BLOCK;dds.block_bytes=16;break;
        case 77:case 78:dds.format=VK_FORMAT_BC3_UNORM_BLOCK;dds.block_bytes=16;break;
        case 98:case 99:dds.format=VK_FORMAT_BC7_UNORM_BLOCK;dds.block_bytes=16;break;
        default:return std::nullopt;
        }
        at=148;
    } else if(fourcc==code("DXT1")) {dds.format=VK_FORMAT_BC1_RGBA_UNORM_BLOCK;dds.block_bytes=8;}
    else if(fourcc==code("DXT3")) {dds.format=VK_FORMAT_BC2_UNORM_BLOCK;dds.block_bytes=16;}
    else if(fourcc==code("DXT5")) {dds.format=VK_FORMAT_BC3_UNORM_BLOCK;dds.block_bytes=16;}
    else return std::nullopt;
    if(!dds.width || !dds.height || dds.width>16384 || dds.height>16384)return std::nullopt;
    for(std::uint32_t level=0,w=dds.width,h=dds.height;level<mips;++level,w=std::max(1u,w/2),h=std::max(1u,h/2)) {
        const std::size_t size=std::size_t((w+3)/4)*((h+3)/4)*dds.block_bytes;
        if(at+size>file.size())break; // A shorter mip chain than the header says: keep the levels present.
        dds.levels.emplace_back(file.begin()+std::ptrdiff_t(at),file.begin()+std::ptrdiff_t(at+size));at+=size;
    }
    if(dds.levels.empty())return std::nullopt;
    return dds;
}

std::optional<HdTexturePack::Key> HdTexturePack::key(const std::string& stem) {
    // Hexadecimal parts separated by '-': two (texture, format) or three (texture, palette, format).
    std::uint64_t parts[3]{};unsigned count=0;
    for(std::size_t at=0;;) {
        const auto end=std::min(stem.find('-',at),stem.size());
        if(count==3 || end==at || end-at>16)return std::nullopt; // More than three parts, or an empty or long one.
        std::uint64_t value=0;
        for(std::size_t i=at;i<end;++i) {
            const char c=stem[i];
            const int digit=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;
            if(digit<0)return std::nullopt;
            value=value<<4|std::uint64_t(digit);
        }
        parts[count++]=value;
        if(end==stem.size())break;
        at=end+1;
    }
    if(count<2 || parts[count-1]>0xFFFFFFFFull)return std::nullopt;
    Key k;k.texture=parts[0];k.format=std::uint32_t(parts[count-1])&~0x4000u;
    if(count==3) {k.palette=parts[1];k.has_palette=true;}
    return k;
}

HdTexturePack::HdTexturePack(Vulkan::Device& device,const std::filesystem::path& folder,std::uint64_t budget_bytes,bool playstation_buttons)
    :device_(device),budget_(budget_bytes) {
    const auto replacements=std::filesystem::is_directory(folder/"replacements")?folder/"replacements":folder;
    if(!std::filesystem::is_directory(replacements))throw std::runtime_error("HD textures: no pack folder at "+folder.string());
    for(const auto& file:ddsFiles(replacements))
        if(const auto k=key(file.stem().string()))if(!index_.emplace(*k,file).second)++duplicates;
    if(playstation_buttons)for(const auto& file:ddsFiles(folder/"Original Buttons"))if(const auto k=key(file.stem().string()))index_[*k]=file;
    for(const auto& named:index_)if(named.first.has_palette)variants_[Key{named.first.texture,0,named.first.format,false}].push_back(&named);
    loader_=std::thread([this]{run();});
}

HdTexturePack::~HdTexturePack() {
    {std::lock_guard<std::mutex> hold(lock_);stop_=true;}
    changed_.notify_all();
    loader_.join();
    for(auto& [name,entry]:cache_)delete entry;
}

HdTexturePack::Entry* HdTexturePack::entry(const std::filesystem::path* file) {
    if(const auto cached=cache_.find(file);cached!=cache_.end())return cached->second;
    auto* made=new Entry;made->path=*file;
    cache_.emplace(file,made);
    wanted_.push_back({file,false});
    changed_.notify_all();
    return made;
}

HdTexturePack::Found HdTexturePack::find(const Key& name) {
    std::unique_lock<std::mutex> hold(lock_);
    const std::filesystem::path* file=nullptr;
    if(const auto same=index_.find(name);same!=index_.end())file=&same->second;
    else if(name.has_palette)
        if(const auto every=index_.find(Key{name.texture,0,name.format,false});every!=index_.end())file=&every->second;
    if(!file) {++missing;return {};}
    const bool first=!cache_.contains(file);
    entry(file);
    // The first time, a little time for the loader (the file is small; most are ready for their first draw).
    if(first)changed_.wait_for(hold,std::chrono::duration<double>(first_wait_seconds),[&]{
        const auto it=cache_.find(file);return it==cache_.end() || it->second->done;
    });
    const auto it=cache_.find(file);
    if(it==cache_.end()) {++waiting;return {nullptr,true};} // Dropped for the budget meanwhile: asked for again next time.
    auto* e=it->second;
    if(e->none) {++missing;return {};}
    if(!e->image.handle) {++waiting;if(first)++late;return {nullptr,true};} // Drawn as the original until loaded.
    e->used=++tick_;++found;
    return {&e->image,false};
}

HdTexturePack::Choice HdTexturePack::closest(std::uint64_t texture,std::uint32_t format,const TextureColours& wanted) {
    std::lock_guard<std::mutex> hold(lock_);
    const auto list=variants_.find(Key{texture,0,format,false});
    if(list==variants_.end())return {};
    const Key* best=nullptr;float nearest=0;bool loading=false;
    for(const auto* variant:list->second) {
        auto& c=colours_[&variant->second];
        if(!c.done) {
            if(!c.asked) {c.asked=true;wanted_.push_back({&variant->second,true});changed_.notify_all();}
            loading=true;continue;
        }
        if(!c.colours)continue;
        if(const auto d=TextureColours::distance(wanted,*c.colours);d && (!best || *d<nearest)) {best=&variant->first;nearest=*d;}
    }
    if(loading)return {std::nullopt,true};
    if(!best)return {};
    if(nearest>variant_limit) {++variants_too_far;return {};}
    ++variants_chosen;
    return {*best,false};
}

void HdTexturePack::waitLoaded() {
    std::unique_lock<std::mutex> hold(lock_);
    changed_.wait(hold,[this]{return wanted_.empty() && !busy_;});
}

void HdTexturePack::load(Entry& entry) {
    std::ifstream in(entry.path,std::ios::binary);
    const std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    const auto dds=readDds(file);
    if(!dds)return;
    auto info=Vulkan::ImageCreateInfo::immutable_2d_image(dds->width,dds->height,VkFormat(dds->format));
    info.levels=unsigned(dds->levels.size());
    std::vector<Vulkan::ImageInitialData> initial;
    std::uint64_t bytes=0;
    for(const auto& level:dds->levels) {initial.push_back({level.data(),0,0});bytes+=level.size();}
    entry.image.handle=device_.create_image(info,initial.data());
    entry.bytes=bytes;
}

namespace {
// A variant's colours: from its smallest level of at least 16 x 16 pixels (enough blocks for the cells'
// averages; a 4 x 4 level is a single block's two colours), or its largest when it has none.
std::optional<TextureColours> variantColours(const HdTexturePack::Dds& dds) {
    const unsigned bc=dds.block_bytes==8?1:dds.format==VK_FORMAT_BC2_UNORM_BLOCK?2:dds.format==VK_FORMAT_BC3_UNORM_BLOCK?3:0;
    std::size_t chosen=0;unsigned width=dds.width,height=dds.height;
    for(std::size_t level=1,w=dds.width/2,h=dds.height/2;level<dds.levels.size() && w>=16 && h>=16;++level,w/=2,h/=2) {
        chosen=level;width=unsigned(w);height=unsigned(h);
    }
    return TextureColours::ofBlocks(bc,width,height,dds.levels[chosen]);
}
}

void HdTexturePack::run() {
    Util::register_thread_index(loader_thread_index);
    for(;;) {
        Entry* e=nullptr;
        {
            std::unique_lock<std::mutex> hold(lock_);
            changed_.wait(hold,[this]{return stop_ || !wanted_.empty();});
            if(stop_)return;
            const auto job=wanted_.front();wanted_.pop_front();
            if(job.colours) {
                busy_=true;hold.unlock();
                std::optional<TextureColours> colours;
                try {
                    std::ifstream in(*job.file,std::ios::binary);
                    const std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
                    if(const auto dds=readDds(file))colours=variantColours(*dds);
                } catch(...) {}
                hold.lock();
                auto& c=colours_[job.file];c.done=true;c.colours=colours;busy_=false;
                hold.unlock();changed_.notify_all();
                continue;
            }
            const auto it=cache_.find(job.file);
            if(it==cache_.end() || it->second->done)continue;
            e=it->second;busy_=true;
        }
        // Not under the lock: only this thread writes an entry that is not done, and nothing drops it.
        Image image;std::uint64_t bytes=0;bool none=false;
        {
            Entry loading;loading.path=e->path;
            try {load(loading);} catch(...) {}
            none=!loading.image.handle;image=std::move(loading.image);bytes=loading.bytes;
        }
        {
            std::lock_guard<std::mutex> hold(lock_);
            e->done=true;e->none=none;busy_=false;
            if(none)++refused;
            else {
                e->image=std::move(image);e->bytes=bytes;e->used=++tick_;++loaded;
                resident_bytes+=bytes;peak_bytes=std::max(peak_bytes,resident_bytes);
                if(resident_bytes>budget_)evict();
            }
        }
        changed_.notify_all();
    }
}

void HdTexturePack::evict() {
    // Least recently used first; one dropped is loaded again when it is drawn again.
    std::vector<std::pair<std::uint64_t,const std::filesystem::path*>> order;
    for(const auto& [file,entry]:cache_)if(entry->image.handle)order.push_back({entry->used,file});
    std::sort(order.begin(),order.end());
    for(const auto& [used,file]:order) {
        if(resident_bytes<=budget_)break;
        auto it=cache_.find(file);
        resident_bytes-=it->second->bytes;++evicted;
        delete it->second;cache_.erase(it);
    }
}
