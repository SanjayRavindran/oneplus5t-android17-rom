# LineageOS 24 / AOSP Patches

Apply these patches to a clean LineageOS 24 tree after copying `device/oneplus/dumpling`:

1. `system/memory/libmeminfo`:
   ```bash
   git -C system/memory/libmeminfo apply ../../patches/system_memory_libmeminfo/0001-sysmeminfo-skip-missing-gpu_mem-bpf-map.patch
   ```
   Guards `ReadPerProcessGpuMem()` and `ReadProcessGpuUsageKb()` with `access("/sys/fs/bpf/map_gpuMem_gpu_mem_total_map", F_OK)` so missing GPU BPF tracing never triggers `BpfMapRO::abortOnMismatch` `SIGABRT` in `system_server`.

2. `hardware/lineage/interfaces`:
   ```bash
   git -C hardware/lineage/interfaces apply ../../patches/hardware_lineage_interfaces/0001-light-support-rgb-status-multi_intensity-sysfs.patch
   ```
   Adds `"rgb:status"` and `"R G B"` `/sys/class/leds/rgb:status/multi_intensity` writes to `android.hardware.light-service.lineage`.

3. Local manifest (Mini GApps):
   Copy `patches/manifests/dumpling.xml` into `.repo/local_manifests/dumpling.xml` before `repo sync`.
