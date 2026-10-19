This reverts my merged pull requests so that the same changes can come back as one-subject PRs that can be reviewed on their own: #401 (598b700), #380 (57b1925) and #376 (a016e4d, which also carried the 15 commits of #370). Each is one `git revert -m 1`, in reverse merge order.

Kept on top: `fix(core): restore Common::Barrier until main's std::barrier compiles` (5ac2c38 from #370), because without it main does not build - #373 left `std::unique_ptr<std::barrier>` without its template argument and `CpuManager` on `Sync()`.

Not touched: #377 (build fixes) and #378 (README). One conflict in `src/core/tools/renderdoc.cpp` between #377's `[[maybe_unused]]` and the pre-#376 `ASSERT`; resolved by keeping both.

How to check it: `git diff --stat ca06073 <this branch>` lists only #377's files, README.md and the three Barrier files. ca06073 is main before any of my PRs merged.

The re-landing PRs follow, each based on the previous one, in this order:

1. `pr/01-android-max-clock` - fix(android): stop the force max clock thread from spinning
2. `pr/02-zpass-counter-cpu` - perf(vulkan): keep the Z-pass counter on the CPU on Adreno, Mali and Turnip
3. `pr/03-thor-second-screen` - feat(android): stats panel on the AYN Thor's second screen
4. `pr/04-texture-table-lookups` - fix(shader_recompiler): follow 16-bit texture table lookups
5. `pr/05-buffer-cache-page-faults` - perf(buffer_cache): stop busy regions faulting page by page
6. `pr/06-maxwell3d-unchanged-writes` - perf(maxwell3d): do not mark state dirty on writes that change nothing
7. `pr/07-android-worker-wake` - perf(vulkan): wake the worker thread less often on Android
8. `pr/08-device-memory-table` - fix(core): do not make the 2 GB device memory table resident at startup
9. `pr/09-dynarmic-arm64-cache` - fix(dynarmic): keep compiled blocks cached on arm64 release builds
10. `pr/10-android-updates-dlc` - feat(android): load updates and DLC that sit beside the game
11. `pr/11-submit-finished-passes` - perf(vulkan): submit finished render passes while the frame is still being built
12. `pr/12-puller-semaphored` - fix(puller): SEMAPHORED release fence, RELEASE_SIZE and failed acquires
13. `pr/13-remove-unused` - chore: remove files and includes nothing uses
14. `pr/14-doom-eternal` - fix: DOOM Eternal storage buffer slots and failed compute pipelines
15. `pr/15-uma-uploads` - perf(buffer_cache): write uploads straight into unified memory buffers
16. `pr/16-vma-stop-token` - chore(common): stop the VMA sampler thread with a stop token
17. `pr/17-driver-guest-memory` - feat(core): allocate guest memory from the GPU driver on Android
18. `pr/18-flat-page-tables` - refactor(video_core): flat page tables in the buffer cache and the GPU memory manager
19. `pr/19-cxx20-stdlib` - chore(common): use the standard library where C++20 has it
20. `pr/20-dynarmic-in-tree` - build(dynarmic): bring Dynarmic e40fa8b into the tree
21. `pr/21-cxx23` - refactor: C++23, plain assert, std::unreachable
22. `pr/22-flat-ir` - perf: index-linked IR in Dynarmic and the shader recompiler
23. `pr/23-host-fixes` - fix: host memory mappings, instruction cache, NCE aborts, host1x and nvnflinger
24. `pr/24-vulkan-clears` - fix(vulkan): colour clears as raw words, by area and per attachment slot
25. `pr/25-engine-fixes` - fix: engine fixes in maxwell3d, dma, fermi2d, the software blitter, the puller and kepler_compute
26. `pr/26-vulkan-barriers` - fix(vulkan): barrier and ordering fixes
27. `pr/27-vulkan-pipeline-state` - fix(vulkan): pipeline and state fixes
28. `pr/28-shader-fixes` - fix(shader): shader recompiler and descriptor fixes
29. `pr/29-geometry-passthrough` - fix(vulkan): lowered passthrough geometry shaders
30. `pr/30-texture-cache-copies` - fix(texture_cache): copy extents in mips, blocks and source texels
31. `pr/31-buffer-cache-fixes` - fix(buffer_cache): unbacked pages, in-pass uniform streaming and per-submission usage
32. `pr/32-core-timing` - perf(core_timing): wake the timing thread only for a new earliest event
33. `pr/33-turnip-tiled-perf` - perf(vulkan): tiled GPU and Turnip fixes - border colour budget, BGR/RGB images, image view format lists
34. `pr/34-submit-perf` - perf: WAIT_FOR_IDLE as a deferred barrier, GPU paths for plain remapped copies
35. `pr/35-scarletnx-launch` - feat(qt): Run with AOT (experimental) through ScarletNX on Windows
