#ifndef WAVREADER_HPP
#define WAVREADER_HPP

#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>

#include "shibatch/ssrc.hpp"
#include "ArrayQueue.hpp"
#include "dr_wav.hpp"

template<typename T> class ssrc::WavReader<T>::WavReaderImpl {
public:
  virtual ~WavReaderImpl() = default;
};

namespace shibatch {
  template<typename T>
  class WavReaderStage : public ssrc::WavReader<T>::WavReaderImpl {
    class WavOutlet : public ssrc::StageOutlet<T> {
      WavReaderStage &reader;
      const uint32_t ch;
      ArrayQueue<T> queue;

    public:
      WavOutlet(WavReaderStage &reader_, int ch_) : reader(reader_), ch(ch_) {}
      ~WavOutlet() {}
      bool atEnd() {
	std::unique_lock lock(reader.mtx);
	return queue.size() == 0 && reader.atEnd();
      }

      size_t read(T *ptr, size_t n) {
	std::unique_lock lock(reader.mtx);

	size_t s = queue.size();

	if (s < n) s += reader.refill(n - s);

	if (s > n) s = n;

	return queue.read(ptr, s);
      }

      friend WavReaderStage;
    };

    static const size_t N = 1024 * 1024;

    std::mutex mtx;
    dr_wav::WavFile wav;
    const bool mt;
    std::vector<std::shared_ptr<ssrc::StageOutlet<T>>> outlet;
    std::vector<T> buf;
    std::shared_ptr<std::thread> th;
    BlockingArrayQueue<T> baq;

    size_t refill(size_t n) {
      buf.resize(std::max(n * getNChannels(), buf.size()));
      size_t z;
      if (!mt) {
	z = wav.readPCM(buf.data(), n);
      } else {
	z = baq.read(buf.data(), n * getNChannels()) / getNChannels();
      }
      unsigned nc = getNChannels();

      for(unsigned c=0;c<nc;c++) {
	auto o = std::dynamic_pointer_cast<WavOutlet>(outlet[c]);

	std::vector<T> v(z);
	for(size_t i=0;i<z;i++) v[i] = buf[i * nc + c];
	o->queue.write(std::move(v));
      }

      return z;
    }

    void thEntry() {
      for(;;) {
	std::vector<T> buf(N * getNChannels());
	size_t z = wav.readPCM(buf.data(), N);
	if (z == 0) {
	  baq.close();
	  break;
	}
	buf.resize(z * getNChannels());
	baq.write(std::move(buf));
      }
    }

  public:
    WavReaderStage(const std::string &filename, bool mt_) : wav(filename.c_str()), mt(mt_), baq(N * wav.getNChannels()) {
      outlet.resize(getNChannels());
      for(unsigned ch=0;ch<getNChannels();ch++)
	outlet[ch] = std::make_shared<WavOutlet>(*this, ch);
      if (mt) th = std::make_shared<std::thread>(&WavReaderStage::thEntry, this);
    }

    WavReaderStage(bool mt_) : wav(), mt(mt_), baq(N * wav.getNChannels()) {
      outlet.resize(getNChannels());
      for(unsigned ch=0;ch<getNChannels();ch++)
	outlet[ch] = std::make_shared<WavOutlet>(*this, ch);
      if (mt) th = std::make_shared<std::thread>(&WavReaderStage::thEntry, this);
    }

    dr_wav::drwav getWav() const { return wav.getWav(); }
    dr_wav::drwav_fmt getFmt() const { return wav.getFmt(); }
    dr_wav::drwav_container getContainer() const { return wav.getContainer(); }
    uint32_t getSampleRate() const { return wav.getSampleRate(); }
    uint16_t getNBitsPerSample() const { return wav.getNBitsPerSample(); }
    uint32_t getNChannels() const { return wav.getNChannels(); }
    uint32_t getNFrames() { return wav.getNFrames(); }
    bool isFloat() { return wav.isFloat(); }

    size_t getPosition() { return wav.getNFrames(); }
    bool atEnd() { return wav.atEnd(); }

    std::shared_ptr<ssrc::StageOutlet<T>> getOutlet(uint32_t channel) {
      if (channel >= outlet.size()) throw(std::runtime_error("WavReaderStage::getOutlet channel too large"));
      return outlet[channel];
    }

    ~WavReaderStage() {
      baq.close();
      if (th) th->join();
    }
  };
}
#endif // #ifndef WAVREADER_HPP
