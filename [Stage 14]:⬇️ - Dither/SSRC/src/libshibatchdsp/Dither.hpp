#ifndef DITHER_HPP
#define DITHER_HPP

#include <memory>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>

#include "shibatch/ssrc.hpp"
#include "RNG.hpp"

template<typename OUTTYPE, typename INTYPE> class ssrc::Dither<OUTTYPE, INTYPE>::DitherImpl {
public:
  virtual ~DitherImpl() = default;
};

namespace shibatch {
  class TriangularDoubleRNG : public ssrc::DoubleRNG {
    const double peak;
    std::shared_ptr<shibatch::RNG> rng;
  public:
    TriangularDoubleRNG(double peak_, std::shared_ptr<shibatch::RNG> rng_ = std::make_shared<shibatch::TLCG64>()) :
      peak(peak_), rng(rng_) {}
    double nextDouble() { return rng->nextTriangularDouble(peak); }
    ~TriangularDoubleRNG() {}
  };

  template<typename OUTTYPE, typename INTYPE>
  class DitherStage : public ssrc::StageOutlet<OUTTYPE>, public ssrc::Dither<OUTTYPE, INTYPE>::DitherImpl {
  public:
    std::shared_ptr<ssrc::StageOutlet<INTYPE>> inlet;
    const double gain;
    const int32_t offset, clipMin, clipMax;
    const ssrc::NoiseShaperCoef *coef;
    std::shared_ptr<ssrc::DoubleRNG> rng;
    std::vector<INTYPE> buf, in;
    std::vector<double> rndbuf;

    DitherStage(std::shared_ptr<ssrc::StageOutlet<INTYPE>> in_, double gain_, int32_t offset_, int32_t clipMin_, int32_t clipMax_,
	       const ssrc::NoiseShaperCoef *coef_, std::shared_ptr<ssrc::DoubleRNG> rng_) :
      inlet(in_), gain(gain_), offset(offset_), clipMin(clipMin_), clipMax(clipMax_), coef(coef_), rng(rng_) {
      buf.resize(coef->len);
    }

    bool atEnd() { return inlet->atEnd(); }

    size_t read(OUTTYPE *out, size_t nSamples) {
      const double *shaperCoefs = coef->coefs;
      const int shaperLen = coef->len;

      if (in.size() < nSamples) in.resize(nSamples);
      nSamples = inlet->read(in.data(), nSamples);

      rndbuf.resize(std::max(rndbuf.size(), nSamples));
      rng->fill(rndbuf.data(), nSamples);

      if (shaperLen != 0) {
	for(size_t p=0;p<nSamples;p++) {
	  double h = shaperCoefs[shaperLen-1] * buf[shaperLen-1];

	  for(int i=shaperLen-2;i>=0;i--) {
	    h += shaperCoefs[i] * buf[i];
	    buf[i+1] = buf[i];
	  }

	  double x = gain * in[p] + offset + h;
	  double q = rint(x + rndbuf[p]);
	  buf[0] = q - x;

	  if (q < clipMin || q > clipMax) {
	    if (q < clipMin) q = clipMin;
	    if (q > clipMax) q = clipMax;
	    buf[0] = q - x;
	    if (buf[0] < -1) buf[0] = -1;
	    if (buf[0] >  1) buf[0] =  1;
	  }

	  out[p] = q;
	}
      } else {
	for(size_t p=0;p<nSamples;p++)
	  out[p] = rint(gain * in[p] + offset + rndbuf[p]);
      }

      return nSamples;
    }
  };
}
#endif // #ifndef DITHER_HPP
