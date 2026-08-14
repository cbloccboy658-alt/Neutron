#ifndef KAISER_HPP
#define KAISER_HPP

#include <cmath>
#include <vector>
#include <memory>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028842
#endif

namespace shibatch {
  class KaiserWindow {
    static std::vector<double> factVector(unsigned n) {
      if (n == 0) {
	return std::vector<double>{ 1 };
      } else {
	std::vector<double> v = factVector(n-1);
	v.push_back(tgamma(n+1));
	return v;
      }
    }

  public:
    static double sinc(double x) { return x == 0 ? 1 : (sin(x) / x); }

    /**
     * @param aa : stop band attenuation (dB)
     */
    static double alpha(double aa) {
      if (aa <= 21) return 0;
      if (aa <= 50) return 0.5842 * pow(aa - 21, 0.4) + 0.07886 * (aa - 21);
      return 0.1102 * (aa - 8.7);
    }

    static double izero(double x, int M = 30) {
      static std::vector<double> factorial = factVector(180);

      double ret = 1;
      for(int m = M;m >= 1;m--) {
	double t = pow(x/2, m) / factorial[m];
	ret += t * t;
      }
      return ret;
    }

    /**
     * @param aa : stop band attenuation (dB)
     * @param fs : sampleing frequency (Hz)
     * @param df : transition band width (Hz)
     */
    static int length(double aa, double fs, double df) {
      double d = aa <= 21 ? 0.9222 : (aa-7.95)/14.36;
      int len = fs * d / df + 1;
      if ((len & 1) == 0) len++;
      return len;
    }

    static double transitionBandWidth(double aa, double fs, int length) {
      double d = aa <= 21 ? 0.9222 : (aa-7.95)/14.36;
      return (fs * d) / (length - 1);
    }

    static double window(int n, int len, double alp, double iza) {
      if (n > len - 1) return 0;
      return izero(alp * sqrt(1 - 4.0*n*n/((len-1.0)*(len-1.0)))) / iza;
    }

    /**
     * @param fp : pass-band edge frequency (Hz)
     * @param fs : sampleing frequency (Hz)
     */
    static double hn_lpf(int n, double fp, double fs) {
      double t = 1.0 / fs;
      double omega = 2 * M_PI * fp;
      return 2 * fp * t * sinc(n * omega * t);
    }

    /**
     * @param fs : sampleing frequency (Hz)
     * @param fp : pass-band edge frequency (Hz)
     * @param df : transition band width (Hz)
     * @param aa : stop band attenuation (dB)
     */
    template<typename REAL>
    static std::shared_ptr<std::vector<REAL>> makeLPF(double fs, double fp, double df, double aa, double gain = 1) {
      double alp = alpha(aa), iza = izero(alp);
      int64_t len = length(aa, fs, df);
      auto filter = std::make_shared<std::vector<REAL>>(len);
      REAL *filterData = filter->data();
      for(int i=0;i<len;i++) filterData[i] = window(i - len/2, len, alp, iza) * hn_lpf(i - len/2, fp, fs) * gain;
      return filter;
    }

    /**
     * @param fs  : sampleing frequency (Hz)
     * @param fp  : pass-band edge frequency (Hz)
     * @param len : filter length
     * @param aa  : stop band attenuation (dB)
     */
    template<typename REAL>
    static std::shared_ptr<std::vector<REAL>> makeLPF(double fs, double fp, int64_t len, double aa, double gain = 1) {
      double alp = alpha(aa), iza = izero(alp);
      if ((len & 1) == 0) len++;
      auto filter = std::make_shared<std::vector<REAL>>(len);
      REAL *filterData = filter->data();
      for(int i=0;i<=len/2;i++)
	filterData[len/2 + i] = filterData[len/2 - i] = window(i, len, alp, iza) * hn_lpf(i, fp, fs) * gain;
      return filter;
    }

    static double hn_bpf(int n, double fp0, double g0, double fp1, double g1, double fs, int K) {
      double sum = 0;
      for(int k=0;k<K;k++) {
	double fl = (k+0)*(fp1-fp0)/K + fp0;
	double fh = (k+1)*(fp1-fp0)/K + fp0;
	double g = exp((k+0)*(log(g1)-log(g0))/K + log(g0));
	sum += (hn_lpf(n, fh, fs) - hn_lpf(n, fl, fs)) * g;
      }
      return sum;
    }

    template<typename REAL>
    static std::vector<REAL> makeBPF(double fs, double fp0, double g0, double fp1, double g1, int64_t len, double aa, int K, double gain = 1) {
      double alp = alpha(aa), iza = izero(alp);
      if ((len & 1) == 0) len++;
      std::vector<REAL> filter(len);
      for(int i=0;i<=len/2;i++)
	filter[len/2 + i] = filter[len/2 - i] = window(i, len, alp, iza) * hn_bpf(i, fp0, g0, fp1, g1, fs, K) * gain;
      return filter;
    }
  };
}
#endif // #ifndef KAISER_HPP
