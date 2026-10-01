#include "AudioPlayer.h"

#include <eacp/Core/ObjC/ObjC.h>
#include <eacp/Core/ObjC/Strings.h>

#import <AVFoundation/AVFoundation.h>

namespace eacp::SA3App
{
struct AudioPlayer::Native
{
    Native()
    {
        [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback
                                               error:nil];
    }

    bool play(const FilePath& file)
    {
        stop();

        auto* url = [NSURL fileURLWithPath:Strings::toNSString(file.str())];
        player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:nil];

        if (player.get() == nil)
            return false;

        [[AVAudioSession sharedInstance] setActive:YES error:nil];
        return [player.get() play];
    }

    void stop()
    {
        if (player.get() != nil)
            [player.get() stop];
    }

    bool isPlaying() const { return player.get() != nil && player.get().playing; }

    float position() const
    {
        if (player.get() == nil || player.get().duration <= 0.0)
            return 0.f;

        return (float) (player.get().currentTime / player.get().duration);
    }

    mutable ObjC::Ptr<AVAudioPlayer> player;
};

AudioPlayer::AudioPlayer() = default;
AudioPlayer::~AudioPlayer() = default;

bool AudioPlayer::play(const FilePath& file)
{
    return impl->play(file);
}

void AudioPlayer::stop()
{
    impl->stop();
}

bool AudioPlayer::isPlaying() const
{
    return impl->isPlaying();
}

float AudioPlayer::position() const
{
    return impl->position();
}
} // namespace eacp::SA3App
