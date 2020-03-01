#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define PACKETS_TO_PROCESS_NUM		12
#define FRAMES_DIR_TO_SAVE			"0_frames"

// print out the steps and errors
static void logging(const char *fmt, ...);
// decode packets into frames
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame);
// save a frame into a .pgm file
static void save_gray_frame(unsigned char **buf, int *wrap, int xsize, int ysize, char *filename);
// initialize codecContext, frame and packet
static int init_p_codec_packet_frame(AVCodecContext** pCodecContext, AVFrame** pFrame, 
		AVPacket** pPacket, AVCodecParameters *pCodecParameters, AVCodec *pCodec);


int main(int argc, const char *argv[])
{
  if (argc < 2) {
    printf("You need to specify a media file.\n");
    return -1;
  }
  
  // Section 1
  logging("initializing all the containers, codecs and protocols.");
  // AVFormatContext holds the header information from the format
  // Allocating memory for this component
  AVFormatContext *pFormatContext = avformat_alloc_context();
  if (!pFormatContext) {
    logging("ERROR could not allocate memory for Format Context");
    return -1;
  }

  // Section 1.1
  logging("opening the input file (%s) and loading format (container) header", argv[1]);
  // Open the file and read its header. The codecs are not opened.
  // The function arguments are:
  // AVFormatContext (the component we allocated memory for),
  // url (filename),
  // AVInputFormat (if you pass NULL it'll do the auto detect)
  // and AVDictionary (which are options to the demuxer)
  if (avformat_open_input(&pFormatContext, argv[1], NULL, NULL) != 0) {
    logging("ERROR could not open the file");
    return -1;
  }

  // Section 1.2
  // now we have access to some information about our file
  // since we read its header we can say what format (container) it's
  // and some other information related to the format itself.
  logging("format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, 
		  pFormatContext->duration, pFormatContext->bit_rate);

  // Section 2
  logging("finding stream info from format");
  // read Packets from the Format to get stream information
  // this function populates pFormatContext->streams
  // (of size equals to pFormatContext->nb_streams)
  // the arguments are:
  // the AVFormatContext
  // and options contains options for codec corresponding to i-th stream.
  // On return each dictionary will be filled with options that were not found.
  if (avformat_find_stream_info(pFormatContext,  NULL) < 0) {
    logging("ERROR could not get the stream info");
    return -1;
  }

  // Section 3
  // the component that knows how to enCOde and DECode the stream
  // it's the codec (audio or video)
  AVCodec *pCodec = NULL;
  
  // this component describes the properties of a codec used by the stream i
  AVCodecParameters *pCodecParameters =  NULL;
  int video_stream_index = -1;

  // Section 3.1
  // loop though all the streams and print its main information
  for (int i = 0; i < pFormatContext->nb_streams; i++)
  {
	// Section 3.2
    AVCodecParameters *pLocalCodecParameters =  NULL;
    pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
    logging("AVStream->time_base before open coded %d/%d", pFormatContext->streams[i]
			->time_base.num, pFormatContext->streams[i]->time_base.den);
    logging("AVStream->r_frame_rate before open coded %d/%d", pFormatContext->streams[i]
			->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
    logging("AVStream->start_time %" PRId64, pFormatContext->streams[i]->start_time);
    logging("AVStream->duration %" PRId64, pFormatContext->streams[i]->duration);
	// Section 3.3
    logging("finding the proper decoder (CODEC)");
    AVCodec *pLocalCodec = NULL;
    // finds the registered decoder for a codec ID
    pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
    if (pLocalCodec==NULL) {
      logging("ERROR unsupported codec!");
      return -1;
    }
    // when the stream is a video we store its index, codec parameters and codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream_index == -1) {
        video_stream_index = i;
        pCodec = pLocalCodec;
        pCodecParameters = pLocalCodecParameters;
      }
      logging("Video Codec: resolution %d x %d", pLocalCodecParameters->width, 
			  pLocalCodecParameters->height);
    } 
	else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
      logging("Audio Codec: %d channels, sample rate %d", pLocalCodecParameters->channels, 
			  pLocalCodecParameters->sample_rate);
    }
    // print its name, id and bitrate
    logging("\tCodec %s ID %d bit_rate %lld", pLocalCodec->name, pLocalCodec->id, 
			pCodecParameters->bit_rate);
  }

  AVCodecContext *pCodecContext = NULL;
  AVFrame *pFrame = NULL;
  AVPacket *pPacket = NULL;

  // Section 4	Initializations
  if (init_p_codec_packet_frame(&pCodecContext, &pFrame, &pPacket, pCodecParameters, pCodec) < 0)
      logging("ERROR on initializing codec, packet and frame.!");

  // Section 5
  int response = 0;
  int how_many_packets_to_process = PACKETS_TO_PROCESS_NUM;
  // fill the Packet with data from the Stream
  while (av_read_frame(pFormatContext, pPacket) >= 0)
  {
    // if it's the video stream
    if (pPacket->stream_index == video_stream_index) {
	  logging("AVPacket->pts %" PRId64, pPacket->pts);
      response = decode_packet(pPacket, pCodecContext, pFrame);
      if (response < 0)
        break;
      // stop it, otherwise we'll be saving hundreds of frames
      if (--how_many_packets_to_process <= 0) break;
    }
    av_packet_unref(pPacket);
  }

  logging("releasing all the resources");

  avformat_close_input(&pFormatContext);
  avformat_free_context(pFormatContext);
  av_packet_free(&pPacket);
  av_frame_free(&pFrame);
  avcodec_free_context(&pCodecContext);
  return 0;
}

static void logging(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "LOG: " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame)
{
  // Supply raw packet data as input to a decoder
  int response = avcodec_send_packet(pCodecContext, pPacket);
  if (response < 0) {
    logging("Error while sending a packet to the decoder: %s", av_err2str(response));
    return response;
  }

  while (response >= 0)
  {
    // Return decoded output data (into a frame) from a decoder
    response = avcodec_receive_frame(pCodecContext, pFrame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      logging("Error while receiving a frame from the decoder: %s", av_err2str(response));
      return response;
    }

    if (response >= 0) {
      logging(
          "Frame %d (type=%c, size=%d bytes) pts %d key_frame %d [DTS %d]",
          pCodecContext->frame_number,
          av_get_picture_type_char(pFrame->pict_type),
          pFrame->pkt_size,
          pFrame->pts,
          pFrame->key_frame,
          pFrame->coded_picture_number
      );

      char frame_filename[1024];
      snprintf(frame_filename, sizeof(frame_filename), "%s/%s-%d.pgm", FRAMES_DIR_TO_SAVE, "frame", 
			  pCodecContext->frame_number);
      // save a grayscale frame into a .pgm file
      save_gray_frame(pFrame->data, pFrame->linesize, pFrame->width, pFrame->height, 
			  frame_filename);
    }
  }
  return 0;
}

static void save_gray_frame(unsigned char **buf, int* wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    f = fopen(filename,"w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

	//for(int i = 0; i < ysize; i++)
	//{
	//	for(int j = 0; j < xsize; j++)
	//	{
	//		char image_data[64];
	//		int Y  = atoi(buf[0] + (i * wrap[0]) + j);
	//		int Cb = atoi(buf[1] + (i * wrap[1]) + j);
	//		int Cr = atoi(buf[2] + (i * wrap[2]) + j);
	//		snprintf(image_data, sizeof(image_data), "%3d %3d %3d\n", Y, Cb, Cr);
	//		//printf(image_data);
	//		fprintf(f, image_data);
	//	}
	//}
    // writing line by line
    for (int i = 0; i < ysize; i++)
        fwrite(buf[0] + i * wrap[0], 1, xsize, f);
    fclose(f);
}


static int init_p_codec_packet_frame(AVCodecContext** pCodecContext, AVFrame** pFrame, 
		AVPacket** pPacket, AVCodecParameters *pCodecParameters, AVCodec *pCodec)
{
  *pCodecContext = avcodec_alloc_context3(pCodec);
  if (!*pCodecContext)
  {
    logging("failed to allocated memory for AVCodecContext");
    return -1;
  }
  // Fill the codec context based on the values from the supplied codec parameters
  if (avcodec_parameters_to_context(*pCodecContext, pCodecParameters) < 0)
  {
    logging("failed to copy codec params to codec context");
    return -1;
  }
  // Initialize the AVCodecContext to use the given AVCodec.
  if (avcodec_open2(*pCodecContext, pCodec, NULL) < 0)
  {
    logging("failed to open codec through avcodec_open2");
    return -1;
  }
  *pFrame = av_frame_alloc();
  if (!*pFrame)
  {
    logging("failed to allocated memory for AVFrame");
    return -1;
  }
  *pPacket = av_packet_alloc();
  if (!*pPacket)
  {
    logging("failed to allocated memory for AVPacket");
    return -1;
  }
  return 0;
}

