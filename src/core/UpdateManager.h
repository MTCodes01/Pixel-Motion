#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <filesystem>

namespace PixelMotion {

enum class UpdateState {
    Idle,
    Checking,
    UpToDate,
    UpdateAvailable,
    Downloading,
    ReadyToInstall,
    Error
};

struct ReleaseInfo {
    std::string tagName;        // e.g. "v1.3.1"
    std::string releaseName;    // e.g. "Pixel-Motion-v1.3.1"
    std::string releaseNotes;   // Markdown or description
    std::string downloadUrl;    // URL to PixelMotion-Setup-vX.Y.Z.exe
    size_t assetSize = 0;       // File size in bytes
};

class UpdateManager {
public:
    static UpdateManager& GetInstance();

    UpdateManager();
    ~UpdateManager();

    // Trigger async check for updates
    void CheckForUpdates(bool isBackground = false);

    // Trigger async download of the installer
    void StartDownload();

    // Launch the downloaded installer and prepare app shutdown
    bool LaunchInstaller();

    // Reset status to idle
    void ResetStatus();

    // Status queries for UI rendering
    UpdateState GetState() const { return m_state.load(); }
    std::string GetCurrentVersion() const { return m_currentVersion; }
    std::string GetLatestVersion() const;
    std::string GetReleaseNotes() const;
    float GetDownloadProgress() const { return m_downloadProgress.load(); }
    size_t GetDownloadedBytes() const { return m_downloadedBytes.load(); }
    size_t GetTotalBytes() const { return m_totalBytes.load(); }
    std::string GetErrorMessage() const;

    static bool IsNewerVersion(const std::string& remote, const std::string& current);

private:
    void CheckThread();
    void DownloadThread();
    
    // WinHTTP helpers
    bool FetchReleaseJson(std::string& outJson);
    bool DownloadFile(const std::string& url, const std::filesystem::path& destPath);

    std::atomic<UpdateState> m_state;
    std::atomic<float> m_downloadProgress;
    std::atomic<size_t> m_downloadedBytes;
    std::atomic<size_t> m_totalBytes;

    std::string m_currentVersion;
    ReleaseInfo m_latestRelease;
    std::filesystem::path m_downloadedFilePath;
    std::string m_errorMessage;
    mutable std::mutex m_mutex;

    std::thread m_workerThread;
    bool m_isBackgroundCheck;
};

} // namespace PixelMotion
