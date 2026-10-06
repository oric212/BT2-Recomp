# EE sound worker dispatch and reverse RPC ownership

## Confirmed next scheduling scope

The immediate native frontier is the StartThread helper's ReferThreadStatus for newly created native ID 4, EE PC 0x32EB68. Its neutral entry is 0x2EE050, stack 0x34E490 / size 0x1000, GP 0x38BDF0 and initial priority 2. The original matching worker has ID **6**, not 4; captures at 0x2EE70C / 0x2EE720 return 6. Its saved GP/SP/FP/RA are [0x38BDF0, 0x34F470, 0x34F470, 0x100250]. Native ID allocation is not assumed equal to retail. Original captures also establish the following worker entry 0x2EE140, stack 0x34F490 / size 0x2000, priority 2, original ID 7 and RA 0x100250. Tool labels worker4/worker5 denote the next native sequence, not observed retail IDs.

The existing generic creation contract constructs the first owned dormant record; its captured SetupThread exit is retained. Ready transitions and execution for these new workers are not implemented. Trace why original earlier thread allocation reached IDs 4/5 while the native sequence has not, and establish semantic identity before extending ID-dependent behavior.

After second CRI setup and generated main-thread initialization, the main thread queries its status and requests ChangeThreadPriority(1, 24) from 0x2EEA2C. Independent original before/after captures show configuration priority 0x18 and return 2 (previous priority). The native main priority is 2; ready EE thread 3 has priority 5, entry 0x315380, GP 0x38BDF0, SP 0x3A6CA0 and owned exit 0x100250. Acknowledging the priority change without dispatching that higher-priority ready child would omit required scheduling.

Original captures already contain queue 0x3A6D00, thread owner 3, server descriptor 0x3A6D18, service 0x80000704, handler 0x3153F0 and buffer 0x3A6CC0. The native SDRDRV helper is waiting for that reverse bind. Generated child code calls SifInitRpc(0), GetThreadId, queue/server registration and tails into 0x333060. The current synchronous EE-to-IOP adapter has never executed this ready child, so original asynchronous startup ordering is not established natively.

## Bounded implementation required

1. Consult REFERENCE_ARCHITECTURE.md, EE_THREAD_START_PLAN.md and SDRDRV_RPC_PLAN.md. Keep original game entries and reference scheduler patterns separate.
2. Capture the original child registration and first actual sleep/wait boundary, including GP/SP/return context, queue state and reverse-bind delivery. Determine whether the helper needs an EE callback before the child can park. Existing live RAM proves registration fields, not a complete child scheduling/callback lifecycle.
3. Define a retained main invocation and a separate owned child CPU. Preserve main registers, stack, call-return validation and requested priority until the child reaches its evidenced wait. Do not replay the child's entry on later resumes or replace blocking code with a successful return.
4. Execute the original generated queue/server/loop path, keep waiting-thread ownership and establish the pending IOP-to-EE bind response using registered EE state. Trace subsequent actual callbacks before admitting them. Capture and compare context preservation and return to the main continuation.
5. Follow the newly reached related blockers through the bounded subsystem, then test, update progress, commit and push. Archive payload loading, presentation and audio consumption remain separate claims.

The current pass completes the consumed main status fields and reaches the new worker query before the later priority-change request. Implementing the handoff without first-wait and cross-CPU ownership evidence would be speculative. The earlier thread-start plan establishes ready state; this scope adds actual worker execution and reverse RPC service lifetime.
