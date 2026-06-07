#include "config.h"
#include "controldatarefs.h"
#include "logger.h"
#include "recognizer.h"
#include "recorder.h"
#include <XPLMPlugin.h>
#include <XPLMUtilities.h>
#include <cstring>
#include <filesystem>
#include <memory>

static std::unique_ptr<Recorder> recorder;
static std::unique_ptr<Recognizer> recognizer;
static std::unique_ptr<ControlDataRefs> datarefs;

static std::string pluginRoot() {
  char buf[512] = {};
  XPLMGetPluginInfo(XPLMGetMyID(), nullptr, buf, nullptr, nullptr);
  return std::filesystem::path(buf).parent_path().parent_path().string();
}

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
  xplog.init(PLUGIN_NAME " ");
  xplog.addSink([](std::string_view msg) { XPLMDebugString(msg.data()); });

  std::strncpy(outName, PLUGIN_NAME, 256);
  std::strncpy(outSig, PLUGIN_SIGNATURE, 256);
  std::strncpy(outDesc, PLUGIN_DESC, 256);

  xplog.info("XPluginStart");

  recorder = std::make_unique<Recorder>();
  recognizer = std::make_unique<Recognizer>(pluginRoot() + "/model");
  datarefs = std::make_unique<ControlDataRefs>(*recorder, *recognizer);

  return 1;
}

PLUGIN_API void XPluginStop() {
  xplog.info("XPluginStop");
  datarefs.reset();
  recognizer.reset();
  recorder.reset();
}

PLUGIN_API int XPluginEnable() {
  xplog.info("XPluginEnable");
  return 1;
}

PLUGIN_API void XPluginDisable() {
  xplog.info("XPluginDisable");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int msg, void* param) {
  (void)from;
  (void)msg;
  (void)param;
}
