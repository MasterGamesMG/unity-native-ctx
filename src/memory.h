#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <sys/uio.h>   // process_vm_readv (Android/Linux)

// ============================================================
// External memory reading (read-only, no injection)
// Requires same UID or root
// ============================================================

static pid_t g_pid = 0;

inline void mem_set_pid(pid_t pid) {
    g_pid = pid;
}

// Read T from address in target process
template<typename T>
inline T read(uintptr_t addr) {
    T value{};
    struct iovec local  = { &value,  sizeof(T) };
    struct iovec remote = { reinterpret_cast<void*>(addr), sizeof(T) };
    process_vm_readv(g_pid, &local, 1, &remote, 1, 0);
    return value;
}

// Read arbitrary byte buffer
inline bool read_buf(uintptr_t addr, void* out, size_t size) {
    struct iovec local  = { out, size };
    struct iovec remote = { reinterpret_cast<void*>(addr), size };
    return process_vm_readv(g_pid, &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

// Follow a pointer chain
// e.g. ptr_chain(base, {0x10, 0x28, 0x50})
inline uintptr_t ptr_chain(uintptr_t base, std::initializer_list<uintptr_t> offsets) {
    uintptr_t cur = base;
    for (uintptr_t off : offsets) {
        cur = read<uintptr_t>(cur + off);
        if (!cur) return 0;
    }
    return cur;
}

// Get base address of a module by reading /proc/pid/maps
inline uintptr_t get_module_base(pid_t pid, const char* module_name) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, module_name)) {
            base = static_cast<uintptr_t>(strtoull(line, nullptr, 16));
            break;
        }
    }
    fclose(f);
    return base;
}
