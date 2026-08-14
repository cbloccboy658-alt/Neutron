#ifndef SHIBATCH_SSRC_HPP
#define SHIBATCH_SSRC_HPP

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>

namespace ssrc {
  struct WavFormat {
    static const inline uint16_t PCM = 0x0001, IEEE_FLOAT = 0x0003, EXTENSIBLE = 0xfffe;

    static const inline uint8_t KSDATAFORMAT_SUBTYPE_PCM[] = {
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
    };
    static const inline uint8_t KSDATAFORMAT_SUBTYPE_IEEE_FLOAT[] = {
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
    };

    uint16_t formatTag, channels;
    uint32_t sampleRate, avgBytesPerSec;
    uint16_t blockAlign, bitsPerSample, extendedSize, validBitsPerSample;
    uint32_t channelMask;
    uint8_t subFormat[16];

    WavFormat() = default;

    WavFormat(const WavFormat &w) :
      formatTag(w.formatTag), channels(w.channels), sampleRate(w.sampleRate), avgBytesPerSec(w.avgBytesPerSec),
      blockAlign(w.blockAlign), bitsPerSample(w.bitsPerSample), extendedSize(w.extendedSize),
      validBitsPerSample(w.validBitsPerSample), channelMask(w.channelMask) {
      memcpy(subFormat, w.subFormat, sizeof(subFormat));
    }

    WavFormat(uint16_t formatTag_, uint16_t channels_, uint32_t sampleRate_, uint16_t bitsPerSample_, uint32_t channelMask_ = 0, const uint8_t *subFormat_ = nullptr) :
      formatTag(formatTag_), channels(channels_), sampleRate(sampleRate_), avgBytesPerSec(0),
      blockAlign((uint32_t)channels_ * bitsPerSample_ / 8), bitsPerSample(bitsPerSample_),
      extendedSize(0), validBitsPerSample(0), channelMask(channelMask_) {
      if (subFormat_) memcpy(subFormat, subFormat_, sizeof(subFormat));
    }
  };

  struct ContainerFormat {
    static const inline uint16_t RIFF = 0x1000, RIFX = 0x1001, W64 = 0x1002;
    static const inline uint16_t RF64 = 0x1003, AIFF = 0x1004;

    uint16_t c;

    ContainerFormat(uint16_t c_) : c(c_) {}
    operator uint16_t() const { return c; }
  };

  inline std::string to_string(ContainerFormat c) {
    switch(c.c) {
    case ContainerFormat::RIFF: return "RIFF";
    case ContainerFormat::RIFX: return "RIFX";
    case ContainerFormat::W64:  return "W64";
    case ContainerFormat::RF64: return "RF64";
    case ContainerFormat::AIFF: return "AIFF";
    default:                    return "N/A";
    }
  }

  struct NoiseShaperCoef {
    const int fs, id;
    const char *name;
    const int len;
    const double coefs[64];
  };

  template<typename T>
  class StageOutlet {
  public:
    virtual ~StageOutlet() = default;
    virtual bool atEnd() = 0;

    /**
     * Returns 0 only when EOF.
     * If not EOF and no data is available for reading, it must block.
     */
    virtual size_t read(T *ptr, size_t n) = 0;
  };

  template<typename T>
  class OutletProvider {
  public:
    virtual ~OutletProvider() = default;
    virtual std::shared_ptr<StageOutlet<T>> getOutlet(uint32_t channel) = 0;
    virtual WavFormat getFormat() = 0;
    virtual ContainerFormat getContainer() { return ContainerFormat(0); }
  };

  class DoubleRNG {
  public:
    virtual double nextDouble() = 0;
    virtual void fill(double *ptr, size_t n) {
      for(;n>0;n--) *ptr++ = nextDouble();
    }
    virtual ~DoubleRNG() = default;
  };

  template<typename REAL>
  class SSRC : public StageOutlet<REAL> {
  public:
    class SSRCImpl;
    SSRC(std::shared_ptr<StageOutlet<REAL>> inlet_, int64_t sfs_, int64_t dfs_,
	 unsigned log2dftfilterlen_ = 10, double aa_ = 80, double guard_ = 1, double gain_ = 1,
	 bool minPhase_ = false, unsigned l2mindftflen_ = 0, bool mt_ = true);
    ~SSRC();
    bool atEnd();
    size_t read(REAL *ptr, size_t n);
    double getDelay();
  private:
    std::shared_ptr<class SSRCImpl> impl;
  };

  template<typename T>
  class WavReader : public OutletProvider<T> {
  public:
    class WavReaderImpl;
    WavReader(const std::string &filename, bool mt_ = true);
    WavReader(bool mt_ = true);
    ~WavReader();
    std::shared_ptr<StageOutlet<T>> getOutlet(uint32_t channel);
    WavFormat getFormat();
    ContainerFormat getContainer();
  private:
    std::shared_ptr<class WavReaderImpl> impl;
  };

  template<typename T>
  class WavWriter {
  public:
    class WavWriterImpl;
    WavWriter(const std::string &filename, const WavFormat& fmt, const ContainerFormat& cont_,
	      const std::vector<std::shared_ptr<StageOutlet<T>>> &in_, uint64_t nFrames = 0, size_t bufsize_ = 65536, bool mt_ = true);
    ~WavWriter();
    void execute();
  private:
    std::shared_ptr<class WavWriterImpl> impl;
  };

  std::shared_ptr<DoubleRNG> createTriangularRNG(double peak = 1.0,
    uint64_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count());

  template<typename OUTTYPE, typename INTYPE>
  class Dither : public StageOutlet<OUTTYPE> {
  public:
    class DitherImpl;
    Dither(std::shared_ptr<StageOutlet<INTYPE>> in_, double gain_, int32_t offset_, int32_t clipMin_, int32_t clipMax_,
	   const ssrc::NoiseShaperCoef *coef_, std::shared_ptr<DoubleRNG> rng_ = createTriangularRNG());
    ~Dither();
    bool atEnd();
    size_t read(OUTTYPE *ptr, size_t n);
  private:
    std::shared_ptr<class DitherImpl> impl;
  };

  template<typename T>
  class ChannelMixer : public OutletProvider<T> {
  public:
    class ChannelMixerImpl;
    ChannelMixer(std::shared_ptr<ssrc::OutletProvider<T>> in_, const std::vector<std::vector<double>>& matrix_);
    ~ChannelMixer();
    std::shared_ptr<ssrc::StageOutlet<T>> getOutlet(uint32_t c);
    WavFormat getFormat();
  private:
    std::shared_ptr<class ChannelMixerImpl> impl;
  };

  std::string versionString();
  std::string buildInfo();
}
#endif // #ifndef SHIBATCH_SSRC_HPP
