// Soxr-ish C API for Shibatch Sample Rate Converter

#ifndef SSRC_SOXR_H_INCLUDED
#define SSRC_SOXR_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char const * ssrc_soxr_error_t;

typedef enum {
  SSRC_SOXR_FLOAT32, SSRC_SOXR_FLOAT32_I = SSRC_SOXR_FLOAT32,
  SSRC_SOXR_FLOAT64, SSRC_SOXR_FLOAT64_I = SSRC_SOXR_FLOAT64,
} ssrc_soxr_datatype_t;

// io_spec

typedef struct ssrc_soxr_io_spec ssrc_soxr_io_spec_t;

struct ssrc_soxr_io_spec {
  ssrc_soxr_datatype_t itype, otype;
  unsigned ditherType;
  unsigned long flags;
};

ssrc_soxr_io_spec_t ssrc_soxr_io_spec(
  ssrc_soxr_datatype_t itype,
  ssrc_soxr_datatype_t otype);

#define SSRC_SOXR_TPDF 0
#define SSRC_SOXR_NO_DITHER 8

// quality_spec

typedef struct ssrc_soxr_quality_spec ssrc_soxr_quality_spec_t;

struct ssrc_soxr_quality_spec {
  unsigned log2dftfilterlen;
  double aa, guard;
  ssrc_soxr_datatype_t dataType;
  unsigned long flags;
};

ssrc_soxr_quality_spec_t ssrc_soxr_quality_spec(
  unsigned long recipe,
  unsigned long flags);

#define SSRC_SOXR_QQ 0
#define SSRC_SOXR_LQ 1
#define SSRC_SOXR_MQ 2
#define SSRC_SOXR_HQ 4
#define SSRC_SOXR_VHQ 6

#define SSRC_SOXR_MINIMUM_PHASE      0x30

// runtime_spec

typedef struct ssrc_soxr_runtime_spec ssrc_soxr_runtime_spec_t;

struct ssrc_soxr_runtime_spec {
  unsigned num_threads;
};

//

struct ssrc_soxr *ssrc_soxr_create(
  double input_rate, double output_rate, unsigned num_channels,
  ssrc_soxr_error_t *eptr, ssrc_soxr_io_spec_t const *iospec,
  ssrc_soxr_quality_spec_t const *qspec,
  ssrc_soxr_runtime_spec_t const *rtspec);

ssrc_soxr_error_t ssrc_soxr_process(
  struct ssrc_soxr *thiz,
  void const *in, size_t ilen, size_t *idone,
  void *out, size_t olen, size_t *odone);

ssrc_soxr_error_t ssrc_soxr_clear(struct ssrc_soxr *thiz);
void ssrc_soxr_delete(struct ssrc_soxr *thiz);

double ssrc_soxr_delay(struct ssrc_soxr *thiz);

ssrc_soxr_error_t ssrc_soxr_oneshot(double in_rate, double out_rate, unsigned num_channels,
				    void const *in, size_t in_len, size_t *in_rd,
				    void *out, size_t out_len, size_t *out_wr,
				    ssrc_soxr_io_spec_t const *io_spec,
				    ssrc_soxr_quality_spec_t const *q_spec,
				    ssrc_soxr_runtime_spec_t const *runtime_spec);

//

#ifdef SSRC_LIBSOXR_EMULATION
typedef ssrc_soxr_error_t soxr_error_t;
typedef ssrc_soxr_io_spec_t soxr_io_spec_t;
typedef ssrc_soxr_quality_spec_t soxr_quality_spec_t;
typedef ssrc_soxr_runtime_spec_t soxr_runtime_spec_t;
typedef struct ssrc_soxr * soxr_t;

typedef enum {
  SOXR_FLOAT32   = SSRC_SOXR_FLOAT32,
  SOXR_FLOAT32_I = SSRC_SOXR_FLOAT32,
  SOXR_FLOAT64   = SSRC_SOXR_FLOAT64,
  SOXR_FLOAT64_I = SSRC_SOXR_FLOAT64,
} soxr_datatype_t;

static inline soxr_io_spec_t soxr_io_spec(soxr_datatype_t itype, soxr_datatype_t otype) {
  return ssrc_soxr_io_spec((ssrc_soxr_datatype_t)itype, (ssrc_soxr_datatype_t)otype);
}

static inline soxr_quality_spec_t soxr_quality_spec(unsigned long recipe, unsigned long flags) {
  return ssrc_soxr_quality_spec(recipe, flags);
}

static inline soxr_t soxr_create(
  double input_rate, double output_rate, unsigned num_channels,
  ssrc_soxr_error_t *eptr, ssrc_soxr_io_spec_t const *iospec, ssrc_soxr_quality_spec_t const *qspec,
  void const *rtspec) {
  return ssrc_soxr_create(input_rate, output_rate, num_channels, eptr, iospec, qspec, rtspec);
}

static inline soxr_error_t soxr_process(
  struct ssrc_soxr *thiz,
  void const *in, size_t ilen, size_t *idone,
  void *out, size_t olen, size_t *odone) {
  return ssrc_soxr_process(thiz, in, ilen, idone, out, olen, odone);
}

static inline soxr_error_t soxr_oneshot(double in_rate, double out_rate, unsigned num_channels,
					void const *in, size_t in_len, size_t *in_rd,
					void *out, size_t out_len, size_t *out_wr,
					soxr_io_spec_t const *io_spec,
					soxr_quality_spec_t const *q_spec,
					soxr_runtime_spec_t const *runtime_spec) {
  return ssrc_soxr_oneshot(in_rate, out_rate, num_channels,
			   in, in_len, in_rd, out, out_len, out_wr,
			   io_spec, q_spec, runtime_spec);
}

static inline soxr_error_t soxr_clear(struct ssrc_soxr *thiz) { return ssrc_soxr_clear(thiz); }
static inline void soxr_delete(struct ssrc_soxr *thiz) { ssrc_soxr_delete(thiz); }
static inline double soxr_delay(struct ssrc_soxr *thiz) { return ssrc_soxr_delay(thiz); }

#define SOXR_TPDF SSRC_SOXR_TPDF
#define SOXR_NO_DITHER SSRC_SOXR_NO_DITHER

#define SOXR_QQ SSRC_SOXR_QQ
#define SOXR_LQ SSRC_SOXR_LQ
#define SOXR_MQ SSRC_SOXR_MQ
#define SOXR_HQ SSRC_SOXR_HQ
#define SOXR_VHQ SSRC_SOXR_VHQ

#define SOXR_MINIMUM_PHASE SSRC_SOXR_MINIMUM_PHASE

#define soxr_strerror(e) ((e)?(e):"no error")

#endif

#ifdef __cplusplus
} // extern "C" {
#endif

#endif // #ifndef SSRC_SOXR_H_INCLUDED
