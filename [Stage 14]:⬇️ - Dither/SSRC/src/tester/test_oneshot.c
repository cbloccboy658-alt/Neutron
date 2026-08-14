#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DR_WAV_IMPLEMENTATION

#ifdef USE_SOXR
#include <soxr.h>
#include "dr_wav.h"
#else
#define SSRC_LIBSOXR_EMULATION
#include "shibatch/ssrcsoxr.h"
#include "xdr_wav.h"
#endif

void print_usage(char *argv0) {
  fprintf(stderr, "Usage: %s <input.wav> <output.wav> <new_sample_rate>\n", argv0);
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  const char* in_filename = argv[1];
  const char* out_filename = argv[2];
  unsigned int out_rate = atoi(argv[3]);

  if (out_rate == 0) {
    fprintf(stderr, "Error: Invalid output sample rate.\n");
    return 1;
  }

  unsigned int channels;
  unsigned int in_rate;
  drwav_uint64 total_frame_count;
  float* p_input_samples = drwav_open_file_and_read_pcm_frames_f32(in_filename, &channels, &in_rate, &total_frame_count, NULL);

  if (p_input_samples == NULL) {
    fprintf(stderr, "Error: Failed to open and read WAV file: %s\n", in_filename);
    return 1;
  }

  fprintf(stderr, "Input file: %s\n", in_filename);
  fprintf(stderr, "  - Channels: %u\n", channels);
  fprintf(stderr, "  - Sample Rate: %u Hz\n", in_rate);
  fprintf(stderr, "  - Total Frames: %llu\n", total_frame_count);

  if (in_rate == out_rate) {
    fprintf(stderr, "Input and output sample rates are the same. No conversion needed.\n");
  }

  drwav_uint64 output_frame_count = (drwav_uint64)((double)total_frame_count * (double)out_rate / (double)in_rate + 0.5) * 2;
  float* p_output_samples = (float*)malloc(sizeof(float) * output_frame_count * channels);
  if (p_output_samples == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory for output buffer.\n");
    drwav_free(p_input_samples, NULL);
    return 1;
  }

  size_t odone;
  soxr_error_t error;

  soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
  soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_MQ, 0);

  fprintf(stderr, "\nStarting resampling...\n");
  fprintf(stderr, "  - From: %u Hz\n", in_rate);
  fprintf(stderr, "  - To:   %u Hz\n", out_rate);

  error = soxr_oneshot(in_rate, out_rate, channels,
		       p_input_samples, total_frame_count, NULL,
		       p_output_samples, output_frame_count, &odone,
		       &io_spec, &q_spec, NULL);

  drwav_free(p_input_samples, NULL);

  if (error) {
    fprintf(stderr, "Error: soxr_oneshot failed: %s\n", soxr_strerror(error));
    free(p_output_samples);
    return 1;
  }

  fprintf(stderr, "Resampling complete. Output frames: %zu\n", odone);

  drwav wav_out;
  drwav_data_format format;
  format.container = drwav_container_riff;
  format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  format.channels = channels;
  format.sampleRate = out_rate;
  format.bitsPerSample = 32;

  if (!drwav_init_file_write(&wav_out, out_filename, &format, NULL)) {
    fprintf(stderr, "Error: Failed to initialize output WAV file: %s\n", out_filename);
    free(p_output_samples);
    return 1;
  }

  drwav_uint64 frames_written = drwav_write_pcm_frames(&wav_out, odone, p_output_samples);
  drwav_uninit(&wav_out);
  free(p_output_samples);

  if (frames_written != odone) {
    fprintf(stderr, "Error: Failed to write all frames to output file.\n");
    return 1;
  }

  fprintf(stderr, "\nSuccessfully created resampled file: %s\n", out_filename);

  return 0;
}
