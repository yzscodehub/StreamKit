/**
 * @file media_info.cpp
 * @brief Media file probing implementation
 */

#include <phoenix/media/media_info.hpp>
#include "ffmpeg/ff_common.hpp"
#include <algorithm>

namespace phoenix::media {

namespace {

// Supported extensions
const std::vector<std::string> kVideoExtensions = {
    ".mp4", ".mov", ".avi", ".mkv", ".webm", ".m4v",
    ".wmv", ".flv", ".mpg", ".mpeg", ".mts", ".m2ts",
    ".ts", ".vob", ".3gp", ".mxf", ".ogv"
};

const std::vector<std::string> kAudioExtensions = {
    ".mp3", ".wav", ".aac", ".m4a", ".flac", ".ogg",
    ".wma", ".aiff", ".aif", ".opus", ".ac3", ".dts"
};

const std::vector<std::string> kImageExtensions = {
    ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff",
    ".tif", ".webp", ".psd", ".tga", ".exr"
};

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

model::MediaItemType detectMediaType(const std::filesystem::path& path) {
    std::string ext = toLower(path.extension().string());
    
    for (const auto& e : kVideoExtensions) {
        if (ext == e) return model::MediaItemType::Video;
    }
    for (const auto& e : kAudioExtensions) {
        if (ext == e) return model::MediaItemType::Audio;
    }
    for (const auto& e : kImageExtensions) {
        if (ext == e) return model::MediaItemType::Image;
    }
    
    return model::MediaItemType::Unknown;
}

} // anonymous namespace

Result<void, Error> MediaInfo::probe(model::MediaItem& item) {
    const std::string path = item.path().string();
    
    // Open format context
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return ff::avError(ret, "Failed to open file");
    }
    
    // Ensure cleanup
    auto cleanup = [&fmtCtx]() {
        if (fmtCtx) avformat_close_input(&fmtCtx);
    };
    
    // Find stream info
    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        cleanup();
        return ff::avError(ret, "Failed to find stream info");
    }
    
    // Get duration
    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        item.setDuration(av_rescale_q(fmtCtx->duration, 
            AV_TIME_BASE_Q, {1, 1000000}));
    }
    
    // Get file size
    if (std::filesystem::exists(item.path())) {
        item.setFileSize(std::filesystem::file_size(item.path()));
    }
    
    // Find video stream
    int videoStreamIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, 
        -1, -1, nullptr, 0);
    if (videoStreamIdx >= 0) {
        item.setHasVideo(true);
        
        AVStream* stream = fmtCtx->streams[videoStreamIdx];
        AVCodecParameters* codecpar = stream->codecpar;
        
        model::VideoProperties& video = item.videoProperties();
        video.resolution = {codecpar->width, codecpar->height};
        video.frameRate = ff::fromAVRational(stream->avg_frame_rate);
        video.pixelFormat = ff::fromAVPixelFormat(
            static_cast<AVPixelFormat>(codecpar->format));
        video.bitrate = codecpar->bit_rate;
        
        const AVCodecDescriptor* desc = avcodec_descriptor_get(codecpar->codec_id);
        if (desc) {
            video.codec = desc->name;
        }
    }
    
    // Find audio stream
    int audioStreamIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO,
        -1, -1, nullptr, 0);
    if (audioStreamIdx >= 0) {
        item.setHasAudio(true);
        
        AVStream* stream = fmtCtx->streams[audioStreamIdx];
        AVCodecParameters* codecpar = stream->codecpar;
        
        model::AudioProperties& audio = item.audioProperties();
        audio.sampleRate = codecpar->sample_rate;
        audio.channels = codecpar->ch_layout.nb_channels;
        audio.sampleFormat = ff::fromAVSampleFormat(
            static_cast<AVSampleFormat>(codecpar->format));
        audio.bitrate = codecpar->bit_rate;
        
        const AVCodecDescriptor* desc = avcodec_descriptor_get(codecpar->codec_id);
        if (desc) {
            audio.codec = desc->name;
        }
    }
    
    // Determine media type
    if (item.hasVideo()) {
        item.setType(model::MediaItemType::Video);
    } else if (item.hasAudio()) {
        item.setType(model::MediaItemType::Audio);
    } else {
        item.setType(detectMediaType(item.path()));
    }
    
    item.setProbed(true);
    
    cleanup();
    return Ok();
}

Result<std::shared_ptr<model::MediaItem>, Error> MediaInfo::probe(
    const std::filesystem::path& path)
{
    auto item = std::make_shared<model::MediaItem>(path);
    
    auto result = probe(*item);
    if (!result.ok()) {
        return result.error();
    }
    
    return item;
}

bool MediaInfo::isSupported(const std::filesystem::path& path) {
    std::string ext = toLower(path.extension().string());
    
    for (const auto& e : kVideoExtensions) {
        if (ext == e) return true;
    }
    for (const auto& e : kAudioExtensions) {
        if (ext == e) return true;
    }
    for (const auto& e : kImageExtensions) {
        if (ext == e) return true;
    }
    
    return false;
}

const std::vector<std::string>& MediaInfo::supportedVideoExtensions() {
    return kVideoExtensions;
}

const std::vector<std::string>& MediaInfo::supportedAudioExtensions() {
    return kAudioExtensions;
}

const std::vector<std::string>& MediaInfo::supportedImageExtensions() {
    return kImageExtensions;
}

} // namespace phoenix::media
