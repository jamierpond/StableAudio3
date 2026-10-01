#pragma once

#include <eacp/Core/Utils/FilePath.h>
#include <eacp/Core/Utils/Pimpl.h>

namespace eacp::SA3App
{
// AVAudioPlayer over a file on disk.
class AudioPlayer
{
public:
    AudioPlayer();
    ~AudioPlayer();

    bool play(const FilePath& file);
    void stop();

    bool isPlaying() const;

    // 0 to 1 through the file, 0 when nothing is loaded.
    float position() const;

private:
    struct Native;
    Pimpl<Native> impl;
};
} // namespace eacp::SA3App
