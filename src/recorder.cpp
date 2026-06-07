#define MINIAUDIO_IMPLEMENTATION
#include "recorder.h"
#include "logger.h"

static void captureCallback(ma_device* device, void* /*output*/, const void* input,
                            ma_uint32 frameCount) {
  auto* data = static_cast<Recorder::CallbackData*>(device->pUserData);
  if (data->buffer && data->bufferMutex) {
    auto* samples = static_cast<const int16_t*>(input);
    std::lock_guard lock(*data->bufferMutex);
    data->buffer->insert(data->buffer->end(), samples, samples + frameCount);
  }
}

Recorder::Recorder() = default;

Recorder::~Recorder() {
  if (m_recording) {
    stop();
  }
}

void Recorder::start() {
  if (m_recording) {
    return;
  }

  {
    std::lock_guard lock(m_bufferMutex);
    m_audioBuffer.clear();
  }

  m_callbackData = {&m_audioBuffer, &m_bufferMutex};

  ma_device_config config = ma_device_config_init(ma_device_type_capture);
  config.capture.format = ma_format_s16;
  config.capture.channels = 1;
  config.sampleRate = 16000;
  config.dataCallback = captureCallback;
  config.pUserData = &m_callbackData;

  if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) {
    xplog.err("Recorder: failed to init capture device");
    return;
  }

  if (ma_device_start(&m_device) != MA_SUCCESS) {
    xplog.err("Recorder: failed to start capture device");
    ma_device_uninit(&m_device);
    return;
  }

  m_recording = true;
  xplog.info("Recorder: started");
}

bool Recorder::stop() {
  if (!m_recording) {
    return false;
  }
  ma_device_stop(&m_device);
  ma_device_uninit(&m_device);
  m_recording = false;
  xplog.info("Recorder: stopped ({} frames buffered)", m_audioBuffer.size());
  return true;
}

bool Recorder::isRecording() const {
  return m_recording;
}

const std::vector<int16_t>& Recorder::audioBuffer() const {
  return m_audioBuffer;
}

void Recorder::clearBuffer() {
  std::lock_guard lock(m_bufferMutex);
  m_audioBuffer.clear();
}
