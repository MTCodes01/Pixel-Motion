#pragma once

namespace PixelMotion {

/**
 * Audio player
 * Handles audio playback from video files
 * TODO: Implement with FFmpeg audio decoding
 */
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    bool Initialize();
    void Shutdown();

    void Play();
    void Pause();
    void SetVolume(float volume);

private:
    float m_volume;
    bool m_playing;
    bool m_initialized;
};

} // namespace PixelMotion
