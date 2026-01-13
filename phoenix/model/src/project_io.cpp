/**
 * @file project_io.cpp
 * @brief Project serialization implementation
 */

#include <phoenix/model/io/project_io.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace phoenix::model {

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

namespace {

// UUID
json uuidToJson(const UUID& uuid) {
    return uuid.toString();
}

UUID uuidFromJson(const json& j) {
    return UUID::fromString(j.get<std::string>());
}

// Size
json sizeToJson(const Size& size) {
    return {{"width", size.width}, {"height", size.height}};
}

Size sizeFromJson(const json& j) {
    return {j.value("width", 0), j.value("height", 0)};
}

// Rational
json rationalToJson(const Rational& r) {
    return {{"num", r.num}, {"den", r.den}};
}

Rational rationalFromJson(const json& j) {
    return {j.value("num", 0), j.value("den", 1)};
}

// VideoProperties
json videoPropsToJson(const VideoProperties& props) {
    return {
        {"resolution", sizeToJson(props.resolution)},
        {"frameRate", rationalToJson(props.frameRate)},
        {"pixelFormat", static_cast<int>(props.pixelFormat)},
        {"codec", props.codec},
        {"bitrate", props.bitrate}
    };
}

VideoProperties videoPropsFromJson(const json& j) {
    VideoProperties props;
    if (j.contains("resolution")) props.resolution = sizeFromJson(j["resolution"]);
    if (j.contains("frameRate")) props.frameRate = rationalFromJson(j["frameRate"]);
    props.pixelFormat = static_cast<PixelFormat>(j.value("pixelFormat", 0));
    props.codec = j.value("codec", "");
    props.bitrate = j.value("bitrate", int64_t(0));
    return props;
}

// AudioProperties
json audioPropsToJson(const AudioProperties& props) {
    return {
        {"sampleRate", props.sampleRate},
        {"channels", props.channels},
        {"sampleFormat", static_cast<int>(props.sampleFormat)},
        {"codec", props.codec},
        {"bitrate", props.bitrate}
    };
}

AudioProperties audioPropsFromJson(const json& j) {
    AudioProperties props;
    props.sampleRate = j.value("sampleRate", 0);
    props.channels = j.value("channels", 0);
    props.sampleFormat = static_cast<SampleFormat>(j.value("sampleFormat", 0));
    props.codec = j.value("codec", "");
    props.bitrate = j.value("bitrate", int64_t(0));
    return props;
}

// MediaItem
json mediaItemToJson(const MediaItem& item) {
    return {
        {"id", uuidToJson(item.id())},
        {"name", item.name()},
        {"path", item.path().string()},
        {"fileSize", item.fileSize()},
        {"type", static_cast<int>(item.type())},
        {"duration", item.duration()},
        {"hasVideo", item.hasVideo()},
        {"hasAudio", item.hasAudio()},
        {"video", videoPropsToJson(item.videoProperties())},
        {"audio", audioPropsToJson(item.audioProperties())},
        {"probed", item.isProbed()}
    };
}

std::shared_ptr<MediaItem> mediaItemFromJson(const json& j) {
    auto item = std::make_shared<MediaItem>();
    
    // Note: ID is regenerated, we store original for reference mapping
    item->setName(j.value("name", ""));
    item->setPath(j.value("path", ""));
    item->setFileSize(j.value("fileSize", uint64_t(0)));
    item->setType(static_cast<MediaItemType>(j.value("type", 0)));
    item->setDuration(j.value("duration", Duration(0)));
    item->setHasVideo(j.value("hasVideo", false));
    item->setHasAudio(j.value("hasAudio", false));
    
    if (j.contains("video")) {
        item->videoProperties() = videoPropsFromJson(j["video"]);
    }
    if (j.contains("audio")) {
        item->audioProperties() = audioPropsFromJson(j["audio"]);
    }
    
    item->setProbed(j.value("probed", false));
    
    return item;
}

// Clip
json clipToJson(const Clip& clip) {
    return {
        {"id", uuidToJson(clip.id())},
        {"mediaItemId", uuidToJson(clip.mediaItemId())},
        {"name", clip.name()},
        {"type", static_cast<int>(clip.type())},
        {"timelineIn", clip.timelineIn()},
        {"timelineOut", clip.timelineOut()},
        {"sourceIn", clip.sourceIn()},
        {"sourceOut", clip.sourceOut()},
        {"speed", clip.speed()},
        {"reversed", clip.reversed()},
        {"opacity", clip.opacity()},
        {"volume", clip.volume()},
        {"muted", clip.muted()},
        {"disabled", clip.disabled()}
    };
}

std::shared_ptr<Clip> clipFromJson(const json& j, 
    const std::unordered_map<std::string, UUID>& idMap) 
{
    auto clip = std::make_shared<Clip>();
    
    // Map old media item ID to new one
    std::string oldMediaId = j.value("mediaItemId", "");
    if (auto it = idMap.find(oldMediaId); it != idMap.end()) {
        clip->setMediaItemId(it->second);
    }
    
    clip->setName(j.value("name", ""));
    clip->setType(static_cast<ClipType>(j.value("type", 0)));
    clip->setTimelineIn(j.value("timelineIn", Timestamp(0)));
    clip->setTimelineOut(j.value("timelineOut", Timestamp(0)));
    clip->setSourceIn(j.value("sourceIn", Timestamp(0)));
    clip->setSourceOut(j.value("sourceOut", Timestamp(0)));
    clip->setSpeed(j.value("speed", 1.0f));
    clip->setReversed(j.value("reversed", false));
    clip->setOpacity(j.value("opacity", 1.0f));
    clip->setVolume(j.value("volume", 1.0f));
    clip->setMuted(j.value("muted", false));
    clip->setDisabled(j.value("disabled", false));
    
    return clip;
}

// Track
json trackToJson(const Track& track) {
    json clipsJson = json::array();
    for (const auto& clip : track.clips()) {
        clipsJson.push_back(clipToJson(*clip));
    }
    
    return {
        {"id", uuidToJson(track.id())},
        {"name", track.name()},
        {"type", static_cast<int>(track.type())},
        {"muted", track.muted()},
        {"locked", track.locked()},
        {"hidden", track.hidden()},
        {"solo", track.solo()},
        {"clips", clipsJson}
    };
}

std::shared_ptr<Track> trackFromJson(const json& j,
    const std::unordered_map<std::string, UUID>& idMap)
{
    auto track = std::make_shared<Track>();
    
    track->setName(j.value("name", ""));
    track->setType(static_cast<TrackType>(j.value("type", 0)));
    track->setMuted(j.value("muted", false));
    track->setLocked(j.value("locked", false));
    track->setHidden(j.value("hidden", false));
    track->setSolo(j.value("solo", false));
    
    if (j.contains("clips")) {
        for (const auto& clipJson : j["clips"]) {
            auto clip = clipFromJson(clipJson, idMap);
            track->addClip(clip);
        }
    }
    
    return track;
}

// SequenceSettings
json seqSettingsToJson(const SequenceSettings& s) {
    return {
        {"resolution", sizeToJson(s.resolution)},
        {"frameRate", rationalToJson(s.frameRate)},
        {"sampleRate", s.sampleRate},
        {"audioChannels", s.audioChannels}
    };
}

SequenceSettings seqSettingsFromJson(const json& j) {
    SequenceSettings s;
    if (j.contains("resolution")) s.resolution = sizeFromJson(j["resolution"]);
    if (j.contains("frameRate")) s.frameRate = rationalFromJson(j["frameRate"]);
    s.sampleRate = j.value("sampleRate", 48000);
    s.audioChannels = j.value("audioChannels", 2);
    return s;
}

// Sequence
json sequenceToJson(const Sequence& seq) {
    json videoTracksJson = json::array();
    for (const auto& track : seq.videoTracks()) {
        videoTracksJson.push_back(trackToJson(*track));
    }
    
    json audioTracksJson = json::array();
    for (const auto& track : seq.audioTracks()) {
        audioTracksJson.push_back(trackToJson(*track));
    }
    
    return {
        {"id", uuidToJson(seq.id())},
        {"name", seq.name()},
        {"settings", seqSettingsToJson(seq.settings())},
        {"videoTracks", videoTracksJson},
        {"audioTracks", audioTracksJson},
        {"playhead", seq.playheadPosition()},
        {"inPoint", seq.inPoint()},
        {"outPoint", seq.outPoint()}
    };
}

std::shared_ptr<Sequence> sequenceFromJson(const json& j,
    const std::unordered_map<std::string, UUID>& idMap)
{
    auto seq = std::make_shared<Sequence>();
    
    seq->setName(j.value("name", "Sequence"));
    
    if (j.contains("settings")) {
        seq->settings() = seqSettingsFromJson(j["settings"]);
    }
    
    // Clear default tracks
    while (seq->videoTrackCount() > 0) {
        seq->removeTrack(seq->videoTracks()[0]->id());
    }
    while (seq->audioTrackCount() > 0) {
        seq->removeTrack(seq->audioTracks()[0]->id());
    }
    
    // Load video tracks
    if (j.contains("videoTracks")) {
        for (const auto& trackJson : j["videoTracks"]) {
            auto track = trackFromJson(trackJson, idMap);
            track->setType(TrackType::Video);
            // Add clips directly since we can't add a pre-made track
            auto newTrack = seq->addVideoTrack();
            newTrack->setName(track->name());
            newTrack->setMuted(track->muted());
            newTrack->setLocked(track->locked());
            newTrack->setHidden(track->hidden());
            newTrack->setSolo(track->solo());
            for (const auto& clip : track->clips()) {
                newTrack->addClip(clip);
            }
        }
    }
    
    // Load audio tracks
    if (j.contains("audioTracks")) {
        for (const auto& trackJson : j["audioTracks"]) {
            auto track = trackFromJson(trackJson, idMap);
            track->setType(TrackType::Audio);
            auto newTrack = seq->addAudioTrack();
            newTrack->setName(track->name());
            newTrack->setMuted(track->muted());
            newTrack->setLocked(track->locked());
            newTrack->setHidden(track->hidden());
            newTrack->setSolo(track->solo());
            for (const auto& clip : track->clips()) {
                newTrack->addClip(clip);
            }
        }
    }
    
    seq->setPlayheadPosition(j.value("playhead", Timestamp(0)));
    seq->setInPoint(j.value("inPoint", Timestamp(0)));
    seq->setOutPoint(j.value("outPoint", Timestamp(0)));
    
    return seq;
}

// ProjectSettings
json projectSettingsToJson(const ProjectSettings& s) {
    return {
        {"name", s.name},
        {"defaultSequence", seqSettingsToJson(s.defaultSequence)},
        {"exportPreset", s.exportPreset},
        {"autoSaveEnabled", s.autoSaveEnabled},
        {"autoSaveIntervalMinutes", s.autoSaveIntervalMinutes},
        {"scratchDisk", s.scratchDisk.string()}
    };
}

ProjectSettings projectSettingsFromJson(const json& j) {
    ProjectSettings s;
    s.name = j.value("name", "Untitled Project");
    if (j.contains("defaultSequence")) {
        s.defaultSequence = seqSettingsFromJson(j["defaultSequence"]);
    }
    s.exportPreset = j.value("exportPreset", "H.264 High Quality");
    s.autoSaveEnabled = j.value("autoSaveEnabled", true);
    s.autoSaveIntervalMinutes = j.value("autoSaveIntervalMinutes", 5);
    s.scratchDisk = j.value("scratchDisk", "");
    return s;
}

} // anonymous namespace

// ============================================================================
// ProjectIO Implementation
// ============================================================================

Result<void, Error> ProjectIO::save(
    const Project& project,
    const std::filesystem::path& path)
{
    try {
        std::string jsonStr = toJson(project);
        
        std::ofstream file(path);
        if (!file.is_open()) {
            return Error(ErrorCode::FileOpenFailed, 
                "Cannot open file for writing: " + path.string());
        }
        
        file << jsonStr;
        file.close();
        
        if (file.fail()) {
            return Error(ErrorCode::WriteError, 
                "Failed to write project file: " + path.string());
        }
        
        return Ok();
    }
    catch (const std::exception& e) {
        return Error(ErrorCode::WriteError, e.what());
    }
}

Result<std::unique_ptr<Project>, Error> ProjectIO::load(
    const std::filesystem::path& path)
{
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return Error(ErrorCode::FileNotFound, 
                "Cannot open project file: " + path.string());
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        auto result = fromJson(buffer.str());
        if (result.ok()) {
            result.value()->setFilePath(path);
        }
        return result;
    }
    catch (const std::exception& e) {
        return Error(ErrorCode::ReadError, e.what());
    }
}

std::string ProjectIO::toJson(const Project& project) {
    // Build media item list with ID mapping
    json mediaItemsJson = json::array();
    for (const auto& item : project.mediaBin().items()) {
        auto itemJson = mediaItemToJson(*item);
        mediaItemsJson.push_back(itemJson);
    }
    
    // Build sequences
    json sequencesJson = json::array();
    for (const auto& seq : project.sequences()) {
        sequencesJson.push_back(sequenceToJson(*seq));
    }
    
    json root = {
        {"formatVersion", kFormatVersion},
        {"id", uuidToJson(project.id())},
        {"settings", projectSettingsToJson(project.settings())},
        {"mediaItems", mediaItemsJson},
        {"sequences", sequencesJson},
        {"activeSequenceIndex", project.activeSequenceIndex()}
    };
    
    return root.dump(2);  // Pretty print with 2-space indent
}

Result<std::unique_ptr<Project>, Error> ProjectIO::fromJson(
    const std::string& jsonStr)
{
    try {
        json root = json::parse(jsonStr);
        
        // Check version
        int version = root.value("formatVersion", 0);
        if (version > kFormatVersion) {
            return Error(ErrorCode::NotSupported,
                "Project file version " + std::to_string(version) + 
                " is newer than supported version " + std::to_string(kFormatVersion));
        }
        
        auto project = std::make_unique<Project>();
        
        // Load settings
        if (root.contains("settings")) {
            project->settings() = projectSettingsFromJson(root["settings"]);
        }
        
        // Load media items and build ID mapping
        std::unordered_map<std::string, UUID> idMap;
        if (root.contains("mediaItems")) {
            for (const auto& itemJson : root["mediaItems"]) {
                std::string oldId = itemJson.value("id", "");
                auto item = mediaItemFromJson(itemJson);
                idMap[oldId] = item->id();
                project->mediaBin().addItem(item);
            }
        }
        
        // Clear default sequence
        while (project->sequenceCount() > 0) {
            project->removeSequence(project->sequences()[0]->id());
        }
        
        // Load sequences
        if (root.contains("sequences")) {
            for (const auto& seqJson : root["sequences"]) {
                auto seq = sequenceFromJson(seqJson, idMap);
                // We need to add it manually since sequences is private
                // For now, create new and copy settings
                auto newSeq = project->createSequence(seq->name());
                newSeq->settings() = seq->settings();
                
                // Copy tracks
                // Clear default tracks first
                while (newSeq->videoTrackCount() > 0) {
                    newSeq->removeTrack(newSeq->videoTracks()[0]->id());
                }
                while (newSeq->audioTrackCount() > 0) {
                    newSeq->removeTrack(newSeq->audioTracks()[0]->id());
                }
                
                // Add video tracks
                for (const auto& srcTrack : seq->videoTracks()) {
                    auto track = newSeq->addVideoTrack();
                    track->setName(srcTrack->name());
                    track->setMuted(srcTrack->muted());
                    track->setLocked(srcTrack->locked());
                    track->setHidden(srcTrack->hidden());
                    for (const auto& clip : srcTrack->clips()) {
                        track->addClip(clip);
                    }
                }
                
                // Add audio tracks
                for (const auto& srcTrack : seq->audioTracks()) {
                    auto track = newSeq->addAudioTrack();
                    track->setName(srcTrack->name());
                    track->setMuted(srcTrack->muted());
                    for (const auto& clip : srcTrack->clips()) {
                        track->addClip(clip);
                    }
                }
                
                newSeq->setPlayheadPosition(seq->playheadPosition());
                newSeq->setInPoint(seq->inPoint());
                newSeq->setOutPoint(seq->outPoint());
            }
        }
        
        // Set active sequence
        int activeIdx = root.value("activeSequenceIndex", 0);
        project->setActiveSequenceIndex(activeIdx);
        
        project->clearModified();
        
        return project;
    }
    catch (const json::exception& e) {
        return Error(ErrorCode::InvalidData, 
            "JSON parse error: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        return Error(ErrorCode::ReadError, e.what());
    }
}

} // namespace phoenix::model
