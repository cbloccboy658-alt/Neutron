# SSRC API Documentation

## 1. C++ Library API (`libshibatchdsp`)

The `ssrc` tool is built on top of the `libshibatchdsp` C++ library. You can use this library directly in your own projects to perform sample rate conversion without shelling out to an external command. The library is header-only and uses templates to support both single-precision (`float`) and double-precision (`double`) processing.

### 1.1. Core Concept: The Processing Pipeline

The library is designed around a pipeline concept. Audio data flows through a series of processing stages. The core abstraction is `ssrc::StageOutlet<T>`, an abstract class that represents a source of audio data that can be read from.

The typical pipeline looks like this:

`WavReader` -> `SSRC` -> `Dither` -> `WavWriter`

-   **`WavReader`**: Reads a `.wav` file and acts as the starting `StageOutlet`.
-   **`SSRC`**: Takes a `StageOutlet` as input (the reader) and performs sample rate conversion. It is also a `StageOutlet` itself.
-   **`Dither`**: Takes the `SSRC`'s output and applies dithering. It is also a `StageOutlet`.
-   **`WavWriter`**: Takes one or more final `StageOutlet`s and writes the data to a `.wav` file.

### 1.2. Pipeline Topology and Data Flow

A key detail of the implementation, as seen in `src/cli/cli.cpp`, is that the pipeline **branches** after the `WavReader` to process each audio channel in parallel. A separate `SSRC` (and `Dither`, if used) instance is created for each channel. The `WavWriter` is then responsible for **merging** these parallel streams back into a single, interleaved audio file.

For a stereo (2-channel) file, the topology looks like this:

```text
                                 +------------------------+
                             +-->| SSRC<T> for Channel 0  |--+
                             |   +------------------------+  |
                             |                             |
+-----------+     +----------+                             +------------+
| WavReader |-->--| (Fork)   |                             | WavWriter  |--> output.wav
+-----------+     +----------+                             +------------+
                             |                             |
                             |   +------------------------+  |
                             +-->| SSRC<T> for Channel 1  |--+
                                 +------------------------+
```

-   **Forking**: The `WavReader` provides a separate `StageOutlet` for each channel (`reader->getOutlet(0)`, `reader->getOutlet(1)`, etc.). This is where the pipeline splits.
-   **Parallel Processing**: Each channel is processed independently. This design is clean and allows for channel-specific processing if needed.
-   **Merging**: The `WavWriter` takes a `std::vector` of `StageOutlet`s in its constructor. It reads one sample from each outlet in turn to reconstruct the interleaved audio data needed for the final WAV file.

#### Data Type Lifecycle
The data type of the samples flowing through the pipeline also follows a specific path:
1.  **Floating-Point Processing**: The initial processing stages (`WavReader`, `SSRC`) are templated and typically operate on `float` or `double`. This maintains high precision during the most critical calculations (resampling).
2.  **Conversion to Integer**: If dithering is used, the `Dither` stage is responsible for converting the high-precision floating-point signal into an integer signal.
3.  **Unified Integer Type (`int32_t`)**: Crucially, the `Dither` stage always outputs `int32_t`, regardless of the final target bit depth (e.g., 16-bit or 24-bit). This simplifies the design, as any downstream stages (like the `WavWriter`) only need to handle a single, universal integer type. The `WavWriter` is then responsible for taking the `int32_t` data and correctly writing it to the file with the specified final bit depth.

### 1.3. Key Classes

All necessary classes are available by including one header:
```cpp
#include "shibatch/ssrc.hpp"
```

#### `ssrc::WavReader<T>`
Reads audio data from a WAV file. `T` can be `float` or `double`.

```cpp
// Constructor for reading from a file
ssrc::WavReader<float> reader("input.wav");

// Get format information
ssrc::WavFormat format = reader.getFormat();
int channels = format.channels;
int sampleRate = format.sampleRate;

// Get an outlet for a specific channel
std::shared_ptr<ssrc::StageOutlet<float>> channel_outlet = reader.getOutlet(0); // For channel 0
```

#### `ssrc::SSRC<T>`
The main sample rate converter.

```cpp
// Create a resampler
// - inlet: The source outlet (e.g., from WavReader)
// - sfs: Source frequency (e.g., 44100)
// - dfs: Destination frequency (e.g., 96000)
// - log2dftfilterlen, aa, guard: Profile parameters (see cli.cpp for examples)
auto resampler = std::make_shared<ssrc::SSRC<float>>(
    reader.getOutlet(0),
    44100,
    96000,
    14,   // log2dftfilterlen (from "standard" profile)
    145,  // aa (stop-band attenuation)
    2.0   // guard factor
);
```

#### `ssrc::WavWriter<T>`
Writes audio data from one or more outlets to a WAV file.

**Constructor**
`WavWriter(filename, format, container, inlets, nFrames, bufsize, mt)`

-   **`const std::string& filename`**: The path to the output WAV file. If the string is empty, it will write to standard output.
-   **`const WavFormat& format`**: A `WavFormat` struct defining the output audio format (channels, sample rate, bit depth, etc.).
-   **`const ContainerFormat& container`**: A `ContainerFormat` struct defining the file container type (e.g., `RIFF`, `W64`).
-   **`const std::vector<std::shared_ptr<StageOutlet<T>>>& inlets`**: A vector of `StageOutlet` pointers, one for each channel to be written. The writer merges these streams into an interleaved output file.
-   **`uint64_t nFrames`**: (Optional) The total number of frames to be written. This is primarily used when writing to a non-seekable destination like standard output, where the file header must be written upfront with the final length. Defaults to `0`.
-   **`size_t bufsize`**: (Optional) The size of the internal buffer used for writing data to disk. Defaults to `65536`.
-   **`bool mt`**: (Optional) A boolean flag to enable or disable multithreaded file writing. Defaults to `true`. When `false`, all file I/O is performed in a single thread. This can be useful for debugging or in environments with specific threading constraints.

**`execute()` Method**
This method starts the pipeline. It pulls data from the `inlets`, processes it, and writes it to the destination file. The function blocks until all data from the input stages has been written.

**Example**
```cpp
// Create a vector of outlets (one for each channel)
std::vector<std::shared_ptr<ssrc::StageOutlet<float>>> outlets;
outlets.push_back(resampler); // Add the resampler for channel 0
// ... add resamplers for other channels ...

// Define the output format
ssrc::WavFormat dstFormat(ssrc::WavFormat::PCM, channels, 96000, 24); // 96kHz, 24-bit PCM
ssrc::ContainerFormat dstContainer(ssrc::ContainerFormat::RIFF);

// Create the writer (with multithreading enabled by default)
ssrc::WavWriter<float> writer("output.wav", dstFormat, dstContainer, outlets);

// Execute the entire pipeline (read -> process -> write)
writer.execute();
```

### 1.4. Complete Example

Here is a complete example that ties everything together. It reads a WAV file, resamples it using single-precision floats, and saves the result as a 24-bit PCM WAV file.

```cpp
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
```

## 2. Advanced Topics

This section delves deeper into specific components of the `libshibatchdsp` API.

### 2.1. Dithering with the `Dither` Class

Converting high-resolution audio to a lower bit depth (e.g., 24-bit to 16-bit) involves quantization, where sample values are rounded to the nearest available level. This process creates quantization errors that manifest as distortion correlated with the original signal, which is musically unpleasant, especially on fading reverb tails.

Dithering is a technique that mitigates this by adding a small amount of uncorrelated noise to the signal prior to quantization. This crucial step trades the harsh, signal-correlated distortion for a more benign and constant noise floor.

To take this a step further, noise shaping can be employed. This process intelligently sculpts the noise floor, pushing the noise energy away from the frequency ranges where the human ear is most sensitive (e.g., 2-5 kHz) and into the far less audible, very high frequencies. While this may physically increase the total noise energy in the system, it results in a significantly lower perceived noise level. This combined process preserves low-level detail and the sense of resolution in the final audio.

The `ssrc::Dither` class is a pipeline stage that performs this function. It takes a high-resolution signal as input (e.g., from the `SSRC` stage) and outputs a quantized signal.

**Pipeline with Dither:**

`WavReader` -> `SSRC` -> `Dither` -> `WavWriter`

**Usage:**

The `Dither` class is templated on its output and input types: `Dither<OUTTYPE, INTYPE>`. Typically, `INTYPE` is `float` or `double` (from the resampler) and `OUTTYPE` is `int32_t` (a standard integer type for PCM data).

```cpp
#include "shibatch/shapercoefs.h" // Required for noise shaper coefficients

// ... inside your conversion function ...

// Assume 'resampler' is a std::shared_ptr<ssrc::SSRC<float>> from the previous stage.
auto gain = (1LL << (16 - 1)) - 1; // For 16-bit output
auto clipMin = -(1LL << (16 - 1));
auto clipMax = (1LL << (16 - 1)) - 1;

// Find the appropriate noise shaper coefficients for the destination sample rate.
const ssrc::NoiseShaperCoef* shaper = nullptr;
for (int i = 0; ssrc::noiseShaperCoef[i].fs >= 0; ++i) {
    if (ssrc::noiseShaperCoef[i].fs == dstRate && ssrc::noiseShaperCoef[i].id == 0) {
        shaper = &ssrc::noiseShaperCoef[i];
        break;
    }
}

if (!shaper) {
    // Handle case where no suitable shaper is found for the target rate
    throw std::runtime_error("No suitable noise shaper found for the destination sample rate.");
}

// Create the dither stage
auto dither_stage = std::make_shared<ssrc::Dither<int32_t, float>>(
    resampler,         // Input outlet
    gain,              // Gain to apply before quantization
    0,                 // DC offset (0 for standard PCM)
    clipMin,           // Minimum clipping value
    clipMax,           // Maximum clipping value
    shaper             // The noise shaper coefficients
);

// Now, pass `dither_stage` to the WavWriter instead of `resampler`.
// The WavWriter's template type should also be int32_t.
// std::vector<std::shared_ptr<ssrc::StageOutlet<int32_t>>> outlets;
// outlets.push_back(dither_stage);
// auto writer = std::make_shared<ssrc::WavWriter<int32_t>>(...);
// writer->execute();
```

### 2.2. Precision: `float` vs. `double`

Most key classes in the library are templated with a `typename T` or `typename REAL`, such as `SSRC<REAL>`, `WavReader<T>`, and `WavWriter<T>`. This template parameter controls the floating-point precision used for internal calculations. You can instantiate these classes with either `float` (single-precision) or `double` (double-precision).

-   **`float` (Single Precision)**
    -   **Pros**: Faster execution, lower memory usage.
    -   **Cons**: Less precision.
    -   **Use Case**: For most standard audio applications, `float` is perfectly sufficient. The "standard" profile and below use `float` by default. The precision of a 32-bit float is already far greater than that of 24-bit audio, so it does not typically become a bottleneck for quality.

-   **`double` (Double Precision)**
    -   **Pros**: Extremely high precision, theoretically higher audio quality.
    -   **Cons**: Slower execution (can be 1.5x to 2x slower), higher memory usage.
    -   **Use Case**: For archival purposes or when working in a signal chain that requires the absolute highest fidelity (e.g., for scientific analysis or a "cost-no-object" audiophile setup). The "high" and "insane" profiles use `double`.

**Example:**
To use double-precision, simply change the template parameter throughout your pipeline:

```cpp
// Double-precision pipeline
auto reader = std::make_shared<ssrc::WavReader<double>>(in_path);
// ...
auto resampler = std::make_shared<ssrc::SSRC<double>>(
    reader->getOutlet(i),
    // ... SSRC parameters ...
);
// ...
// Note: Dither still takes a float/double and outputs int32_t
auto dither_stage = std::make_shared<ssrc::Dither<int32_t, double>>(resampler, ...);
```

### 2.3. `WavFormat` and `ContainerFormat`

These two structs work together to describe the audio data's format and the file structure that contains it.

#### `ssrc::ContainerFormat`
This struct specifies the overall file type by defining its main **`ChunkID`**. This ID tells parsers what kind of file they are dealing with. The choice of container is passed to the underlying `dr_wav` library.

- `ContainerFormat::RIFF`: The `ChunkID` is `'RIFF'`. This is the classic WAV format, but it is limited to a maximum file size of 4 GB.
- `ContainerFormat::W64`: Sony Wave64 format. This is one of several competing formats designed to exceed the 4GB limit using 64-bit addressing.
- `ContainerFormat::RF64`: An extension of RIFF that is also 64-bit compatible. It is designed to be backwards-compatible with systems that don't recognize it.

Choosing a 64-bit compatible container like `RF64` or `W64` is essential if your output file might be larger than 4 GB.

#### `ssrc::WavFormat`
This struct's contents correspond directly to the data stored in a WAV file's **`fmt ` chunk**. It describes the specific properties of the raw audio data itself.

- `formatTag`: The audio codec. Key values are:
    - `WavFormat::PCM`: Standard Pulse Code Modulation.
    - `WavFormat::IEEE_FLOAT`: 32-bit or 64-bit floating-point samples.
    - `WavFormat::EXTENSIBLE`: A newer format tag used for audio that doesn't fit the classic `PCM` specification, such as multi-channel audio (more than 2 channels).
- `channels`: Number of audio channels.
- `sampleRate`: The sample rate in Hz (e.g., 44100).
- `bitsPerSample`: The bit depth (e.g., 16, 24, 32).
- `channelMask`: A bitmask specifying the speaker layout for multi-channel audio (e.g., `0x3F` for 5.1 surround, `0x63F` for 7.1 surround). Only used when `formatTag` is `EXTENSIBLE`.
- `subFormat`: A GUID specifying the sub-format, also used with `EXTENSIBLE`. The library provides constants for PCM and IEEE Float (`KSDATAFORMAT_SUBTYPE_PCM` and `KSDATAFORMAT_SUBTYPE_IEEE_FLOAT`).

**Example: Creating a format for a 5.1 Surround, 24-bit, 48kHz WAV file**

```cpp
// This requires the extensible format.
uint32_t channelMask_5_1 = 0x3F; // Front L/R, Center, LFE, Back L/R
ssrc::WavFormat format_5_1(
    ssrc::WavFormat::EXTENSIBLE,
    6,     // channels
    48000, // sampleRate
    24,    // bitsPerSample
    channelMask_5_1,
    ssrc::WavFormat::KSDATAFORMAT_SUBTYPE_PCM
);

// Use a container that supports large files, like W64 or RF64
ssrc::ContainerFormat container(ssrc::ContainerFormat::RF64);

// auto writer = std::make_shared<ssrc::WavWriter<float>>(..., format_5_1, container, ...);
```

### 2.4. Custom Dithering with `DoubleRNG`

The dithering process relies on a stream of random numbers to generate the dither noise. The library provides a default triangular PDF random number generator. However, you can provide your own by implementing the `ssrc::DoubleRNG` abstract base class.

This allows you to experiment with different types of noise (e.g., Gaussian, or different distributions) for dithering.

**Interface:**

The `DoubleRNG` interface is very simple:
```cpp
class DoubleRNG {
public:
    virtual double nextDouble() = 0; // Must return a random double
    virtual ~DoubleRNG() = default;
};
```

**Example: Implementing a Simple Uniform RNG**

Here is how you could implement a simple RNG that produces a uniform distribution between -1.0 and 1.0 and use it in the dither stage.

```cpp
#include <random>

// 1. Implement the DoubleRNG interface
class MyUniformRNG : public ssrc::DoubleRNG {
private:
    std::mt19937 engine_;
    std::uniform_real_distribution<double> dist_;

public:
    MyUniformRNG(uint64_t seed = 0) : engine_(seed), dist_(-1.0, 1.0) {}

    double nextDouble() override {
        return dist_(engine_);
    }
};

// ... inside your conversion function ...

// 2. Create an instance of your custom RNG
auto my_rng = std::make_shared<MyUniformRNG>(/* seed */);

// 3. Pass it to the Dither constructor
auto dither_stage = std::make_shared<ssrc::Dither<int32_t, float>>(
    resampler,
    // ... other dither parameters ...
    shaper,
    my_rng // Your custom RNG
);
```

### 2.5. `SSRC` Constructor Parameters

The `SSRC` constructor takes three parameters that define the conversion profile, controlling the trade-off between quality and speed.

`SSRC(inlet, sfs, dfs, log2dftfilterlen, aa, guard, gain, minPhase, l2mindftflen, mt)`

-   **`unsigned log2dftfilterlen`**
    -   **Description**: The base-2 logarithm of the FFT filter length. The actual filter length is `1 << log2dftfilterlen`.
    -   **Impact**: This is the most significant parameter for quality. A longer filter (higher `log2dftfilterlen`) allows for a steeper, more precise anti-aliasing filter, which better removes unwanted frequencies. However, it dramatically increases computational cost.
    -   **Values**: Typical values range from `8` ("lightning") to `18` ("insane").

-   **`double aa` (Stop-band Attenuation)**
    -   **Description**: The required attenuation in the stop-band, measured in decibels (dB). The stop-band is the range of frequencies that should be completely eliminated by the filter.
    -   **Impact**: A higher value (e.g., 170 dB) results in a "blacker" background with less aliasing noise, at the cost of increased computational complexity.
    -   **Values**: Typical values range from `96` dB to `200` dB.

-   **`double guard` (Guard Band)**
    -   **Description**: A factor that determines the width of the guard band between the pass-band (frequencies to keep) and the stop-band (frequencies to remove).
    -   **Impact**: A larger guard band makes the filter's job easier, allowing for faster computation, but it does so by slightly narrowing the range of frequencies that are passed through. This is mostly relevant for conversions between very close sample rates (like 44.1kHz and 48kHz).
    -   **Values**: Typical values range from `1.0` to `8.0`.

-   **`double gain`**
    -   **Description**: A linear gain multiplier applied to the signal. Defaults to `1.0` (no change).
    -   **Values**: Any double-precision floating-point value. For example, `0.5` would reduce the signal level by 6 dB.

-   **`bool minPhase`**
    -   **Description**: A boolean flag to select the filter type. Defaults to `false`.
    -   **Impact**: When `false` (default), the converter uses **linear-phase filters**, which preserve the waveform's shape but introduce a processing delay equal to half the filter length. When `true`, it uses **minimum-phase filters**, which significantly reduce this delay, making the process suitable for real-time applications. The trade-off is a change in phase response, which is generally inaudible.
    -   **Values**: `true` or `false`.

-   **`unsigned l2mindftflen` (Log2 Minimum DFT Filter Length)**
    -   **Description**: When using partitioned convolution for low-latency processing, this parameter sets the base-2 logarithm of the minimum FFT size for the filter partitions. Defaults to `0` (disabled).
    -   **Impact**: This parameter is key for tuning real-time performance. A non-zero value enables the partitioned convolution algorithm, which breaks the main filter into smaller chunks to reduce latency. A smaller `l2mindftflen` leads to lower latency but higher CPU usage. This is often used in conjunction with `minPhase`.
    -   **Values**: An unsigned integer, typically between `8` and `12` for real-time applications.

-   **`bool mt` (Multithreading)**
    -   **Description**: A boolean flag to enable or disable multithreaded processing. Defaults to `true`.
    -   **Impact**: When `true`, the resampler may use multiple threads to accelerate the computation, particularly the FFT. Setting this to `false` forces the resampler to operate in a single-threaded mode. This is useful for debugging, ensuring determinism, or in environments where thread management is handled externally.
    -   **Values**: `true` (default) or `false`.

These parameters are bundled together in the command-line tool's "profiles". When using the library directly, you can mix and match these values to create a custom profile tailored to your specific needs.

### 2.6. Implementing Custom Processing Stages with `StageOutlet`

The entire `libshibatchdsp` library is built on a simple but powerful design pattern: the **processing pipeline**. Audio data flows from a source, through one or more processing stages, to a destination. Each of these stages is connected by a unified interface: `ssrc::StageOutlet<T>`.

This interface is the fundamental building block of the library. `WavReader` is a `StageOutlet`, `SSRC` is a `StageOutlet`, and `Dither` is a `StageOutlet`. By making your own class that implements this interface, you can create custom audio effects, generators, or other processing tools and seamlessly insert them anywhere in the pipeline.

To create a custom stage, you must inherit from `ssrc::StageOutlet<T>` and implement its two pure virtual functions:

-   `virtual bool atEnd()`
    -   **Purpose**: This function should return `true` if the stream has no more data to provide, and `false` otherwise.
    -   **Implementation**: If your stage is processing data from an input stage, you should typically call `atEnd()` on your input and return its value. If you are generating data, you return `true` when your generation logic is complete.

-   `virtual size_t read(T *ptr, size_t n)`
    -   **Purpose**: This is the core function where data is processed and provided. It should fill the provided buffer `ptr` with up to `n` samples of audio data.
    -   **Return Value**: It must return the number of samples that were actually written to `ptr`. A return value of `0` signals that the stream has ended (i.e., `atEnd()` is now `true`).
    -   **Blocking**: If no data is currently available but the stream is not at the end, this function should block until data becomes available.

**Example: Creating a Custom Gain (Volume) Stage**

Here is a complete example of a simple gain stage. It reads data from an input outlet, multiplies each sample by a constant factor, and can be inserted anywhere in the pipeline.

```cpp
#include <memory>
#include "shibatch/ssrc.hpp"

template <typename T>
class GainStage : public ssrc::StageOutlet<T> {
private:
    std::shared_ptr<ssrc::StageOutlet<T>> inlet_; // The previous stage in the pipeline
    double gain_factor_;

public:
    // Constructor takes the input stage and the gain factor (e.g., 1.0 is no change)
    GainStage(std::shared_ptr<ssrc::StageOutlet<T>> inlet, double gain_factor)
        : inlet_(inlet), gain_factor_(gain_factor) {}

    // atEnd() is true if the input stage is at its end.
    bool atEnd() override {
        return inlet_->atEnd();
    }

    // read() fetches data from the input, applies gain, and returns it.
    size_t read(T* ptr, size_t n) override {
        // Read data from the input stage into our buffer.
        size_t samples_read = inlet_->read(ptr, n);

        // Apply the gain to each sample that was read.
        for (size_t i = 0; i < samples_read; ++i) {
            ptr[i] *= gain_factor_;
        }

        return samples_read;
    }
};

// --- How to use it in a pipeline ---

//
// WavReader -> SSRC -> GainStage -> WavWriter
//

// ... after creating the SSRC resampler ...
// auto resampler = std::make_shared<ssrc::SSRC<float>>(...);

// Create an instance of your gain stage, wrapping the resampler.
// This example reduces the volume by half (gain factor 0.5).
auto gain_stage = std::make_shared<GainStage<float>>(resampler, 0.5);

// Pass the gain stage to the writer instead of the resampler.
// std::vector<std::shared_ptr<ssrc::StageOutlet<float>>> outlets;
// outlets.push_back(gain_stage);
// auto writer = std::make_shared<ssrc::WavWriter<float>>(..., outlets);
// writer->execute();
```

### 2.7. Manipulating Channels with ChannelMixer

While the standard pipeline processes each channel independently, there are many cases where you need to combine, re-route, or change the number of channels. This is the role of the `shibatch::ChannelMixerStage<T>` stage. It takes any number of input channels and produces any number of output channels, with the transformation being defined by a mixing matrix.

Common use cases include:
-   **Downmixing**: Converting a multi-channel source (e.g., 5.1 surround, stereo) to a format with fewer channels (e.g., stereo, mono).
-   **Upmixing**: Creating a multi-channel output from a source with fewer channels (e.g., converting a stereo track to a "pseudo-5.1" track).
-   **Channel Re-routing**: Swapping the left and right channels, or re-ordering channels in a multi-channel file.
-   **Applying Gain**: Applying a specific gain to each channel independently.

The `ChannelMixer` is a processing stage that is typically inserted early in the pipeline, often right after the `WavReader`, before resampling occurs. This is usually more efficient, as it means the resampling stage only has to process the final number of channels.

**Pipeline with ChannelMixer (Stereo to Mono Downmix):**

```text
                                 +-------------------------+
                             +-->| ChannelMixer<T> (Mono)  |--> SSRC<T> --> ...
                             |   +-------------------------+
                             |
+-----------+     +----------+
| WavReader |-->--| (Fork)   |
+-----------+     +----------+
                             |
                             |   (Input Channel 1 is also
                             +-->  fed into the mixer, but
                                   is not an independent
                                   output from this stage)
```

#### The Mixing Matrix

The core of the `ChannelMixer` is the matrix, which you provide to its constructor as a `std::vector<std::vector<double>>`. This matrix defines exactly how the input channels are combined to create the output channels.

-   The **number of rows** in the matrix determines the **number of output channels**.
-   The **number of columns** in each row must equal the **number of input channels**.

The value at `matrix[out_channel][in_channel]` is the gain (multiplier) applied to the input channel's signal before it is summed into the output channel's signal.

**Formula:**
`output[out_ch] = sum(input[in_ch] * matrix[out_ch][in_ch] for all in_ch)`

**Example 1: Stereo to Mono Downmix**

To convert a 2-channel stereo input to a 1-channel mono output, you need a matrix with 1 row and 2 columns. A standard downmix formula is `Mono = 0.5 * Left + 0.5 * Right`.

The corresponding matrix would be:
```cpp
std::vector<std::vector<double>> matrix = {
    {0.5, 0.5} // Output Channel 0 = 0.5 * Input 0 + 0.5 * Input 1
};
```

**Example 2: Swapping Stereo Channels**

To swap the left and right channels of a 2-channel input, you need a 2x2 matrix.

-   Output 0 (new Left) should be 1.0 * Input 1 (old Right).
-   Output 1 (new Right) should be 1.0 * Input 0 (old Left).

The matrix would be:
```cpp
std::vector<std::vector<double>> matrix = {
    {0.0, 1.0}, // Output 0 = 0.0 * Input 0 + 1.0 * Input 1
    {1.0, 0.0}  // Output 1 = 1.0 * Input 0 + 0.0 * Input 1
};
```

#### Complete Example: Stereo to Mono Conversion

Here is a full example that demonstrates how to read a stereo WAV file, downmix it to mono using the `ChannelMixer`, resample the mono signal, and write the result to a new WAV file.

```cpp
#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include "shibatch/ssrc.hpp"

void stereo_to_mono_conversion(const std::string& in_path, const std::string& out_path, int dstRate) {
    try {
        // 1. Set up the reader
        auto reader = std::make_shared<ssrc::WavReader<float>>(in_path);
        ssrc::WavFormat srcFormat = reader->getFormat();

        if (srcFormat.channels != 2) {
            throw std::runtime_error("Input file must be stereo.");
        }

        // 2. Define the mixing matrix for stereo-to-mono
        std::vector<std::vector<double>> mix_matrix = {
            {0.5, 0.5} // Mono = 0.5 * Left + 0.5 * Right
        };

        // 3. Create the ChannelMixer stage
        // The mixer takes the WavReader as its input.
        auto mixer = std::make_shared<shibatch::ChannelMixerStage<float>>(reader, mix_matrix);

        // 4. Define destination format
        // The number of channels for the output is now determined by the mixer.
        ssrc::WavFormat dstFormat(ssrc::WavFormat::PCM, mixer->getFormat().channels, dstRate, 24);
        ssrc::ContainerFormat dstContainer(ssrc::ContainerFormat::RIFF);

        // 5. Create a resampler for each output channel of the mixer
        // In this case, there is only one channel (mono).
        std::vector<std::shared_ptr<ssrc::StageOutlet<float>>> resampler_outlets;
        for (uint32_t i = 0; i < mixer->getFormat().channels; ++i) {
            auto resampler = std::make_shared<ssrc::SSRC<float>>(
                mixer->getOutlet(i),    // Input is now the mixer's outlet
                srcFormat.sampleRate,
                dstRate,
                14,   // "standard" profile
                145,  // "standard" profile
                2.0   // "standard" profile
            );
            resampler_outlets.push_back(resampler);
        }

        // 6. Set up the writer
        auto writer = std::make_shared<ssrc::WavWriter<float>>(out_path, dstFormat, dstContainer, resampler_outlets);

        // 7. Execute the entire process
        std::cout << "Converting " << in_path << " (stereo) to " << out_path << " (mono)..." << std::endl;
        writer->execute();
        std::cout << "Conversion complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main(int argc, char **argv) {
  if (argc == 4) {
    stereo_to_mono_conversion(argv[1], argv[2], atoi(argv[3]));
    return 0;
  }

  std::cerr << "Usage : " << argv[0] << " <input_stereo.wav> <output_mono.wav> <new_rate>" << std::endl;

  return -1;
}
```

## 3. C API (`libssrc-soxr`)

In addition to the C++ template library, `ssrc` provides a C-language API with a calling convention somewhat similar to the popular `libsoxr`. This API is easier to integrate into non-C++ projects and provides a more straightforward, stateful interface for resampling.

To use this API, include the header:
```c
#include "shibatch/ssrcsoxr.h"
```

### 3.1. libsoxr Compatibility

This C API is designed to be a somewhat drop-in replacement for `libsoxr`. By defining the `SSRC_LIBSOXR_EMULATION` macro before including the header, all `ssrc_soxr_*` functions and types are aliased to their `soxr_*` equivalents (e.g., `ssrc_soxr_create` becomes `soxr_create`). This allows for relatively easy migration of existing codebases that already use `libsoxr`.

```c
#define SSRC_LIBSOXR_EMULATION
#include "shibatch/ssrcsoxr.h"
```
The examples in this documentation will use the `soxr_*` names, assuming this macro is defined.

### 3.2. Core Functions and Workflow

The API is stateful. The typical workflow is:
1.  **Create** a resampler object (`soxr_t`) with `soxr_create()`.
2.  **Process** audio data in chunks by repeatedly calling `soxr_process()`.
3.  **Flush** the resampler by calling `soxr_process()` with `NULL` input to retrieve any remaining buffered samples.
4.  **Delete** the resampler object with `soxr_delete()` to free resources.

#### `soxr_t soxr_create(input_rate, output_rate, num_channels, *error, *iospec, *qspec, *rtspec)`
Creates and initializes a resampler instance.

-   `input_rate`, `output_rate`: The source and destination sample rates.
-   `num_channels`: The number of audio channels to process.
-   `error`: A pointer to a `soxr_error_t` that will be set if creation fails.
-   `iospec`: A `soxr_io_spec_t` struct specifying data formats.
-   `qspec`: A `soxr_quality_spec_t` struct specifying the conversion quality profile.
-   `rtspec`: Reserved for future use; should be `NULL`.

Returns a `soxr_t` handle on success or `NULL` on failure.

#### `soxr_error_t soxr_process(soxr, in, ilen, *idone, out, olen, *odone)`
Processes a chunk of audio data.

-   `soxr`: The resampler handle.
-   `in`, `ilen`: Pointer to the input buffer and the number of frames it contains. To flush the internal buffers at the end of the stream, set `in` to `NULL` and `ilen` to `0`.
-   `idone`: (Optional) A pointer to a `size_t` that will be set to the number of frames consumed from the input buffer.
-   `out`, `olen`: Pointer to the output buffer and its capacity in frames.
-   `odone`: A pointer to a `size_t` that will be set to the number of frames written to the output buffer.

Returns an error code if an error occurs during processing.

#### `soxr_error_t soxr_clear(soxr_t soxr)`
Resets the resampler to its initial state, clearing all internal buffers. This is useful for processing a new signal with the same configuration without the overhead of destroying and recreating the resampler.

#### `void soxr_delete(soxr_t soxr)`
Frees all memory and resources associated with the resampler handle.

#### `double soxr_delay(soxr_t soxr)`
Returns the processing delay of the resampler in samples. This represents the number of zero samples that should be discarded from the beginning of the output to maintain signal synchronization.

### 3.3. One-Shot (Single-Call) Resampling

For convenience, the API provides a "one-shot" function to resample a signal that is already entirely in memory. This function handles the creation, processing, flushing, and deletion of a resampler in a single call.

#### `soxr_error_t soxr_oneshot(in_rate, out_rate, num_ch, in, ilen, *idone, out, olen, *odone, *io_spec, *q_spec, *rt_spec)`
Resamples a block of in-memory audio data in a single function call.

-   `in_rate`, `out_rate`, `num_ch`: The sample rates and channel count.
-   `in`, `ilen`: Pointer to the input buffer and its length in frames.
-   `out`, `olen`: Pointer to the output buffer and its capacity in frames.
-   `idone`, `odone`: Pointers to store the number of frames consumed and produced.
-   `io_spec`, `q_spec`, `rt_spec`: (Optional) Pointers to I/O, quality, and runtime specifications.

### 3.4. Configuration

#### `soxr_io_spec_t soxr_io_spec(itype, otype)`
This helper function creates an I/O specification object.
-   `itype`, `otype`: The data type for the input and output buffers. Supported types are `SOXR_FLOAT32` (for `float[]`) and `SOXR_FLOAT64` (for `double[]`).

#### `soxr_quality_spec_t soxr_quality_spec(recipe, flags)`
This helper function creates a quality specification object.
-   `recipe`: A preset that defines the quality level.
    -   `SSRC_SOXR_QQ`: "Quick" quality
    -   `SSRC_SOXR_LQ`: "Low" quality
    -   `SSRC_SOXR_MQ`: "Medium" quality (default)
    -   `SSRC_SOXR_HQ`: "High" quality
    -   `SSRC_SOXR_VHQ`: "Very High" quality
-   `flags`: Additional flags (e.g., for dithering options). `0` is a safe default. The following values can be used.
    -   `SOXR_MINIMUM_PHASE`: Use minimum phase filter.

### 3.5. Complete Example

This example demonstrates how to convert a WAV file from one sample rate to another. It uses the popular `dr_wav` single-file library for file I/O, which is included in this repository for convenience.

```c
#include <stdio.h>
#include <stdlib.h>

// Use the libsoxr compatibility layer
#define SSRC_LIBSOXR_EMULATION
#include "shibatch/ssrcsoxr.h"

// dr_wav for file I/O.
// In this project, a modified version `xdr_wav.h` is used.
#define DR_WAV_IMPLEMENTATION
#include "xdr_wav.h"

#define BUFFER_FRAMES 3000

int main(int argc, char *argv[]) {
  if (argc < 4) {
    printf("Usage: %s <input.wav> <output.wav> <new_rate>\n", argv[0]);
    return 1;
  }

  const char* in_filename = argv[1];
  const char* out_filename = argv[2];
  double const out_rate = atof(argv[3]);

  // 1. Open input WAV file
  drwav wav_in;
  if (!drwav_init_file(&wav_in, in_filename, NULL)) {
    fprintf(stderr, "Failed to open input file: %s\n", in_filename);
    return 1;
  }

  double const in_rate = (double)wav_in.sampleRate;
  unsigned int const num_channels = wav_in.channels;

  // 2. Configure and create the resampler
  soxr_error_t error;
  soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32, SOXR_FLOAT32);
  soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_MQ, 0);
  soxr_t soxr = soxr_create(in_rate, out_rate, num_channels, &error, &io_spec, &q_spec, NULL);

  if (!soxr) {
    fprintf(stderr, "soxr_create failed: %s\n", error);
    drwav_uninit(&wav_in);
    return 1;
  }

  // 3. Open output WAV file
  drwav_data_format format;
  format.container = drwav_container_riff;
  format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  format.channels = num_channels;
  format.sampleRate = (drwav_uint32)out_rate;
  format.bitsPerSample = 32;

  drwav wav_out;
  if (!drwav_init_file_write(&wav_out, out_filename, &format, NULL)) {
    fprintf(stderr, "Failed to open output file: %s\n", out_filename);
    soxr_delete(soxr);
    drwav_uninit(&wav_in);
    return 1;
  }

  // 4. Set up I/O buffers
  float* in_buffer = (float*)malloc(sizeof(float) * BUFFER_FRAMES * num_channels);
  size_t out_buffer_capacity = (size_t)(BUFFER_FRAMES * out_rate / in_rate + 0.5) + 16;
  float* out_buffer = (float*)malloc(sizeof(float) * out_buffer_capacity * num_channels);

  size_t frames_read;
  // 5. Process data in a loop
  while ((frames_read = drwav_read_pcm_frames_f32(&wav_in, BUFFER_FRAMES, in_buffer)) > 0) {
    size_t frames_consumed;
    size_t frames_produced;

    error = soxr_process(soxr,
			 in_buffer, frames_read, &frames_consumed,
			 out_buffer, out_buffer_capacity, &frames_produced);

    if (error) fprintf(stderr, "soxr_process error: %s\n", error);

    if (frames_produced > 0) {
      drwav_write_pcm_frames(&wav_out, frames_produced, out_buffer);
    }
  }

  // 6. Flush the resampler's internal buffer
  size_t frames_produced;
  do {
    error = soxr_process(soxr, NULL, 0, NULL, out_buffer, out_buffer_capacity, &frames_produced);
    if (error) fprintf(stderr, "soxr_process (flush) error: %s\n", error);
    if (frames_produced > 0) {
      drwav_write_pcm_frames(&wav_out, frames_produced, out_buffer);
    }
  } while (frames_produced > 0);

  // 7. Clean up
  free(in_buffer);
  free(out_buffer);
  soxr_delete(soxr);
  drwav_uninit(&wav_in);
  drwav_uninit(&wav_out);

  printf("Successfully created %s\n", out_filename);
  return 0;
}
```

