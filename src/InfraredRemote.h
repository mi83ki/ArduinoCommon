#pragma once

#ifdef UNIT_TEST
#include <cstdint>
#else
#include <Arduino.h>
#endif

class InfraredRemote {
 public:
  struct ReceivedData {
    uint32_t rawData;
    int8_t protocol;
  };

  InfraredRemote(uint8_t receivePin, uint8_t sendPin, int8_t protocol,
                 uint8_t dataBits);
  virtual ~InfraredRemote();

  int32_t getData(void) const;
  int8_t getFormat(void) const;
  ReceivedData getReceivedData(void) const;
  void clearReceivedData(void);
  void setDebug(bool debug);

  virtual bool recieveIR(void);
  virtual void printDebug(void);
  virtual void sendIR(uint32_t data, int nbits);
  void requestSend(uint32_t data);
  bool isRequestSend(void);
  virtual bool drive(void);

 protected:
  uint8_t getReceivePin(void) const;
  uint8_t getSendPin(void) const;
  int8_t getProtocol(void) const;
  uint8_t getDataBits(void) const;
  uint32_t getTxData(void) const;
  bool isDebug(void) const;

 private:
  ReceivedData receivedData_;
  uint32_t txData_;
  uint8_t receivePin_;
  uint8_t sendPin_;
  int8_t protocol_;
  uint8_t dataBits_;
  bool requestSend_;
  bool debug_;
};

#ifdef UNIT_TEST

inline InfraredRemote::InfraredRemote(uint8_t receivePin, uint8_t sendPin,
                                      int8_t protocol, uint8_t dataBits)
    : receivedData_{0, 0},
      txData_(0),
      receivePin_(receivePin),
      sendPin_(sendPin),
      protocol_(protocol),
      dataBits_(dataBits),
      requestSend_(false),
      debug_(false) {}

inline InfraredRemote::~InfraredRemote() = default;

inline int32_t InfraredRemote::getData(void) const {
  return static_cast<int32_t>(receivedData_.rawData);
}

inline int8_t InfraredRemote::getFormat(void) const {
  return receivedData_.protocol;
}

inline InfraredRemote::ReceivedData InfraredRemote::getReceivedData(void) const {
  return receivedData_;
}

inline void InfraredRemote::clearReceivedData(void) {
  receivedData_.rawData = 0;
  receivedData_.protocol = 0;
}

inline void InfraredRemote::setDebug(bool debug) { debug_ = debug; }

inline bool InfraredRemote::recieveIR(void) {
  clearReceivedData();
  return false;
}

inline void InfraredRemote::printDebug(void) {}

inline void InfraredRemote::sendIR(uint32_t data, int nbits) {
  (void)data;
  (void)nbits;
}

inline void InfraredRemote::requestSend(uint32_t data) {
  txData_ = data;
  requestSend_ = true;
}

inline bool InfraredRemote::isRequestSend(void) {
  if (!requestSend_) {
    return false;
  }
  requestSend_ = false;
  return true;
}

inline bool InfraredRemote::drive(void) {
  if (!isRequestSend()) {
    return false;
  }
  sendIR(txData_, dataBits_);
  return true;
}

inline uint8_t InfraredRemote::getReceivePin(void) const { return receivePin_; }

inline uint8_t InfraredRemote::getSendPin(void) const { return sendPin_; }

inline int8_t InfraredRemote::getProtocol(void) const { return protocol_; }

inline uint8_t InfraredRemote::getDataBits(void) const { return dataBits_; }

inline uint32_t InfraredRemote::getTxData(void) const { return txData_; }

inline bool InfraredRemote::isDebug(void) const { return debug_; }

#endif
