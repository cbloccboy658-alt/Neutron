#ifndef MINRCEPS_HPP
#define MINRCEPS_HPP

#include <vector>
#include <memory>
#include <cmath>
#include <cstring>

#include "ObjectCache.hpp"

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028842
#endif

namespace shibatch {
  class Minrceps {
    static constexpr const size_t toPow2(size_t n) {
      size_t ret = 1;
      for(;ret < n && ret != 0;ret *= 2) ;
      return ret;
    }

    std::vector<double> createWindow(size_t N) {
      // 7-term Blackman-Harris
      // https://dsp.stackexchange.com/questions/51095/seven-term-blackman-harris-window
      static const double coef[] = {
	.27105140069342, -0.43329793923448, 0.21812299954311, -0.06592544638803,
	0.01081174209837, -0.00077658482522, 0.00001388721735
      };

      std::vector<double> v(N);

      for(size_t n=0;n<N;n++) {
	for(int k=0;k<7;k++) v[n] += coef[k] * (1.0 / .27105140069342) * cos((2*M_PI/(N*2))*k*(n + N));
      }

      return v;
    }

    const unsigned L;
    std::shared_ptr<SleefDFT> dftf, dftb;
    double *dftbuf = nullptr;

  public:
    Minrceps(unsigned L_) : L(toPow2(L_)) {
      dftf = ssrc::constructSleefDFT<double>(SLEEF_MODE_REAL | SLEEF_MODE_ALT | SLEEF_MODE_FORWARD  | SLEEF_MODE_NO_MT, L);
      dftb = ssrc::constructSleefDFT<double>(SLEEF_MODE_REAL | SLEEF_MODE_ALT | SLEEF_MODE_BACKWARD | SLEEF_MODE_NO_MT, L);
      dftbuf = (double *)Sleef_malloc(L * sizeof(double));
    }

    ~Minrceps() {
      Sleef_free(dftbuf);
    }

    // Smith AD, Ferguson RJ. Minimum-phase signal calculation using the real cepstrum. CREWES Res. Report. 2014;26(72).

    template<typename REAL>
    std::shared_ptr<std::vector<REAL>> execute(std::shared_ptr<std::vector<REAL>> in, const double alpha = 1.0 - ldexp(1, -20)) {
      REAL *inp = in->data();
      auto window = createWindow(in->size());

      memset(dftbuf, 0, L * sizeof(double));

      double a = 1.0, ein = 0;
      for(unsigned i=0;i<in->size();i++) { dftbuf[i] = inp[i] * a; ein += inp[i]; a *= alpha; }

      SleefDFT_execute(dftf.get(), dftbuf, dftbuf);

      for(unsigned i=0;i<L/2;i++) {
	dftbuf[i*2+0] = log(hypot(dftbuf[i*2+0], dftbuf[i*2+1])) * (1.0 / L);
	dftbuf[i*2+1] = 0;
      }

      SleefDFT_execute(dftb.get(), dftbuf, dftbuf);

      for(unsigned i=1;i<L/2;i++) dftbuf[i] += dftbuf[L - i];

      auto out = std::make_shared<std::vector<REAL>>(in->size());
      REAL *outp = out->data();

      outp[0] = exp(dftbuf[0] / 2) * window[0];
      double eout = outp[0] * outp[0];
      a = 1.0 / alpha;
      for(unsigned n=1;n<out->size();n++) {
	double sum = 0;
	for(unsigned k=1;k<=n;k++) sum += k * (1.0 / n) * dftbuf[k] * outp[n - k];
	outp[n] = sum * a * window[n];
	eout += outp[n];
	a *= (1.0 / alpha);
      }

      for(unsigned n=0;n<out->size();n++) outp[n] *= ein / eout;

      return out;
    }
  };
}
#endif // #ifndef MINRCEPS_HPP
