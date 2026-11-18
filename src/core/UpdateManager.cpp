#include "UpdateManager.h"
#include "Logger.h"

#include <Windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

namespace PixelMotion {

UpdateManager& UpdateManager::GetInstance() {
    static UpdateManager instance;
    return instance;
}

UpdateManager::UpdateManager()
    : m_state(UpdateState::Idle)
    , m_downloadProgress(0.0f)
    , m_downloadedBytes(0)
    , m_totalBytes(0)
    , m_currentVersion("v1.3.3")
    , m_isBackgroundCheck(false)
{
}

UpdateManager::~UpdateManager() {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void UpdateManager::ResetStatus() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = UpdateState::Idle;
    m_errorMessage.clear();
    m_downloadProgress = 0.0f;
    m_downloadedBytes = 0;
    m_totalBytes = 0;
}

std::string UpdateManager::GetLatestVersion() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_latestRelease.tagName;
}

std::string UpdateManager::GetReleaseNotes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_latestRelease.releaseNotes;
}

std::string UpdateManager::GetErrorMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorMessage;
}

bool UpdateManager::IsNewerVersion(const std::string& remote, const std::string& current) {
    auto parseVersion = [](std::string str) -> std::tuple<int, int, int> {
        if (!str.empty() && (str[0] == 'v' || str[0] == 'V')) {
            str = str.substr(1);
        }
        int maj = 0, min = 0, patch = 0;
        sscanf_s(str.c_str(), "%d.%d.%d", &maj, &min, &patch);
        return { maj, min, patch };
    };

    auto [rMaj, rMin, rPatch] = parseVersion(remote);
    auto [cMaj, cMin, cPatch] = parseVersion(current);

    if (rMaj > cMaj) return true;
    if (rMaj == cMaj && rMin > cMin) return true;
    if (rMaj == cMaj && rMin == cMin && rPatch > cPatch) return true;

    return false;
}

void UpdateManager::CheckForUpdates(bool isBackground) {
    if (m_state == UpdateState::Checking || m_state == UpdateState::Downloading) {
        return; // Already working
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_isBackgroundCheck = isBackground;
    m_state = UpdateState::Checking;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage.clear();
    }

    m_workerThread = std::thread(&UpdateManager::CheckThread, this);
}

void UpdateManager::CheckThread() {
    Logger::Info("Checking for Pixel Motion updates on GitHub...");

    std::string jsonStr;
    if (!FetchReleaseJson(jsonStr)) {
        if (!m_isBackgroundCheck) {
            m_state = UpdateState::Error;
        } else {
            m_state = UpdateState::Idle;
        }
        return;
    }

    try {
        auto j = json::parse(jsonStr);
        std::string tag = j.value("tag_name", "");
        std::string name = j.value("name", "");
        std::string notes = j.value("body", "");

        if (tag.empty()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_errorMessage = "Invalid release info returned from GitHub";
            m_state = UpdateState::Error;
            return;
        }

        std::string downloadUrl = "";
        size_t assetSize = 0;

        if (j.contains("assets") && j["assets"].is_array()) {
            // First pass: look for exact canonical "PixelMotion-Setup.exe"
            for (const auto& asset : j["assets"]) {
                std::string assetName = asset.value("name", "");
                if (assetName == "PixelMotion-Setup.exe") {
                    downloadUrl = asset.value("browser_download_url", "");
                    assetSize = asset.value("size", 0);
                    break;
                }
            }

            // Second pass: look for versioned "PixelMotion-Setup-*.exe"
            if (downloadUrl.empty()) {
                for (const auto& asset : j["assets"]) {
                    std::string assetName = asset.value("name", "");
                    if (assetName.rfind("PixelMotion-Setup", 0) == 0 && assetName.ends_with(".exe")) {
                        downloadUrl = asset.value("browser_download_url", "");
                        assetSize = asset.value("size", 0);
                        break;
                    }
                }
            }
        }

        // Fallback: look for any .exe installer asset
        if (downloadUrl.empty() && j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset : j["assets"]) {
                std::string assetName = asset.value("name", "");
                if (assetName.ends_with(".exe")) {
                    downloadUrl = asset.value("browser_download_url", "");
                    assetSize = asset.value("size", 0);
                    break;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_latestRelease.tagName = tag;
            m_latestRelease.releaseName = name;
            m_latestRelease.releaseNotes = notes;
            m_latestRelease.downloadUrl = downloadUrl;
            m_latestRelease.assetSize = assetSize;
        }

        Logger::Info("Latest GitHub release: " + tag + ", Current version: " + m_currentVersion);

        if (IsNewerVersion(tag, m_currentVersion)) {
            Logger::Info("Update available: " + tag);
            m_state = UpdateState::UpdateAvailable;
        } else {
            Logger::Info("Pixel Motion is up to date");
            m_state = UpdateState::UpToDate;
        }

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = std::string("Failed to parse release response: ") + e.what();
        m_state = UpdateState::Error;
        Logger::Error("Failed to parse GitHub JSON: " + std::string(e.what()));
    }
}

void UpdateManager::StartDownload() {
    if (m_state != UpdateState::UpdateAvailable) {
        return;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_state = UpdateState::Downloading;
    m_downloadProgress = 0.0f;
    m_downloadedBytes = 0;
    m_totalBytes = m_latestRelease.assetSize;

    m_workerThread = std::thread(&UpdateManager::DownloadThread, this);
}

void UpdateManager::DownloadThread() {
    std::string url;
    std::string tag;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        url = m_latestRelease.downloadUrl;
        tag = m_latestRelease.tagName;
    }

    if (url.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = "No installer executable found in the latest release assets.";
        m_state = UpdateState::Error;
        return;
    }

    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "PixelMotion-Setup.exe";

    Logger::Info("Downloading installer from: " + url + " to: " + tempPath.string());

    if (DownloadFile(url, tempPath)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_downloadedFilePath = tempPath;
        m_downloadProgress = 1.0f;
        m_state = UpdateState::ReadyToInstall;
        Logger::Info("Download complete! Ready to install: " + tempPath.string());
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_errorMessage.empty()) {
            m_errorMessage = "Failed to download update package.";
        }
        m_state = UpdateState::Error;
        Logger::Error("Installer download failed.");
    }
}

bool UpdateManager::LaunchInstaller() {
    std::filesystem::path path;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        path = m_downloadedFilePath;
    }

    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }

    Logger::Info("Launching silent update installer: " + path.string());

    // Run silently, preserve previous user options, suppress prompts, force-close old app, and relaunch
    LPCWSTR parameters = L"/SILENT /SUPPRESSMSGBOXES /FORCECLOSEAPPLICATIONS /NORESTART";

    HINSTANCE hInst = ShellExecuteW(
        nullptr,
        L"open",
        path.c_str(),
        parameters,
        nullptr,
        SW_SHOWNORMAL
    );

    if (reinterpret_cast<uintptr_t>(hInst) > 32) {
        // Exit application cleanly so installer can overwrite files and relaunch
        PostQuitMessage(0);
        return true;
    }

    return false;
}

bool UpdateManager::FetchReleaseJson(std::string& outJson) {
    HINTERNET hSession = WinHttpOpen(
        L"Pixel-Motion-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = "Failed to initialize WinHTTP session";
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        L"api.github.com",
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = "Failed to connect to api.github.com";
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        L"/repos/MTCodes01/Pixel-Motion/releases/latest",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = "Failed to create HTTP request";
        return false;
    }

    // GitHub API requires User-Agent header
    LPCWSTR headers = L"User-Agent: Pixel-Motion-App\r\nAccept: application/vnd.github.v3+json\r\n";
    BOOL bSend = WinHttpSendRequest(
        hRequest,
        headers,
        static_cast<DWORD>(-1L),
        nullptr,
        0,
        0,
        0
    );

    if (!bSend || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = "Failed to receive response from GitHub";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage = "GitHub API returned HTTP status: " + std::to_string(statusCode);
        return false;
    }

    std::string responseData;
    DWORD bytesRead = 0;
    char buffer[4096];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        responseData.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    outJson = std::move(responseData);
    return true;
}

bool UpdateManager::DownloadFile(const std::string& url, const std::filesystem::path& destPath) {
    // Convert URL to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return false;
    std::wstring wUrl(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wUrl[0], wlen);

    std::wstring currentUrl = wUrl;
    const int maxRedirects = 10;
    int redirects = 0;

    HINTERNET hSession = WinHttpOpen(
        L"Pixel-Motion-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) return false;

    // Enable automatic redirect policy
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    bool success = false;

    while (redirects < maxRedirects) {
        URL_COMPONENTS urlComp = {};
        urlComp.dwStructSize = sizeof(urlComp);
        wchar_t hostName[256] = { 0 };
        wchar_t urlPath[2048] = { 0 };
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = ARRAYSIZE(hostName);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = ARRAYSIZE(urlPath);

        if (!WinHttpCrackUrl(currentUrl.c_str(), 0, 0, &urlComp)) {
            break;
        }

        INTERNET_PORT port = urlComp.nPort;
        bool isSecure = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

        HINTERNET hConnect = WinHttpConnect(hSession, hostName, port, 0);
        if (!hConnect) break;

        DWORD openFlags = isSecure ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            urlPath,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            openFlags
        );

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            break;
        }

        // Enable redirect policy on request level as well
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

        LPCWSTR headers = L"User-Agent: Pixel-Motion-App\r\n";
        BOOL bSend = WinHttpSendRequest(
            hRequest,
            headers,
            static_cast<DWORD>(-1L),
            nullptr,
            0,
            0,
            0
        );

        if (!bSend || !WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            break;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX
        );

        // Check for manual redirect if not automatically followed
        if (statusCode == 301 || statusCode == 302 || statusCode == 307 || statusCode == 308) {
            wchar_t location[2048] = { 0 };
            DWORD locSize = sizeof(location);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location, &locSize, WINHTTP_NO_HEADER_INDEX)) {
                currentUrl = location;
                redirects++;
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                continue;
            }
        }

        if (statusCode != 200) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_errorMessage = "Download server returned HTTP " + std::to_string(statusCode);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            break;
        }

        // Read Content-Length if available
        DWORD contentLength = 0;
        DWORD clSize = sizeof(contentLength);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &clSize, WINHTTP_NO_HEADER_INDEX)) {
            if (contentLength > 0) {
                m_totalBytes = contentLength;
            }
        }

        std::ofstream outFile(destPath, std::ios::binary);
        if (!outFile.is_open()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_errorMessage = "Failed to create temporary file for installer";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            break;
        }

        std::vector<char> buffer(65536); // 64 KB chunk
        DWORD bytesRead = 0;
        size_t totalDownloaded = 0;

        while (WinHttpReadData(hRequest, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead) && bytesRead > 0) {
            outFile.write(buffer.data(), bytesRead);
            totalDownloaded += bytesRead;
            m_downloadedBytes = totalDownloaded;

            size_t total = m_totalBytes.load();
            if (total > 0) {
                m_downloadProgress = static_cast<float>(totalDownloaded) / static_cast<float>(total);
            }
        }

        outFile.close();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);

        if (totalDownloaded > 0) {
            success = true;
        }
        break;
    }

    WinHttpCloseHandle(hSession);
    return success;
}

} // namespace PixelMotion
