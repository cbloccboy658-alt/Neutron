#include <iostream>
#include <vector>
#include <memory>
#include <cstdlib>
#include "shibatch/ssrc.hpp"

void convert_file(const std::string& in_path, const std::string& out_path, int dstRate) {
    try {
        // 1. Set up the reader for single-precision floats
        auto reader = std::make_shared<ssrc::WavReader<float>>(in_path);
        ssrc::WavFormat srcFormat = reader->getFormat();

        // 2. Define destination format and conversion parameters
        int dstBits = 24;

        ssrc::WavFormat dstFormat(ssrc::WavFormat::PCM, srcFormat.channels, dstRate, dstBits);
        ssrc::ContainerFormat dstContainer(ssrc::ContainerFormat::RIFF);

        // 3. Create a resampler for each channel
        std::vector<std::shared_ptr<ssrc::StageOutlet<float>>> outlets;
        for (int i = 0; i < srcFormat.channels; ++i) {
            auto resampler = std::make_shared<ssrc::SSRC<float>>(
                reader->getOutlet(i),
                srcFormat.sampleRate,
                dstRate,
                14,   // log2dftfilterlen for "standard" profile
                145,  // aa for "standard" profile
                2.0   // guard for "standard" profile
            );
            outlets.push_back(resampler);
        }

        // 4. Set up the writer
        auto writer = std::make_shared<ssrc::WavWriter<float>>(out_path, dstFormat, dstContainer, outlets);

        // 5. Execute the entire process
        std::cout << "Converting " << in_path << " to " << out_path << "..." << std::endl;
        writer->execute();
        std::cout << "Conversion complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main(int argc, char **argv) {
  if (argc == 4) {
    convert_file(argv[1], argv[2], atoi(argv[3]));
    return 0;
  }

  std::cerr << "Usage : " << argv[0] << " <input.wav> <output.wav> <new_rate>" << std::endl;

  return -1;
}
