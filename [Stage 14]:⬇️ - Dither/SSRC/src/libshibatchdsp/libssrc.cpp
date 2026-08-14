#include <vector>
#include <cstring>
#include <unordered_set>
#include <queue>
#include "SRC.hpp"
#include "WavReader.hpp"
#include "WavWriter.hpp"
#include "Dither.hpp"
#include "ChannelMixer.hpp"
#include "BGExecutor.hpp"
#include "ObjectCache.hpp"

#ifndef SSRC_VERSION
#error SSRC_VERSION not defined
#endif

#ifndef BUILDINFO
#error BULIDINFO not defined
#endif

using namespace std;
using namespace ssrc;
using namespace shibatch;

//

namespace ssrc {
  string versionString() { return SSRC_VERSION; }
  string buildInfo() { return BUILDINFO; }
}

namespace shibatch {
  class LambdaRunner : public Runnable {
    const function<void(void *)> f;
    const function<void(void *, void *)> g;
    void *ptrf, *ptrg;
  public:
    LambdaRunner(function<void(void *)> f_, void *ptrf_,
		 function<void(void *, void *)> g_, void *ptrg_) : f(f_), g(g_), ptrf(ptrf_), ptrg(ptrg_) {}
    ~LambdaRunner() {}
    void run() { f(ptrf); }
    void postProcess(void *p) { g(ptrg, p); }
  };

  class BGExecutorStatic {
    int nIdleWorkers = 0;
    vector<shared_ptr<thread>> vth;
    unordered_set<thread::id> thIds;
    queue<shared_ptr<Runnable>> que;
    mutex mtx;
    condition_variable condVar;

    shared_ptr<Runnable> pop() {
      auto ret = std::move(que.front());
      que.pop();
      return ret;
    }

    void thEntry() {
      {
	unique_lock lock(mtx);
	thIds.insert(this_thread::get_id());
      }

      shared_ptr<Runnable> r = nullptr;

      for(;;) {
	if (r) r->run();

	unique_lock lock(mtx);

	if (r) {
	  r->belongsTo->que.push(r);
	  condVar.notify_all();
	}

	nIdleWorkers++;
	while(que.empty()) condVar.wait(lock);
	nIdleWorkers--;
	r = pop();
	if (!r) break;
      }
    }

    void addWorkerIfNecessary() {
      if (nIdleWorkers == 0 && vth.size() < thread::hardware_concurrency())
	vth.push_back(make_shared<thread>(&BGExecutorStatic::thEntry, this));
    }

  public:
    ~BGExecutorStatic() {
      {
	unique_lock lock(mtx);
	for(unsigned i=0;i < vth.size();i++) que.push(nullptr);
	condVar.notify_all();
      }
      for(auto t : vth) t->join();
    }

    friend class shibatch::BGExecutor;
  };

  BGExecutorStatic bgExecutorStatic;

  shared_ptr<Runnable> Runnable::factory(function<void(void *)> f, void *p,
					 function<void(void *, void *)> g, void *q) {
    return make_shared<LambdaRunner>(f, p, g, q);
  }

  void BGExecutor::push(shared_ptr<Runnable> job) {
    job->belongsTo = this;
    unique_lock lock(bgExecutorStatic.mtx);
    bgExecutorStatic.addWorkerIfNecessary();
    bgExecutorStatic.que.push(job);
    bgExecutorStatic.condVar.notify_all();
    size_++;
  }

  shared_ptr<Runnable> BGExecutor::pop() {
    if (bgExecutorStatic.thIds.count(this_thread::get_id()) == 0) {
      unique_lock lock(bgExecutorStatic.mtx);

      while(que.empty()) bgExecutorStatic.condVar.wait(lock);

      auto r = std::move(que.front());
      que.pop();
      size_--;

      return r;
    }

    for(;;) {
      shared_ptr<Runnable> r = nullptr;

      {
	unique_lock lock(bgExecutorStatic.mtx);

	bgExecutorStatic.nIdleWorkers++;
	while(que.empty() && bgExecutorStatic.que.empty()) bgExecutorStatic.condVar.wait(lock);
	bgExecutorStatic.nIdleWorkers--;

	if (!que.empty()) {
	  auto ret = std::move(que.front());
	  que.pop();
	  size_--;
	  return ret;
	}

	r = bgExecutorStatic.pop();
      }

      r->run();
    }
  }

  size_t BGExecutor::size() {
    unique_lock lock(bgExecutorStatic.mtx);
    return size_;
  }
}

//

template<typename REAL> SSRC<REAL>::SSRC(shared_ptr<StageOutlet<REAL>> inlet_, int64_t sfs_, int64_t dfs_,
					 unsigned l2dftflen_, double aa_, double guard_, double gain_,
					 bool minPhase_, unsigned l2mindftflen_, bool mt_) :
  impl(make_shared<SSRCStage<REAL>>(inlet_, sfs_, dfs_, l2dftflen_, aa_, guard_, gain_, minPhase_, l2mindftflen_, mt_)) {}

template<typename REAL> SSRC<REAL>::~SSRC() {}

template<typename REAL> size_t SSRC<REAL>::read(REAL *ptr, size_t n) {
  return dynamic_pointer_cast<SSRCStage<REAL>>(impl)->read(ptr, n);
}

template<typename REAL> bool SSRC<REAL>::atEnd() {
  return dynamic_pointer_cast<SSRCStage<REAL>>(impl)->atEnd();
}

template<typename REAL> double SSRC<REAL>::getDelay() {
  return dynamic_pointer_cast<SSRCStage<REAL>>(impl)->getDelay();
}

//

template SSRC<float>::SSRC(shared_ptr<StageOutlet<float>>, int64_t, int64_t, unsigned, double, double, double, bool, unsigned, bool);
template SSRC<float>::~SSRC();
template size_t SSRC<float>::read(float *ptr, size_t n);
template bool SSRC<float>::atEnd();
template double SSRC<float>::getDelay();

template SSRC<double>::SSRC(shared_ptr<StageOutlet<double>>, int64_t, int64_t, unsigned, double, double, double, bool, unsigned, bool);
template SSRC<double>::~SSRC();
template size_t SSRC<double>::read(double *ptr, size_t n);
template bool SSRC<double>::atEnd();
template double SSRC<double>::getDelay();

//

template<typename T> WavReader<T>::WavReader(const string &filename, bool mt_) :
  impl(make_shared<WavReaderStage<T>>(filename, mt_)) {}

template<typename T> WavReader<T>::WavReader(bool mt_) :
  impl(make_shared<WavReaderStage<T>>(mt_)) {}

template<typename T> WavReader<T>::~WavReader() {}

template<typename T> shared_ptr<StageOutlet<T>> WavReader<T>::getOutlet(uint32_t channel) {
  return dynamic_pointer_cast<WavReaderStage<T>>(impl)->getOutlet(channel);
}

template<typename T> WavFormat WavReader<T>::getFormat() {
  dr_wav::drwav_fmt fmt = dynamic_pointer_cast<WavReaderStage<T>>(impl)->getFmt();
  ssrc::WavFormat ret;

  ret.formatTag = fmt.formatTag;
  ret.channels = fmt.channels;
  ret.sampleRate = fmt.sampleRate;
  ret.avgBytesPerSec = fmt.avgBytesPerSec;
  ret.blockAlign = fmt.blockAlign;
  ret.bitsPerSample = fmt.bitsPerSample;
  ret.extendedSize = fmt.extendedSize;
  ret.validBitsPerSample = fmt.validBitsPerSample;
  ret.channelMask = fmt.channelMask;
  memcpy(&ret.subFormat, &fmt.subFormat, sizeof(ret.subFormat));

  return ret;
}

template<typename T> ContainerFormat WavReader<T>::getContainer() {
  return ContainerFormat((uint16_t)dr_wav::Container(dynamic_pointer_cast<WavReaderStage<T>>(impl)->getContainer()));
}

//

template WavReader<float>::WavReader(const string &filename, bool mt_);
template WavReader<float>::WavReader(bool mt_);
template WavReader<float>::~WavReader();
template shared_ptr<StageOutlet<float>> WavReader<float>::getOutlet(uint32_t);
template WavFormat WavReader<float>::getFormat();
template ContainerFormat WavReader<float>::getContainer();

template WavReader<double>::WavReader(const string &filename, bool mt_);
template WavReader<double>::WavReader(bool mt_);
template WavReader<double>::~WavReader();
template shared_ptr<StageOutlet<double>> WavReader<double>::getOutlet(uint32_t);
template WavFormat WavReader<double>::getFormat();
template ContainerFormat WavReader<double>::getContainer();

//

template<typename T> WavWriter<T>::WavWriter(const string &filename,
					     const ssrc::WavFormat& fmt_, const ssrc::ContainerFormat& cont_,
					     const vector<shared_ptr<StageOutlet<T>>> &in_,
					     uint64_t nFrames, size_t bufsize_, bool mt_) {
  dr_wav::drwav_fmt fmt;
  memcpy(&fmt, &fmt_, sizeof(fmt));
  impl = make_shared<WavWriterStage<T>>(filename, fmt, dr_wav::Container(cont_.c), in_, nFrames, bufsize_, mt_);
}

template<typename T> WavWriter<T>::~WavWriter() {}

template<typename T> void WavWriter<T>::execute() {
  dynamic_pointer_cast<WavWriterStage<T>>(impl)->execute();
}

//

template WavWriter<int32_t>::WavWriter(const string &, const ssrc::WavFormat&, const ssrc::ContainerFormat&,
				       const vector<shared_ptr<StageOutlet<int32_t>>> &, uint64_t, size_t, bool);
template WavWriter<int32_t>::~WavWriter();
template void WavWriter<int32_t>::execute();

template WavWriter<float>::WavWriter(const string &, const ssrc::WavFormat&, const ssrc::ContainerFormat&,
				     const vector<shared_ptr<StageOutlet<float>>> &, uint64_t, size_t, bool);
template WavWriter<float>::~WavWriter();
template void WavWriter<float>::execute();

template WavWriter<double>::WavWriter(const string &, const ssrc::WavFormat&, const ssrc::ContainerFormat&,
				      const vector<shared_ptr<StageOutlet<double>>> &, uint64_t, size_t, bool);
template WavWriter<double>::~WavWriter();
template void WavWriter<double>::execute();

//

template<typename OUTTYPE, typename INTYPE>
Dither<OUTTYPE, INTYPE>::Dither(shared_ptr<StageOutlet<INTYPE>> in_, double gain_, int32_t offset_,
				int32_t clipMin_, int32_t clipMax_,
				const ssrc::NoiseShaperCoef *coef_, shared_ptr<DoubleRNG> rng_) :
  impl(make_shared<DitherStage<OUTTYPE, INTYPE>>(in_, gain_, offset_, clipMin_, clipMax_, coef_, rng_)) {}

template<typename OUTTYPE, typename INTYPE> Dither<OUTTYPE, INTYPE>::~Dither() {}

template<typename OUTTYPE, typename INTYPE> bool Dither<OUTTYPE, INTYPE>::atEnd() {
  return dynamic_pointer_cast<Dither<OUTTYPE, INTYPE>>(impl)->atEnd();
}

template<typename OUTTYPE, typename INTYPE> size_t Dither<OUTTYPE, INTYPE>::read(OUTTYPE *ptr, size_t n) {
  return dynamic_pointer_cast<DitherStage<OUTTYPE, INTYPE>>(impl)->read(ptr, n);
}

//

template Dither<int32_t, float>::Dither(shared_ptr<StageOutlet<float>> in_, double gain_, int32_t offset_,
					int32_t clipMin_, int32_t clipMax_,
					const ssrc::NoiseShaperCoef *coef_, shared_ptr<DoubleRNG> rng_);
template Dither<int32_t, float>::~Dither();
template size_t Dither<int32_t, float>::read(int32_t *ptr, size_t n);
template bool Dither<int32_t, float>::atEnd();

template Dither<int32_t, double>::Dither(shared_ptr<StageOutlet<double>> in_, double gain_, int32_t offset_,
					 int32_t clipMin_, int32_t clipMax_,
					 const ssrc::NoiseShaperCoef *coef_, shared_ptr<DoubleRNG> rng_);
template Dither<int32_t, double>::~Dither();
template size_t Dither<int32_t, double>::read(int32_t *ptr, size_t n);
template bool Dither<int32_t, double>::atEnd();

//

shared_ptr<DoubleRNG> ssrc::createTriangularRNG(double peak, uint64_t seed) {
  return make_shared<TriangularDoubleRNG>(peak, make_shared<LCG64>(seed));
}

//

template<typename REAL> ChannelMixer<REAL>::ChannelMixer(shared_ptr<ssrc::OutletProvider<REAL>> in_,
							 const vector<vector<double>>& matrix_) :
  impl(make_shared<ChannelMixerStage<REAL>>(in_, matrix_)) {}

template<typename REAL> ChannelMixer<REAL>::~ChannelMixer() {}

template<typename REAL> shared_ptr<ssrc::StageOutlet<REAL>> ChannelMixer<REAL>::getOutlet(uint32_t c) {
  return dynamic_pointer_cast<ChannelMixerStage<REAL>>(impl)->getOutlet(c);
}

template<typename REAL> WavFormat ChannelMixer<REAL>::getFormat() {
  return dynamic_pointer_cast<ChannelMixerStage<REAL>>(impl)->getFormat();
}

//

template ChannelMixer<float>::ChannelMixer(shared_ptr<ssrc::OutletProvider<float>> in_,
					   const vector<vector<double>>& matrix_);
template ChannelMixer<float>::~ChannelMixer();
template shared_ptr<ssrc::StageOutlet<float>> ChannelMixer<float>::getOutlet(uint32_t c);
template WavFormat ChannelMixer<float>::getFormat();

template ChannelMixer<double>::ChannelMixer(shared_ptr<ssrc::OutletProvider<double>> in_,
					   const vector<vector<double>>& matrix_);
template ChannelMixer<double>::~ChannelMixer();
template shared_ptr<ssrc::StageOutlet<double>> ChannelMixer<double>::getOutlet(uint32_t c);
template WavFormat ChannelMixer<double>::getFormat();

//

template ObjectCache<double>::Internal ssrc::ObjectCache<double>::internal;
template ObjectCache<float>::Internal ssrc::ObjectCache<float>::internal;

template ObjectCache<SleefDFT>::Internal ssrc::ObjectCache<SleefDFT>::internal;
