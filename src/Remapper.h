#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include "../include/ViGEm/Client.h"

namespace BitDoFixer {

enum class RemapperStatus {
    Searching,
    Connected,
    Disconnected,
    Stopped
};

using LogCallback = std::function<void(const std::wstring&)>;
using StatusCallback = std::function<void(RemapperStatus, const std::wstring&)>;

class Remapper {
public:
    Remapper();
    ~Remapper();

    bool Start(HWND hwnd, LogCallback logCb, StatusCallback statusCb);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

private:
    void WorkerLoop(HWND hwnd);
    bool InitViGEm();
    void UninitViGEm();

    static SHORT NormalizeAxis(LONG v);
    static SHORT ApplyDeadzone(SHORT v);
    static SHORT NegateAxis(SHORT v);

    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
    LogCallback m_logCallback;
    StatusCallback m_statusCallback;

    HMODULE m_hViGEmDll{nullptr};
    PVIGEM_CLIENT m_vigemClient{nullptr};
    PVIGEM_TARGET m_vigemTarget{nullptr};

    // ViGEm function pointers
    typedef PVIGEM_CLIENT (*pfn_vigem_alloc)();
    typedef void (*pfn_vigem_free)(PVIGEM_CLIENT);
    typedef VIGEM_ERROR (*pfn_vigem_connect)(PVIGEM_CLIENT);
    typedef void (*pfn_vigem_disconnect)(PVIGEM_CLIENT);
    typedef PVIGEM_TARGET (*pfn_vigem_target_x360_alloc)();
    typedef void (*pfn_vigem_target_free)(PVIGEM_TARGET);
    typedef VIGEM_ERROR (*pfn_vigem_target_add)(PVIGEM_CLIENT, PVIGEM_TARGET);
    typedef VIGEM_ERROR (*pfn_vigem_target_remove)(PVIGEM_CLIENT, PVIGEM_TARGET);
    typedef VIGEM_ERROR (*pfn_vigem_target_x360_update)(PVIGEM_CLIENT, PVIGEM_TARGET, XUSB_REPORT);

    pfn_vigem_alloc m_fn_alloc{nullptr};
    pfn_vigem_free m_fn_free{nullptr};
    pfn_vigem_connect m_fn_connect{nullptr};
    pfn_vigem_disconnect m_fn_disconnect{nullptr};
    pfn_vigem_target_x360_alloc m_fn_target_alloc{nullptr};
    pfn_vigem_target_free m_fn_target_free{nullptr};
    pfn_vigem_target_add m_fn_target_add{nullptr};
    pfn_vigem_target_remove m_fn_target_remove{nullptr};
    pfn_vigem_target_x360_update m_fn_target_update{nullptr};
};

} // namespace BitDoFixer
