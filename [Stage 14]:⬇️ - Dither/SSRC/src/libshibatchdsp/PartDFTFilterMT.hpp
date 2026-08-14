#ifndef PARTDFTFILTERMT_HPP
#define PARTDFTFILTERMT_HPP

#include <vector>
#include <cstring>
#include <cassert>

#include "ObjectCache.hpp"

#include "shibatch/ssrc.hpp"

#ifndef _MSC_VER
#define RESTRICT __restrict__
#else
#define RESTRICT
#endif

namespace shibatch {
  template<typename REAL>
  class PartDFTFilterMT : public ssrc::StageOutlet<REAL> {
    static constexpr const size_t toPow2(size_t n) {
      size_t ret = 1;
      for(;ret < n && ret != 0;ret *= 2) ;
      return ret;
    }

    static constexpr const size_t ilog2(size_t n) {
      size_t ret = 1;
      for(;n > (1ULL << ret) && ret < 64;ret++) ;
      return ret;
    }

    //

    std::shared_ptr<ssrc::StageOutlet<REAL>> in;
    const size_t firlen, maxdftleno2, maxdftlen, l2maxdftlen, mindftlen, mindftleno2, l2mindftlen;

    std::vector<REAL> inBuf, overlapBuf, fractionBuf;
    size_t overlapLen = 0, fractionLen = 0, nZeroPadding = 0;
    bool endReached = false;

    std::vector<std::shared_ptr<SleefDFT>> dftf, dftb;

    std::vector<std::shared_ptr<void>> dftfilter_;
    std::vector<REAL *> dftfilter;

    std::shared_ptr<void> dftfilter0_, dftbuf_;
    REAL *dftfilter0, *dftbuf;

    size_t dftCount = 0;

  public:
    PartDFTFilterMT(std::shared_ptr<ssrc::StageOutlet<REAL>> in_, const REAL *fircoef_, size_t firlen_, size_t mindftlen_) :
      in(in_), firlen(firlen_), maxdftleno2(toPow2(firlen_)/2), maxdftlen(maxdftleno2 * 2), l2maxdftlen(ilog2(maxdftlen)),
      mindftlen(toPow2(mindftlen_)), mindftleno2(mindftlen / 2), l2mindftlen(ilog2(mindftlen)) {

      inBuf.resize(maxdftleno2 + mindftleno2);
      overlapBuf.resize(maxdftlen);
      fractionBuf.resize(mindftleno2 + maxdftlen);

      dftf.resize(l2maxdftlen+1);
      dftb.resize(l2maxdftlen+1);
      dftfilter_.resize(l2maxdftlen+1);
      dftfilter.resize(l2maxdftlen+1);
      dftfilter0_ = std::shared_ptr<void>(Sleef_malloc(mindftlen * sizeof(REAL)), Sleef_free);
      dftbuf_     = std::shared_ptr<void>(Sleef_malloc(maxdftlen * sizeof(REAL)), Sleef_free);
      dftfilter0 = (REAL *)dftfilter0_.get();
      dftbuf     = (REAL *)dftbuf_.get();

      {
	const REAL *p = fircoef_;
	size_t r = std::min(firlen_ - (p - fircoef_), mindftleno2);
	for(size_t z=0;z<r;z++) dftfilter0[z] = p[z] * (1.0 / mindftleno2);
	memset(dftfilter0 + r, 0, (mindftlen - r) * sizeof(REAL));
	p += r;

	for(unsigned level = 0;level <= (l2maxdftlen - l2mindftlen);level++) {
	  const unsigned l2dftlen = l2mindftlen + level;
	  const size_t dftlen = size_t(1) << l2dftlen, dftleno2 = dftlen / 2;

	  const auto m = SLEEF_MODE_REAL | SLEEF_MODE_ALT | SLEEF_MODE_NO_MT;
	  dftf[l2dftlen] = ssrc::constructSleefDFT<REAL>(m | SLEEF_MODE_FORWARD , dftlen);
	  dftb[l2dftlen] = ssrc::constructSleefDFT<REAL>(m | SLEEF_MODE_BACKWARD, dftlen);
	  dftfilter_[l2dftlen] = std::shared_ptr<void>(Sleef_malloc(dftlen * sizeof(REAL)), Sleef_free);
	  dftfilter[l2dftlen] = (REAL *)dftfilter_[l2dftlen].get();

	  r = std::min(firlen_ - (p - fircoef_), dftleno2);
	  for(size_t z=0;z<r;z++) dftfilter[l2dftlen][z] = p[z] * (1.0 / dftleno2);
	  memset(dftfilter[l2dftlen] + r, 0, (dftlen - r) * sizeof(REAL));
	  p += r;

	  SleefDFT_execute(dftf[l2dftlen].get(), dftfilter[l2dftlen], dftfilter[l2dftlen]);
	}

	SleefDFT_execute(dftf[l2mindftlen].get(), dftfilter0, dftfilter0);
      }
    }

    bool atEnd() { return fractionLen > 0 || !endReached; }

    size_t read(REAL *RESTRICT out, size_t nSamples) {
      size_t ret = 0;

      if (fractionLen > 0) {
	size_t nOut = std::min(fractionLen, nSamples);
	memcpy(out, fractionBuf.data(), nOut * sizeof(REAL));
	memmove(fractionBuf.data(), fractionBuf.data() + nOut, (fractionLen - nOut) * sizeof(REAL));
	fractionLen -= nOut;
	nSamples -= nOut;
	out += nOut;
	ret += nOut;
      }

      while(nSamples > 0 && (!endReached || nZeroPadding != 0)) {
	size_t nRead = 0;

	{
	  REAL *RESTRICT ptrRead = inBuf.data() + inBuf.size() - mindftleno2;

	  while(nRead < mindftleno2) {
	    if (!endReached) {
	      size_t r = in->read(ptrRead + nRead, mindftleno2 - nRead);
	      if (r == 0) {
		endReached = true;
		nZeroPadding = firlen;
	      }
	      nRead += r;
	    } else {
	      size_t r = std::min(mindftleno2 - nRead, nZeroPadding);
	      memset(ptrRead + nRead, 0, r * sizeof(REAL));
	      nRead += r;
	      nZeroPadding -= r;
	      if (nZeroPadding == 0) break;
	    }
	  }

	  memset(ptrRead + nRead, 0, (mindftleno2 - nRead) * sizeof(REAL));

	  //

	  memcpy(dftbuf              , ptrRead, mindftleno2 * sizeof(REAL));
	  memset(dftbuf + mindftleno2, 0      , mindftleno2 * sizeof(REAL));

	  SleefDFT_execute(dftf[l2mindftlen].get(), dftbuf, dftbuf);

	  dftbuf[0] = dftfilter0[0] * dftbuf[0];
	  dftbuf[1] = dftfilter0[1] * dftbuf[1]; 

	  for(unsigned i=1;i<mindftleno2;i++) {
	    REAL re = dftfilter0[i*2  ] * dftbuf[i*2] - dftfilter0[i*2+1] * dftbuf[i*2+1];
	    REAL im = dftfilter0[i*2+1] * dftbuf[i*2] + dftfilter0[i*2  ] * dftbuf[i*2+1];

	    dftbuf[i*2  ] = re;
	    dftbuf[i*2+1] = im;
	  }

	  SleefDFT_execute(dftb[l2mindftlen].get(), dftbuf, dftbuf);

	  //

	  for(size_t i=0;i<mindftlen;i++) overlapBuf[i] += dftbuf[i];
	  overlapLen = std::max(overlapLen, mindftlen);
	}

	//

	for(unsigned level = 0;level <= (l2maxdftlen - l2mindftlen);level++) {
	  const unsigned l2dftlen = l2mindftlen + level;
	  const size_t dftlen = size_t(1) << l2dftlen, dftleno2 = dftlen / 2;

	  if (!(level == 0 || (dftCount & ((1U << level) - 1)) == 0)) continue;

	  memcpy(dftbuf           , inBuf.data() + maxdftleno2 - dftleno2, dftleno2 * sizeof(REAL));
	  memset(dftbuf + dftleno2, 0                                    , dftleno2 * sizeof(REAL));

	  SleefDFT_execute(dftf[l2dftlen].get(), dftbuf, dftbuf);

	  dftbuf[0] = dftfilter[l2dftlen][0] * dftbuf[0];
	  dftbuf[1] = dftfilter[l2dftlen][1] * dftbuf[1]; 

	  for(unsigned i=1;i<dftleno2;i++) {
	    REAL re = dftfilter[l2dftlen][i*2  ] * dftbuf[i*2] - dftfilter[l2dftlen][i*2+1] * dftbuf[i*2+1];
	    REAL im = dftfilter[l2dftlen][i*2+1] * dftbuf[i*2] + dftfilter[l2dftlen][i*2  ] * dftbuf[i*2+1];

	    dftbuf[i*2  ] = re;
	    dftbuf[i*2+1] = im;
	  }

	  SleefDFT_execute(dftb[l2dftlen].get(), dftbuf, dftbuf);

	  //

	  for(size_t i=0;i<dftlen;i++) overlapBuf[i] += dftbuf[i];
	  overlapLen = std::max(overlapLen, dftlen);
	}

	const size_t nOut = std::min(nRead, nSamples);

	for(size_t i=0;i<nOut;i++) out[i] = overlapBuf[i];

	if (nOut < nRead) {
	  for(size_t i=0;i<nRead - nOut;i++) fractionBuf[i] = overlapBuf[nOut + i];
	  fractionLen = nRead - nOut;
	}

	memmove(inBuf.data(), inBuf.data() + mindftleno2, (inBuf.size() - mindftleno2) * sizeof(REAL));
	memmove(overlapBuf.data(), overlapBuf.data() + mindftleno2, (overlapBuf.size() - mindftleno2) * sizeof(REAL));
	memset(overlapBuf.data() + overlapBuf.size() - mindftleno2, 0, mindftleno2 * sizeof(REAL));
	if (overlapLen >= mindftleno2) overlapLen -= mindftleno2; else overlapLen = 0;

	out += nOut;
	nSamples -= nOut;
	ret += nOut;

	dftCount++;

	if (fractionLen > 0) break;
      }

      if (nSamples > 0) {
	size_t nOut = std::min(fractionLen, nSamples);
	memcpy(out, fractionBuf.data(), nOut * sizeof(REAL));
	memmove(fractionBuf.data(), fractionBuf.data() + nOut, (fractionLen - nOut) * sizeof(REAL));
	fractionLen -= nOut;
	nSamples -= nOut;
	out += nOut;
	ret += nOut;
      }

      return ret;
    }
  };
}
#endif // #ifndef PARTDFTFILTERMT_HPP
