#pragma once

#include <array>
#include <gst/base/gstbasetransform.h>
#include <vector>

constexpr int AUDIO_DATA_SHARDS = 4;
constexpr int AUDIO_FEC_SHARDS = 2;
constexpr int AUDIO_TOTAL_SHARDS = AUDIO_DATA_SHARDS + AUDIO_FEC_SHARDS;
constexpr int AUDIO_MAX_BLOCK_SIZE = 1400;

// For unknown reasons, the RS parity matrix computed by our RS implementation
// doesn't match the one Nvidia uses for audio data. I'm not exactly sure why,
// but we can simply replace it with the matrix generated by OpenFEC which
// works correctly. This is possible because the data and FEC shard count is
// constant and known in advance.
constexpr unsigned char AUDIO_FEC_PARITY[] = {0x77, 0x40, 0x38, 0x0e, 0xc7, 0xa7, 0x0d, 0x6c};

G_BEGIN_DECLS

#define gst_TYPE_rtp_moonlight_pay_audio (gst_rtp_moonlight_pay_audio_get_type())
#define gst_rtp_moonlight_pay_audio(obj)                                                                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), gst_TYPE_rtp_moonlight_pay_audio, gst_rtp_moonlight_pay_audio))
#define gst_rtp_moonlight_pay_audio_CLASS(klass)                                                                       \
  (G_TYPE_CHECK_CLASS_CAST((klass), gst_TYPE_rtp_moonlight_pay_audio, gst_rtp_moonlight_pay_audioClass))
#define gst_IS_rtp_moonlight_pay_audio(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), gst_TYPE_rtp_moonlight_pay_audio))
#define gst_IS_rtp_moonlight_pay_audio_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), gst_TYPE_rtp_moonlight_pay_audio))

typedef struct _gst_rtp_moonlight_pay_audio gst_rtp_moonlight_pay_audio;
typedef struct _gst_rtp_moonlight_pay_audioClass gst_rtp_moonlight_pay_audioClass;

struct _gst_rtp_moonlight_pay_audio {
  GstBaseTransform base_rtpmoonlightpay_audio;

  uint16_t cur_seq_number;

  bool encrypt;
  std::string aes_key;
  std::string aes_iv;

  int packet_duration;

  std::array<unsigned char *, AUDIO_TOTAL_SHARDS> packets_buffer;
  _reed_solomon *rs;
};

struct _gst_rtp_moonlight_pay_audioClass {
  GstBaseTransformClass base_rtpmoonlightpay_audio_class;
};

GType gst_rtp_moonlight_pay_audio_get_type(void);

G_END_DECLS
