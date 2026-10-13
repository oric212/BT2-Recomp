// A memory card as a host folder (docs/plans/memory-card/MEMORY_CARD_PLAN.md): one sub-folder per
// save with the save's files, and `entries.txt` in it for what the host file system cannot keep
// (each entry's mode and its two time stamps in the card's own 8-byte form; "." is the folder).
// This is the file layer the native card service answers from: open, close, seek, read and listing,
// with the card library's result codes. Paths are the card's ("/SAVE/file", or relative to the
// current directory); nothing outside the folder can be named. Writing is not implemented yet.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <vector>

class CardFolder {
public:
    static constexpr int no_entry=-4,denied=-5,too_many_open=-7,max_open=3;
    static constexpr std::uint16_t file_mode=0x8497,folder_mode=0x8427;
    struct Entry {
        std::string name;std::uint16_t mode=0;std::uint32_t size=0;
        std::array<std::uint8_t,8> created{},modified{};
    };

    explicit CardFolder(std::filesystem::path root):root_(std::move(root)) {}

    // The file's descriptor (0 to 2), or a negative result code. Only reading (flag bit 0 alone).
    int open(const std::string& name,unsigned flags) {
        if(flags!=1)return denied;
        const auto path=resolve(name);
        std::error_code error;
        if(path.empty() || !std::filesystem::is_regular_file(path,error) || hidden(path))return no_entry;
        for(int fd=0;fd<max_open;++fd)if(!open_[fd].used) {
            std::ifstream in(path,std::ios::binary);
            open_[fd].data.assign(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
            open_[fd].used=true;open_[fd].position=0;
            return fd;
        }
        return too_many_open;
    }
    int close(int fd) {
        if(!valid(fd))return no_entry;
        open_[fd]={};return 0;
    }
    // The new position, or a negative result code. Origin: 0 from the start, 1 from here, 2 from the end.
    int seek(int fd,std::int32_t offset,unsigned origin) {
        if(!valid(fd) || origin>2)return no_entry;
        auto& f=open_[fd];
        const std::int64_t target=(origin==0?0:origin==1?std::int64_t(f.position):std::int64_t(f.data.size()))+offset;
        if(target<0 || target>std::int64_t(f.data.size()))return denied;
        f.position=std::size_t(target);return int(target);
    }
    // Bytes read into `out` (fewer at the end of the file), or a negative result code.
    int read(int fd,std::span<std::uint8_t> out) {
        if(!valid(fd))return no_entry;
        auto& f=open_[fd];
        const std::size_t count=std::min(out.size(),f.data.size()-f.position);
        std::copy_n(f.data.begin()+std::ptrdiff_t(f.position),count,out.begin());
        f.position+=count;return int(count);
    }
    // The entries a path names: one file or folder, or with "*" as its last part everything in that
    // folder ("." and ".." first below the top, as a card lists them).
    std::vector<Entry> list(const std::string& pattern) {
        std::vector<Entry> found;
        const bool all=pattern.ends_with("/*") || pattern=="*";
        const auto path=resolve(all?pattern.substr(0,pattern.size()-1):pattern);
        std::error_code error;
        if(path.empty())return found;
        if(!all) {
            if(std::filesystem::exists(path,error) && !hidden(path))found.push_back(entry(path));
            return found;
        }
        if(!std::filesystem::is_directory(path,error))return found;
        if(!std::filesystem::equivalent(path,root_,error)) {
            Entry self=entry(path);self.name=".";found.push_back(self);
            Entry parent;parent.name="..";parent.mode=folder_mode;found.push_back(parent);
        }
        std::vector<std::filesystem::path> children;
        for(std::filesystem::directory_iterator it(path,error),end;!error && it!=end;it.increment(error))if(!hidden(it->path()))children.push_back(it->path());
        std::sort(children.begin(),children.end());
        for(const auto& child:children)found.push_back(entry(child));
        return found;
    }
    // Makes `name` the current directory; 0, or a negative result code.
    int changeDirectory(const std::string& name) {
        const auto path=resolve(name);
        std::error_code error;
        if(path.empty() || !std::filesystem::is_directory(path,error))return no_entry;
        current_=std::filesystem::relative(path,root_,error).generic_string();
        if(current_==".")current_.clear();
        return 0;
    }
    std::string currentDirectory() const {return "/"+current_;}

private:
    struct Open {bool used=false;std::vector<std::uint8_t> data;std::size_t position=0;};
    std::filesystem::path root_;std::string current_;std::array<Open,max_open> open_{};

    bool valid(int fd) const {return fd>=0 && fd<max_open && open_[fd].used;}
    static bool hidden(const std::filesystem::path& path) {return path.filename()=="entries.txt";}
    // The host path of a card path, or empty when it would leave the card or uses characters a card name cannot have.
    std::filesystem::path resolve(const std::string& name) const {
        std::vector<std::string> parts;
        if(!name.starts_with("/")) {std::istringstream current(current_);for(std::string part;std::getline(current,part,'/');)if(!part.empty())parts.push_back(part);}
        std::istringstream in(name);
        for(std::string part;std::getline(in,part,'/');) {
            if(part.empty() || part==".")continue;
            if(part=="..") {if(parts.empty())return {};parts.pop_back();continue;}
            if(part.find_first_of("\\:*?\"<>|")!=std::string::npos)return {};
            parts.push_back(part);
        }
        auto path=root_;
        for(const auto& part:parts)path/=part;
        return path;
    }
    Entry entry(const std::filesystem::path& path) const {
        std::error_code error;
        Entry e;e.name=path.filename().string();
        const bool folder=std::filesystem::is_directory(path,error);
        e.mode=folder?folder_mode:file_mode;
        e.size=folder?0u:std::uint32_t(std::filesystem::file_size(path,error));
        // Mode and time stamps from the save folder's entries.txt, where it has the entry.
        std::ifstream meta((folder?path:path.parent_path())/"entries.txt");
        const std::string wanted=folder?".":e.name;
        for(std::string line;std::getline(meta,line);) {
            std::istringstream fields(line);std::string name,mode,created,modified;
            if(!(fields>>name>>mode>>created>>modified) || name!=wanted || mode.size()!=4 || created.size()!=16 || modified.size()!=16)continue;
            try {
                const auto value=std::uint16_t(std::stoul(mode,nullptr,16));
                if(value)e.mode=value;
                for(unsigned n=0;n<8;++n) {
                    e.created[n]=std::uint8_t(std::stoul(created.substr(n*2,2),nullptr,16));
                    e.modified[n]=std::uint8_t(std::stoul(modified.substr(n*2,2),nullptr,16));
                }
            } catch(const std::exception&) {} // A damaged line: the defaults stay.
            break;
        }
        return e;
    }
};
