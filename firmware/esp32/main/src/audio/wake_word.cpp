#include "audio/wake_word.h"

#include <cstring>

#include <esp_log.h>
#include <esp_wn_iface.h>
#include <esp_wn_models.h>
#include <model_path.h>

namespace {
constexpr char kTag[] = "wake";
}

WakeWord::~WakeWord() {
  if (iface_ != nullptr && model_data_ != nullptr) {
    auto* iface = static_cast<const esp_wn_iface_t*>(iface_);
    auto* data = static_cast<model_iface_data_t*>(model_data_);
    iface->destroy(data);
  }
  if (models_ != nullptr) {
    esp_srmodel_deinit(static_cast<srmodel_list_t*>(models_));
  }
}

bool WakeWord::init() {
  auto* models = esp_srmodel_init("model");
  if (models == nullptr || models->num <= 0) {
    ESP_LOGE(kTag, "no ESP-SR model partition found");
    if (models != nullptr) {
      esp_srmodel_deinit(models);
    }
    return false;
  }
  models_ = models;

  for (int i = 0; i < models->num; ++i) {
    ESP_LOGI(kTag, "model[%d]=%s (%s)", i, models->model_name[i],
             models->model_info[i]);
  }

  char* name = esp_srmodel_filter(models, ESP_WN_PREFIX, "computer");
  if (name == nullptr) {
    name = esp_srmodel_filter(models, ESP_WN_PREFIX, nullptr);
  }
  if (name == nullptr) {
    ESP_LOGE(kTag, "no wake word model in model partition");
    return false;
  }
  std::strncpy(model_name_, name, sizeof(model_name_) - 1);

  const esp_wn_iface_t* iface = esp_wn_handle_from_name(name);
  if (iface == nullptr) {
    ESP_LOGE(kTag, "no WakeNet interface for %s", name);
    return false;
  }

  model_iface_data_t* data = iface->create(name, DET_MODE_90);
  if (data == nullptr) {
    ESP_LOGE(kTag, "failed to create WakeNet model %s", name);
    return false;
  }
  iface->set_det_threshold(data, 0.5f, 1);
  // Do not call iface->clean() after detection: it crashes in this model.
  // WakeNet9 re-arms itself; the next detect() call works normally.

  iface_ = iface;
  model_data_ = data;
  chunk_size_ = iface->get_samp_chunksize(data);
  sample_rate_ = iface->get_samp_rate(data);

  ESP_LOGI(kTag, "model=%s rate=%d chunk=%d", model_name_, sample_rate_,
           chunk_size_);
  if (sample_rate_ != 16000) {
    ESP_LOGW(kTag, "WakeNet expects %d Hz; mic is 16000 Hz", sample_rate_);
  }
  return true;
}

bool WakeWord::detect(const int16_t* samples) {
  if (iface_ == nullptr || model_data_ == nullptr || samples == nullptr) {
    return false;
  }
  auto* iface = static_cast<const esp_wn_iface_t*>(iface_);
  auto* data = static_cast<model_iface_data_t*>(model_data_);
  const wakenet_state_t state =
      iface->detect(data, const_cast<int16_t*>(samples));
  return state == WAKENET_DETECTED;
}
