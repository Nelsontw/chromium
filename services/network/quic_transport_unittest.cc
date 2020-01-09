// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/quic_transport.h"

#include <vector>

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

// We don't use mojo::BlockingCopyToString because it leads to deadlocks.
std::string Read(mojo::ScopedDataPipeConsumerHandle readable) {
  std::string output;
  while (true) {
    char buffer[1024];
    uint32_t size = sizeof(buffer);
    MojoResult result =
        readable->ReadData(buffer, &size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      return output;
    }
    DCHECK_EQ(result, MOJO_RESULT_OK);
    output.append(buffer, size);
  }
}

class TestHandshakeClient final : public mojom::QuicTransportHandshakeClient {
 public:
  TestHandshakeClient(mojo::PendingReceiver<mojom::QuicTransportHandshakeClient>
                          pending_receiver,
                      base::OnceClosure callback)
      : receiver_(this, std::move(pending_receiver)),
        callback_(std::move(callback)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestHandshakeClient::OnMojoConnectionError, base::Unretained(this)));
  }
  ~TestHandshakeClient() override = default;

  void OnConnectionEstablished(
      mojo::PendingRemote<mojom::QuicTransport> transport,
      mojo::PendingReceiver<mojom::QuicTransportClient> client_receiver)
      override {
    transport_ = std::move(transport);
    client_receiver_ = std::move(client_receiver);
    has_seen_connection_establishment_ = true;
    receiver_.reset();
    std::move(callback_).Run();
  }

  void OnHandshakeFailed() override {
    has_seen_handshake_failure_ = true;
    receiver_.reset();
    std::move(callback_).Run();
  }

  void OnMojoConnectionError() {
    has_seen_handshake_failure_ = true;
    std::move(callback_).Run();
  }

  mojo::PendingRemote<mojom::QuicTransport> PassTransport() {
    return std::move(transport_);
  }
  mojo::PendingReceiver<mojom::QuicTransportClient> PassClientReceiver() {
    return std::move(client_receiver_);
  }
  bool has_seen_connection_establishment() const {
    return has_seen_connection_establishment_;
  }
  bool has_seen_handshake_failure() const {
    return has_seen_handshake_failure_;
  }
  bool has_seen_mojo_connection_error() const {
    return has_seen_mojo_connection_error_;
  }

 private:
  mojo::Receiver<mojom::QuicTransportHandshakeClient> receiver_;

  mojo::PendingRemote<mojom::QuicTransport> transport_;
  mojo::PendingReceiver<mojom::QuicTransportClient> client_receiver_;
  base::OnceClosure callback_;
  bool has_seen_connection_establishment_ = false;
  bool has_seen_handshake_failure_ = false;
  bool has_seen_mojo_connection_error_ = false;
};

class TestClient final : public mojom::QuicTransportClient {
 public:
  explicit TestClient(
      mojo::PendingReceiver<mojom::QuicTransportClient> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  void WaitUntilMojoConnectionError() {
    base::RunLoop run_loop;

    receiver_.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  mojo::Receiver<mojom::QuicTransportClient> receiver_;
};

class QuicTransportTest : public testing::Test {
 public:
  QuicTransportTest()
      : origin_(url::Origin::Create(GURL("https://example.org/"))),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()),
        network_context_remote_(mojo::NullRemote()),
        network_context_(network_service_.get(),
                         network_context_remote_.BindNewPipeAndPassReceiver(),
                         mojom::NetworkContextParams::New()),
        server_(/* port= */ 0,
                {origin_},
                quic::test::crypto_test_utils::ProofSourceForTesting()) {
    EXPECT_EQ(EXIT_SUCCESS, server_.Start());

    cert_verifier_.set_default_result(net::OK);
    host_resolver_.rules()->AddRule("test.example.com", "127.0.0.1");

    network_context_.url_request_context()->set_cert_verifier(&cert_verifier_);
    network_context_.url_request_context()->set_host_resolver(&host_resolver_);
    auto* quic_context = network_context_.url_request_context()->quic_context();
    quic_context->params()->supported_versions.push_back(
        quic::ParsedQuicVersion{quic::PROTOCOL_TLS1_3, quic::QUIC_VERSION_99});
    quic_context->params()->origins_to_force_quic_on.insert(
        net::HostPortPair("test.example.com", 0));
  }
  ~QuicTransportTest() override = default;

  void CreateQuicTransport(
      const GURL& url,
      const url::Origin& origin,
      const net::NetworkIsolationKey& key,
      mojo::PendingRemote<mojom::QuicTransportHandshakeClient>
          handshake_client) {
    network_context_.CreateQuicTransport(url, origin, key,
                                         std::move(handshake_client));
  }
  void CreateQuicTransport(
      const GURL& url,
      const url::Origin& origin,
      mojo::PendingRemote<mojom::QuicTransportHandshakeClient>
          handshake_client) {
    CreateQuicTransport(url, origin, net::NetworkIsolationKey(),
                        std::move(handshake_client));
  }

  GURL GetURL(base::StringPiece suffix) {
    return GURL(quiche::QuicheStrCat("quic-transport://test.example.com:",
                                     server_.server_address().port(), suffix));
  }

  const url::Origin& origin() const { return origin_; }
  const NetworkContext& network_context() const { return network_context_; }

 private:
  const url::Origin origin_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  net::MockCertVerifier cert_verifier_;
  net::MockHostResolver host_resolver_;

  NetworkContext network_context_;

  net::QuicTransportSimpleServer server_;
};

TEST_F(QuicTransportTest, ConnectSuccessfully) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/discard"), origin(),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  EXPECT_TRUE(test_handshake_client.has_seen_connection_establishment());
  EXPECT_FALSE(test_handshake_client.has_seen_handshake_failure());
  EXPECT_FALSE(test_handshake_client.has_seen_mojo_connection_error());
  EXPECT_EQ(1u, network_context().NumOpenQuicTransports());
}

TEST_F(QuicTransportTest, ConnectWithWrongOrigin) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/discard"),
                      url::Origin::Create(GURL("https://evil.com")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  EXPECT_TRUE(test_handshake_client.has_seen_connection_establishment());
  EXPECT_FALSE(test_handshake_client.has_seen_handshake_failure());
  EXPECT_FALSE(test_handshake_client.has_seen_mojo_connection_error());

  // Server resets the connection due to origin mismatch.
  TestClient client(test_handshake_client.PassClientReceiver());
  client.WaitUntilMojoConnectionError();

  EXPECT_EQ(0u, network_context().NumOpenQuicTransports());
}

TEST_F(QuicTransportTest, SendDatagram) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/discard"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  base::RunLoop run_loop_for_datagram;
  bool result;
  uint8_t data[] = {1, 2, 3, 4, 5};
  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());
  transport_remote->SendDatagram(base::make_span(data),
                                 base::BindLambdaForTesting([&](bool r) {
                                   result = r;
                                   run_loop_for_datagram.Quit();
                                 }));
  run_loop_for_datagram.Run();
  EXPECT_TRUE(result);
}

TEST_F(QuicTransportTest, SendToolargeDatagram) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/discard"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  base::RunLoop run_loop_for_datagram;
  bool result;
  // The actual upper limit for one datagram is platform specific, but
  // 786kb should be large enough for any platform.
  std::vector<uint8_t> data(786 * 1024, 99);
  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());

  transport_remote->SendDatagram(base::make_span(data),
                                 base::BindLambdaForTesting([&](bool r) {
                                   result = r;
                                   run_loop_for_datagram.Quit();
                                 }));
  run_loop_for_datagram.Run();
  EXPECT_FALSE(result);
}

TEST_F(QuicTransportTest, EchoOnUnidirectionalStreams) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/echo"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  ASSERT_TRUE(test_handshake_client.has_seen_connection_establishment());

  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());

  mojo::ScopedDataPipeConsumerHandle readable_for_outgoing;
  mojo::ScopedDataPipeProducerHandle writable_for_outgoing;
  const MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 4 * 1024};
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, &writable_for_outgoing,
                                 &readable_for_outgoing));
  uint32_t size = 5;
  ASSERT_EQ(MOJO_RESULT_OK, writable_for_outgoing->WriteData(
                                "hello", &size, MOJO_WRITE_DATA_FLAG_NONE));
  // Signal the end-of-data.
  writable_for_outgoing.reset();

  base::RunLoop run_loop_for_stream_creation;
  uint32_t stream_id;
  bool stream_created;
  transport_remote->CreateStream(
      std::move(readable_for_outgoing),
      /*writable=*/{}, base::BindLambdaForTesting([&](bool b, uint32_t id) {
        stream_created = b;
        stream_id = id;
        run_loop_for_stream_creation.Quit();
      }));
  run_loop_for_stream_creation.Run();
  ASSERT_TRUE(stream_created);

  mojo::ScopedDataPipeConsumerHandle readable_for_incoming;
  uint32_t incoming_stream_id = stream_id;
  base::RunLoop run_loop_for_incoming_stream;
  transport_remote->AcceptUnidirectionalStream(base::BindLambdaForTesting(
      [&](uint32_t id, mojo::ScopedDataPipeConsumerHandle readable) {
        incoming_stream_id = id;
        readable_for_incoming = std::move(readable);
        run_loop_for_incoming_stream.Quit();
      }));

  run_loop_for_incoming_stream.Run();
  ASSERT_TRUE(readable_for_incoming);
  EXPECT_NE(stream_id, incoming_stream_id);

  std::string echo_back = Read(std::move(readable_for_incoming));
  EXPECT_EQ("hello", echo_back);
}

TEST_F(QuicTransportTest, EchoOnBidirectionalStream) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/echo"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  ASSERT_TRUE(test_handshake_client.has_seen_connection_establishment());

  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());

  mojo::ScopedDataPipeConsumerHandle readable_for_outgoing;
  mojo::ScopedDataPipeProducerHandle writable_for_outgoing;
  mojo::ScopedDataPipeConsumerHandle readable_for_incoming;
  mojo::ScopedDataPipeProducerHandle writable_for_incoming;
  const MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 4 * 1024};
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, &writable_for_outgoing,
                                 &readable_for_outgoing));
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, &writable_for_incoming,
                                 &readable_for_incoming));
  uint32_t size = 5;
  ASSERT_EQ(MOJO_RESULT_OK, writable_for_outgoing->WriteData(
                                "hello", &size, MOJO_WRITE_DATA_FLAG_NONE));
  // Signal the end-of-data.
  writable_for_outgoing.reset();

  base::RunLoop run_loop_for_stream_creation;
  uint32_t stream_id;
  bool stream_created;
  transport_remote->CreateStream(
      std::move(readable_for_outgoing), std::move(writable_for_incoming),
      base::BindLambdaForTesting([&](bool b, uint32_t id) {
        stream_created = b;
        stream_id = id;
        run_loop_for_stream_creation.Quit();
      }));
  run_loop_for_stream_creation.Run();
  ASSERT_TRUE(stream_created);

  std::string echo_back = Read(std::move(readable_for_incoming));
  EXPECT_EQ("hello", echo_back);
}

}  // namespace
}  // namespace network
