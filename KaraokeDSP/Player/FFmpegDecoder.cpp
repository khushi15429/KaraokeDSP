#include "FFmpegDecoder.h"

#include <iostream>

FFmpegDecoder::FFmpegDecoder()
{
    mFormatContext = nullptr;
    mCodecContext = nullptr;
    mSwrContext = nullptr;

    mPacket = av_packet_alloc();
    mFrame = av_frame_alloc();

    mAudioStreamIndex = -1;

    mOutputSampleRate = 48000;
    mOutputChannels = 2;

    mEndOfFile = false;
}

FFmpegDecoder::~FFmpegDecoder()
{
    Close();

    if (mPacket)
    {
        av_packet_free(&mPacket);
        mPacket = nullptr;
    }

    if (mFrame)
    {
        av_frame_free(&mFrame);
        mFrame = nullptr;
    }
}

bool FFmpegDecoder::IsOpen() const
{
    return
        mFormatContext &&
        mCodecContext &&
        mSwrContext;
}

int FFmpegDecoder::GetSampleRate() const
{
    if (!mCodecContext)
        return 0;

    return mCodecContext->sample_rate;
}

int FFmpegDecoder::GetChannels() const
{
    if (!mCodecContext)
        return 0;

    return mCodecContext->ch_layout.nb_channels;
}

void FFmpegDecoder::SetOutputFormat(int sampleRate, int channels)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (IsOpen())
        return;

    if (sampleRate > 0)
        mOutputSampleRate = sampleRate;
    if (channels > 0)
        mOutputChannels = channels;
}

bool FFmpegDecoder::Open(const char* filename)
{
    std::lock_guard<std::mutex> lock(mMutex);

    // This function owns mMutex, so cleanup must not lock it again.
    CloseUnlocked();

    avformat_network_init();

    mEndOfFile = false;

    //----------------------------------------------------------
    // Open File
    //----------------------------------------------------------

    int ret = avformat_open_input(
        &mFormatContext,
        filename,
        nullptr,
        nullptr
    );

    if (ret < 0)
    {
        std::cout << "Cannot open audio file." << std::endl;
        return false;
    }

    //----------------------------------------------------------
    // Stream Info
    //----------------------------------------------------------

    ret = avformat_find_stream_info(
        mFormatContext,
        nullptr
    );

    if (ret < 0)
    {
        std::cout << "Cannot find stream info." << std::endl;
        CloseUnlocked();
        return false;
    }

    //----------------------------------------------------------
    // Find Audio Stream
    //----------------------------------------------------------

    mAudioStreamIndex = -1;

    for (unsigned int i = 0; i < mFormatContext->nb_streams; i++)
    {
        if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            mAudioStreamIndex = (int)i;
            break;
        }
    }

    if (mAudioStreamIndex == -1)
    {
        std::cout << "No audio stream found." << std::endl;
        CloseUnlocked();
        return false;
    }

    //----------------------------------------------------------
    // Find Decoder
    //----------------------------------------------------------

    AVCodecParameters* codecParams =
        mFormatContext->streams[mAudioStreamIndex]->codecpar;

    const AVCodec* codec =
        avcodec_find_decoder(codecParams->codec_id);

    if (!codec)
    {
        std::cout << "Decoder not found." << std::endl;
        CloseUnlocked();
        return false;
    }

    //----------------------------------------------------------
    // Codec Context
    //----------------------------------------------------------

    mCodecContext = avcodec_alloc_context3(codec);

    if (!mCodecContext)
    {
        std::cout << "Cannot allocate codec context." << std::endl;
        CloseUnlocked();
        return false;
    }

    ret = avcodec_parameters_to_context(
        mCodecContext,
        codecParams
    );

    if (ret < 0)
    {
        std::cout << "Cannot copy codec parameters." << std::endl;
        CloseUnlocked();
        return false;
    }

    //----------------------------------------------------------
    // Open Codec
    //----------------------------------------------------------

    ret = avcodec_open2(
        mCodecContext,
        codec,
        nullptr
    );

    if (ret < 0)
    {
        std::cout << "Cannot open codec." << std::endl;
        CloseUnlocked();
        return false;
    }

    //----------------------------------------------------------
    // Create Stereo Layout
    //----------------------------------------------------------

    AVChannelLayout outLayout;

    av_channel_layout_default(
        &outLayout,
        mOutputChannels
    );

    //----------------------------------------------------------
    // Create Resampler
    //----------------------------------------------------------

    ret = swr_alloc_set_opts2(
        &mSwrContext,

        &outLayout,
        AV_SAMPLE_FMT_FLT,
        mOutputSampleRate,

        &mCodecContext->ch_layout,
        mCodecContext->sample_fmt,
        mCodecContext->sample_rate,

        0,
        nullptr
    );

    av_channel_layout_uninit(&outLayout);

    if (ret < 0)
    {
        std::cout << "Cannot create resampler." << std::endl;
        CloseUnlocked();
        return false;
    }

    ret = swr_init(mSwrContext);

    if (ret < 0)
    {
        std::cout << "Cannot initialize resampler." << std::endl;
        CloseUnlocked();
        return false;
    }

    std::cout << "Input Sample Rate : "
        << mCodecContext->sample_rate << std::endl;

    std::cout << "Input Channels : "
        << mCodecContext->ch_layout.nb_channels << std::endl;

    std::cout << "FFmpeg Decoder Opened" << std::endl;

    return true;
}

void FFmpegDecoder::Close()
{
    std::lock_guard<std::mutex> lock(mMutex);

    CloseUnlocked();
}

void FFmpegDecoder::CloseUnlocked()
{

    mEndOfFile = false;

    if (mSwrContext)
    {
        swr_free(&mSwrContext);
        mSwrContext = nullptr;
    }

    if (mCodecContext)
    {
        avcodec_free_context(&mCodecContext);
        mCodecContext = nullptr;
    }

    if (mFormatContext)
    {
        avformat_close_input(&mFormatContext);
        mFormatContext = nullptr;
    }

    mAudioStreamIndex = -1;
}

int FFmpegDecoder::ReadFrame(float* outputBuffer, int maxSamples)
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (!IsOpen())
        return 0;

    while (true)
    {
        //----------------------------------------------------------
        // Always clear frame before receive
        //----------------------------------------------------------

        av_frame_unref(mFrame);

        //----------------------------------------------------------
        // Try to receive decoded frame
        //----------------------------------------------------------

        int ret = avcodec_receive_frame(
            mCodecContext,
            mFrame
        );

        if (ret == 0)
        {
            uint8_t* out[] =
            {
                reinterpret_cast<uint8_t*>(outputBuffer)
            };

            int outSamples = swr_convert(
                mSwrContext,
                out,
                maxSamples / mOutputChannels,
                (const uint8_t**)mFrame->extended_data,
                mFrame->nb_samples
            );

            if (outSamples < 0)
            {
                std::cout
                    << "Resampler Error"
                    << std::endl;

                return 0;
            }

            return outSamples * mOutputChannels;
        }

        //----------------------------------------------------------
        // Decoder needs packet
        //----------------------------------------------------------

        if (ret == AVERROR(EAGAIN))
        {
            while (true)
            {
                ret = av_read_frame(
                    mFormatContext,
                    mPacket
                );

                if (ret < 0)
                {
                    //--------------------------------------------------
                    // End Of File
                    //--------------------------------------------------

                    if (!mEndOfFile)
                    {
                        mEndOfFile = true;

                        avcodec_send_packet(
                            mCodecContext,
                            nullptr
                        );

                        break;
                    }

                    return 0;
                }

                //--------------------------------------------------
                // Ignore non-audio packets
                //--------------------------------------------------

                if (mPacket->stream_index != mAudioStreamIndex)
                {
                    av_packet_unref(mPacket);
                    continue;
                }

                //--------------------------------------------------
                // Send Packet
                //--------------------------------------------------

                ret = avcodec_send_packet(
                    mCodecContext,
                    mPacket
                );

                av_packet_unref(mPacket);

                if (ret == AVERROR(EAGAIN))
                {
                    break;
                }

                if (ret < 0)
                {
                    char err[256];

                    av_strerror(
                        ret,
                        err,
                        sizeof(err)
                    );

                    std::cout
                        << "Send Packet Error : "
                        << err
                        << std::endl;

                    return 0;
                }

                break;
            }

            continue;
        }

        //----------------------------------------------------------
        // Decoder Finished
        //----------------------------------------------------------

        if (ret == AVERROR_EOF)
        {
            return 0;
        }

        //----------------------------------------------------------
        // Decoder Error
        //----------------------------------------------------------

        char err[256];

        av_strerror(
            ret,
            err,
            sizeof(err)
        );

        std::cout
            << "Receive Error : "
            << err
            << std::endl;

        return 0;
    }
}
