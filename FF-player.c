// https://www.ffmpeg.org/doxygen/trunk/index.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

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
    if (video_decodec == NULL) {
        fprintf(stderr, "Invalid video codec\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    AVCodec *audio_decodec = avcodec_find_decoder(ctx->streams[audio_stream_inx]->codecpar->codec_id);
    if (audio_decodec == NULL) {
        fprintf(stderr, "Invalid audio codec\n");
        avformat_close_input(&ctx);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}