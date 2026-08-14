#ifndef MIXER_HPP
#define MIXER_HPP

#include <string>
#include <cstring>
#include <memory>
#include <vector>
#include <mutex>

#include "shibatch/ssrc.hpp"
#include "ArrayQueue.hpp"

template<typename T> class ssrc::ChannelMixer<T>::ChannelMixerImpl {
public:
  virtual ~ChannelMixerImpl() = default;
};

namespace shibatch {
  template<typename T>
  class ChannelMixerStage : public ssrc::OutletProvider<T>, public ssrc::ChannelMixer<T>::ChannelMixerImpl {
    class Outlet : public ssrc::StageOutlet<T> {
      ChannelMixerStage &parent;
      shibatch::ArrayQueue<T> queue;
    public:
      Outlet(ChannelMixerStage &parent_) : parent(parent_) {}

      bool atEnd() {
	std::unique_lock lock(parent.mtx);
	return queue.size() == 0 && parent.allInputAtEnd();
      }

      size_t read(T *ptr, size_t n) {
	std::unique_lock lock(parent.mtx);
	size_t s = queue.size();
	if (s < n) s += parent.refill(n - s);
	if (s > n) s = n;
	return queue.read(ptr, s);
      }

      friend ChannelMixerStage;
    };
    
    std::shared_ptr<ssrc::OutletProvider<T>> in;
    const std::vector<std::vector<double>> matrix;
    ssrc::WavFormat format;
    const unsigned snch, dnch;
    std::vector<std::shared_ptr<Outlet>> out;
    std::vector<std::vector<T>> buf;
    std::mutex mtx;

    size_t refill(size_t n) {
      for(unsigned c=0;c<buf.size();c++) buf[c].resize(n);

      size_t nRead = 0;
      for(unsigned ic=0;ic<snch;ic++) {
	size_t z = in->getOutlet(ic)->read(buf[ic].data(), n);
	memset(buf[ic].data() + z, 0, (n - z) * sizeof(T));
	nRead = std::max(nRead, z);
      }

      std::vector<T> v(dnch);
      for(size_t pos = 0;pos < nRead;pos++) {
	for(unsigned oc = 0;oc < dnch;oc++) {
	  double s = 0;
	  for(unsigned ic = 0;ic < snch;ic++) s += buf[ic][pos] * matrix[oc][ic];
	  v[oc] = s;
	}
	for(unsigned oc = 0;oc < dnch;oc++) buf[oc][pos] = v[oc];
      }

      for(unsigned oc = 0;oc < dnch;oc++) out[oc]->queue.write(buf[oc].data(), nRead);

      return nRead;
    }

    bool allInputAtEnd() {
      for(unsigned ic = 0;ic < snch;ic++) if (!in->getOutlet(ic)->atEnd()) return false;
      return true;
    }
  public:
    ChannelMixerStage(std::shared_ptr<ssrc::OutletProvider<T>> in_, const std::vector<std::vector<double>>& matrix_) :
      in(in_), matrix(matrix_), format(in_->getFormat()), snch(format.channels), dnch(matrix.size()) {
      format.channels = dnch;
      buf.resize(std::max(snch, dnch));
      for(unsigned i=0;i<dnch;i++) out.push_back(std::make_shared<Outlet>(*this));
    }

    std::shared_ptr<ssrc::StageOutlet<T>> getOutlet(uint32_t c) { return out[c]; }
    ssrc::WavFormat getFormat() { return format; }
  };
}
#endif // #ifndef MIXER_HPP
