#include "providers.h"

// Kernel library services: stdio printf, sysmem Kprintf, secrman callback setters, sysclib
// memory/string helpers and modload KELF path callback.

void install_iop_system_providers(IopRuntime& rt) {
    rt.providers[{"stdio",4}]=[](IopRuntime& memory,IopContext& c) {
        auto string=[&](std::uint32_t p) {
            std::string value;
            for(unsigned i=0;i<1024;++i) {const auto ch=memory.read(p+i,1);if(!ch)return value;value.push_back(static_cast<char>(ch));}
            throw std::runtime_error("Unterminated IOP printf string");
        };
        const auto format=string(c.get(4));std::string output;unsigned argument=5;
        for(unsigned i=0;i<format.size();++i) {
            if(format[i]!='%')output.push_back(format[i]);
            else {
                if(++i==format.size())throw std::runtime_error("Incomplete IOP printf conversion");
                if(format[i]=='%')output.push_back('%');
                else if(format[i]=='s' && argument<=7)output+=string(c.get(argument++));
                else if(format[i]=='d' && argument<=7)output+=std::to_string(iop_signed(c.get(argument++)));
                else throw std::runtime_error("Unsupported IOP printf conversion");
            }
            if(output.size()>4096)throw std::runtime_error("IOP printf output exceeds diagnostic bound");
        }
        memory.stdio_messages.push_back(output);return static_cast<std::uint32_t>(output.size());
    };

    rt.providers[{"sysmem",14}]=[](IopRuntime& memory,IopContext& c) {
        // No Kprintf callback is installed: SDK returns zero. Retain reached literal diagnostics.
        std::string message;
        for(unsigned i=0;i<1024;++i) {
            const auto ch=memory.read(c.get(4)+i,1);
            if(!ch) {memory.kernel_messages.push_back(message);return 0u;}
            if(ch=='%')throw std::runtime_error("Formatted IOP Kprintf unsupported");
            message.push_back(static_cast<char>(ch));
        }
        throw std::runtime_error("Unterminated IOP Kprintf message");
    };
    for(unsigned ordinal:{4u,5u})rt.providers[{"secrman",ordinal}]=[ordinal](IopRuntime& memory,IopContext& c) {
        const auto handler=c.get(4);
        if(handler) {if(handler%4)throw std::runtime_error("Unaligned native security callback");memory.check(IopRuntime::physical(handler),4);}
        memory.security_handlers[ordinal]=handler;if(handler)memory.entry_targets.insert(handler);
        return c.get(2); // Callback setter is void; authentication remains unsupported.
    };

    rt.providers[{"sysclib",11}]=[](IopRuntime& memory,IopContext& c) {
        const auto count=c.get(6),a=IopRuntime::physical(c.get(4)),b=IopRuntime::physical(c.get(5));
        if(count) {memory.check(a,count);memory.check(b,count);}
        for(std::uint32_t n=0;n<count;++n)if(memory.ram[a+n]!=memory.ram[b+n])
            return static_cast<std::uint32_t>(int(memory.ram[a+n])-int(memory.ram[b+n]));
        return 0u;
    };
    rt.providers[{"sysclib",12}]=[](IopRuntime& memory,IopContext& c) {
        const auto destination=c.get(4),source=c.get(5),count=c.get(6);
        const auto d=IopRuntime::physical(destination),s=IopRuntime::physical(source);
        if(count) {
            memory.check(d,count);memory.check(s,count);
            if(d<s+count && s<d+count)throw std::runtime_error("Overlapping IOP memcpy unsupported");
            std::memcpy(memory.ram.data()+d,memory.ram.data()+s,count);
        }
        return destination;
    };

    rt.providers[{"sysclib",17}]=[](IopRuntime& memory,IopContext& c) {
        const auto p=c.get(4),size=c.get(5);memory.check(IopRuntime::physical(p),size);
        for(std::uint32_t i=0;i<size;++i)memory.write(p+i,0,1);
        return c.get(2); // bzero is void.
    };
    rt.providers[{"sysclib",14}]=[](IopRuntime& memory,IopContext& c) {
        const auto destination=c.get(4),count=c.get(6);const auto p=IopRuntime::physical(destination);
        if(count) {memory.check(p,count);std::fill_n(memory.ram.begin()+p,count,static_cast<std::uint8_t>(c.get(5)));}
        return destination;
    };

    rt.providers[{"modload",13}]=[](IopRuntime& memory,IopContext& c) {
        const auto handler=c.get(4);
        if(handler) {if(handler%4)throw std::runtime_error("Unaligned KELF path callback");memory.check(IopRuntime::physical(handler),4);}
        memory.kelf_path_handler=handler;if(handler)memory.entry_targets.insert(handler);return c.get(2);
    };

    rt.providers[{"sysclib",29}]=[](IopRuntime& memory,IopContext& c) {
        const auto count=c.get(6);
        if(count>IopRuntime::ram_size)throw std::runtime_error("IOP string comparison budget exceeded");
        for(unsigned n=0;n<count;++n) {
            const auto a=memory.read(c.get(4)+n,1),b=memory.read(c.get(5)+n,1);
            if(a!=b)return a-b;if(!a)return 0u;
        }
        return 0u;
    };
}
