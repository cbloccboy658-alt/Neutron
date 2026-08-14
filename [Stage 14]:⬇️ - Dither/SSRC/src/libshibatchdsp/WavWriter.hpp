#ifndef WAVWRITER_HPP
#define WAVWRITER_HPP

#include <string>
#include <memory>
#include <vector>
#include <algorithm>

#include "shibatch/ssrc.hpp"
#include "dr_wav.hpp"
#include "BGExecutor.hpp"
#include "BlockingQueue.hpp"

template<typename T> class ssrc::WavWriter<T>::WavWriterImpl {
public:
  virtual ~WavWriterImpl() = default;
};

namespace shibatch {
  template<typename T>
  class WavWriterStage : public ssrc::WavWriter<T>::WavWriterImpl {
    const size_t N;
    dr_wav::WavFile wav;
    const std::vector<std::shared_ptr<ssrc::StageOutlet<T>>> in;
    const bool mt;
    std::shared_ptr<BGExecutor> bgExecutor;
    BlockingQueue<std::vector<T>> queue;
    std::shared_ptr<std::thread> th;

    void thEntry() {
      const unsigned nch = wav.getNChannels();

      for(;;) {
	auto v = queue.pop();
	if (v.size() == 0) break;
	wav.writePCM(v.data(), v.size() / nch);
      }
    }

  public:
    WavWriterStage(const std::string &filename, const dr_wav::drwav_fmt &fmt, const dr_wav::Container& container,
	      const std::vector<std::shared_ptr<ssrc::StageOutlet<T>>> &in_, uint64_t nFrames = 0, size_t bufsize = 65536, bool mt_ = true) :
      N(bufsize), wav(filename.c_str(), fmt, container, nFrames), in(in_), mt(mt_) {
      if (fmt.channels != in.size()) throw(std::runtime_error("WavWriterStage::WavWriterStage fmt.channels != in.size()"));
      if (mt) bgExecutor = std::make_shared<BGExecutor>();
    }

    ~WavWriterStage() {
      if (th) {
	queue.push(std::vector<T>(0));
	th->join();
	th = nullptr;
      }
    }

    void execute() {
      const unsigned nch = wav.getNChannels();

      if (!mt) {
	std::vector<T> cbuf(N), fbuf(N * nch);
	for(;;) {
	  size_t zmax = 0;
	  for(unsigned c=0;c<nch;c++) {
	    size_t z = in[c]->read(cbuf.data(), N);
	    zmax = std::max(z, zmax);
	    for(size_t i=0;i<z;i++) fbuf[i * nch + c] = cbuf[i];
	    for(size_t i=z;i<N;i++) fbuf[i * nch + c] = 0;
	  }
	  if (zmax == 0) break;
	  wav.writePCM(fbuf.data(), zmax);
	}
      } else {
	th = std::make_shared<std::thread>(&WavWriterStage::thEntry, this);
	std::vector<T> fbuf(N * nch);
	std::vector<std::vector<size_t>> vz(2);

	std::vector<std::vector<std::vector<T>>> cbuf(2);
	for(unsigned p=0;p<2;p++) {
	  vz[p].resize(nch);
	  cbuf[p].resize(nch);
	  for(unsigned c=0;c<nch;c++) cbuf[p][c].resize(N);
	}

	for(unsigned c=0;c<nch;c++) {
	  bgExecutor->push(Runnable::factory([&, c](void *ptr) {
	    vz[0][c] = in[c]->read((T*)ptr, N);
	  }, cbuf[0][c].data()));
	}

	for(unsigned p=0;;p ^= 1) {
	  for(unsigned c=0;c<nch;c++) bgExecutor->pop();

	  for(unsigned c=0;c<nch;c++) {
	    bgExecutor->push(Runnable::factory([&, p, c](void *ptr) {
	      vz[p ^ 1][c] = in[c]->read((T*)ptr, N);
	    }, cbuf[p ^ 1][c].data()));
	  }

	  size_t zmax = 0;
	  for(unsigned c=0;c<nch;c++) {
	    size_t z = vz[p][c];
	    zmax = std::max(z, zmax);
	    for(size_t i=0;i<z;i++) fbuf[i * nch + c] = cbuf[p][c][i];
	    for(size_t i=z;i<N;i++) fbuf[i * nch + c] = 0;
	  }
	  if (zmax == 0) break;
	  std::vector<T> v(zmax * nch);
	  memcpy(v.data(), fbuf.data(), zmax * nch * sizeof(T));
	  queue.push(std::move(v));
	}

	for(unsigned c=0;c<nch;c++) bgExecutor->pop();
      }
    }
  };
}
#endif // #ifndef WAVWRITER_HPP
