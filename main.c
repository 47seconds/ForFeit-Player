// https://www.ffmpeg.org/doxygen/trunk/index.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>

/*
int avformat_open_input 	( 	AVFormatContext **  	ps,
    const char *  	url,
    const AVInputFormat *  	fmt,
    AVDictionary **  	options 
)

- Parameters
    ps	Pointer to user-supplied AVFormatContext (allocated by avformat_alloc_context). May be a pointer to NULL, in which case an AVFormatContext is allocated by this function and written into ps. Note that a user-supplied AVFormatContext will be freed on failure and its pointer set to NULL.
    url	URL of the stream to open.
    fmt	If non-NULL, this parameter forces a specific input format. Otherwise the format is autodetected.
    options	A dictionary filled with AVFormatContext and demuxer-private options. On return this parameter will be destroyed and replaced with a dict containing options that were not found. May be NULL.

- Returns
    0 on success; on failure: frees ps, sets its pointer to NULL, and returns a negative AVERROR.
*/

/*
int avformat_find_stream_info 	( 	AVFormatContext *  	ic,
		AVDictionary **  	options 
	) 

- Parameters
    ic	media file handle
    options	If non-NULL, an ic.nb_streams long array of pointers to dictionaries, where i-th member contains options for codec corresponding to i-th stream. On return each dictionary will be filled with options that were not found.

- Returns
    >=0 if OK, AVERROR_xxx on error
*/

int main(int argc, char **argv) {
    /*
    |================================================================|
    |                                                                |
    | Step 0: Check command line arguments                           |
    |                                                                |
    |================================================================|
   */

    if (argc < 3) {
        fprintf(stderr, "Usage: %s t1.c <videofile.mp4> <outputfile>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*
    |=======================================================================|
    |                                                                       |
    | Step 1: Open the input file, get the format context and streams       |
    |                                                                       |
    |=======================================================================|
   */

    // https://www.ffmpeg.org/doxygen/trunk/structAVFormatContext.html
    AVFormatContext *ctx = NULL;
    int file_open = avformat_open_input(&ctx, argv[1], NULL, NULL);
    if (file_open != 0) {
        fprintf(stderr, "Could not open file: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // The above code attempts to allocate an AVFormatContext, open the specified file (autodetecting the format) and read the header, exporting the information stored there into s. Some formats do not have a header or do not store enough information there, so it is recommended that you call the avformat_find_stream_info() function which tries to read and decode a few frames to find missing information.
    int stream_info = avformat_find_stream_info(ctx, NULL);
    if (stream_info < 0) {
        fprintf(stderr, "Could not find stream information\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    // https://www.ffmpeg.org/doxygen/trunk/structAVStream.html
    // Find the first video stream, practically there's only 1 video stream in most videos
    int video_stream_inx = -1;
    for (unsigned int i = 0; i < ctx->nb_streams; i++) {
        if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_inx = i;
            break;
        }
    }

    if (video_stream_inx == -1) {
        fprintf(stderr, "Could not find a video stream in the input file\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    // Find the first audio stream, practically there's multiple audio streams in most videos, but for now we just get the first one
    int audio_stream_inx = -1;
    for (unsigned int i = 0; i < ctx->nb_streams; i++) {
        if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_inx = i;
            break;
        }
    }

    if (audio_stream_inx == -1) {
        fprintf(stderr, "Could not find an audio stream in the input file\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    /*
    |===================================================================|
    |                                                                   |
    | Step 2: Get decoder contexts for the video and audio streams      |  
    |                                                                   |
    |===================================================================|
   */

    // get streams codec info context
    AVCodec *video_decodec = avcodec_find_decoder(ctx->streams[video_stream_inx]->codecpar->codec_id);
    if (!video_decodec) {
        fprintf(stderr, "Invalid video codec\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    AVCodec *audio_decodec = avcodec_find_decoder(ctx->streams[audio_stream_inx]->codecpar->codec_id);
    if (!audio_decodec) {
        fprintf(stderr, "Invalid audio codec\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    /*
    |===================================================================|
    |                                                                   |
    | Step 3: Setup codec contexts for the video and audio streams      |  
    |                                                                   |
    |===================================================================|
   */

    /*
    - Stream Context (codecpar):
    Read-only metadata from file headers
    Contains static information (resolution, format, bitrate)
    Shared across the entire stream
    Tied to the file - destroyed when file closes

    - Codec Context:
    Working context for actual decoding operations
    Mutable - can be configured with decoder options
    Independent - can exist without the file
    Thread-specific - each decoder needs its own
    */

    // In short, we imbedding the (de)codec parameters from the stream context into a codec context for actual decoding operations

    // allocate codec contexts for respective (de)codecs
    AVCodecContext *video_codec_ctx = avcodec_alloc_context3(video_decodec);
    if (!video_codec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    AVCodecContext *audio_codec_ctx = avcodec_alloc_context3(audio_decodec);
    if (!audio_codec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    /*
    int avcodec_parameters_from_context 	( 	struct AVCodecParameters *  	par,
		const AVCodecContext *  	codec 
	)
    
    Fill the parameters struct based on the values from the supplied codec context.

    Any allocated fields in par are freed and replaced with duplicates of the corresponding fields in codec.

    - Returns
        >= 0 on success, a negative AVERROR code on failure 
    */

    // copy codec parameters from stream context to codec context
    int video_ctx_copy_success = avcodec_parameters_to_context(video_codec_ctx, ctx->streams[video_stream_inx]->codecpar);
    if (video_ctx_copy_success < 0) {
        fprintf(stderr, "Could not copy video codec parameters to codec context\n");
        avcodec_free_context(&video_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    int audio_ctx_copy_success = avcodec_parameters_to_context(audio_codec_ctx, ctx->streams[audio_stream_inx]->codecpar);
    if (audio_ctx_copy_success < 0) {
        fprintf(stderr, "Could not copy audio codec parameters to codec context\n");
        avcodec_free_context(&video_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    /*
    |===================================================================|
    |                                                                   |
    | Step 4: Initialize decoders for the video and audio streams       |  
    |                                                                   |
    |===================================================================|
   */

    // initialize decoders

    /*
    Initialize the AVCodecContext to use the given AVCodec.

    Prior to using this function the context has to be allocated with avcodec_alloc_context3().

    The functions avcodec_find_decoder_by_name(), avcodec_find_encoder_by_name(), avcodec_find_decoder() and avcodec_find_encoder() provide an easy way for retrieving a codec.

    Depending on the codec, you might need to set options in the codec context also for decoding (e.g. width, height, or the pixel or audio sample format in the case the information is not available in the bitstream, as when decoding raw audio or video).

    Options in the codec context can be set either by setting them in the options AVDictionary, or by setting the values in the context itself, directly or by using the av_opt_set() API before calling this function.
    */
    
    /*
    int avcodec_open2 	( 	AVCodecContext *  	avctx,
		const AVCodec *  	codec,
		AVDictionary **  	options 
	)
    
    - Note
        Always call this function before using decoding routines (such as avcodec_receive_frame()).

    - Parameters
        avctx	The context to initialize.
        codec	The codec to open this context for. If a non-NULL codec has been previously passed to avcodec_alloc_context3() or for this context, then this parameter MUST be either NULL or equal to the previously passed codec.
        options	A dictionary filled with AVCodecContext and codec-private options, which are set on top of the options already set in avctx, can be NULL. On return this object will be filled with options that were not found in the avctx codec context.

    - Returns
        zero on success, a negative value on error 
    */

    int video_decoder_init = avcodec_open2(video_codec_ctx, video_decodec, NULL);
    if (video_decoder_init < 0) {
        fprintf(stderr, "Could not open video decoder\n");
        avcodec_free_context(&video_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    int audio_decoder_init = avcodec_open2(audio_codec_ctx, audio_decodec, NULL);
    if (audio_decoder_init < 0) {
        fprintf(stderr, "Could not open audio decoder\n");
        avcodec_free_context(&video_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    /*
    |=============================================================================================|
    |                                                                                             |
    | Step 5: Allocating memory for encoded packets from reading file and decoded A/V frame       |  
    |                                                                                             |
    |=============================================================================================|
   */

   AVPacket *packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "Could not allocate packet\n");
        avcodec_free_context(&video_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        avcodec_free_context(&video_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}