// Discover only an execution-observed missing entry; never assign a semantic name.
// @category BT2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

public class DiscoverBT2Target extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Supply one observed target address.");
        Address start = toAddr(Long.decode(args[0]));
        MemoryBlock block = currentProgram.getMemory().getBlock(start);
        if ((start.getOffset() & 3) != 0 || block == null || !block.isInitialized()
            || !block.isExecute() || !block.getName().equals(".text"))
            throw new IllegalArgumentException("Target must be aligned initialized executable .text.");
        Function containing = currentProgram.getFunctionManager().getFunctionContaining(start);
        if (containing != null && !containing.getEntryPoint().equals(start))
            throw new IllegalStateException("Target lies within another function; review overlap.");
        if (containing == null) {
            if (!disassemble(start)) throw new IllegalStateException("Target disassembly failed.");
            containing = createFunction(start, String.format("FUN_%08X", start.getOffset()));
            if (containing == null) throw new IllegalStateException("Function discovery failed.");
        }
        println("Observed target discovered provisionally: " + containing.getEntryPoint()
            + " body bytes=" + containing.getBody().getNumAddresses());
    }
}
