#include "recognizer.h"
#include "config.h"
#include "logger.h"
#include <nlohmann/json.hpp>

Recognizer::Recognizer(const std::string& modelPath, float sampleRate)
    : m_sampleRate(sampleRate) {
#if VOSK_VERBOSE
  vosk_set_log_level(-1);
#else
  vosk_set_log_level(-2);
#endif

  m_model = vosk_model_new(modelPath.c_str());
  if (!m_model) {
    xplog.err("Recognizer: failed to load model from '{}'", modelPath);
    return;
  }

  xplog.info("Recognizer: model loaded from '{}'", modelPath);
  // Recognizer starts without a grammar; Lua loads one via vcontrol/grammar.
}

Recognizer::~Recognizer() {
  if (m_recognizer) {
    vosk_recognizer_free(m_recognizer);
  }
  if (m_model) {
    vosk_model_free(m_model);
  }
}

void Recognizer::reload(const std::string& grammarJson) {
  if (m_recognizer) {
    vosk_recognizer_free(m_recognizer);
    m_recognizer = nullptr;
  }

  if (grammarJson.empty()) {
    xplog.info("Recognizer: unloaded");
    return;
  }

  if (!m_model) {
    xplog.err("Recognizer: cannot reload — model not loaded");
    return;
  }

  m_recognizer = vosk_recognizer_new_grm(m_model, m_sampleRate, grammarJson.c_str());
  if (!m_recognizer) {
    xplog.err("Recognizer: failed to create recognizer with grammar: {}", grammarJson);
    return;
  }

  xplog.info("Recognizer: loaded grammar: {}", grammarJson);
}

void Recognizer::accept(const void* data, int numFrames) {
  if (!m_recognizer) {
    return;
  }
  vosk_recognizer_accept_waveform_s(m_recognizer, static_cast<const short*>(data), numFrames);
}

void Recognizer::flush() {
  if (!m_recognizer) {
    xplog.info("Recognizer: flush called but no recognizer loaded");
    return;
  }
  const char* result = vosk_recognizer_final_result(m_recognizer);
  dispatchResult(result);
}

void Recognizer::dispatchResult(const char* result) {
  if (!result || !m_callback) {
    return;
  }
  auto json = nlohmann::json::parse(result, nullptr, false);
  if (!json.is_discarded() && json.contains("text")) {
    std::string text = json["text"].get<std::string>();
    xplog.info("Recognizer: '{}'", text);
    if (!text.empty() && text != "[unk]") {
      m_callback(text);
    }
  }
}

void Recognizer::setResultCallback(ResultCallback cb) {
  m_callback = std::move(cb);
}

bool Recognizer::isReady() const {
  return m_model != nullptr && m_recognizer != nullptr;
}
