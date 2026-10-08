#include "providers.h"

// Kernel DMA (dmacman): the SIO2 memory-card DMA channels 11/12 with the absent-card device profile.

void install_iop_dma_providers(IopRuntime& rt) {
    const auto owned_card_dma=[](const IopRuntime& memory) {
        const auto server=memory.rpc_servers.find(0x80000400);
        return server!=memory.rpc_servers.end() && memory.rpc_queues.contains(server->second.queue)
            && memory.current_thread && memory.threads.contains(memory.current_thread)
            && memory.threads.at(memory.current_thread).entry==0x1039C
            && memory.threads.at(memory.current_thread).priority==16 && !memory.in_interrupt;
    };
    rt.providers[{"dmacman",28}]=[owned_card_dma](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4),source=IopRuntime::physical(c.get(5));
        memory.check(IopRuntime::physical(c.get(29))+16,4);
        const auto direction=memory.read(IopRuntime::physical(c.get(29))+16,4);
        const auto found=memory.dma_channels.find(channel);
        if(!owned_card_dma(memory) || c.pc!=0x10E28 || (channel!=11 && channel!=12) || source!=(channel==11?0x32020u:0x32650u) || c.get(6)!=0x24 || c.get(7)!=1 || direction!=(channel==11?1u:0u)
            || found==memory.dma_channels.end() || !found->second.enabled_known || !found->second.enabled
            || !found->second.priority_known || found->second.priority!=3 || (found->second.slice_known && (found->second.chcr&0x1000000)))
            throw std::runtime_error("IOP slice DMA outside first MCSERV/SIO2 profile");
        memory.check(source,0x90);
        auto& dma=found->second;dma.madr=source;dma.bcr=0x10024;dma.chcr=direction?0x201:0x40000200;dma.slice_known=true;
        return 1u; // Programmed slice only; no START, bytes, completion or IRQ supplied.
    };
    rt.providers[{"dmacman",32}]=[owned_card_dma](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4);const auto found=memory.dma_channels.find(channel);
        if(!owned_card_dma(memory) || c.pc!=0x10E30 || (channel!=11 && channel!=12) || found==memory.dma_channels.end()
            || !found->second.slice_known || !found->second.enabled || found->second.bcr!=0x10024)
            throw std::runtime_error("SIO2 DMA START outside owned card slice");
        auto& dma=found->second;
        if(channel==11) {
            if(dma.madr!=0x32020 || dma.chcr!=0x201 || !memory.sio2_transmit.empty())
                throw std::runtime_error("SIO2 card send state differs");
            memory.check(dma.madr,144);
            const auto payload=std::vector<std::uint8_t>(memory.ram.begin()+dma.madr,memory.ram.begin()+dma.madr+144);
            auto expected=std::vector<std::uint8_t>(144);expected[0]=0x81;expected[1]=payload[1];
            const bool probe=payload[1]==0x11 && memory.sio2_parameters.at(0x1F808200)==0x100472;
            const bool auth=payload[1]==0xF3 && memory.sio2_parameters.at(0x1F808200)==0x140572;
            if(!memory.sio2_card_absent || payload!=expected || (!probe && !auth)
                || (probe && memory.cardSends()>=5) || (auth && memory.cardSends()!=5)
                || memory.sio2_card_sends!=memory.sio2_card_receives
                || memory.sio2_parameters.at(0x1F808204)!=0
                || memory.sio2_parameters.at(0x1F808250)!=0xFF020405 || memory.sio2_parameters.at(0x1F808254)!=0x5FFFF)
                throw std::runtime_error("SIO2 card probe packet/configuration differs");
            ++memory.sio2_card_sends;memory.sio2_transmit=payload;
            memory.sio2_receive.assign(144,0);std::fill_n(memory.sio2_receive.begin(),6,0xFF); // Explicit absent-card device profile, not a stored retail result.
            memory.sio2_receive_cursor=0;memory.sio2_command_status=0x1D100;
        } else {
            if(dma.madr!=0x32650 || dma.chcr!=0x40000200 || memory.sio2_transmit.size()!=144
                || memory.sio2_receive.size()!=144 || memory.sio2_receive_cursor
                || memory.sio2_card_sends!=memory.sio2_card_receives+1)
                throw std::runtime_error("SIO2 card receive state differs");
            memory.check(dma.madr,144);
            std::copy(memory.sio2_receive.begin(),memory.sio2_receive.end(),memory.ram.begin()+dma.madr);
            memory.sio2_receive_cursor=144;++memory.sio2_card_receives;
        }
        dma.madr+=144;dma.chcr|=0x1000000;
        return c.get(2); // START is void; completion/IRQ is a separate SIO2 control transition.
    };
    rt.providers[{"dmacman",33}]=[](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4);
        if(channel!=11 && channel!=12)throw std::runtime_error("Unsupported IOP DMA channel");
        auto& dma=memory.dma_channels[channel];dma.priority=c.get(5)&7;dma.priority_known=true;
        return c.get(2); // Public API is void; no success result is supplied.
    };
    rt.providers[{"dmacman",34}]=[](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4);
        if(channel!=11 && channel!=12)throw std::runtime_error("Unsupported IOP DMA channel");
        auto& dma=memory.dma_channels[channel];dma.enabled=true;dma.enabled_known=true;return c.get(2);
    };
}
