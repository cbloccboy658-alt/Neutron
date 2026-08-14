// Shibatch Sample Rate Converter written by Naoki Shibata  https://shibatch.org

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <random>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028842
#endif

static const size_t BUFSIZE = 1 << 20;

#include "ArrayQueue.hpp"

#include "shibatch/ssrc.hpp"
#include "shibatch/shapercoefs.h"

using namespace std;
using namespace ssrc;
using namespace shibatch;

struct ConversionProfile {
  unsigned log2dftfilterlen;
  double aa, guard;
  bool doublePrecision;
};

const unordered_map<string, ConversionProfile> availableProfiles = {
  { "insane",    { 18, 200, 8.0, true } },
  { "high",      { 16, 170, 4.0, true } },
  { "long",      { 15, 145, 4.0, true } },
  { "standard",  { 14, 145, 2.0, false} },
  { "short",     { 12,  96, 1.0, false} },
  { "fast",      { 10,  96, 1.0, false} },
  { "lightning", {  8,  96, 1.0, false} },
};

const unordered_map<string, ContainerFormat> availableContainers = {
  { "riff", ContainerFormat::RIFF },
  { "RIFF", ContainerFormat::RIFF },
  { "w64" , ContainerFormat::W64  },
  { "W64" , ContainerFormat::W64  },
  { "rf64", ContainerFormat::RF64 },
  { "RF64", ContainerFormat::RF64 },
};

void showProfileOptions() {
  cerr << "Available profiles :" << endl;
  for(auto p : availableProfiles) {
    cerr << "Profile name : " << p.first << endl;
    cerr << "  FFT length : " << (1LL << p.second.log2dftfilterlen) << endl;
    cerr << "  Stop band attenuation : " << p.second.aa << " dB" << endl;
    cerr << "  Guard factor : " << p.second.guard << endl;
    cerr << "  Floating point precision : " << (p.second.doublePrecision ? "double" : "single") << endl;
    cerr << endl;
  }
  exit(-1);
}

void showDitherOptions() {
  cerr << "Available dither options :" << endl;
  int lastfs = 0;
  for(int i=0;;i++) {
    const NoiseShaperCoef &c = noiseShaperCoef[i];
    if (c.fs < 0) break;
    if (lastfs != c.fs) {
      cerr << endl << "Sampling freq : " << c.fs << endl;
      lastfs = c.fs;
    }
    cerr << "  ID " << c.id << " : " << c.name << endl;
  }
  exit(-1);
}

void showContainerOptions() {
  cerr << "Available containers : riff, w64, rf64" << endl;
  exit(-1);
}

void showUsage(const string& argv0, const string& mes = "") {
  cerr << ("Shibatch Sample Rate Converter  Version " + versionString()) << endl;
  cerr << endl;
  cerr << "Usage: " << argv0 << " [<options>] <source file name> <destination file name>" << endl;
  cerr << endl;
  cerr << "Options : --rate <sampling rate(Hz)> Specify a sample rate" << endl;
  cerr << "          --att <attenuation(dB)>    Specify an attenuation level of the output signal" << endl;
  cerr << "          --bits <number of bits>    Specify an output quantization bit length" << endl;
  cerr << "                                     Specify -32 to convert to an IEEE 32-bit FP wav file" << endl;
  cerr << "          --dither <type>            Select a type of noise shaper" << endl;
  cerr << "                                       0    : Low intensity ATH-based noise shaping" << endl;
  cerr << "                                       98   : Triangular noise shaping" << endl;
  cerr << "                                       help : Show all available options" << endl;
  cerr << "          --mixChannels <matrix>     Mix channels" << endl;
  cerr << "                                       '0.5,0.5' : stereo to mono" << endl;
  cerr << "                                       '1;1'     : mono to stereo" << endl;
  cerr << "          --pdf <type> [<amp>]       Select a probability distribution function for dithering" << endl;
  cerr << "                                       0 : Rectangular" << endl;
  cerr << "                                       1 : Triangular" << endl;
  //cerr << "                                       2 : Gaussian" << endl;
  //cerr << "                                       3 : Two-level (experimental)" << endl;
  cerr << "          --profile <name>           Select a conversion profile" << endl;
  cerr << "                                       fast : Enough quality for almost every purpose" << endl;
  cerr << "                                       help : Show all available options" << endl;
  cerr << "          --minPhase                 Use minimum phase filters instead of linear phase filters" << endl;
  cerr << "          --partConv <log2len>       Divide a long filter into smaller sub-filters so that they"<< endl;
  cerr << "                                     can be applied without significant processing delays." << endl;
  cerr << "          --st                       Disable multithreading" << endl;
  cerr << "          --dstContainer <name>      Select a container of output file" << endl;
  cerr << "                                       riff : The most common WAV format" << endl;
  cerr << "                                       help : Show all available options" << endl;
  cerr << "          --genImpulse <fs> <nch> <period>" << endl;
  cerr << "                                     Generate an impulse signal" << endl;
  cerr << "          --genSweep <fs> <nch> <length> <startfs> <endfs>" << endl;
  cerr << "                                     Generate a sweep signal" << endl;
  cerr << endl;
  cerr << "If you like this tool, visit https://github.com/shibatch/ssrc and give it a star." << endl;
  cerr << endl;

  if (mes != "") cerr << "Error : " << mes << endl;

  exit(-1);
}

class RectangularRNG : public DoubleRNG {
  const double min, max;
  default_random_engine rng;
  uniform_real_distribution<double> dist{0.0, 1.0};
public:
  RectangularRNG(double min_, double max_, uint64_t seed) : min(min_), max(max_), rng(seed) {}
  double nextDouble() { return min + dist(rng) * (max - min); }
};

template<typename T>
class ImpulseGenerator : public ssrc::OutletProvider<T> {
  class Outlet : public ssrc::StageOutlet<T> {
    const double amp;
    const size_t period;
    size_t remaining, n;

  public:
    Outlet(double amp_, size_t period_, size_t n_) :
      amp(amp_), period(period_), remaining(period_-1), n(n_) {}
    ~Outlet() {}

    bool atEnd() { return n == 0; }

    size_t read(T *out, size_t nSamples) {
      size_t ret = 0;

      nSamples = min(n, nSamples);

      while(nSamples > 0) {
	while(remaining > 0 && nSamples > 0) {
	  *out++ = 0;
	  ret++;
	  nSamples--;
	  remaining--;
	}

	if (nSamples == 0) break;
	*out++ = amp;
	ret++;
	nSamples--;

	remaining = period - 1;
      }

      n -= ret;

      return ret;
    }
  };

  WavFormat format;
  vector<shared_ptr<Outlet>> v;

public:
  ImpulseGenerator(const WavFormat& format_, double amp_, size_t period_, size_t n_) : format(format_) {
    v.resize(format.channels);
    for(unsigned i=0;i<format.channels;i++) v[i] = make_shared<Outlet>(amp_, period_, n_);
  }

  shared_ptr<ssrc::StageOutlet<T>> getOutlet(uint32_t c) { return v[c]; }
  WavFormat getFormat() { return format; }
};

template<typename T>
class SweepGenerator : public ssrc::OutletProvider<T> {
  class Outlet : public ssrc::StageOutlet<T> {
    const uint32_t fs, ch;
    const double start, end, amp;
    const size_t total;
    size_t n;
    double phase = 0;

  public:
    Outlet(uint32_t fs_, uint32_t ch_, double start_, double end_, double amp_, size_t total_) :
      fs(fs_), ch(ch_), start(start_), end(end_), amp(amp_), total(total_), n(total_) {}
    ~Outlet() {}

    bool atEnd() { return n == 0; }

    size_t read(T *out, size_t nSamples) {
      nSamples = min(n, nSamples);

      for(size_t i=0;i<nSamples;i++) {
	*out++ = amp * sin(phase + ch);
	phase += M_PI * 2 * (end + (start - end) * (n - i) / total) / fs;
      }

      n -= nSamples;

      return nSamples;
    }
  };

  WavFormat format;
  vector<shared_ptr<Outlet>> v;

public:
  SweepGenerator(const WavFormat& format_, double start_, double end_, double amp_, size_t n_) : format(format_) {
    v.resize(format.channels);
    for(unsigned i=0;i<format.channels;i++)
      v[i] = make_shared<Outlet>(format.sampleRate, (start_ == 0 && end_ == 0) ? 0 : i, start_, end_, amp_, n_);
  }

  shared_ptr<ssrc::StageOutlet<T>> getOutlet(uint32_t c) { return v[c]; }
  WavFormat getFormat() { return format; }
};

template<typename T>
class BufferStage : public StageOutlet<T> {
  shared_ptr<ssrc::StageOutlet<T>> inlet;
  const size_t N;
  vector<T> buf;
  size_t pos = 0;
public:
  BufferStage(shared_ptr<ssrc::StageOutlet<T>> in_, size_t N_ = 65536) : inlet(in_), N(N_) {}

  size_t size() { return buf.size(); }

  bool atEnd() { return pos == buf.size(); }

  void execute() {
    pos = 0;
    for(;;) {
      buf.resize(buf.size() + N);
      size_t z = inlet->read(buf.data() + pos, N);
      pos += z;
      buf.resize(pos);
      if (z == 0) break;
    }
    pos = 0;
  }

  size_t read(T *out, size_t nSamples) {
    nSamples = min(buf.size() - pos, nSamples);
    for(size_t i = 0;i < nSamples;i++) out[i] = buf[i + pos];
    pos += nSamples;
    return nSamples;
  }
};

static inline int64_t timeus() {
  return chrono::duration_cast<chrono::microseconds>
    (chrono::system_clock::now() - chrono::system_clock::from_time_t(0)).count();
}

enum SrcType { FILEIN, STDIN, IMPULSE, SWEEP };
enum DstType { FILEOUT, STDOUT };

template<typename REAL>
struct Pipeline {
  string argv0, srcfn, dstfn, profileName, dstContainerName;
  uint64_t dstChannelMask;
  int64_t rate, bits, dither, pdf;
  const vector<vector<double>>& mixMatrix;
  uint64_t seed;
  double att, peak;
  bool minPhase, quiet, debug, mt;
  int l2mindftflen;

  enum SrcType src;
  enum DstType dst;

  size_t impulsePeriod, sweepLength;
  double sweepStart, sweepEnd;
  int generatorNch, generatorFs;

  ConversionProfile profile;

  Pipeline(const string& argv0_, const string &srcfn_, const string &dstfn_,
	   const string &profileName_, const string &dstContainerName_, uint64_t dstChannelMask_,
	   int64_t rate_, int64_t bits_, int64_t dither_, int64_t pdf_, const vector<vector<double>>& mixMatrix_,
	   uint64_t seed_, double att_, double peak_, bool minPhase_, bool quiet_, bool debug_, bool mt_,
	   int l2mindftflen_,
	   enum SrcType src_, enum DstType dst_, size_t impulsePeriod_, size_t sweepLength_,
	   double sweepStart_, double sweepEnd_, int generatorNch_, int generatorFs_, ConversionProfile profile_) :
    argv0(argv0_), srcfn(srcfn_), dstfn(dstfn_),
    profileName(profileName_), dstContainerName(dstContainerName_), dstChannelMask(dstChannelMask_),
    rate(rate_), bits(bits_), dither(dither_), pdf(pdf_), mixMatrix(mixMatrix_),
    seed(seed_), att(att_), peak(peak_), minPhase(minPhase_), quiet(quiet_), debug(debug_), mt(mt_),
    l2mindftflen(l2mindftflen_), src(src_), dst(dst_), impulsePeriod(impulsePeriod_), sweepLength(sweepLength_),
    sweepStart(sweepStart_), sweepEnd(sweepEnd_), generatorNch(generatorNch_), generatorFs(generatorFs_), profile(profile_) {}

  void execute() {
    if (seed == ~0ULL) seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    const double gain = (1LL << (bits - 1)) - 1;
    const int32_t clipMin = bits != 8 ? -(1LL << (bits - 1)) + 0 : 0x00;
    const int32_t clipMax = bits != 8 ? +(1LL << (bits - 1)) - 1 : 0xff;
    const int32_t offset  = bits != 8 ? 0 : 0x80;

    shared_ptr<OutletProvider<REAL>> origin;

    switch(src) {
    case FILEIN:
      origin = make_shared<WavReader<REAL>>(srcfn, mt);
      break;
    case STDIN:
      origin = make_shared<WavReader<REAL>>(mt);
      break;
    case IMPULSE:
      origin = make_shared<ImpulseGenerator<REAL>>
	(WavFormat(WavFormat::IEEE_FLOAT, generatorNch, generatorFs, 32), 0.5, impulsePeriod, impulsePeriod * 2);
      break;
    case SWEEP:
      origin = make_shared<SweepGenerator<REAL>>
	(WavFormat(WavFormat::IEEE_FLOAT, generatorNch, generatorFs, 32), sweepStart, sweepEnd, 0.5, sweepLength);
      break;
    }

    const WavFormat srcFormat = origin->getFormat();
    const ContainerFormat srcContainer = origin->getContainer();
    const int sfs = srcFormat.sampleRate;
    const int dfs = rate < 0 ? srcFormat.sampleRate : rate;
    const int snch = srcFormat.channels, dnch = mixMatrix.size() == 0 ? snch : mixMatrix.size();

    if (mixMatrix.size() != 0 && mixMatrix[0].size() != (size_t)snch)
      showUsage(argv0, "The number of channels in the source and the matrix you specified with --mixChannels do not match");

    if (dstContainerName == "" && srcContainer.c == 0) dstContainerName = "RIFF";
    if (dstContainerName == "" && srcContainer.c != 0) dstContainerName = to_string(srcContainer);

    if (availableContainers.count(dstContainerName) == 0)
      showUsage(argv0, "There is no container of name \"" + dstContainerName + "\"");

    const ContainerFormat dstContainer = availableContainers.at(dstContainerName);

    WavFormat dstFormat;

    switch(srcFormat.formatTag) {
    case WavFormat::PCM:
    case WavFormat::IEEE_FLOAT:
      dstFormat = bits < 0 ?
	WavFormat(WavFormat::IEEE_FLOAT, dnch, dfs, -bits) :
	WavFormat(WavFormat::PCM       , dnch, dfs,  bits);
      break;
    case WavFormat::EXTENSIBLE:
      if (dstChannelMask == ~0ULL && mixMatrix.size() != 0)
	showUsage(argv0, "You have to specify --channelMask because you specified --mixChannels and the source format tag is extensible");

      dstFormat =
	WavFormat(WavFormat::EXTENSIBLE, dnch, dfs, abs(bits), srcFormat.channelMask,
		  bits < 0 ? WavFormat::KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : WavFormat::KSDATAFORMAT_SUBTYPE_PCM);
      if (dstChannelMask != ~0ULL) dstFormat.channelMask = dstChannelMask;
      break;
    default:
      showUsage(argv0, "Unsupported format tag in the source wav");
      break;
    }

    int shaperid = -1;

    for(int i=0;;i++) {
      const NoiseShaperCoef &c = noiseShaperCoef[i];
      if (c.fs < 0) break;
      if (c.fs == dfs && c.id == dither) {
	shaperid = i;
	break;
      }
    }

    if (dither != -1 && shaperid == -1)
      showUsage(argv0, "Dither type " + to_string(dither) + " is not available for destination sampling frequency " + to_string(dfs) + "Hz");

    if (l2mindftflen < 0) l2mindftflen += profile.log2dftfilterlen + 1;

    //

    int64_t timeBeforeInit = 0, timeBeforeExec = 0;

    if (debug) {
      cerr << "Version = " << versionString() << endl;
      cerr << "Build info = " << buildInfo() << endl;
      cerr << endl;

      cerr << "srcfn = "        << srcfn << endl;
      cerr << "sfs = "          << sfs << endl;
      cerr << "snch = "         << snch << endl;
      cerr << "srcContainer = " << to_string(srcContainer) << endl;
      cerr << "srcFormatTag = ";
      switch(srcFormat.formatTag) {
      case WavFormat::PCM: cerr << "PCM" << endl; break;
      case WavFormat::IEEE_FLOAT: cerr << "IEEE_FLOAT" << endl; break;
      case WavFormat::EXTENSIBLE: cerr << "EXTENSIBLE" << endl;
	cerr << "srcChannelMask = 0x" << format("{:x}", srcFormat.channelMask) << endl;
	break;
      }
      cerr << endl;

      cerr << "dstfn = "        << dstfn << endl;
      cerr << "dfs = "          << dfs << endl;
      cerr << "dnch = "         << dnch << endl;
      cerr << "dstContainer = " << to_string(dstContainer) << endl;
      cerr << "bits = "         << bits << endl;
      cerr << endl;

      if (mixMatrix.size() != 0) {
	cerr << "mixMatrix = ";
	for(auto r : mixMatrix) {
	  cerr << "[";
	  for(auto c : r) {
	    cerr << c << ",";
	  }
	  cerr << "];";
	}
	cerr << endl << endl;
      }

      cerr << "profileName = "  << profileName << endl;
      cerr << "dftfilterlen = " << (1LL << profile.log2dftfilterlen) << endl;
      cerr << "doublePrec = "   << profile.doublePrecision << endl;
      cerr << "aa = "           << profile.aa << endl;
      cerr << "guard = "        << profile.guard << endl;
      cerr << endl;

      cerr << "dither = "       << dither << endl;
      cerr << "shaperid = "     << shaperid;
      if (shaperid != -1) cerr << " (fs = " << noiseShaperCoef[shaperid].fs <<
			    ", name = " << noiseShaperCoef[shaperid].name << ")";
      cerr << endl;
      cerr << "pdf = "          << pdf << endl;
      cerr << "peak = "         << peak << endl;
      cerr << endl;

      cerr << "att = "          << att << endl;
      cerr << "seed = "         << seed << endl;
      cerr << "quiet = "        << quiet << endl;
      cerr << "minPhase = "     << minPhase << endl;
      cerr << "l2mindftflen = " << l2mindftflen << endl;
      cerr << "mt = "           << mt << endl;
      cerr << endl;

      if (src == IMPULSE || src == SWEEP) {
	cerr << "generatorFs = "  << generatorFs << endl;
	cerr << "impulsePeriod = " << impulsePeriod << endl;
	cerr << "sweepLength = "  << sweepLength << endl;
	cerr << "sweepStart = "   << sweepStart << endl;
	cerr << "sweepEnd = "     << sweepStart << endl;
      }

      timeBeforeInit = timeus();
    }

    shared_ptr<OutletProvider<REAL>> in = origin;

    if (mixMatrix.size() != 0) in = make_shared<ChannelMixer<REAL>>(in, mixMatrix);

    double delay = 0;

    if (shaperid == -1 || bits < 0) {
      vector<shared_ptr<ssrc::StageOutlet<REAL>>> out(dnch);
      size_t nFrames = 0;

      for(int i=0;i<dnch;i++) {
	auto ssrc = make_shared<SSRC<REAL>>(in->getOutlet(i), sfs, dfs,
					    profile.log2dftfilterlen, profile.aa, profile.guard, pow(10, att/-20.0), minPhase, l2mindftflen, mt);
	out[i] = ssrc;
	delay = ssrc->getDelay();

	if (dst == STDOUT) {
	  auto buf = make_shared<BufferStage<REAL>>(ssrc);
	  out[i] = buf;
	  buf->execute();
	  nFrames = max(nFrames, buf->size());
	}
      }

      auto writer = dst == FILEOUT ? make_shared<WavWriter<REAL>>(dstfn, dstFormat, dstContainer, out, 0, BUFSIZE, mt) :
	make_shared<WavWriter<REAL>>("", dstFormat, dstContainer, out, nFrames, BUFSIZE, mt);

      timeBeforeExec = timeus();

      writer->execute();
    } else {
      vector<shared_ptr<ssrc::StageOutlet<int32_t>>> out(dnch);
      size_t nFrames = 0;

      for(int i=0;i<dnch;i++) {
	std::shared_ptr<DoubleRNG> rng;
	if (pdf == 0) {
	  rng = createTriangularRNG(peak, seed + i);
	} else {
	  rng = make_shared<RectangularRNG>(-peak, peak, seed + i);
	}

	auto ssrc = make_shared<SSRC<REAL>>(in->getOutlet(i), sfs, dfs,
					    profile.log2dftfilterlen, profile.aa, profile.guard, pow(10, att/-20.0), minPhase, l2mindftflen, mt);

	delay = ssrc->getDelay();

	auto dither = make_shared<Dither<int32_t, REAL>>(ssrc, gain, offset, clipMin, clipMax, &ssrc::noiseShaperCoef[shaperid], rng);
	out[i] = dither;

	if (dst == STDOUT) {
	  auto buf = make_shared<BufferStage<int32_t>>(dither);
	  out[i] = buf;
	  buf->execute();
	  nFrames = max(nFrames, buf->size());
	}
      }

      auto writer = dst == FILEOUT ? make_shared<WavWriter<int32_t>>(dstfn, dstFormat, dstContainer, out, 0, BUFSIZE, mt) :
	make_shared<WavWriter<int32_t>>("", dstFormat, dstContainer, out, nFrames, BUFSIZE, mt);

      timeBeforeExec = timeus();

      writer->execute();
    }

    if (debug) {
      cerr << endl << "Delay : " << delay << " samples" << endl;
      int64_t timeEnd = timeus();
      cerr << endl << "Elapsed time : " << ((timeEnd - timeBeforeInit) * 0.000001) << " seconds" << endl;
      if (dst != STDOUT) cerr << "Processing time : " << ((timeEnd - timeBeforeExec) * 0.000001) << " seconds" << endl;
    }
  }
};

vector<vector<double>> parseMixString(const string &s) {
  vector<vector<double>> ret;
  const char* p = s.c_str();
  size_t nc = 0;

  vector<double> r;
  for(;;) {
    char *q;
    double d = strtod(p, &q);
    if (p == q) throw(runtime_error(("parseMixString : syntax error : " + s.substr(p - s.c_str())).c_str()));
    r.push_back(d);
    switch(*q) {
    case ',': break;
    case ';': case '_':
      if (nc == 0) nc = r.size();
      if (nc != r.size()) throw(runtime_error("parseMixString : inconsistent number of column"));
      ret.push_back(r);
      r.clear();
      break;
    case '\0':
      if (nc == 0) nc = r.size();
      if (nc != r.size()) throw(runtime_error("parseMixString : inconsistent number of column"));
      ret.push_back(r);
      return ret;
    default: throw(runtime_error(("parseMixString : syntax error : " + s.substr(q - s.c_str())).c_str()));
    }
    p = q + 1;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) showUsage(argv[0], "");

  string srcfn, dstfn, profileName = "standard", dstContainerName = "";
  int64_t rate = -1, bits = 16, dither = -1, pdf = 0;
  uint64_t seed = ~0ULL, dstChannelMask = ~0ULL;
  double att = 0, peak = 1.0;
  bool minPhase = false;
  vector<vector<double>> mixMatrix;
  bool mt = true, quiet = false, debug = false;
  int l2mindftflen = 0;

  enum SrcType src = FILEIN;
  enum DstType dst = FILEOUT;

  size_t impulsePeriod = 0, sweepLength = 0;
  double sweepStart = 0, sweepEnd = 0;
  int generatorNch = 1, generatorFs = 0;

  int nextArg;
  for(nextArg = 1;nextArg < argc;nextArg++) {
    if (string(argv[nextArg]) == "--rate") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      rate = strtol(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p || rate < 0)
	showUsage(argv[0], "A non-negative integer is expected after --rate.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--att") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      att = strtod(argv[nextArg+1], &p);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "A number is expected after --att.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--bits") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      bits = strtol(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "An integer is expected after --bits.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--dither") {
      if (nextArg == 1 && argc == 2) showDitherOptions();
      if (nextArg+1 >= argc) showUsage(argv[0]);
      if (argv[nextArg+1] == string("help")) showDitherOptions();
      char *p;
      dither = strtol(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p || dither < 0)
	showUsage(argv[0], "A positive value is expected after --dither.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--pdf") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      pdf = strtol(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p || pdf < 0)
	showUsage(argv[0], "A positive value is expected after --pdf.");
      nextArg++;
      double peak_ = strtod(argv[nextArg+1], &p);
      if (*p == '\0') { peak = peak_; nextArg++; }
    } else if (string(argv[nextArg]) == "--profile") {
      if (nextArg == 1 && argc == 2) showProfileOptions();
      if (nextArg+1 >= argc) showUsage(argv[0], "Specify a profile name after --profile");
      if (argv[nextArg+1] == string("help")) showProfileOptions();
      profileName = argv[nextArg+1];
      nextArg++;
    } else if (string(argv[nextArg]) == "--genImpulse") {
      char *p;

      if (nextArg+1 >= argc) showUsage(argv[0], "Three positive values are expected after --genImpulse.");
      generatorFs = strtoul(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "Three positive values are expected after --genImpulse.");
      nextArg++;

      if (nextArg+1 >= argc) showUsage(argv[0], "Three positive values are expected after --genImpulse.");
      generatorNch = strtoul(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "Three positive values are expected after --genImpulse.");
      nextArg++;

      if (nextArg+1 >= argc) showUsage(argv[0], "Three positive values are expected after --genImpulse.");
      impulsePeriod = strtoull(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "Three positive values are expected after --genImpulse.");
      nextArg++;

      src = IMPULSE;
      srcfn = "[IMPULSE]";
    } else if (string(argv[nextArg]) == "--genSweep") {
      const string mes = "Five positive values are expected after --genSweep.";
      char *p;

      if (nextArg+1 >= argc) showUsage(argv[0], mes);
      generatorFs = strtoul(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p) showUsage(argv[0], mes);
      nextArg++;

      if (nextArg+1 >= argc) showUsage(argv[0], mes);
      generatorNch = strtoul(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p) showUsage(argv[0], mes);
      nextArg++;

      if (nextArg+1 >= argc) showUsage(argv[0], mes);
      sweepLength = strtoull(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p) showUsage(argv[0], mes);
      nextArg++;

      if (nextArg+1 >= argc) showUsage(argv[0], mes);
      sweepStart = strtod(argv[nextArg+1], &p);
      if (p == argv[nextArg+1] || *p) showUsage(argv[0], mes);
      nextArg++;

      if (nextArg+1 >= argc) showUsage(argv[0], mes);
      sweepEnd = strtod(argv[nextArg+1], &p);
      if (p == argv[nextArg+1] || *p) showUsage(argv[0], mes);
      nextArg++;

      src = SWEEP;
      srcfn = "[SWEEP]";
    } else if (string(argv[nextArg]) == "--stdin") {
      src = STDIN;
      srcfn = "[STDIN]";
    } else if (string(argv[nextArg]) == "--stdout") {
      dst = STDOUT;
      dstfn = "[STDOUT]";
    } else if (string(argv[nextArg]) == "--dstContainer") {
      if (nextArg == 1 && argc == 2) showContainerOptions();
      if (nextArg+1 >= argc) showUsage(argv[0], "Specify a format/container name after --dstContainer");
      if (argv[nextArg+1] == string("help")) showContainerOptions();
      dstContainerName = argv[nextArg+1];
      nextArg++;
    } else if (string(argv[nextArg]) == "--quiet") {
      quiet = true;
    } else if (string(argv[nextArg]) == "--debug") {
      debug = true;
    } else if (string(argv[nextArg]) == "--st") {
      mt = false;
    } else if (string(argv[nextArg]) == "--minPhase") {
      minPhase = true;
    } else if (string(argv[nextArg]) == "--partConv") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      l2mindftflen = strtol(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "An integer is expected after --partConv.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--seed") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      seed = strtoull(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "A positive integer is expected after --seed.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--channelMask") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      dstChannelMask = strtoul(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "A positive integer is expected after --channelMask.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--mixChannels") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      try {
	mixMatrix = parseMixString(argv[nextArg+1]);
      } catch(exception &ex) {
	showUsage(argv[0], ex.what());
      }
      nextArg++;
    } else if (string(argv[nextArg]) == "--tmpfile") {
      showUsage(argv[0], "--tmpfile option is no longer available.");
    } else if (string(argv[nextArg]) == "--twopass") {
      showUsage(argv[0], "--twopass option is no longer available.");
    } else if (string(argv[nextArg]) == "--normalize") {
      showUsage(argv[0], "--normalize option is no longer available.");
    } else if (string(argv[nextArg]).substr(0, 2) == "--") {
      showUsage(argv[0], string("Unrecognized option : ") + argv[nextArg]);
    } else {
      break;
    }
  }

  if (src == FILEIN) {
    if (nextArg < argc) {
      srcfn = argv[nextArg++];
    } else {
      showUsage(argv[0], "Specify a source file name.");
    }
  } else if (!quiet && src == STDIN) {
    cerr << "Warning : --stdin is an experimental feature. This function may not work in every environment." << endl;
  }

  if (dst == FILEOUT) {
    if (nextArg < argc) {
      dstfn = argv[nextArg++];
    } else {
      showUsage(argv[0], "Specify a destination file name.");
    }
  }

  if (nextArg != argc) showUsage(argv[0], "Extra arguments after the destination file name.");

  if (pdf > 1)
    showUsage(argv[0], "PDF ID " + to_string(pdf) + " is not supported");

  if (bits != 8 && bits != 16 && bits != 24 && bits != 32 && bits != -32 && bits != -64)
    showUsage(argv[0], to_string(bits) + "-bit quantization is not supported");

  ConversionProfile profile;

  if (availableProfiles.count(profileName) == 0) {
    char c = '\0';
    if (sscanf(profileName.c_str(), "%u,%lf,%lf,%c",
	       &profile.log2dftfilterlen, &profile.aa, &profile.guard, &c) == 4 && (c == 'd' || c == 'f')) {
      profile.doublePrecision = (c == 'd');
    } else {
      showUsage(argv[0], "There is no profile of name \"" + profileName + "\"");
    }
  } else {
    profile = availableProfiles.at(profileName);
  }

  //

  try {
    if (!profile.doublePrecision) {
      Pipeline<float> pipeline(argv[0], srcfn, dstfn, profileName, dstContainerName,
			       dstChannelMask, rate, bits, dither, pdf, mixMatrix,
			       seed, att, peak, minPhase, quiet, debug, mt, l2mindftflen,
			       src, dst, impulsePeriod, sweepLength,
			       sweepStart, sweepEnd, generatorNch, generatorFs, profile);
      pipeline.execute();
    } else {
      Pipeline<double> pipeline(argv[0], srcfn, dstfn, profileName, dstContainerName,
				dstChannelMask, rate, bits, dither, pdf, mixMatrix,
				seed, att, peak, minPhase, quiet, debug, mt, l2mindftflen,
				src, dst, impulsePeriod, sweepLength,
				sweepStart, sweepEnd, generatorNch, generatorFs, profile);
      pipeline.execute();
    }
  } catch(exception &ex) {
    cerr << argv[0] << " Error : " << ex.what() << endl;
    return -1;
  }

  return 0;
}
