#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#include "dr_wav.hpp"

using namespace std;
using namespace dr_wav;

string container_to_string(drwav_container container) {
    if (container == drwav_container_riff) return "riff";
    if (container == drwav_container_w64) return "w64";
    if (container == drwav_container_rf64) return "rf64";
    if (container == drwav_container_aiff) return "aiff";
    return "unknown";
}

double compare(const string& file0, const string& file1) {
  static const size_t N = 4096;

  WavFile wav0(file0), wav1(file1);

  if (wav0.getNChannels() != wav1.getNChannels()) throw(runtime_error("Number of channels does not match"));
  if (wav0.getSampleRate() != wav1.getSampleRate()) throw(runtime_error("Sample rates do not match"));
  if (wav0.getNFrames() != wav1.getNFrames())
    throw(runtime_error(("Number of frames does not match : " + file0 + ":" + to_string(wav0.getNFrames()) +
			 " vs. " + file1 + ":" + to_string(wav1.getNFrames())).c_str()));

  const unsigned nch = wav0.getNChannels();

  vector<double> buf0(N * nch), buf1(N * nch);

  double maxDif = 0;

  for(;;) {
    size_t nr0 = wav0.readPCM(buf0.data(), N), nr1 = wav1.readPCM(buf1.data(), N);
    if (nr0 != nr1) throw(runtime_error("File lengths do not match"));

    for(size_t i=0;i<nr0 * nch;i++) maxDif = max(maxDif, abs(buf0[i] - buf1[i]));

    if (nr0 == 0) break;
  }

  return maxDif;
}

int main(int argc, char **argv) {
  if (argc == 4 && string(argv[1]) == "--check-channels") {
    WavFile wav(argv[2]);
    return wav.getNChannels() == stoul(argv[3]) ? 0 : 1;
  } else if (argc == 4 && string(argv[1]) == "--check-container") {
    WavFile wav(argv[2]);
    return container_to_string(wav.getContainer()) == argv[3] ? 0 : 1;
  } else if (argc == 4) {
    try {
      double maxDif = compare(argv[1], argv[2]);
      cerr << "Max difference : " << maxDif << endl;
      return maxDif <= atof(argv[3]) ? 0 : 1;
    } catch (const ::std::exception& e) {
      cerr << "Error: " << e.what() << endl;
    }
  } else {
    cerr << "Usage : " << argv[0] << " <file0.wav> <file1.wav> <threshold>" << endl;
    cerr << " or " << argv[0] << " --check-channels <file.wav> <# of channels>" << endl;
    cerr << " or " << argv[0] << " --check-container <file.wav> <container>" << endl;
  }

  return -1;
}
