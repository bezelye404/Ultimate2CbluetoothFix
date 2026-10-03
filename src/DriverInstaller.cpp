#include "DriverInstaller.h"
#include <urlmon.h>
#include <shellapi.h>
#include <thread>
#include <vector>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace Ultimate2CFixer {

namespace {

// MARK: - Download Progress Callback
class DownloadCallback : public IBindStatusCallback {
public:
    DownloadCallback(DriverProgressCb cb) : m_cb(std::move(cb)), m_ref(1) {}

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IBindStatusCallback) {
            *ppv = static_cast<IBindStatusCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_ref);
    }

    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG count = InterlockedDecrement(&m_ref);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    IFACEMETHODIMP OnStartBinding(DWORD, IBinding*) override { return S_OK; }
    IFACEMETHODIMP GetPriority(LONG*) override { return S_OK; }
    IFACEMETHODIMP OnLowResource(DWORD) override { return S_OK; }
    IFACEMETHODIMP OnStopBinding(HRESULT, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP GetBindInfo(DWORD* grfBINDF, BINDINFO* pbindinfo) override {
        if (!grfBINDF || !pbindinfo) return E_POINTER;
        *grfBINDF = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA;
        return S_OK;
    }
    IFACEMETHODIMP OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) override { return S_OK; }
    IFACEMETHODIMP OnObjectAvailable(REFIID, IUnknown*) override { return S_OK; }

    IFACEMETHODIMP OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG, LPCWSTR) override {
        if (m_cb && ulProgressMax > 0) {
            int pct = static_cast<int>((static_cast<double>(ulProgress) / ulProgressMax) * 100.0);
            if (pct > 100) pct = 100;
            m_cb(pct, L"");
        }
        return S_OK;
    }

private:
    DriverProgressCb m_cb;
    LONG m_ref;
};

} // namespace

// MARK: - Driver Detection
bool IsViGEmBusInstalled() {
    // 1. Registry service key check
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\ViGEmBus", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }

    // 2. Service Control Manager check
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, L"ViGEmBus", SERVICE_QUERY_STATUS);
        if (svc) {
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return true;
        }
        CloseServiceHandle(scm);
    }

    return false;
}

// MARK: - Asynchronous Downloader & Installer
void StartViGEmBusInstall(HWND hwnd, DriverProgressCb onProgress, DriverFinishedCb onFinished) {
    std::thread([hwnd, onProgress = std::move(onProgress), onFinished = std::move(onFinished)]() {
        const wchar_t* kDownloadUrl = L"https://github.com/nefarius/ViGEmBus/releases/download/v1.21.442.0/ViGEmBus_1.21.442_x64_x86_arm64.exe";

        wchar_t tempDir[MAX_PATH];
        DWORD tempLen = GetTempPathW(MAX_PATH, tempDir);
        if (tempLen == 0 || tempLen > MAX_PATH) {
            if (onFinished) onFinished(false, L"Could not resolve Windows temp directory.");
            return;
        }

        std::wstring destFile = std::wstring(tempDir) + L"ViGEmBus_Setup.exe";

        // Download with progress callback
        DownloadCallback* pCallback = new DownloadCallback(onProgress);
        pCallback->AddRef();

        if (onProgress) onProgress(0, L"downloading");
        HRESULT hr = URLDownloadToFileW(NULL, kDownloadUrl, destFile.c_str(), 0, pCallback);
        pCallback->Release();

        if (FAILED(hr)) {
            if (onFinished) onFinished(false, L"Download failed. Please check your internet connection.");
            return;
        }

        if (onProgress) onProgress(100, L"installing");

        // Run installer with UAC elevation
        SHELLEXECUTEINFOW shEx = {};
        shEx.cbSize = sizeof(SHELLEXECUTEINFOW);
        shEx.fMask = SEE_MASK_NOCLOSEPROCESS;
        shEx.hwnd = hwnd;
        shEx.lpVerb = L"runas";
        shEx.lpFile = destFile.c_str();
        shEx.lpParameters = L"";
        shEx.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&shEx) || !shEx.hProcess) {
            DeleteFileW(destFile.c_str());
            if (onFinished) onFinished(false, L"Setup cancelled or permission denied.");
            return;
        }

        // Wait for setup to finish
        WaitForSingleObject(shEx.hProcess, INFINITE);
        CloseHandle(shEx.hProcess);
        DeleteFileW(destFile.c_str());

        // Verify driver presence
        bool installed = IsViGEmBusInstalled();
        if (onFinished) {
            onFinished(installed, installed ? L"" : L"Driver not detected after installation.");
        }
    }).detach();
}

} // namespace Ultimate2CFixer
