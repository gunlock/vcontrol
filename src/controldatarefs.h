#pragma once

#include "recognizer.h"
#include "recorder.h"
#include <XPLMDataAccess.h>
#include <array>
#include <string>

class ControlDataRefs {
public:
  ControlDataRefs(Recorder& recorder, Recognizer& recognizer);
  ~ControlDataRefs();

private:
  static void recordWrite(void* refcon, int value);
  static void recognizeWrite(void* refcon, int value);
  static int recordingRead(void* refcon);
  static int resultRead(void* refcon, void* outBuffer, int inOffset, int inMaxBytes);
  static void resultWrite(void* refcon, void* inBuffer, int inOffset, int inLength);
  static void grammarWrite(void* refcon, void* inBuffer, int inOffset, int inLength);

  Recorder& m_recorder;
  Recognizer& m_recognizer;
  std::string m_result;
  std::array<XPLMDataRef, 5> m_refs{};
};
