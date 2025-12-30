extern "C" {
#include <libavcodec/avcodec.h>
}
#include <cstdio>

int main() {
    // Uncomment if needed
    // avcodec_register_all();

    void* it = nullptr;
    const AVCodec* codec = nullptr;

    printf("All H.264 and HEVC decoders in FFmpeg:\n");
    printf("======================================\n");

    while ((codec = av_codec_iterate(&it))) {
        if (codec->type == AVMEDIA_TYPE_VIDEO && av_codec_is_decoder(codec)) {
            if (codec->id == AV_CODEC_ID_H264) {
                printf("  H.264: %-30s\n", codec->name);
            } else if (codec->id == AV_CODEC_ID_HEVC) {
                printf("  HEVC:  %-30s\n", codec->name);
            }
        }
    }

    printf("\n\nLooking for specific hardware decoders:\n");
    printf("======================================\n");

    const char* target_decoders[] = {
        "h264_d3d11va", "h264_dxva2", "h264_cuvid", "h264_qsv",
        "hevc_d3d11va", "hevc_dxva2", "hevc_cuvid", "hevc_qsv",
        nullptr
    };

    for (int i = 0; target_decoders[i]; i++) {
        const AVCodec* c = avcodec_find_decoder_by_name(target_decoders[i]);
        printf("  %-20s: %s\n", target_decoders[i], c ? "FOUND" : "NOT FOUND");
    }

    return 0;
}
