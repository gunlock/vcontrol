#pragma once

#include <functional>
#include <string>
#include <vosk_api.h>

class Recognizer final {
public:
  using ResultCallback = std::function<void(const std::string& text)>;

  Recognizer(const std::string& modelPath, float sampleRate = 16000.0f);
  ~Recognizer();

  void accept(const void* data, int numFrames);
  void flush();
  void setResultCallback(ResultCallback cb);
  bool isReady() const;

  // Reinitialize with a new Vosk grammar JSON array.
  // Model is reused; only the recognizer is swapped.
  // Empty string unloads the recognizer without freeing the model.
  void reload(const std::string& grammarJson);

private:
  void dispatchResult(const char* result);

  VoskModel* m_model{nullptr};
  VoskRecognizer* m_recognizer{nullptr};
  float m_sampleRate;
  ResultCallback m_callback;
};
