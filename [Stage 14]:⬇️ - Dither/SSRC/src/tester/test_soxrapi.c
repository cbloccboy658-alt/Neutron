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

#define BUFFER_FRAMES 3000

void print_usage(const char *prog_name) {
  printf("Usage: %s <new_rate> <output.wav> <input1.wav> [input2.wav] ...\n", prog_name);
  printf("  Concatenates and resamples multiple WAV files into a single output file.\n");
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  double out_rate = atof(argv[1]);
  const char* out_filename = argv[2];
  int minPhase = out_rate < 0;

  if (out_rate <= 0) out_rate = -out_rate;

  drwav first_wav_in;
  if (!drwav_init_file(&first_wav_in, argv[3], NULL)) {
    fprintf(stderr, "Failed to open initial input file: %s\n", argv[3]);
    return 1;
  }
  double current_in_rate = (double)first_wav_in.sampleRate;
  unsigned int const num_channels = first_wav_in.channels;
  drwav_uninit(&first_wav_in);

  fprintf(stderr, "Output format will be: %.0f Hz, %u channels\n", out_rate, num_channels);
  fprintf(stderr, "----------------------------------------\n");

  //

  soxr_error_t error;
  soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
  soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_MQ, minPhase ? SOXR_MINIMUM_PHASE : 0);
  soxr_t soxr = soxr_create(current_in_rate, out_rate, num_channels, &error, &io_spec, &q_spec, NULL);
  if (!soxr) {
    fprintf(stderr, "soxr_create failed: %s\n", soxr_strerror(error));
    return 1;
  }

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
    return 1;
  }
    
  float* in_buffer = (float*)malloc(sizeof(float) * BUFFER_FRAMES * num_channels);
  size_t out_buffer_capacity = (size_t)(BUFFER_FRAMES * (out_rate / 8000.0 > 1.0 ? out_rate / 8000.0 : 1.0) + 0.5) * 2;
  float* out_buffer = (float*)malloc(sizeof(float) * out_buffer_capacity * num_channels);

  for (int i = 3; i < argc; ++i) {
    const char* in_filename = argv[i];
    fprintf(stderr, "Processing: %s\n", in_filename);

    drwav wav_in;
    if (!drwav_init_file(&wav_in, in_filename, NULL)) {
      fprintf(stderr, "  -> Failed to open. Skipping.\n");
      continue;
    }

    if (wav_in.channels != num_channels) {
      fprintf(stderr, "  -> Channel count mismatch (%u channels, expected %u). Skipping.\n", wav_in.channels, num_channels);
      drwav_uninit(&wav_in);
      continue;
    }

    if ((double)wav_in.sampleRate != current_in_rate) {
      current_in_rate = (double)wav_in.sampleRate;
      fprintf(stderr, "  -> Sample rate is %.0f Hz. Recreating resampler.\n", current_in_rate);
      soxr_delete(soxr);
      soxr = soxr_create(current_in_rate, out_rate, num_channels, &error, &io_spec, &q_spec, NULL);
      if (!soxr) {
	fprintf(stderr, "soxr_create failed during recreation. Aborting.\n");
	drwav_uninit(&wav_in);
	break;
      }
    } else if (i > 3) {
      fprintf(stderr, "  -> Same sample rate. Clearing resampler state.\n");
      soxr_clear(soxr);
    }

    size_t frames_read;
    while ((frames_read = drwav_read_pcm_frames_f32(&wav_in, BUFFER_FRAMES, in_buffer)) > 0) {
      size_t frames_produced;
      error = soxr_process(soxr, in_buffer, frames_read, NULL, out_buffer, out_buffer_capacity, &frames_produced);
      if (error) {
	fprintf(stderr, "soxr_process error: %s\n", soxr_strerror(error));
	exit(-1);
      }
      if (frames_produced > 0) {
	drwav_write_pcm_frames(&wav_out, frames_produced, out_buffer);
      }
    }
        
    size_t frames_produced;
    do {
      error = soxr_process(soxr, NULL, 0, NULL, out_buffer, out_buffer_capacity, &frames_produced);
      if (error) fprintf(stderr, "soxr_process (flush) error: %s\n", soxr_strerror(error));
      if (frames_produced > 0) {
	drwav_write_pcm_frames(&wav_out, frames_produced, out_buffer);
      }
    } while (frames_produced > 0);

    drwav_uninit(&wav_in);
  }

  free(in_buffer);
  free(out_buffer);
  soxr_delete(soxr);
  drwav_uninit(&wav_out);

  fprintf(stderr, "----------------------------------------\n");
  fprintf(stderr, "Successfully created %s\n", out_filename);
  return 0;
}
