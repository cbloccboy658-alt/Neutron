#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>

#include <sleef.h>
#include <sleefdft.h>

#include "shibatch/ssrc.hpp"
#include "dr_wav.hpp"

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028842
#endif

using namespace std;

namespace {
  struct SpectrumCheckItem {
    const double lf, hf;
    const char op;
    const double thres;

    SpectrumCheckItem(double lf_, double hf_, char op_, double thres_) :
      lf(lf_), hf(hf_), op(op_), thres(thres_) {}

#if 0
    friend ostream& operator<<(ostream& os, const SpectrumCheckItem& si) {
      return os << "[" << si.lf << "Hz ... " << si.hf << "Hz " << si.op << " " << si.thres << "dB]";
    }
#endif
  };

  //

  class Color {
  public:
    const double r, g, b;

    Color(double r_, double g_, double b_) : r(r_), g(g_), b(b_) {}

    friend ostream& operator<<(ostream& os, const Color& c) {
      vector<char> s(10);
      unsigned ri = max(min((int)rint(c.r * 0xff), 0xff), 0);
      unsigned gi = max(min((int)rint(c.g * 0xff), 0xff), 0);
      unsigned bi = max(min((int)rint(c.b * 0xff), 0xff), 0);
      snprintf(s.data(), s.size(), "#%02x%02x%02x", ri, gi, bi);
      return os << string(s.data());
    }
  };

  class StrokeStyle {
  public:
    const Color color;
    const double width;

    StrokeStyle(const Color& color_, double width_) : color(color_), width(width_) {}

    friend ostream& operator<<(ostream& os, const StrokeStyle& ss) {
      return os << "stroke:" << ss.color << "; stroke-width:" << ss.width << "; ";
    }
  };

  class FillStyle {
  public:
    const Color color;
    const double opacity;

    FillStyle(const Color& color_, double opacity_ = 1) : color(color_), opacity(opacity_) {}

    friend ostream& operator<<(ostream& os, const FillStyle& fs) {
      os << "fill:" << fs.color << "; ";
      if (fs.opacity != 1.0) os << "fill-opacity:" << fs.opacity << "; ";
      return os;
    }
  };

  class Font {
  public:
    const string family;
    const double size;
    const string weight, style;

    Font(double size_, const string &family_="sans-serif", const string &weight_="normal", const string &style_="normal") :
      family(family_), size(size_), weight(weight_), style(style_) {}

    friend ostream& operator<<(ostream& os, const Font& f) {
      os << "font-family:" << f.family << "; font-size:" << f.size << "; ";
      os << "font-weight:" << f.weight << "; font-style:" << f.style << "; ";
      return os;
    }
  };

  class TextAnchor {
  public:
    const string textAnchor, dominantBaseline;

    TextAnchor(const string& textAnchor_, const string& dominantBaseline_) :
      textAnchor(textAnchor_), dominantBaseline(dominantBaseline_) {}

    friend ostream& operator<<(ostream& os, const TextAnchor& ta) {
      os << "text-anchor=\"" << ta.textAnchor << "\" ";
      os << "dominant-baseline=\"" << ta.dominantBaseline << "\" ";
      return os;
    }
  };

  static const TextAnchor TOP {"middle", "hanging"};
  static const TextAnchor BOTTOM {"middle", "text-bottom"};
  static const TextAnchor CENTER {"middle", "central"};
  static const TextAnchor RIGHT {"end", "middle"};
  static const TextAnchor LEFT {"start", "middle"};
  static const TextAnchor BOTTOMLEFT {"start", "text-bottom"};
  static const TextAnchor BOTTOMRIGHT {"end", "text-bottom"};
  static const TextAnchor TOPLEFT {"start", "hanging"};
  static const TextAnchor TOPRIGHT {"end", "hanging"};

  class SVGCanvas {
    ostream &os;
  public:
    const double width, height;

    SVGCanvas(ostream &os_, double width_, double height_) :
      os(os_), width(width_), height(height_) {
      os << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" << endl;
      os << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">" << endl;
      os << "<svg width=\"" << width << "px\" height=\"" << height << "px\" viewBox=\"0 0 " << width << " " << height << "\"";
      os << " xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" >" << endl;
      os << "<style> rect { stroke-width:0; fill:none; } </style>" << endl;
      os << "<style> polyline { fill:none; } </style>" << endl;
    }

    ~SVGCanvas() {
      os << "</svg>" << endl;
    }

    void drawRect(double x, double y, double w, double h, const FillStyle& fs, const StrokeStyle& ss, const string& clipID = "") {
      os << "<rect ";
      if (clipID != "") os << "clip-path=\"url(#" << clipID << ")\" ";
      os << "x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h << "\" ";
      os << "style=\"" << fs << ss << "\"/>" << endl;
    }

    void drawRect(double x, double y, double w, double h, const FillStyle& fs) {
      os << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h << "\" ";
      os << "style=\"" << fs << "\"/>" << endl;
    }

    void drawRect(double x, double y, double w, double h, const StrokeStyle& ss=StrokeStyle(Color(0,0,0), 0), const string& clipID = "") {
      os << "<rect ";
      if (clipID != "") os << "clip-path=\"url(#" << clipID << ")\" ";
      os << "x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h << "\" ";
      os << "style=\"" << ss << "\"/>" << endl;
    }

    void drawText(double x, double y, const string &str, const FillStyle &fs=Color(1, 1, 1), const Font& f=Font(10), const TextAnchor &ta=BOTTOMLEFT) {
      os << "<text x=\"" << x << "\" y=\"" << y << "\" " << ta;
      os << "style=\"" << f << fs << "\">";
      os << str << "</text>" << endl;
    }

    void drawLine(double x1, double y1, double x2, double y2, const StrokeStyle& ss) {
      os << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2 =\"" << x2 << "\" y2 =\"" << y2 << "\" ";
      os << "style=\"" << ss << "\"/>" << endl;
    }

    void defineClip(string id, double x, double y, double w, double h) {
      os << "<clipPath id=\"" << id << "\"> <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h << "\"/> </clipPath>" << endl;
    }

    void drawPolyline(const vector<pair<double, double>> points, const StrokeStyle& ss) {
      os << "<polyline points=\"";
      for(auto p : points) os << to_string(p.first) << "," << to_string(p.second) << " ";
      os << "\" style=\"" << ss << "\"/>" << endl;
    }

    void drawPolyline(const vector<pair<double, double>> points, const StrokeStyle& ss, const string& clipID = "") {
      os << "<polyline ";
      if (clipID != "") os << "clip-path=\"url(#" << clipID << ")\" ";
      os << "points=\"";
      for(auto p : points) os << to_string(p.first) << "," << to_string(p.second) << " ";
      os << "\" style=\"" << ss << "\"/>" << endl;
    }
  };

  class SpectrumDisplay {
    const double width, height, topMargin = 30, bottomMargin = 40, leftMargin = 60, rightMargin = 20;
    const double rangekHz, rangedB, intervalX, intervalY, gw, gh;
    SVGCanvas c;

  public:
    SpectrumDisplay(ostream &os_, double width_, double height_, double rangekHz_, double rangedB_,
		    double intervalX_, double intervalY_, bool logFreq_) :
      width(width_), height(height_), rangekHz(rangekHz_), rangedB(rangedB_),
      intervalX(intervalX_), intervalY(intervalY_), gw(width - leftMargin - rightMargin), gh(height - topMargin - bottomMargin),
      c(os_, width_, height_) {

      c.drawRect(0, 0, width, height, StrokeStyle(Color(0,0,0),1));

      c.drawRect(leftMargin, topMargin, gw, gh, StrokeStyle(Color(.5,.5,.5),1));

      for(double y=intervalY;y<rangedB;y += intervalY) {
	double py = topMargin + gh * y / rangedB;
	c.drawLine(leftMargin, py, width - rightMargin, py, StrokeStyle(Color(.8,.8,.8),1));
      }

      for(double y=0;y<=rangedB;y += intervalY) {
	double py = topMargin + gh * y / rangedB;
	c.drawText(leftMargin - 5, py, to_string(-int64_t(y)), Color(0,0,0), Font(10), RIGHT);
      }

      for(double x=intervalX;x<rangekHz;x += intervalX) {
	double px = leftMargin + gw * x / rangekHz;
	c.drawLine(px, topMargin, px, height - bottomMargin, StrokeStyle(Color(.8,.8,.8),1));
      }

      for(double x=intervalX;x<=rangekHz;x += intervalX) {
	double px = leftMargin + gw * x / rangekHz;
	c.drawText(px, topMargin + gh + 5, to_string(int64_t(x)), Color(0,0,0), Font(10), TOP);
      }

      c.defineClip("graph", leftMargin, topMargin, gw, gh);
    }

    void showGraph(const vector<pair<double, double>> data, const StrokeStyle ss = StrokeStyle(Color(0,0,0), 1)) {
      vector<pair<double, double>> proj(data.size());

      double m = -10000;
      for(size_t i = 0;i<data.size();i++) {
	proj[i] = { leftMargin + gw * data[i].first / (rangekHz * 1000),
		    topMargin  + gh * -data[i].second / rangedB };
	m = max(m, data[i].second);
      }

      c.drawPolyline(proj, ss, "graph");
    }

    void showCheckItems(const vector<SpectrumCheckItem> items, const FillStyle fs = FillStyle(Color(0,0,0), 0.1)) {
      for(auto e : items) {
	double l = leftMargin + gw * e.lf / (rangekHz * 1000), r = leftMargin + gw * e.hf / (rangekHz * 1000);
	double y = topMargin  + gh * -e.thres / rangedB;
	if (e.op == '>') {
	  c.drawRect(l, y, r-l, gh + topMargin - y, fs, StrokeStyle(Color(0,0,0), 1), "graph");
	} else if (e.op == '<') {
	  c.drawRect(l, topMargin, r-l, y - topMargin, fs, StrokeStyle(Color(0,0,0), 1), "graph");
	} else if (e.op == '^') {
	  c.drawRect(l, topMargin, r-l, y - topMargin, StrokeStyle(Color(0,0,0), 1), "graph");
	}
      }
    }
  };

  vector<SpectrumCheckItem> loadCheckItems(const string& fn) {
    vector<SpectrumCheckItem> ret;

    ifstream f(fn);
    if (!f.is_open()) throw(runtime_error(("Could not open file " + fn).c_str()));

    string line;
    int ln = 1;
    while(getline(f, line)) {
      double lf, hf, thres;
      char c;
      if (line.find_first_not_of(' ') == string::npos || line[0] == '#') { ln++; continue; }
      if (sscanf(line.c_str(), "%lf %lf %c %lf", &lf, &hf, &c, &thres) == 4) {
	if (!(c == '>' || c == '<' || c == '^')) throw(runtime_error((fn + ":" + to_string(ln) + " : error <>^").c_str()));
	ret.push_back(SpectrumCheckItem(lf, hf, c, thres));
      } else throw(runtime_error((fn + ":" + to_string(ln) + " : error").c_str()));
      ln++;
    }
    f.close();

    return ret;
  }

  class SpectrumAnalyzer {
    const double fs;
    const size_t dftlen;
    const vector<double> window;

    SleefDFT *dft;
    double *dftbuf = nullptr;
  public:
    SpectrumAnalyzer(double fs_, unsigned log2dftlen_) :
      fs(fs_), dftlen(1ULL << log2dftlen_), window(createWindow(dftlen)) {
      dft    = SleefDFT_double_init1d(dftlen, NULL, NULL, SLEEF_MODE_REAL | SLEEF_MODE_ALT | SLEEF_MODE_FORWARD  | SLEEF_MODE_NO_MT);
      dftbuf = (double *)Sleef_malloc(dftlen * sizeof(double));
    }

    ~SpectrumAnalyzer() {
      Sleef_free(dftbuf);
      SleefDFT_dispose(dft);
    }

    virtual vector<double> createWindow(size_t N) {
      // 7-term Blackman-Harris
      // https://dsp.stackexchange.com/questions/51095/seven-term-blackman-harris-window
      static const double coef[] = {
	.27105140069342, -0.43329793923448, 0.21812299954311, -0.06592544638803,
	0.01081174209837, -0.00077658482522, 0.00001388721735
      };

      vector<double> v(N);

      for(size_t n=0;n<N;n++)
	for(int k=0;k<7;k++) v[n] += coef[k] * (1.0 / .27105140069342) * cos((2*M_PI/N)*k*n);

      return v;
    }

    static inline double hypot_(double x, double y) { return sqrt(x*x + y*y); }

    vector<pair<double, double>> doAnalysis(const double* data) {
      for(size_t i=0;i<dftlen;i++) dftbuf[i] = window[i] * data[i];

      SleefDFT_execute(dft, dftbuf, dftbuf);

      vector<pair<double, double>> ret(dftlen/2);

      for(unsigned i=0;i<dftlen/2;i++) {
	ret[i] = { fs / 2 * i / (dftlen / 2), 20 * log10(max(hypot_(dftbuf[i*2+0], dftbuf[i*2+1]) * (2.0 / dftlen), ldexp(2, -64))) };
      }

      return ret;
    }

    static bool checkCompliance(const vector<SpectrumCheckItem>& checkItems, vector<pair<double, double>> analysis) {
      for(auto p : analysis) {
	for(auto it : checkItems) {
	  if (p.first < it.lf || it.hf < p.first) continue;
	  if (it.op == '>') {
	    if (!(p.second > it.thres)) return false;
	  } else if (it.op == '<') {
	    if (!(p.second < it.thres)) return false;
	  }
	}
      }
      for(auto it : checkItems) {
	if (it.op != '^') continue;
	double peak = -10000;
	for(auto p : analysis) {
	  if (p.first < it.lf || it.hf < p.first) continue;
	  peak = max(peak, p.second);
	}
	if (peak < it.thres) return false;
      }
      return true;
    }

    bool check(const vector<SpectrumCheckItem>& checkItems, const double* data) {
      return checkCompliance(checkItems, doAnalysis(data));
    }
  };
}

using namespace dr_wav;

void showUsage(const string& argv0, const string& mes = "") {
  cerr << "Shibatch command-line spectrum analyzer (accompanying SSRC Version " << ssrc::versionString() << ")" << endl;
  cerr << endl;
  cerr << "usage: " << argv0 << " [<options>] <source file name> <first position> <last position> <interval>" << endl;
  cerr << endl;
  cerr << "options : --log2dftlen <log2 of dftlen>" << endl;
  cerr << "          --check <check file>" << endl;
  cerr << "          --svgout <svg file name>" << endl;
  cerr << endl;
  cerr << "If you like this tool, visit https://github.com/shibatch/ssrc and give it a star." << endl;
  cerr << endl;

  if (mes != "") cerr << "Error : " << mes << endl;

  exit(-1);
}

bool analyzeAndCheck(WavFile &wav, SpectrumAnalyzer &ana, vector<SpectrumCheckItem> &checkItems, size_t dftlen, size_t pos, const string &svgoutfn) {
  const unsigned nch = wav.getNChannels();

  wav.seek(pos - dftlen / 2);
  vector<double> wavbuf(dftlen * nch);
  wav.readPCM(wavbuf.data(), dftlen);

  vector<double> chbuf(dftlen);

  bool compliant = true;

  if (checkItems.size() != 0) {
    for(unsigned ch = 0;ch < nch;ch++) {
      for(size_t i=0;i<dftlen;i++) chbuf[i] = wavbuf[i * nch + ch];

      if (!ana.check(checkItems, chbuf.data())) {
	compliant = false;
	break;
      }
    }
  }

  if ((checkItems.size() == 0 || !compliant) && svgoutfn != "") {
    ofstream fout(svgoutfn);
    if (!fout.good()) throw(runtime_error(("Could not open file " + svgoutfn).c_str()));

    SpectrumDisplay sd(fout, 1024, 768, wav.getSampleRate() / 2000, 200, 2, 20, false);

    sd.showCheckItems(checkItems);

    for(unsigned ch = 0;ch < nch;ch++) {
      for(size_t i=0;i<dftlen;i++) chbuf[i] = wavbuf[i * nch + ch];
      sd.showGraph(ana.doAnalysis(chbuf.data()));
    }
  }

  if (checkItems.size() == 0) return true;

  return compliant;
}

int main(int argc, char **argv) {
  if (argc < 2) showUsage(argv[0], "");

  string srcfn, checkfn, svgoutfn;
  uint64_t log2dftlen = 12;
  size_t start = 0, end = 0, interval = 0;
  bool debug = false;

  int nextArg;
  for(nextArg = 1;nextArg < argc;nextArg++) {
    if (string(argv[nextArg]) == "--log2dftlen") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      log2dftlen = strtoul(argv[nextArg+1], &p, 0);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "A non-negative integer is expected after --log2dftlen.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--check") {
      if (nextArg+1 >= argc) showUsage(argv[0], "Specify a check file name after --check");
      checkfn = argv[nextArg+1];
      nextArg++;
    } else if (string(argv[nextArg]) == "--svgout") {
      if (nextArg+1 >= argc) showUsage(argv[0], "Specify a SVG file name after --svgout");
      svgoutfn = argv[nextArg+1];
      nextArg++;
    } else if (string(argv[nextArg]) == "--debug") {
      debug = true;
    } else if (string(argv[nextArg]).substr(0, 2) == "--") {
      showUsage(argv[0], string("Unrecognized option : ") + argv[nextArg]);
    } else {
      break;
    }
  }

  if (nextArg < argc) srcfn = argv[nextArg]; else showUsage(argv[0], "Specify a WAV file name.");
  nextArg++;

  if (nextArg < argc) {
    char *p;
    start = strtoull(argv[nextArg], &p, 0);
    if (p == argv[nextArg] || *p)
      showUsage(argv[0], "Specify the position for checking.");
    nextArg++;
  } else showUsage(argv[0], "Specify the position for checking.");

  if (nextArg < argc) {
    char *p;
    end = strtoull(argv[nextArg], &p, 0);
    if (p == argv[nextArg] || *p)
      showUsage(argv[0], "Specify the ending position for checking.");
    nextArg++;
  }

  if (nextArg < argc) {
    char *p;
    interval = strtoull(argv[nextArg], &p, 0);
    if (p == argv[nextArg] || *p)
      showUsage(argv[0], "Specify the interval for checking.");
    nextArg++;
  }

  if (nextArg != argc) showUsage(argv[0], "Extra arguments detected.");

  const size_t dftlen = 1ULL << log2dftlen;

  if (debug) {
    cerr << "log2dftlen = " << log2dftlen << endl;
    cerr << "dftlen = " << dftlen << endl;
    cerr << "srcfn = " << srcfn << endl;
    cerr << "start = " << start << endl;
    cerr << "end = " << end << endl;
    cerr << "interval = " << interval << endl;
    cerr << "checkfn = " << checkfn << endl;
    cerr << "svgoutfn = " << svgoutfn << endl;
  }

  if (end != 0 && end <= start) showUsage(argv[0], "The ending position must be greater than the starting position.");
  if (end != 0 && interval == 0) showUsage(argv[0], "You must specify an interval.");
  if (checkfn == "" && end != 0) showUsage(argv[0], "You must specify a check file.");
  if (checkfn == "" && svgoutfn == "") showUsage(argv[0], "You must specify an SVG file name.");
  if (start < dftlen / 2) showUsage(argv[0], "Start position must be greater than dftlen/2.");

  try {
    vector<SpectrumCheckItem> checkItems;

    if (checkfn != "") checkItems = loadCheckItems(checkfn);

    const size_t dftlen = 1ULL << log2dftlen;

    WavFile wav(srcfn);

    if (start > wav.getNFrames() - dftlen / 2) showUsage(argv[0], "Start position must be smaller than (nFrames - dftlen/2).");

    SpectrumAnalyzer ana(wav.getSampleRate(), log2dftlen);

    if (end == 0) {
      if (!analyzeAndCheck(wav, ana, checkItems, dftlen, start, svgoutfn)) {
	cerr << "NG" << endl;
	return -1;
      }

      return 0;
    } else {
      for(size_t pos = start;pos <= end;pos += interval) {
	if (!analyzeAndCheck(wav, ana, checkItems, dftlen, pos, svgoutfn)) {
	  cerr << "NG" << endl;
	  return -1;
	}
      }

      return 0;
    }
  } catch(exception &ex) {
    cerr << argv[0] << " Error : " << ex.what() << endl;
    return -1;
  }
}
