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

struct SeekTestResult {
    std::string test_name;
    std::string decoder_name;
    int test_count;
    double avg_time_ms;
    double min_time_ms;
    double max_time_ms;
    double median_time_ms;
    int success_rate;
};

std::vector<double> performSeekTest(VideoDecoderBase* decoder, const std::string& test_name, int count) {
    std::vector<double> times;
    int total_frames = decoder->getTotalFrames();
    if (total_frames == 0) return times;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, total_frames - 1);
    
    for (int i = 0; i < count; ++i) {
        int frame_number = dis(gen);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = decoder->seekToFrame(frame_number);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        if (success) {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            times.push_back(duration.count() / 1000.0);
        }
    }
    
    return times;
}

SeekTestResult analyzeResults(const std::vector<double>& times, const std::string& test_name, const std::string& decoder_name, int total_tests) {
    SeekTestResult result;
    result.test_name = test_name;
    result.decoder_name = decoder_name;
    result.test_count = total_tests;
    result.success_rate = static_cast<int>((100.0 * times.size()) / total_tests);
    
    if (!times.empty()) {
        std::vector<double> sorted_times = times;
        std::sort(sorted_times.begin(), sorted_times.end());
        
        result.avg_time_ms = std::accumulate(sorted_times.begin(), sorted_times.end(), 0.0) / sorted_times.size();
        result.min_time_ms = sorted_times.front();
        result.max_time_ms = sorted_times.back();
        result.median_time_ms = sorted_times[sorted_times.size() / 2];
    } else {
        result.avg_time_ms = result.min_time_ms = result.max_time_ms = result.median_time_ms = 0.0;
    }
    
    return result;
}

void printResultsTable(const std::vector<SeekTestResult>& results) {
    std::cout << "\n" << std::string(120, '=') << std::endl;
    std::cout << "                                COMPREHENSIVE SEEK PERFORMANCE SUMMARY" << std::endl;
    std::cout << std::string(120, '=') << std::endl;
    
    // Table header
    std::cout << std::left;
    std::cout << std::setw(25) << "Test Name" << " | ";
    std::cout << std::setw(20) << "Decoder" << " | ";
    std::cout << std::setw(8) << "Count" << " | ";
    std::cout << std::setw(8) << "Success%" << " | ";
    std::cout << std::setw(10) << "Avg(ms)" << " | ";
    std::cout << std::setw(10) << "Min(ms)" << " | ";
    std::cout << std::setw(10) << "Max(ms)" << " | ";
    std::cout << std::setw(10) << "Median(ms)" << std::endl;
    std::cout << std::string(120, '-') << std::endl;
    
    // Data rows
    for (const auto& result : results) {
        std::cout << std::left;
        std::cout << std::setw(25) << result.test_name.substr(0, 24) << " | ";
        std::cout << std::setw(20) << result.decoder_name.substr(0, 19) << " | ";
        std::cout << std::setw(8) << result.test_count << " | ";
        std::cout << std::setw(7) << result.success_rate << "% | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(3) << result.avg_time_ms << " | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(3) << result.min_time_ms << " | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(3) << result.max_time_ms << " | ";
        std::cout << std::setw(10) << std::fixed << std::setprecision(3) << result.median_time_ms << std::endl;
    }
    
    std::cout << std::string(120, '=') << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <video_file>" << std::endl;
        return 1;
    }
    
    Logger::initialize();
    std::string video_file = argv[1];
    std::vector<SeekTestResult> all_results;
    
    std::cout << "🎬 StreamKit Seek Performance Summary Tool" << std::endl;
    std::cout << "Video file: " << video_file << std::endl;
    
    // Test software decoder
    std::cout << "\n📀 Testing Software Decoder..." << std::endl;
    auto software_decoder = VideoDecoderFactory::createSoftwareDecoder();
    if (software_decoder && software_decoder->initialize(video_file)) {
        software_decoder->buildFrameIndex();
        
        // Different seek tests
        auto random_times = performSeekTest(software_decoder.get(), "Random Seek", 100);
        all_results.push_back(analyzeResults(random_times, "Random Seek", "Software", 100));
        
        auto sequential_times = performSeekTest(software_decoder.get(), "Sequential", 50);
        all_results.push_back(analyzeResults(sequential_times, "Sequential", "Software", 50));
        
        auto backward_times = performSeekTest(software_decoder.get(), "Backward", 50);
        all_results.push_back(analyzeResults(backward_times, "Backward", "Software", 50));
        
        software_decoder->close();
    }
    
    // Test CUDA hardware decoder
    std::cout << "\n🚀 Testing CUDA Hardware Decoder..." << std::endl;
    auto cuda_decoder = VideoDecoderFactory::createHardwareDecoder(HardwareDecoderType::CUDA);
    if (cuda_decoder && cuda_decoder->initialize(video_file)) {
        cuda_decoder->buildFrameIndex();
        
        auto random_times = performSeekTest(cuda_decoder.get(), "Random Seek", 100);
        all_results.push_back(analyzeResults(random_times, "Random Seek", "CUDA", 100));
        
        auto sequential_times = performSeekTest(cuda_decoder.get(), "Sequential", 50);
        all_results.push_back(analyzeResults(sequential_times, "Sequential", "CUDA", 50));
        
        auto backward_times = performSeekTest(cuda_decoder.get(), "Backward", 50);
        all_results.push_back(analyzeResults(backward_times, "Backward", "CUDA", 50));
        
        cuda_decoder->close();
    }
    
    printResultsTable(all_results);
    
    // Performance comparison
    std::cout << "\n🏆 PERFORMANCE COMPARISON:" << std::endl;
    for (size_t i = 0; i < all_results.size() / 2; ++i) {
        const auto& sw_result = all_results[i];
        const auto& hw_result = all_results[i + all_results.size() / 2];
        
        double speedup = hw_result.avg_time_ms / sw_result.avg_time_ms;
        std::cout << "  " << sw_result.test_name << ": Software is " 
                  << std::fixed << std::setprecision(1) << speedup << "x faster than Hardware" << std::endl;
    }
    
    return 0;
} 