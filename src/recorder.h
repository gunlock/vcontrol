#pragma once

#include <cstdint>
#include <miniaudio.h>
#include <mutex>
#include <vector>

class Recorder final {
public:
  Recorder();
  ~Recorder();

  void start();
  bool stop();
  bool isRecording() const;

  const std::vector<int16_t>& audioBuffer() const;
  void clearBuffer();

  struct CallbackData {
    std::vector<int16_t>* buffer;
    std::mutex* bufferMutex;
  };

private:
  ma_device m_device;
  CallbackData m_callbackData;
  bool m_recording{false};
  std::vector<int16_t> m_audioBuffer;
  std::mutex m_bufferMutex;
};
