#include "rtsp/net.cpp"
#include <boost/beast/_experimental/test/stream.hpp>
#include <rtsp/parser.hpp>

using namespace rtsp;

/**
 * In order to test rtsp::tcp_connection we create a derived class that does the opposite:
 * instead of waiting for a message; it'll first send a message and then wait for a response.
 */
class tcp_tester : public tcp_connection {

public:
  static auto create_client(asio::io_context &io_context, int port, immer::box<state::StreamSession> session) {
    auto tester = new tcp_tester(io_context, std::move(session));

    tcp::resolver resolver(io_context);
    asio::connect(tester->socket(), resolver.resolve("0.0.0.0", std::to_string(port)));

    return boost::shared_ptr<tcp_tester>(tester);
  }

  void run(std::string_view raw_msg, std::function<void(std::optional<RTSP_PACKET> /* response */)> on_response) {
    auto send_msg = rtsp::parse(raw_msg).value();

    send_message(send_msg, [self = shared_from_this(), on_response](auto bytes) {
      self->receive_message([self = self->shared_from_this(), on_response](auto reply_msg) {
        on_response(reply_msg);
        self->socket().close();
      });
    });

    run_context();
  }

  void run_context() {
    while (socket().is_open()) {
      ioc.run_one();
    }
  }

protected:
  explicit tcp_tester(asio::io_context &io_context, immer::box<state::StreamSession> session)
      : tcp_connection(io_context, std::move(session)), ioc(io_context) {}

  boost::asio::io_context &ioc;
};

TEST_CASE("Custom Parser", "[RTSP]") {
  SECTION("Requests") {
    SECTION("Non valid packet") {
      auto parsed = rtsp::parse("OPTIONS rtsp://10.1.2.49:48010 RTSP/1.0"); // missing CSeq
      REQUIRE(parsed.has_value() == false);
    }

    SECTION("Basic packet") {
      auto payload = "MissingNo rtsp://1.1.1.1:1234 RTSP/1.0\r\n"
                     "CSeq: 1993\r\n\r\n"s;
      auto parsed = rtsp::parse(payload).value();

      REQUIRE(parsed.type == REQUEST);
      REQUIRE(parsed.request.type == TARGET_URI);
      REQUIRE_THAT(parsed.request.cmd, Equals("MissingNo"));
      REQUIRE_THAT(parsed.request.uri.ip, Equals("1.1.1.1"));
      REQUIRE_THAT(parsed.request.uri.protocol, Equals("rtsp"));
      REQUIRE(parsed.request.uri.port == 1234);
      REQUIRE(parsed.seq_number == 1993);

      REQUIRE(parsed.options.empty());
      REQUIRE(parsed.payloads.empty());

      // Round trip
      REQUIRE(rtsp::to_string(parsed) == rtsp::to_string(rtsp::parse(rtsp::to_string(parsed)).value()));
    }

    SECTION("Stream target") {
      auto payload = "MissingNo streamid=audio/1/2/3 RTSP/1.0\r\n"
                     "CSeq: 1993\r\n\r\n"s;
      auto parsed = rtsp::parse(payload).value();

      REQUIRE(parsed.type == REQUEST);
      REQUIRE(parsed.request.type == TARGET_STREAM);
      REQUIRE_THAT(parsed.request.cmd, Equals("MissingNo"));
      REQUIRE_THAT(parsed.request.stream.type, Equals("audio"));
      REQUIRE_THAT(parsed.request.stream.params, Equals("/1/2/3"));
      REQUIRE(parsed.seq_number == 1993);

      REQUIRE(parsed.options.empty());
      REQUIRE(parsed.payloads.empty());

      // Round trip
      REQUIRE(rtsp::to_string(parsed) == rtsp::to_string(rtsp::parse(rtsp::to_string(parsed)).value()));
    }

    SECTION("Complete packet") {
      auto payload = "OPTIONS rtsp://10.1.2.49:48010 RTSP/1.0\n"
                     "CSeq: 1\n"
                     "X-GS-ClientVersion: 14\n"
                     "Host: 10.1.2.49"
                     "\r\n\r\n"
                     "v=0\n"
                     "o=android 0 14 IN IPv4 0.0.0.0\n"
                     "s=NVIDIA Streaming Client\n"
                     "a=x-nv-video[0].clientViewportWd:1920\n"
                     "a=x-nv-video[0].clientViewportHt:1080"s;
      auto parsed = rtsp::parse(payload).value();

      REQUIRE(parsed.type == REQUEST);
      REQUIRE_THAT(parsed.request.cmd, Equals("OPTIONS"));
      REQUIRE(parsed.request.type == TARGET_URI);
      REQUIRE_THAT(parsed.request.uri.ip, Equals("10.1.2.49"));
      REQUIRE_THAT(parsed.request.uri.protocol, Equals("rtsp"));
      REQUIRE(parsed.request.uri.port == 48010);
      REQUIRE(parsed.seq_number == 1);

      // Options
      REQUIRE_THAT(parsed.options["X-GS-ClientVersion"], Equals("14"));
      REQUIRE_THAT(parsed.options["Host"], Equals("10.1.2.49"));

      // Payloads
      REQUIRE_THAT(parsed.payloads[0].second, Equals("0"));
      REQUIRE_THAT(parsed.payloads[1].second, Equals("android 0 14 IN IPv4 0.0.0.0"));
      REQUIRE_THAT(parsed.payloads[2].second, Equals("NVIDIA Streaming Client"));
      REQUIRE_THAT(parsed.payloads[3].second, Equals("x-nv-video[0].clientViewportWd:1920"));
      REQUIRE_THAT(parsed.payloads[4].second, Equals("x-nv-video[0].clientViewportHt:1080"));

      // Round trip
      REQUIRE(rtsp::to_string(parsed) == rtsp::to_string(rtsp::parse(rtsp::to_string(parsed)).value()));
    }
  }

  SECTION("Response") {
    SECTION("Non valid packet") {
      auto parsed = rtsp::parse("RTSP/1.0 200 OK"); // missing CSeq
      REQUIRE(parsed.has_value() == false);
    }

    SECTION("Basic response") {
      auto payload = "RTSP/1.0 200 OK\r\n"
                     "CSeq: 123\r\n\r\n";
      auto parsed = rtsp::parse(payload).value();

      REQUIRE(parsed.type == RESPONSE);
      REQUIRE(parsed.seq_number == 123);

      REQUIRE_THAT(parsed.response.msg, Equals("OK"));
      REQUIRE(parsed.response.status_code == 200);

      REQUIRE(parsed.payloads.empty());
      REQUIRE(parsed.options.empty());

      // Round trip
      REQUIRE(rtsp::to_string(parsed) == rtsp::to_string(rtsp::parse(rtsp::to_string(parsed)).value()));
    }

    SECTION("Complete packet") {
      auto payload = "RTSP/1.0 404 NOT OK\n"
                     "CSeq: 1\n"
                     "X-GS-ClientVersion: 14\n"
                     "Host: 10.1.2.49"
                     "\r\n\r\n"
                     "v=0\n"
                     "o=android 0 14 IN IPv4 0.0.0.0\n"
                     "s=NVIDIA Streaming Client\n"
                     "a=x-nv-video[0].clientViewportWd:1920\n"
                     "a=x-nv-video[0].clientViewportHt:1080"s;
      auto parsed = rtsp::parse(payload).value();

      REQUIRE(parsed.type == RESPONSE);
      REQUIRE_THAT(parsed.response.msg, Equals("NOT OK"));
      REQUIRE(parsed.response.status_code == 404);
      REQUIRE(parsed.seq_number == 1);

      // Options
      REQUIRE_THAT(parsed.options["X-GS-ClientVersion"], Equals("14"));
      REQUIRE_THAT(parsed.options["Host"], Equals("10.1.2.49"));

      // Payloads
      REQUIRE_THAT(parsed.payloads[0].second, Equals("0"));
      REQUIRE_THAT(parsed.payloads[1].second, Equals("android 0 14 IN IPv4 0.0.0.0"));
      REQUIRE_THAT(parsed.payloads[2].second, Equals("NVIDIA Streaming Client"));
      REQUIRE_THAT(parsed.payloads[3].second, Equals("x-nv-video[0].clientViewportWd:1920"));
      REQUIRE_THAT(parsed.payloads[4].second, Equals("x-nv-video[0].clientViewportHt:1080"));

      // Round trip
      REQUIRE(rtsp::to_string(parsed) == rtsp::to_string(rtsp::parse(rtsp::to_string(parsed)).value()));
    }
  }
}

immer::box<StreamSession> test_init_state() {
  StreamSession session = {.session_id = 1234,
                           .event_bus = std::make_shared<dp::event_bus>(),
                           .display_mode = {1920, 1080, 60},
                           .audio_mode = {2, 1, 1, {state::AudioMode::FRONT_LEFT, state::AudioMode::FRONT_RIGHT}},
                           .app = {},
                           .gcm_key = crypto::hex_to_str("9d804e47a6aa6624b7d4b502b32cc522", true),
                           .gcm_iv_key = crypto::hex_to_str("01234567890", true),
                           .unique_id = "0f691f13730748328a22a6952a5ac3a2",
                           .ip = "192.168.1.1",
                           .rtsp_port = 1,
                           .control_port = 2,
                           .audio_port = 3,
                           .video_port = 4};
  return {session};
}

TEST_CASE("Commands", "[RTSP]") {
  constexpr int port = 8080;
  boost::asio::io_context ioc;
  auto state = test_init_state();
  auto wolf_server = tcp_server(ioc, port, state);
  auto wolf_client = tcp_tester::create_client(ioc, port, state);

  SECTION("MissingNo") {
    wolf_client->run("MissingNo rtsp://10.1.2.49:48010 RTSP/1.0\r\n"
                     "CSeq: 1\r\n\r\n"sv,
                     [](std::optional<RTSP_PACKET> response) {
                       REQUIRE(response.has_value());
                       REQUIRE(response.value().response.status_code == 404);
                       REQUIRE(response.value().seq_number == 1);
                     });
  }

  SECTION("OPTION") {
    wolf_client->run("OPTIONS rtsp://10.1.2.49:48010 RTSP/1.0\r\n"
                     "CSeq: 1\r\n"
                     "X-GS-ClientVersion: 14\r\n"
                     "Host: 10.1.2.49"
                     "\r\n\r\n"sv,
                     [](std::optional<RTSP_PACKET> response) {
                       REQUIRE(response.has_value());
                       REQUIRE(response.value().response.status_code == 200);
                       REQUIRE(response.value().seq_number == 1);
                     });
  }

  SECTION("DESCRIBE") {
    wolf_client->run("DESCRIBE rtsp://10.1.2.49:48010 RTSP/1.0\n"
                     "CSeq: 2\n"
                     "X-GS-ClientVersion: 14\n"
                     "Host: 10.1.2.49\n"
                     "Accept: application/sdp"
                     "\r\n\r\n"sv,
                     [](std::optional<RTSP_PACKET> response) {
                       REQUIRE(response);
                       REQUIRE(response.value().response.status_code == 200);
                       REQUIRE(response.value().seq_number == 2);
                       REQUIRE_THAT(response.value().payloads[0].first, Equals("sprop-parameter-sets"));
                       REQUIRE_THAT(response.value().payloads[0].second, Equals("AAAAAU"));
                       REQUIRE_THAT(response.value().payloads[1].second, Equals("fmtp:97 surround-params=21101"));
                     });
  }

  SECTION("SETUP audio") {
    wolf_client->run(
        "SETUP streamid=audio/0/0 RTSP/1.0\n"
        "CSeq: 3\n"
        "X-GS-ClientVersion: 14\n"
        "Host: 10.1.2.49\n"
        "Transport: unicast;X-GS-ClientPort=50000-50001\n"
        "If-Modified-Since: Thu, 01 Jan 1970 00:00:00 GMT"
        "\r\n\r\n"sv,
        [&state](std::optional<RTSP_PACKET> response) {
          REQUIRE(response.has_value());
          REQUIRE(response.value().response.status_code == 200);
          REQUIRE(response.value().seq_number == 3);

          REQUIRE_THAT(response.value().options["Session"], Equals("DEADBEEFCAFE;timeout = 90"));
          REQUIRE_THAT(response.value().options["Transport"], Equals(fmt::format("server_port={}", state->audio_port)));
        });
  }

  SECTION("SETUP video") {
    wolf_client->run(
        "SETUP streamid=video/0/0 RTSP/1.0\n"
        "CSeq: 4\n"
        "X-GS-ClientVersion: 14\n"
        "Host: 10.1.2.49\n"
        "Session:  DEADBEEFCAFE\n"
        "Transport: unicast;X-GS-ClientPort=50000-50001\n"
        "If-Modified-Since: Thu, 01 Jan 1970 00:00:00 GMT"
        "\r\n\r\n"sv,
        [&state](std::optional<RTSP_PACKET> response) {
          REQUIRE(response.has_value());
          REQUIRE(response.value().response.status_code == 200);
          REQUIRE(response.value().seq_number == 4);

          REQUIRE_THAT(response.value().options["Session"], Equals("DEADBEEFCAFE;timeout = 90"));
          REQUIRE_THAT(response.value().options["Transport"], Equals(fmt::format("server_port={}", state->video_port)));
        });
  }

  SECTION("SETUP control") {
    wolf_client->run("SETUP streamid=control/0/0 RTSP/1.0\n"
                     "CSeq: 5\n"
                     "X-GS-ClientVersion: 14\n"
                     "Host: 10.1.2.49\n"
                     "Session:  DEADBEEFCAFE\n"
                     "Transport: unicast;X-GS-ClientPort=50000-50001\n"
                     "If-Modified-Since: Thu, 01 Jan 1970 00:00:00 GMT"
                     "\r\n\r\n"sv,
                     [&state](std::optional<RTSP_PACKET> response) {
                       REQUIRE(response.has_value());
                       REQUIRE(response.value().response.status_code == 200);
                       REQUIRE(response.value().seq_number == 5);

                       REQUIRE_THAT(response.value().options["Session"], Equals("DEADBEEFCAFE;timeout = 90"));
                       REQUIRE_THAT(response.value().options["Transport"],
                                    Equals(fmt::format("server_port={}", state->control_port)));
                     });
  }

  SECTION("ANNOUNCE control") {
    // This is a very long message, it'll kick the recursion in receive_message()
    wolf_client->run("ANNOUNCE streamid=control/13/0 RTSP/1.0\n"
                     "CSeq: 6\n"
                     "X-GS-ClientVersion: 14\n"
                     "Host: 0.0.0.0\n"
                     "Session:  DEADBEEFCAFE\n"
                     "Content-type: application/sdp\n"
                     "Content-length: 1308"
                     "\r\n\r\n" // start of payload
                     "v=0\n"
                     "o=android 0 14 IN IPv4 0.0.0.0\n"
                     "s=NVIDIA Streaming Client\n"
                     "a=x-nv-video[0].clientViewportWd:1920 \n"
                     "a=x-nv-video[0].clientViewportHt:1080 \n"
                     "a=x-nv-video[0].maxFPS:60 \n"
                     "a=x-nv-video[0].packetSize:1024 \n"
                     "a=x-nv-video[0].rateControlMode:4 \n"
                     "a=x-nv-video[0].timeoutLengthMs:7000 \n"
                     "a=x-nv-video[0].framesWithInvalidRefThreshold:0 \n"
                     "a=x-nv-video[0].initialBitrateKbps:15500 \n"
                     "a=x-nv-video[0].initialPeakBitrateKbps:15500 \n"
                     "a=x-nv-vqos[0].bw.minimumBitrateKbps:15500 \n"
                     "a=x-nv-vqos[0].bw.maximumBitrateKbps:15500 \n"
                     "a=x-nv-vqos[0].fec.enable:1 \n"
                     "a=x-nv-vqos[0].videoQualityScoreUpdateTime:5000 \n"
                     "a=x-nv-vqos[0].qosTrafficType:0 \n"
                     "a=x-nv-aqos.qosTrafficType:0 \n"
                     "a=x-nv-general.featureFlags:167 \n"
                     "a=x-nv-general.useReliableUdp:13 \n"
                     "a=x-nv-vqos[0].fec.minRequiredFecPackets:2 \n"
                     "a=x-nv-vqos[0].drc.enable:0 \n"
                     "a=x-nv-general.enableRecoveryMode:0 \n"
                     "a=x-nv-video[0].videoEncoderSlicesPerFrame:1 \n"
                     "a=x-nv-clientSupportHevc:0 \n"
                     "a=x-nv-vqos[0].bitStreamFormat:0 \n"
                     "a=x-nv-video[0].dynamicRangeMode:0 \n"
                     "a=x-nv-video[0].maxNumReferenceFrames:1 \n"
                     "a=x-nv-video[0].clientRefreshRateX100:0 \n"
                     "a=x-nv-audio.surround.numChannels:2 \n"
                     "a=x-nv-audio.surround.channelMask:3 \n"
                     "a=x-nv-audio.surround.enable:0 \n"
                     "a=x-nv-audio.surround.AudioQuality:0 \n"
                     "a=x-nv-aqos.packetDuration:5 \n"
                     "a=x-nv-video[0].encoderCscMode:0 \n"
                     "t=0 0\n"
                     "m=video 47998 \n"sv,
                     [](std::optional<RTSP_PACKET> response) {
                       REQUIRE(response.has_value());
                       REQUIRE(response.value().response.status_code == 200);
                       REQUIRE(response.value().seq_number == 6);
                     });
  }
}