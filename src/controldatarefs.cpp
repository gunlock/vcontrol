#include "controldatarefs.h"
#include "logger.h"
#include <cstring>

ControlDataRefs::ControlDataRefs(Recorder& recorder, Recognizer& recognizer)
    : m_recorder(recorder), m_recognizer(recognizer) {

  m_recognizer.setResultCallback([this](const std::string& text) {
    m_result = text;
  });

  m_refs[0] = XPLMRegisterDataAccessor(
    "vcontrol/record", xplmType_Int, /*writable=*/1,
    nullptr, recordWrite,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, this
  );
  m_refs[1] = XPLMRegisterDataAccessor(
    "vcontrol/recognize", xplmType_Int, /*writable=*/1,
    nullptr, recognizeWrite,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, this
  );
  m_refs[2] = XPLMRegisterDataAccessor(
    "vcontrol/recording", xplmType_Int, /*writable=*/0,
    recordingRead, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    this, nullptr
  );
  m_refs[3] = XPLMRegisterDataAccessor(
    "vcontrol/result", xplmType_Data, /*writable=*/1,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    resultRead, resultWrite,
    this, this
  );
  m_refs[4] = XPLMRegisterDataAccessor(
    "vcontrol/grammar", xplmType_Data, /*writable=*/1,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, grammarWrite,
    nullptr, this
  );

  xplog.info("registered {} control datarefs", m_refs.size());
}

ControlDataRefs::~ControlDataRefs() {
  for (auto& ref : m_refs) {
    if (ref) {
      XPLMUnregisterDataAccessor(ref);
      ref = nullptr;
    }
  }
}

void ControlDataRefs::recordWrite(void* refcon, int value) {
  auto& self = *static_cast<ControlDataRefs*>(refcon);
  if (value == 1) {
    self.m_recorder.start();
  } else {
    self.m_recorder.stop();
  }
}

void ControlDataRefs::recognizeWrite(void* refcon, int value) {
  if (value != 1) {
    return;
  }
  auto& self = *static_cast<ControlDataRefs*>(refcon);
  const auto& buf = self.m_recorder.audioBuffer();
  if (buf.empty()) {
    xplog.info("recognize: buffer is empty");
    return;
  }
  self.m_recognizer.accept(buf.data(), static_cast<int>(buf.size()));
  self.m_recognizer.flush();
  self.m_recorder.clearBuffer();
}

int ControlDataRefs::recordingRead(void* refcon) {
  auto& self = *static_cast<ControlDataRefs*>(refcon);
  return self.m_recorder.isRecording() ? 1 : 0;
}

int ControlDataRefs::resultRead(void* refcon, void* outBuffer, int inOffset, int inMaxBytes) {
  auto& self = *static_cast<ControlDataRefs*>(refcon);
  int len = static_cast<int>(self.m_result.size());
  if (outBuffer && inMaxBytes > 0) {
    int toCopy = std::min(inMaxBytes, len - inOffset);
    if (toCopy > 0) {
      std::memcpy(outBuffer, self.m_result.data() + inOffset, toCopy);
    }
  }
  return len;
}

void ControlDataRefs::resultWrite(void* refcon, void* inBuffer, int /*inOffset*/, int inLength) {
  auto& self = *static_cast<ControlDataRefs*>(refcon);
  self.m_result.assign(static_cast<const char*>(inBuffer), inLength);
}

void ControlDataRefs::grammarWrite(void* refcon, void* inBuffer, int /*inOffset*/, int inLength) {
  auto& self = *static_cast<ControlDataRefs*>(refcon);
  self.m_recognizer.reload(std::string(static_cast<const char*>(inBuffer), inLength));
}
