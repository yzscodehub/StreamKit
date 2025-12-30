#include <iostream>
#include <memory>
#include <string>
#include <iomanip>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include "common/logger.hpp"
#include "video/decoder_base.hpp"
#include "video/software_decoder.hpp"
#include "video/hardware_decoder.hpp"
#include "video/decoder_factory.hpp"

using namespace StreamKit;

// Performance test result structure
struct SeekPerformance {
    int frame_number;
    double seek_time_ms;
    bool is_keyframe;
    bool success;
};

// Decoder performance statistics structure
struct DecoderPerformanceStats {
    std::string decoder_name;
    DecoderType decoder_type;
    double avg_seek_time_ms;
    double min_seek_time_ms;
    double max_seek_time_ms;
    double median_seek_time_ms;
    double keyframe_avg_ms;
    double non_keyframe_avg_ms;
    int total_seeks;
    int successful_seeks;
    double success_rate;
};

void printFrameInfo(const std::vector<FrameInfo>& frame_index, int start = 0, int count = 10) {
    std::cout << "\n=== Frame Index (showing " << count << " frames from " << start << ") ===" << std::endl;
    std::cout << std::setw(6) << "Frame" << " | " 
              << std::setw(12) << "Time(s)" << " | "
              << std::setw(8) << "PTS" << " | "
              << std::setw(8) << "KeyFrame" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    for (int i = start; i < std::min(start + count, static_cast<int>(frame_index.size())); ++i) {
        const auto& frame = frame_index[i];
        std::cout << std::setw(6) << frame.frame_number << " | "
                  << std::setw(12) << std::fixed << std::setprecision(3) << frame.timestamp_sec << " | "
                  << std::setw(8) << frame.pts << " | "
                  << std::setw(8) << (frame.is_keyframe ? "YES" : "NO") << std::endl;
    }
}

DecoderPerformanceStats calculatePerformanceStats(const std::vector<SeekPerformance>& performances, 
                                                  const std::string& decoder_name,
                                                  DecoderType decoder_type) {
    DecoderPerformanceStats stats;
    stats.decoder_name = decoder_name;
    stats.decoder_type = decoder_type;
    stats.total_seeks = performances.size();
    
    std::vector<double> successful_times;
    std::vector<double> keyframe_times;
    std::vector<double> non_keyframe_times;
    
    int successful_count = 0;
    for (const auto& perf : performances) {
        if (perf.success) {
            successful_count++;
            successful_times.push_back(perf.seek_time_ms);
            
            if (perf.is_keyframe) {
                keyframe_times.push_back(perf.seek_time_ms);
            } else {
                non_keyframe_times.push_back(perf.seek_time_ms);
            }
        }
    }
    
    stats.successful_seeks = successful_count;
    stats.success_rate = successful_count > 0 ? (100.0 * successful_count / stats.total_seeks) : 0.0;
    
    if (!successful_times.empty()) {
        std::sort(successful_times.begin(), successful_times.end());
        stats.avg_seek_time_ms = std::accumulate(successful_times.begin(), successful_times.end(), 0.0) / successful_times.size();
        stats.min_seek_time_ms = successful_times.front();
        stats.max_seek_time_ms = successful_times.back();
        stats.median_seek_time_ms = successful_times[successful_times.size() / 2];
    } else {
        stats.avg_seek_time_ms = stats.min_seek_time_ms = stats.max_seek_time_ms = stats.median_seek_time_ms = 0.0;
    }
    
    stats.keyframe_avg_ms = keyframe_times.empty() ? 0.0 : 
        (std::accumulate(keyframe_times.begin(), keyframe_times.end(), 0.0) / keyframe_times.size());
    
    stats.non_keyframe_avg_ms = non_keyframe_times.empty() ? 0.0 : 
        (std::accumulate(non_keyframe_times.begin(), non_keyframe_times.end(), 0.0) / non_keyframe_times.size());
    
    return stats;
}

void printPerformanceStats(const DecoderPerformanceStats& stats) {
    std::cout << "\n=== " << stats.decoder_name << " Performance Statistics ===" << std::endl;
    std::cout << "🔍 SEEK PERFORMANCE (I/O intensive):" << std::endl;
    std::cout << "Decoder Type: " << static_cast<int>(stats.decoder_type) << std::endl;
    std::cout << "Total Seeks: " << stats.total_seeks << std::endl;
    std::cout << "Successful: " << stats.successful_seeks << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1) << stats.success_rate << "%" << std::endl;
    
    if (stats.successful_seeks > 0) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Average Time: " << stats.avg_seek_time_ms << " ms" << std::endl;
        std::cout << "Min Time: " << stats.min_seek_time_ms << " ms" << std::endl;
        std::cout << "Max Time: " << stats.max_seek_time_ms << " ms" << std::endl;
        std::cout << "Median: " << stats.median_seek_time_ms << " ms" << std::endl;
        
        if (stats.keyframe_avg_ms > 0) {
            std::cout << "Keyframe Avg: " << stats.keyframe_avg_ms << " ms" << std::endl;
        }
        if (stats.non_keyframe_avg_ms > 0) {
            std::cout << "Non-keyframe Avg: " << stats.non_keyframe_avg_ms << " ms" << std::endl;
        }
        
        std::cout << "💡 NOTE: Seek performance depends on file I/O and memory operations." << std::endl;
        std::cout << "   Hardware decoders may be slower due to CPU-GPU communication overhead." << std::endl;
    }
    std::cout << std::string(50, '=') << std::endl;
}

std::vector<SeekPerformance> testRandomSeekPerformance(VideoDecoderBase* decoder, int test_count = 50) {
    std::cout << "\n=== Testing Random Seek Performance (" << test_count << " seeks) ===" << std::endl;
    
    std::vector<SeekPerformance> performances;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) {
        std::cout << "No frames available for testing" << std::endl;
        return performances;
    }
    
    auto frame_index = decoder->getFrameIndex();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, total_frames - 1);
    
    std::cout << "Testing " << test_count << " random seeks on " << total_frames << " frames..." << std::endl;
    
    for (int i = 0; i < test_count; ++i) {
        int frame_number = dis(gen);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(frame_number);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double seek_time_ms = duration.count() / 1000.0;
        
        SeekPerformance perf;
        perf.frame_number = frame_number;
        perf.seek_time_ms = seek_time_ms;
        perf.is_keyframe = (frame_number < static_cast<int>(frame_index.size())) ? 
                          frame_index[frame_number].is_keyframe : false;
        perf.success = success;
        
        performances.push_back(perf);
        
        if ((i + 1) % 10 == 0 || i == test_count - 1) {
            std::cout << "Progress: " << (i + 1) << "/" << test_count 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (100.0 * (i + 1) / test_count) << "%)" << std::endl;
        }
    }
    
    return performances;
}

std::vector<SeekPerformance> testSequentialSeekPerformance(VideoDecoderBase* decoder, int step = 100) {
    std::cout << "\n=== Testing Sequential Seek Performance (every " << step << " frames) ===" << std::endl;
    
    std::vector<SeekPerformance> performances;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) {
        std::cout << "No frames available for testing" << std::endl;
        return performances;
    }
    
    auto frame_index = decoder->getFrameIndex();
    
    for (int frame_number = 0; frame_number < total_frames; frame_number += step) {
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(frame_number);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double seek_time_ms = duration.count() / 1000.0;
        
        SeekPerformance perf;
        perf.frame_number = frame_number;
        perf.seek_time_ms = seek_time_ms;
        perf.is_keyframe = (frame_number < static_cast<int>(frame_index.size())) ? 
                          frame_index[frame_number].is_keyframe : false;
        perf.success = success;
        
        performances.push_back(perf);
    }
    
    std::cout << "Sequential seek test completed: " << performances.size() << " seeks" << std::endl;
    return performances;
}

std::vector<SeekPerformance> testBackwardSeekPerformance(VideoDecoderBase* decoder, int test_count = 50) {
    std::cout << "\n=== Testing Backward Seek Performance (" << test_count << " seeks) ===" << std::endl;
    
    std::vector<SeekPerformance> performances;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) {
        std::cout << "No frames available for testing" << std::endl;
        return performances;
    }
    
    auto frame_index = decoder->getFrameIndex();
    
    // Start from the end and seek backward
    for (int i = 0; i < test_count; ++i) {
        int frame_number = total_frames - 1 - (i * (total_frames / test_count));
        if (frame_number < 0) frame_number = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(frame_number);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double seek_time_ms = duration.count() / 1000.0;
        
        SeekPerformance perf;
        perf.frame_number = frame_number;
        perf.seek_time_ms = seek_time_ms;
        perf.is_keyframe = (frame_number < static_cast<int>(frame_index.size())) ? 
                          frame_index[frame_number].is_keyframe : false;
        perf.success = success;
        
        performances.push_back(perf);
        
        if ((i + 1) % 10 == 0 || i == test_count - 1) {
            std::cout << "Backward seek progress: " << (i + 1) << "/" << test_count << std::endl;
        }
    }
    
    return performances;
}

std::vector<SeekPerformance> testShortDistanceSeeks(VideoDecoderBase* decoder, int test_count = 50) {
    std::cout << "\n=== Testing Short Distance Seeks (1-10 frames) (" << test_count << " seeks) ===" << std::endl;
    
    std::vector<SeekPerformance> performances;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) {
        std::cout << "No frames available for testing" << std::endl;
        return performances;
    }
    
    auto frame_index = decoder->getFrameIndex();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> start_dis(0, total_frames - 20); // Leave room for short seeks
    std::uniform_int_distribution<> offset_dis(1, 10); // Short distance: 1-10 frames
    
    int current_frame = 0;
    for (int i = 0; i < test_count; ++i) {
        // Choose a small offset from current position
        int offset = offset_dis(gen);
        int target_frame = current_frame + offset;
        if (target_frame >= total_frames) {
            target_frame = current_frame - offset;
            if (target_frame < 0) target_frame = start_dis(gen);
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(target_frame);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double seek_time_ms = duration.count() / 1000.0;
        
        SeekPerformance perf;
        perf.frame_number = target_frame;
        perf.seek_time_ms = seek_time_ms;
        perf.is_keyframe = (target_frame < static_cast<int>(frame_index.size())) ? 
                          frame_index[target_frame].is_keyframe : false;
        perf.success = success;
        
        performances.push_back(perf);
        current_frame = target_frame;
        
        if ((i + 1) % 10 == 0 || i == test_count - 1) {
            std::cout << "Short seek progress: " << (i + 1) << "/" << test_count << std::endl;
        }
    }
    
    return performances;
}

std::vector<SeekPerformance> testLongDistanceSeeks(VideoDecoderBase* decoder, int test_count = 30) {
    std::cout << "\n=== Testing Long Distance Seeks (>100 frames) (" << test_count << " seeks) ===" << std::endl;
    
    std::vector<SeekPerformance> performances;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) {
        std::cout << "No frames available for testing" << std::endl;
        return performances;
    }
    
    auto frame_index = decoder->getFrameIndex();
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (int i = 0; i < test_count; ++i) {
        // Choose two random frames that are far apart
        int frame1 = std::uniform_int_distribution<>(0, total_frames / 2)(gen);
        int frame2 = std::uniform_int_distribution<>(total_frames / 2, total_frames - 1)(gen);
        
        int target_frame = (i % 2 == 0) ? frame2 : frame1; // Alternate between first and second half
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(target_frame);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double seek_time_ms = duration.count() / 1000.0;
        
        SeekPerformance perf;
        perf.frame_number = target_frame;
        perf.seek_time_ms = seek_time_ms;
        perf.is_keyframe = (target_frame < static_cast<int>(frame_index.size())) ? 
                          frame_index[target_frame].is_keyframe : false;
        perf.success = success;
        
        performances.push_back(perf);
        
        if ((i + 1) % 5 == 0 || i == test_count - 1) {
            std::cout << "Long seek progress: " << (i + 1) << "/" << test_count << std::endl;
        }
    }
    
    return performances;
}

std::vector<SeekPerformance> testKeyframeSeeks(VideoDecoderBase* decoder, int test_count = 30) {
    std::cout << "\n=== Testing Keyframe Seeks (" << test_count << " seeks) ===" << std::endl;
    
    std::vector<SeekPerformance> performances;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) {
        std::cout << "No frames available for testing" << std::endl;
        return performances;
    }
    
    auto frame_index = decoder->getFrameIndex();
    
    // Find keyframes
    std::vector<int> keyframes;
    for (size_t i = 0; i < frame_index.size(); ++i) {
        if (frame_index[i].is_keyframe) {
            keyframes.push_back(i);
        }
    }
    
    if (keyframes.empty()) {
        std::cout << "No keyframes found for testing" << std::endl;
        return performances;
    }
    
    std::cout << "Found " << keyframes.size() << " keyframes in video" << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, keyframes.size() - 1);
    
    for (int i = 0; i < test_count; ++i) {
        int keyframe_idx = dis(gen);
        int frame_number = keyframes[keyframe_idx];
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(frame_number);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double seek_time_ms = duration.count() / 1000.0;
        
        SeekPerformance perf;
        perf.frame_number = frame_number;
        perf.seek_time_ms = seek_time_ms;
        perf.is_keyframe = true;
        perf.success = success;
        
        performances.push_back(perf);
        
        if ((i + 1) % 10 == 0 || i == test_count - 1) {
            std::cout << "Keyframe seek progress: " << (i + 1) << "/" << test_count << std::endl;
        }
    }
    
    return performances;
}

std::vector<SeekPerformance> testNonKeyframeSeeks(VideoDecoderBase* decoder, int test_count = 30) {
    std::cout << "\n=== Testing Non-Keyframe Seeks (" << test_count << " seeks) ===" << std::endl;
    
    std::vector<SeekPerformance> performances;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) {
        std::cout << "No frames available for testing" << std::endl;
        return performances;
    }
    
    auto frame_index = decoder->getFrameIndex();
    
    // Find non-keyframes
    std::vector<int> non_keyframes;
    for (size_t i = 0; i < frame_index.size(); ++i) {
        if (!frame_index[i].is_keyframe) {
            non_keyframes.push_back(i);
        }
    }
    
    if (non_keyframes.empty()) {
        std::cout << "No non-keyframes found for testing" << std::endl;
        return performances;
    }
    
    std::cout << "Found " << non_keyframes.size() << " non-keyframes in video" << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, non_keyframes.size() - 1);
    
    for (int i = 0; i < test_count; ++i) {
        int non_keyframe_idx = dis(gen);
        int frame_number = non_keyframes[non_keyframe_idx];
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(frame_number);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double seek_time_ms = duration.count() / 1000.0;
        
        SeekPerformance perf;
        perf.frame_number = frame_number;
        perf.seek_time_ms = seek_time_ms;
        perf.is_keyframe = false;
        perf.success = success;
        
        performances.push_back(perf);
        
        if ((i + 1) % 10 == 0 || i == test_count - 1) {
            std::cout << "Non-keyframe seek progress: " << (i + 1) << "/" << test_count << std::endl;
        }
    }
    
    return performances;
}

void testDecodingPerformance(VideoDecoderBase* decoder, int frame_count = 100) {
    std::cout << "\n=== Testing Decoding Performance (" << frame_count << " frames) ===" << std::endl;
    
    // Reset to beginning
    decoder->seekToFrame(0);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int decoded_count = 0;
    
    for (int i = 0; i < frame_count; ++i) {
        AVFrame* frame = decoder->decodeNextFrame();
        if (frame) {
            decoded_count++;
            // Note: In real applications, frame should be released, but for performance testing we may need special handling
        } else {
            break; // End of file or decode failure
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double total_time_ms = duration.count();
    
    double avg_time_per_frame = decoded_count > 0 ? (total_time_ms / decoded_count) : 0.0;
    double decoding_fps = decoded_count > 0 ? (1000.0 * decoded_count / total_time_ms) : 0.0;
    
    std::cout << "📊 DECODING PERFORMANCE (CPU intensive):" << std::endl;
    std::cout << "Frames Decoded: " << decoded_count << "/" << frame_count << std::endl;
    std::cout << "Total Time: " << std::fixed << std::setprecision(2) << total_time_ms << " ms" << std::endl;
    std::cout << "Avg Per Frame: " << std::fixed << std::setprecision(2) << avg_time_per_frame << " ms" << std::endl;
    std::cout << "Decoding FPS: " << std::fixed << std::setprecision(2) << decoding_fps << std::endl;
    std::cout << "🎯 " << (decoder->getStats().is_hardware_accelerated ? "Hardware" : "Software") 
              << " decoder is " << std::fixed << std::setprecision(1) << decoding_fps << "x realtime (at 30fps)" << std::endl;
}

void printDecoderInfo(VideoDecoderBase* decoder) {
    if (!decoder || !decoder->isInitialized()) {
        std::cout << "Decoder not initialized" << std::endl;
        return;
    }
    
    std::cout << "\n=== Decoder Information ===" << std::endl;
    std::cout << "Decoder Name: " << decoder->getDecoderName() << std::endl;
    std::cout << "Decoder Type: " << static_cast<int>(decoder->getDecoderType()) << std::endl;
    
    if (decoder->hasVideoStream()) {
        std::cout << "Video Size: " << decoder->getVideoWidth() << "x" << decoder->getVideoHeight() << std::endl;
        std::cout << "Frame Rate: " << decoder->getVideoFPS() << " fps" << std::endl;
        std::cout << "Duration: " << std::fixed << std::setprecision(2) << decoder->getDuration() << " seconds" << std::endl;
        std::cout << "Total Frames: " << decoder->getTotalFrames() << std::endl;
    }
    
    if (decoder->hasAudioStream()) {
        std::cout << "Audio Sample Rate: " << decoder->getAudioSampleRate() << " Hz" << std::endl;
        std::cout << "Audio Channels: " << decoder->getAudioChannels() << std::endl;
    }
    
    auto stats = decoder->getStats();
    std::cout << "\n=== Current Statistics ===" << std::endl;
    std::cout << "Frames Decoded: " << stats.total_frames_decoded << std::endl;
    std::cout << "Avg Decode Time: " << std::fixed << std::setprecision(2) << stats.average_decode_time_ms << " ms" << std::endl;
    std::cout << "Decode FPS: " << std::fixed << std::setprecision(2) << stats.fps << std::endl;
    std::cout << "Hardware Accel: " << (stats.is_hardware_accelerated ? "Yes" : "No") << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

void compareDecoderPerformance(const std::vector<DecoderPerformanceStats>& all_stats) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "                    Decoder Performance Comparison Report" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    // Table header
    std::cout << std::left;
    std::cout << std::setw(20) << "Decoder" << " | ";
    std::cout << std::setw(8) << "Type" << " | ";
    std::cout << std::setw(8) << "Success%" << " | ";
    std::cout << std::setw(10) << "Avg(ms)" << " | ";
    std::cout << std::setw(10) << "Median" << " | ";
    std::cout << std::setw(10) << "Keyframe" << " | ";
    std::cout << std::setw(10) << "Non-Key" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    // Data rows
    for (const auto& stats : all_stats) {
        std::cout << std::left;
        std::cout << std::setw(20) << stats.decoder_name.substr(0, 19) << " | ";
        std::cout << std::setw(8) << static_cast<int>(stats.decoder_type) << " | ";
        std::cout << std::setw(7) << std::fixed << std::setprecision(1) << stats.success_rate << "% | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(2) << stats.avg_seek_time_ms << " | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(2) << stats.median_seek_time_ms << " | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(2) << stats.keyframe_avg_ms << " | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(2) << stats.non_keyframe_avg_ms << std::endl;
    }
    
    std::cout << std::string(80, '=') << std::endl;
    
    // Find best performers
    if (!all_stats.empty()) {
        auto best_avg = std::min_element(all_stats.begin(), all_stats.end(),
            [](const auto& a, const auto& b) { return a.avg_seek_time_ms < b.avg_seek_time_ms; });
        
        auto best_success = std::max_element(all_stats.begin(), all_stats.end(),
            [](const auto& a, const auto& b) { return a.success_rate < b.success_rate; });
        
        std::cout << "\n🏆 Performance Champions:" << std::endl;
        std::cout << "  Fastest Average: " << best_avg->decoder_name 
                  << " (" << std::fixed << std::setprecision(2) << best_avg->avg_seek_time_ms << " ms)" << std::endl;
        std::cout << "  Highest Success Rate: " << best_success->decoder_name 
                  << " (" << std::fixed << std::setprecision(1) << best_success->success_rate << "%)" << std::endl;
    }
}

void testAllDecoders(const std::string& video_path) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "                    Complete Decoder Performance Test" << std::endl;
    std::cout << "                    File: " << video_path << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    std::vector<DecoderPerformanceStats> all_performance_stats;
    
    // 1. Test software decoder
    std::cout << "\n📀 Testing Software Decoder..." << std::endl;
    auto software_decoder = VideoDecoderFactory::createSoftwareDecoder();
    if (software_decoder && software_decoder->initialize(video_path)) {
        printDecoderInfo(software_decoder.get());
        
        // Build frame index
        std::cout << "Building frame index..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        software_decoder->buildFrameIndex();
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Frame index built successfully, time: " << duration.count() << " ms" << std::endl;
        
        // Show some frame info
        auto frame_index = software_decoder->getFrameIndex();
        printFrameInfo(frame_index, 0, 5);
        
        // Test decoding performance
        testDecodingPerformance(software_decoder.get(), 50);
        
        // Test multiple seek scenarios
        std::cout << "\n🔍 COMPREHENSIVE SEEK PERFORMANCE TESTS" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        // 1. Random seek performance - more tests
        auto random_perf = testRandomSeekPerformance(software_decoder.get(), 100);
        auto random_stats = calculatePerformanceStats(random_perf, "Software Decoder (Random)", DecoderType::SOFTWARE);
        printPerformanceStats(random_stats);
        all_performance_stats.push_back(random_stats);
        
        // 2. Sequential seek performance - different steps
        auto sequential_perf_50 = testSequentialSeekPerformance(software_decoder.get(), 50);
        auto sequential_stats_50 = calculatePerformanceStats(sequential_perf_50, "Software Decoder (Seq-50)", DecoderType::SOFTWARE);
        printPerformanceStats(sequential_stats_50);
        
        auto sequential_perf_200 = testSequentialSeekPerformance(software_decoder.get(), 200);
        auto sequential_stats_200 = calculatePerformanceStats(sequential_perf_200, "Software Decoder (Seq-200)", DecoderType::SOFTWARE);
        printPerformanceStats(sequential_stats_200);
        
        // 3. Backward seek performance
        auto backward_perf = testBackwardSeekPerformance(software_decoder.get(), 50);
        auto backward_stats = calculatePerformanceStats(backward_perf, "Software Decoder (Backward)", DecoderType::SOFTWARE);
        printPerformanceStats(backward_stats);
        
        // 4. Short distance vs long distance seeks
        auto short_seek_perf = testShortDistanceSeeks(software_decoder.get(), 50);
        auto short_seek_stats = calculatePerformanceStats(short_seek_perf, "Software Decoder (Short Distance)", DecoderType::SOFTWARE);
        printPerformanceStats(short_seek_stats);
        
        auto long_seek_perf = testLongDistanceSeeks(software_decoder.get(), 30);
        auto long_seek_stats = calculatePerformanceStats(long_seek_perf, "Software Decoder (Long Distance)", DecoderType::SOFTWARE);
        printPerformanceStats(long_seek_stats);
        
        // 5. Keyframe vs Non-keyframe comparison
        auto keyframe_perf = testKeyframeSeeks(software_decoder.get(), 30);
        auto keyframe_stats = calculatePerformanceStats(keyframe_perf, "Software Decoder (Keyframes)", DecoderType::SOFTWARE);
        printPerformanceStats(keyframe_stats);
        
        auto non_keyframe_perf = testNonKeyframeSeeks(software_decoder.get(), 30);
        auto non_keyframe_stats = calculatePerformanceStats(non_keyframe_perf, "Software Decoder (Non-Keyframes)", DecoderType::SOFTWARE);
        printPerformanceStats(non_keyframe_stats);
        
        software_decoder->close();
    } else {
        std::cout << "❌ Software decoder initialization failed" << std::endl;
    }
    
    // 2. Test all available hardware decoders
    auto hw_decoders = VideoDecoderBase::getAvailableHardwareDecoders();
    std::cout << "\n🔧 Detected Hardware Decoders:" << std::endl;
    for (const auto& hw_info : hw_decoders) {
        std::cout << "  " << hw_info.name << ": " 
                  << (hw_info.available ? "✓ Available" : "✗ Not Available") 
                  << " - " << hw_info.description << std::endl;
    }
    
    // Test each available hardware decoder
    std::vector<HardwareDecoderType> test_hw_types = {
        HardwareDecoderType::CUDA,
        HardwareDecoderType::D3D11VA,
        HardwareDecoderType::DXVA2,
        HardwareDecoderType::QSV
    };
    
    for (auto hw_type : test_hw_types) {
        // Check if this hardware type is available
        auto it = std::find_if(hw_decoders.begin(), hw_decoders.end(),
            [hw_type](const HardwareDecoderInfo& info) {
                return info.type == hw_type && info.available;
            });
        
        if (it == hw_decoders.end()) {
            continue; // Skip unavailable hardware decoders
        }
        
        std::cout << "\n🚀 Testing " << it->name << " Hardware Decoder..." << std::endl;
        
        auto hardware_decoder = VideoDecoderFactory::createHardwareDecoder(hw_type);
        if (hardware_decoder && hardware_decoder->initialize(video_path)) {
            printDecoderInfo(hardware_decoder.get());
            
            // Build frame index
            std::cout << "Building frame index..." << std::endl;
            auto start_time = std::chrono::high_resolution_clock::now();
            hardware_decoder->buildFrameIndex();
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << "Frame index built successfully, time: " << duration.count() << " ms" << std::endl;
            
            // Test decoding performance
            testDecodingPerformance(hardware_decoder.get(), 50);
            
            // Test comprehensive seek performance for hardware decoder
            std::cout << "\n🔍 COMPREHENSIVE SEEK PERFORMANCE TESTS (Hardware)" << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            
            // Random seek performance
            auto random_perf = testRandomSeekPerformance(hardware_decoder.get(), 100);
            auto random_stats = calculatePerformanceStats(random_perf, it->name + " (Random)", hardwareTypeToDecoderType(hw_type));
            printPerformanceStats(random_stats);
            all_performance_stats.push_back(random_stats);
            
            // Sequential seek performance - different steps
            auto sequential_perf_50 = testSequentialSeekPerformance(hardware_decoder.get(), 50);
            auto sequential_stats_50 = calculatePerformanceStats(sequential_perf_50, it->name + " (Seq-50)", hardwareTypeToDecoderType(hw_type));
            printPerformanceStats(sequential_stats_50);
            
            // Backward seek performance
            auto backward_perf = testBackwardSeekPerformance(hardware_decoder.get(), 50);
            auto backward_stats = calculatePerformanceStats(backward_perf, it->name + " (Backward)", hardwareTypeToDecoderType(hw_type));
            printPerformanceStats(backward_stats);
            
            // Short distance vs long distance seeks
            auto short_seek_perf = testShortDistanceSeeks(hardware_decoder.get(), 50);
            auto short_seek_stats = calculatePerformanceStats(short_seek_perf, it->name + " (Short)", hardwareTypeToDecoderType(hw_type));
            printPerformanceStats(short_seek_stats);
            
            auto long_seek_perf = testLongDistanceSeeks(hardware_decoder.get(), 30);
            auto long_seek_stats = calculatePerformanceStats(long_seek_perf, it->name + " (Long)", hardwareTypeToDecoderType(hw_type));
            printPerformanceStats(long_seek_stats);
            
            // Keyframe vs Non-keyframe comparison
            auto keyframe_perf = testKeyframeSeeks(hardware_decoder.get(), 30);
            auto keyframe_stats = calculatePerformanceStats(keyframe_perf, it->name + " (Keyframes)", hardwareTypeToDecoderType(hw_type));
            printPerformanceStats(keyframe_stats);
            
            auto non_keyframe_perf = testNonKeyframeSeeks(hardware_decoder.get(), 30);
            auto non_keyframe_stats = calculatePerformanceStats(non_keyframe_perf, it->name + " (Non-Keyframes)", hardwareTypeToDecoderType(hw_type));
            printPerformanceStats(non_keyframe_stats);
            
            hardware_decoder->close();
        } else {
            std::cout << "❌ " << it->name << " hardware decoder initialization failed" << std::endl;
        }
    }
    
    // 3. Compare all decoder performance
    if (!all_performance_stats.empty()) {
        compareDecoderPerformance(all_performance_stats);
    }
}

void testHardwareDecoderSupport() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "                Hardware Decoder Support Detection" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    auto hw_decoders = VideoDecoderBase::getAvailableHardwareDecoders();
    
    if (hw_decoders.empty()) {
        std::cout << "❌ No hardware decoders detected" << std::endl;
        return;
    }
    
    std::cout << "Detected " << hw_decoders.size() << " hardware decoders:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    for (const auto& decoder : hw_decoders) {
        std::string status = decoder.available ? "✓ Available" : "✗ Not Available";
        std::cout << std::left;
        std::cout << std::setw(20) << decoder.name << " | ";
        std::cout << std::setw(12) << status << " | ";
        std::cout << decoder.description << std::endl;
    }
    
    std::cout << std::string(60, '=') << std::endl;
}

int main(int argc, char* argv[]) {
    // Initialize logging system
    Logger::initialize();
    
    std::cout << "🎬 StreamKit Decoder Performance Test Tool" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // 1. Detect hardware decoder support
    testHardwareDecoderSupport();
    
    // 2. If video file is provided, perform complete performance test
    if (argc > 1) {
        std::string video_file = argv[1];
        std::cout << "\n🎥 Starting complete performance test..." << std::endl;
        std::cout << "Video file: " << video_file << std::endl;
        
        testAllDecoders(video_file);
        
    } else {
        std::cout << "\n💡 Usage: " << argv[0] << " <video_file>" << std::endl;
        std::cout << "Example: " << argv[0] << " test_video.mp4" << std::endl;
        std::cout << "\nFor complete performance testing, please provide a video file." << std::endl;
    }
    
    std::cout << "\n✅ Test completed!" << std::endl;
    return 0;
} 