#include <unity.h>

#include "MQTTClientESP32.h"
#include "PubSubClient.h"

void setUp(void) { FakePubSubClientState::reset(); }

void tearDown(void) {}

/**
 * @brief コンストラクタで初期MQTT接続先を設定することを検証する。
 */
void test_constructor_sets_initial_server(void) {
  MQTTClientESP32 client("mqtt-primary.local", 1883);

  TEST_ASSERT_EQUAL_UINT32(1, FakePubSubClientState::serverCalls.size());
  TEST_ASSERT_EQUAL_STRING(
      "mqtt-primary.local",
      FakePubSubClientState::serverCalls[0].host.c_str());
  TEST_ASSERT_EQUAL_UINT16(1883, FakePubSubClientState::serverCalls[0].port);
}

/**
 * @brief 同じMQTT接続先の再設定では切断も再設定もしないことを検証する。
 */
void test_set_server_is_noop_when_destination_is_unchanged(void) {
  MQTTClientESP32 client("mqtt-primary.local", 1883);
  FakePubSubClientState::connected = true;

  TEST_ASSERT_TRUE(client.setServer("mqtt-primary.local", 1883));
  TEST_ASSERT_EQUAL_UINT32(1, FakePubSubClientState::serverCalls.size());
  TEST_ASSERT_EQUAL_UINT32(0, FakePubSubClientState::disconnectCalls);
}

/**
 * @brief MQTT接続先の変更時に現在の接続を切断して新しい接続先を設定することを検証する。
 */
void test_set_server_disconnects_and_updates_destination(void) {
  MQTTClientESP32 client("mqtt-primary.local", 1883);
  FakePubSubClientState::connected = true;

  TEST_ASSERT_TRUE(client.setServer("mqtt-fallback.local", 2883));
  TEST_ASSERT_EQUAL_UINT32(1, FakePubSubClientState::disconnectCalls);
  TEST_ASSERT_EQUAL_UINT32(2, FakePubSubClientState::serverCalls.size());
  TEST_ASSERT_EQUAL_STRING(
      "mqtt-fallback.local",
      FakePubSubClientState::serverCalls[1].host.c_str());
  TEST_ASSERT_EQUAL_UINT16(2883, FakePubSubClientState::serverCalls[1].port);
}

/**
 * @brief 空のMQTTホストを拒否して現在設定を維持することを検証する。
 */
void test_set_server_rejects_empty_host(void) {
  MQTTClientESP32 client("mqtt-primary.local", 1883);
  FakePubSubClientState::connected = true;

  TEST_ASSERT_FALSE(client.setServer("", 2883));
  TEST_ASSERT_EQUAL_UINT32(0, FakePubSubClientState::disconnectCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FakePubSubClientState::serverCalls.size());
}

/**
 * @brief 接続先変更後は待機せず再接続し、保存済みトピックを再購読することを検証する。
 */
void test_set_server_reconnects_immediately_and_resubscribes(void) {
  MQTTClientESP32 client("mqtt-primary.local", 1883);
  FakePubSubClientState::connected = true;
  TEST_ASSERT_TRUE(client.subscribe("sensor/value"));
  FakePubSubClientState::subscribeCalls.clear();
  FakePubSubClientState::connectResult = true;

  TEST_ASSERT_TRUE(client.setServer("mqtt-fallback.local", 1883));
  TEST_ASSERT_TRUE(client.healthCheck());
  TEST_ASSERT_EQUAL_UINT32(1, FakePubSubClientState::connectCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FakePubSubClientState::subscribeCalls.size());
  TEST_ASSERT_EQUAL_STRING(
      "sensor/value", FakePubSubClientState::subscribeCalls[0].c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_constructor_sets_initial_server);
  RUN_TEST(test_set_server_is_noop_when_destination_is_unchanged);
  RUN_TEST(test_set_server_disconnects_and_updates_destination);
  RUN_TEST(test_set_server_rejects_empty_host);
  RUN_TEST(test_set_server_reconnects_immediately_and_resubscribes);
  return UNITY_END();
}
