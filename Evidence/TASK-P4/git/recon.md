# TASK-P4 Recon

- P3 base: f3c56eebffb690c70f7a7699ee8ed78cda8df931; source worktree clean.
- P4: feat/triple-screen-p4-workflow-localization, C:/Users/wy331/Documents/UE5DroneControl-p4; clean immediately after checkout.
- Existing P1 log/data changes retained. Existing Backend processes retained (PIDs 24508 and 20468 at recon; not authorization to stop either).
- Protected local and remote refs captured in refs-before.txt. No merge, stash, reset, or deletion.
- Git checkout reported a preexisting non-pointer material asset; resulting worktree status was clean.
- Reuse installed vcpkg dependencies read-only. Backend build output is Backend/build-p4.
- UE baseline is an independent UE5.8 Development Editor build, two compile actions at a time.
- Required changes: Backend role matrix, revision-bound review and deployment snapshot; leased Map drafts with ACK lifecycle; split Command and Map widgets; UE localization target and shared preferences; independent video domain on existing transport.
- Retained: 1082 vs 1080; two legacy planner failures; low-altitude 3D occlusion; map network dependency; 6 GB VRAM risk; isolated P3 startup assertion; native axis-drag tooling limitation.
- No implementation has begun before baseline build and core automation.
