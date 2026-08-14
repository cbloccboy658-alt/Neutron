#ifndef FASTPP_HPP
#define FASTPP_HPP

#include <memory>
#include <cstring>
#include <vector>
#include <cstdint>

#include "shibatch/ssrc.hpp"

namespace shibatch {
  template<typename REAL>
  class FastPP : public ssrc::StageOutlet<REAL> {
    const size_t N = 65536;
    std::shared_ptr<ssrc::StageOutlet<REAL>> inlet;
    const size_t sfs, lcmfs, dfs, sstep, dstep, firlen;

    std::vector<REAL> buf;
    std::vector<std::vector<REAL>> fircoef;
    size_t dpos = 0, ssize = 0, dsize = 0, buflast = 0;

  public:
    FastPP(std::shared_ptr<ssrc::StageOutlet<REAL>> in_, int64_t sfs_, int64_t lcmfs_, int64_t dfs_, const REAL *fircoef_, size_t firlen_) :
      inlet(in_), sfs(sfs_), lcmfs(lcmfs_), dfs(dfs_), sstep(lcmfs / sfs), dstep(lcmfs / dfs), firlen(firlen_) {
      fircoef.resize(sstep);
      for(size_t i=0;i<sstep;i++) fircoef[i].resize((firlen + sstep - 1) / sstep);
      for(size_t i=0;i<firlen;i++) fircoef[i % sstep][i / sstep] = fircoef_[firlen - 1 - i];

      buf.resize((firlen + N * dstep) / sstep + 2);
    }

    bool atEnd() {
      return dpos >= dsize;
    }

    size_t read(REAL *out, size_t nSamples) {
      size_t nOut = 0;

      while(nSamples > 0) {
	size_t nRead = inlet->read(buf.data() + buflast, buf.size() - buflast);
	ssize += nRead;
	dsize = ssize * sstep / dstep;

	bool endReached = nRead == 0;

	if (dpos >= dsize) return nOut;

	buflast += nRead;
	memset(buf.data() + buflast, 0, (buf.size() - buflast) * sizeof(REAL));

	const size_t sorg = (dpos * dstep + sstep - 1) / sstep;
	const size_t bs = std::min(nSamples, N);

	for(size_t i=0;i<bs && dpos < dsize;i++) {
	  const size_t spos = (dpos * dstep + sstep - 1) / sstep;
	  const size_t filterpos = spos * sstep - dpos * dstep;

	  if ((firlen + sstep - 1) / sstep - 1 + (spos - sorg) >= buflast && !endReached) break;

	  REAL sum = 0;
	  for(size_t p = 0;p < (firlen + sstep - 1) / sstep;p++) {
	    sum += fircoef[filterpos][p] * buf[p + (spos - sorg)];
	  }

	  *out++ = sum;
	  dpos++;
	  nOut++;
	  nSamples--;
	}

	const size_t slast = (dpos * dstep + sstep - 1) / sstep;
	memmove(buf.data(), buf.data() + (slast - sorg), (buf.size() - (slast - sorg)) * sizeof(REAL));
	buflast -= slast - sorg;
      }

      return nOut;
    }
  };
}
#endif // #ifndef FASTPP_HPP
