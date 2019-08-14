#ifndef LIGHTDB_DECODEREADER_H
#define LIGHTDB_DECODEREADER_H

#include "spsc_queue.h"
#include <nvcuvid.h>
#include <thread>
#include <experimental/filesystem>

//#include "gpac/isomedia.h"
//#include "gpac/internal/isomedia_dev.h"
//#include "gpac/list.h"

#include "MP4Reader.h"

#include <iostream>


struct DecodeReaderPacket: public CUVIDSOURCEDATAPACKET {
public:
    DecodeReaderPacket() : CUVIDSOURCEDATAPACKET{} { }

    DecodeReaderPacket(const DecodeReaderPacket &packet) = default;

    explicit DecodeReaderPacket(const CUVIDSOURCEDATAPACKET &packet)
        : CUVIDSOURCEDATAPACKET{packet.flags, packet.payload_size, nullptr, packet.timestamp},
          buffer_(std::make_shared<std::vector<unsigned char>>()) {
        buffer_->reserve(payload_size);
        buffer_->insert(buffer_->begin(), packet.payload, packet.payload + payload_size);
        payload = buffer_->data();
    }

    explicit DecodeReaderPacket(const std::vector<char> &data, const unsigned long flags=0,
                                const CUvideotimestamp timestamp=0)
            : DecodeReaderPacket(CUVIDSOURCEDATAPACKET{flags, data.size(),
                                                       reinterpret_cast<const unsigned char*>(data.data()), timestamp})
    { }

    DecodeReaderPacket& operator=(const DecodeReaderPacket &packet) = default;
    bool operator==(const DecodeReaderPacket &packet) const noexcept {
        return this->payload_size == packet.payload_size &&
               this->flags == packet.flags &&
               this->timestamp == packet.timestamp &&
               this->buffer_ == packet.buffer_;
    }

private:
    std::shared_ptr<std::vector<unsigned char>> buffer_;
};

class DecodeReader {
public:
    class iterator {
        friend class DecodeReader;

    public:
        bool operator==(const iterator& other) const { return (eos_ && other.eos_) ||
                                                              (reader_ == other.reader_ &&
                                                               *current_ == *other.current_); }
        bool operator!=(const iterator& other) const { return !(*this == other); }
        void operator++()
        {
            if (!(current_ = reader_->read()).has_value())
                eos_ = true;
        }
        const DecodeReaderPacket operator++(int)
        {
            auto value = **this;
            ++*this;
            return value;
        }
        DecodeReaderPacket operator*() { return current_.value(); }

    protected:
        explicit iterator(DecodeReader &reader)
                : reader_(&reader), current_({reader.read()}), eos_(false)
        { }
        constexpr explicit iterator()
                : reader_(nullptr), current_(), eos_(true)
        { }

    private:
        DecodeReader *reader_;
        std::optional<DecodeReaderPacket> current_;
        bool eos_;
    };

    virtual iterator begin() { return iterator(*this); }
    virtual iterator end() { return iterator(); }

    virtual std::optional<DecodeReaderPacket> read() = 0;
    virtual CUVIDEOFORMAT format() const = 0;
    virtual bool isComplete() const = 0;
};

class FileDecodeReader: public DecodeReader {
public:
    explicit FileDecodeReader(const std::string &filename)
        : FileDecodeReader(filename.c_str()) { }

    explicit FileDecodeReader(const char *filename)
            : filename_(filename),
              packets_(std::make_unique<lightdb::spsc_queue<DecodeReaderPacket>>(4096)), // Must be initialized before source
              source_(CreateVideoSource(filename)),
              format_(GetVideoSourceFormat(source_)),
              decoded_bytes_(0) {
        if(format().codec != cudaVideoCodec_H264 && format().codec != cudaVideoCodec_HEVC)
            throw GpuRuntimeError("FileDecodeReader only supports H264/HEVC input video");
        else if(format().chroma_format != cudaVideoChromaFormat_420)
            throw GpuRuntimeError("FileDecodeReader only supports 4:2:0 chroma");
    }

    FileDecodeReader(const FileDecodeReader&) = delete;
    FileDecodeReader(FileDecodeReader&& other) noexcept
        : filename_(std::move(other.filename_)),
          packets_(std::move(other.packets_)),
          source_(other.source_),
          format_(other.format_),
          decoded_bytes_(other.decoded_bytes_) {
          other.source_ = nullptr;
    }

    ~FileDecodeReader() {
        CUresult status;

        if(source_ == nullptr)
            ;
        else if(!CompleteVideo())
            LOG(ERROR) << "Swallowed CompleteVideo failure";
        else if((status = cuvidSetVideoSourceState(source_, cudaVideoState_Stopped)) != CUDA_SUCCESS)
            LOG(ERROR) << "Swallowed cuvidSetVideoSourceState failure (" << status << ')';
        else if((status = cuvidDestroyVideoSource(source_)) != CUDA_SUCCESS)
            LOG(ERROR) << "Swallowed cuvidDestroyVideoSource failure (" << status << ')';
        else
            source_ = nullptr;
    }

    inline CUVIDEOFORMAT format() const override { return format_; }
    inline const std::string &filename() const { return filename_; }

    std::optional<DecodeReaderPacket> read() override {
        DecodeReaderPacket packet;

        while (cuvidGetVideoSourceState(source_) == cudaVideoState_Started &&
                !packets_->read_available())
            std::this_thread::yield();

        if(packets_->pop(packet)) {
            decoded_bytes_ += packet.payload_size;
            return {packet};
        } else {
            LOG(INFO) << "Decoded " << decoded_bytes_ << " bytes from " << filename();
            return {};
        }
    }

    inline bool isComplete() const override {
        return !packets_->read_available() && cuvidGetVideoSourceState(source_) != cudaVideoState_Started;
    }

private:
    static int CUDAAPI HandleVideoData(void *userData, CUVIDSOURCEDATAPACKET *packet) {
        auto *packets = static_cast<lightdb::spsc_queue<DecodeReaderPacket>*>(userData);

        while(!packets->push(DecodeReaderPacket(*packet)))
            std::this_thread::yield();

        return 1;
    }

    CUvideosource CreateVideoSource(const char *filename) {
        CUresult status;
        CUvideosource source;
        CUVIDSOURCEPARAMS videoSourceParameters = {
                .ulClockRate = 0,
                .uReserved1 = {},
                .pUserData = packets_.get(),
                .pfnVideoDataHandler = HandleVideoData,
                .pfnAudioDataHandler = nullptr,
                {nullptr}
        };

        if(!std::experimental::filesystem::exists(filename))
            throw InvalidArgumentError("File does not exist", "filename");
        else if(GPUContext::device_count() == 0)
            throw GpuCudaRuntimeError("No CUDA device was found", CUDA_ERROR_NOT_INITIALIZED);
        if((status = cuvidCreateVideoSource(&source, filename, &videoSourceParameters)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidCreateVideoSource failed", status);
        else if((status = cuvidSetVideoSourceState(source, cudaVideoState_Started)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidSetVideoSourceState failed", status);

        return source;
    }

    CUVIDEOFORMAT GetVideoSourceFormat(CUvideosource source) {
        CUresult status;
        CUVIDEOFORMAT format;

        if((status = cuvidGetSourceVideoFormat(source, &format, 0)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidGetSourceVideoFormat failed", status);
        return format;
    }

    bool CompleteVideo() {
        packets_->reset();
        return true;
    }

    std::string filename_;
    std::unique_ptr<lightdb::spsc_queue<DecodeReaderPacket>> packets_;
    CUvideosource source_;
    CUVIDEOFORMAT format_;
    size_t decoded_bytes_;
};

struct GOPReaderPacket {
public:
    explicit GOPReaderPacket(lightdb::bytestring data, unsigned int firstFrameIndex, unsigned int numberOfFrames)
            : data_(std::move(data)),
              firstFrameIndex_(firstFrameIndex),
              numberOfFrames_(numberOfFrames)
    {}

    const lightdb::bytestring &data() const { return data_; }
    unsigned int firstFrameIndex() const { return firstFrameIndex_; }
    unsigned int numberOfFrames() const { return numberOfFrames_; }

private:
    lightdb::bytestring data_;
    unsigned int firstFrameIndex_;
    unsigned int numberOfFrames_;
};

class EncodedFrameReader {
public:
    // Assume frames is sorted.
    explicit EncodedFrameReader(std::filesystem::path filename, std::vector<int> frames)
        : filename_(std::move(filename)),
        mp4Reader_(filename_),
        frames_(std::move(frames)),
        numberOfSamplesRead_(0)
    {
        frameIterator_ = frames_.begin();
        shouldReadAllFrames_ = frames_.empty();
        keyframeIterator_ = mp4Reader_.keyframeSampleNumbers().begin();
    }

    std::optional<GOPReaderPacket> read() {
        // If we are reading all of the frames, return the frames for the next GOP.
        if (shouldReadAllFrames_) {
            if (keyframeIterator_ == mp4Reader_.keyframeSampleNumbers().end())
                return {};

            auto firstSampleToRead = *keyframeIterator_++;
            auto lastSampleToRead = keyframeIterator_ == mp4Reader_.keyframeSampleNumbers().end() ? mp4Reader_.numberOfSamples() : *keyframeIterator_ - 1;

            return dataForSamples(firstSampleToRead, lastSampleToRead);
        } else {
            if (frameIterator_ == frames_.end())
                return {};

            // Unideal, but frames_ is 0-indexed, but keyframes are 1-indexed because they are sample numbers.

            // Find GOP that the current frame is in.
            while (keyframeIterator_ != mp4Reader_.keyframeSampleNumbers().end() &&
                    MP4Reader::sampleNumberToFrameNumber(*keyframeIterator_) <= *frameIterator_)
                keyframeIterator_++;

            // Now keyframeIterator is pointing to the keyframe of the next GOP.
            // Find the rest of the frames that we want and are in the same GOP.
            if (keyframeIterator_ == mp4Reader_.keyframeSampleNumbers().end())
                frameIterator_ = frames_.end();
            else {
                while (frameIterator_ != frames_.end() && *frameIterator_ <
                                                                  MP4Reader::sampleNumberToFrameNumber(*keyframeIterator_))
                    frameIterator_++;
            }

            // Now frameIterator_ is point to one past the last frame we are interested in.
            // Read frames from the previous GOP to the last frame we are interested in.
            auto firstSampleToRead = *std::prev(keyframeIterator_);
            auto lastSampleToRead = MP4Reader::frameNumberToSampleNumber(*std::prev(frameIterator_));

            numberOfSamplesRead_ += lastSampleToRead - firstSampleToRead + 1;
            return dataForSamples(firstSampleToRead, lastSampleToRead);
        }
    }

    ~EncodedFrameReader() {
        std::cout << "Number of samples read: " << numberOfSamplesRead_ << std::endl;
    }

private:
    std::optional<GOPReaderPacket> dataForSamples(unsigned int firstSampleToRead, unsigned int lastSampleToRead) {
        // -1 from firstSampleToRead to go from sample number -> index.
        return { GOPReaderPacket(mp4Reader_.dataForSamples(firstSampleToRead, lastSampleToRead), MP4Reader::sampleNumberToFrameNumber(firstSampleToRead), lastSampleToRead - firstSampleToRead + 1) };
    }

    std::filesystem::path filename_;
    MP4Reader mp4Reader_;
    std::vector<int> frames_;
    bool shouldReadAllFrames_;
    std::vector<int>::const_iterator frameIterator_;
    std::vector<unsigned int>::const_iterator keyframeIterator_;
    unsigned int numberOfSamplesRead_;
};

class FrameDecodeReader: public DecodeReader {
public:
    explicit FrameDecodeReader(std::filesystem::path filename, std::vector<unsigned int> frames)
        : filename_(std::move(filename)),
        frames_(std::move(frames)),
        frameIterator_(frames_.begin()),
        currentKeyframeSampleNumber_(-1),
        decodedBytes_(0)
    {
        // In order to get the headers, mode cannot be READ_DUMP, and the extract mode must be set.
        file_ = gf_isom_open(filename_.c_str(), GF_ISOM_OPEN_READ, nullptr);
        assert(gf_isom_set_nalu_extract_mode(file_, 1, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG | GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG) == GF_OK);

        frames_ = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                   60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 79,
                   80, 120, 121, 126, 130, 197, 199, 200, 201, 202, 203, 210, 211, 212};
        frameIterator_ = frames_.begin();
    }

    std::optional<DecodeReaderPacket> read() override {
        // Assume frames are sorted.
        // Go through frames, and use GPAC to find out what GOP each frame belongs to.
        // Return the data for all frames in the same GOP
        //      Data needs to be:
        //          [Keyframe,
        //              last interesting frame in the GOP]

        // Can get data_offset for keyframe to get start of reading.
        // Also get data_offset for last interesting frame in the GOP + its dataLength to see where to stop reading.
        auto dataFromNextGOP = framesForNextGOP_();
        if (dataFromNextGOP.has_value()) {
            unsigned long flags = CUVID_PKT_DISCONTINUITY;
            if (frameIterator_ == frames_.end())
                flags |= CUVID_PKT_ENDOFSTREAM;

            return DecodeReaderPacket(dataFromNextGOP.value(), flags);
        } else
            return {};
    }

    // FIXME: Is this necessary for the frame reader?
    CUVIDEOFORMAT format() const override {
        CUVIDEOFORMAT format;
        return format;
    }

    // FIXME: implement this.
    bool isComplete() const override {
        // FIXME: This isn't right.
        return frameIterator_ == frames_.end();
    }

private:
    std::optional<lightdb::bytestring> framesForNextGOP_() {
        // This isn't right because we'll miss the last GOP.
        if (frameIterator_ == frames_.end()) {
            gf_isom_close(file_);
            return std::nullopt;
        }

        unsigned int track_index = 1;

        GF_ISOSample *sample = gf_isom_get_sample_info(file_, track_index, *frameIterator_, NULL, NULL);
        unsigned int prevRAP = 0;
        unsigned int nextRAP = 0;
        unsigned int streamDescriptionIndex = 0;
        GF_ISOSample *rapSample = NULL;
        GF_Err result = gf_isom_get_sample_for_media_time(file_, track_index, sample->DTS, &streamDescriptionIndex, GF_ISOM_SEARCH_SYNC_BACKWARD, NULL, &prevRAP);
        assert(result == GF_OK);


//        GF_TrackBox *trak = gf_isom_get_track_from_file2(file_, track_index);
//        SAPType isRAP;
//        unsigned int prevRAP;
//        unsigned int nextRAP;
//        GF_Err result = stbl_GetSampleRAP2(trak->Media->information->sampleTable->SyncSample, *frameIterator_, &isRAP, &prevRAP, &nextRAP);
//        assert(result == GF_OK);

        // Find all frames that have the same prevRAP.
        while (frameIterator_ != frames_.end() && (*frameIterator_ < nextRAP || !nextRAP))
            frameIterator_++;

        // Read frame data from prevRAP to prev(frameIterator_).
        unsigned long size = 0;
        unsigned int lastFrame = *std::prev(frameIterator_);

        // First read to get sizes.
        for (unsigned int i = prevRAP; i <= lastFrame; i++) {
            GF_ISOSample *sample = gf_isom_get_sample_info(file_, track_index, i, NULL, NULL);
            size += sample->dataLength;
            gf_isom_sample_del(&sample);
        }

        // Then read to get the actual data.
        lightdb::bytestring videoData;
        videoData.reserve(size);
        for (unsigned int i = prevRAP; i <= lastFrame; i++) {
            GF_ISOSample *sample = gf_isom_get_sample(file_, track_index, i, NULL);
            videoData.insert(videoData.end(), sample->data, sample->data + sample->dataLength);
            gf_isom_sample_del(&sample);
        }

        return {videoData};
    }

    std::filesystem::path filename_;
    std::vector<unsigned int> frames_;
    std::vector<unsigned int>::const_iterator frameIterator_;
    long currentKeyframeSampleNumber_;
    unsigned long decodedBytes_;
    GF_ISOFile *file_;
};

#endif //LIGHTDB_DECODEREADER_H
