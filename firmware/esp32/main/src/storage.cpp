#include "storage.h"

#include <cstddef>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_spiffs.h>

namespace {
constexpr char kTag[] = "storage";
constexpr char kPartitionLabel[] = "spiffs";
constexpr char kMountPoint[] = "/spiffs";
}  // namespace

void mountSpiffs() {
  esp_vfs_spiffs_conf_t conf = {};
  conf.base_path = kMountPoint;
  conf.partition_label = kPartitionLabel;
  conf.max_files = 5;
  conf.format_if_mount_failed = true;

  const esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "SPIFFS mount failed: %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
  }

  size_t total = 0;
  size_t used = 0;
  if (esp_spiffs_info(kPartitionLabel, &total, &used) == ESP_OK) {
    ESP_LOGI(kTag, "SPIFFS mounted: total=%u used=%u",
             static_cast<unsigned>(total), static_cast<unsigned>(used));
  } else {
    ESP_LOGW(kTag, "failed to read SPIFFS usage");
  }
}
