#include "device_scanner.h"

#include <iio.h>
#include <libhackrf/hackrf.h>

#include "hackrf_init_guard.h"

namespace iqforge {

std::vector<ScannedDevice> scanPlutoDevices() {
  std::vector<ScannedDevice> result;

  // "ip" scans the network (mDNS/Avahi), "usb" scans local USB — together
  // this covers both ways a Pluto can be reached, matching what a "Scan"
  // button in SDR++ does. Comma is the libiio >= 0.24 backend delimiter.
  iio_scan_context* ctx = iio_create_scan_context("ip,usb", 0);
  if (!ctx) return result;

  iio_context_info** infoList = nullptr;
  ssize_t count = iio_scan_context_get_info_list(ctx, &infoList);
  if (count > 0 && infoList) {
    for (ssize_t i = 0; i < count; ++i) {
      ScannedDevice d;
      d.uri = iio_context_info_get_uri(infoList[i]);
      d.description = iio_context_info_get_description(infoList[i]);
      result.push_back(std::move(d));
    }
  }
  if (infoList) iio_context_info_list_free(infoList);
  iio_scan_context_destroy(ctx);
  return result;
}

std::vector<ScannedDevice> scanHackrfDevices() {
  std::vector<ScannedDevice> result;
  ensureHackrfInitialized();

  hackrf_device_list_t* list = hackrf_device_list();
  if (!list) return result;

  for (int i = 0; i < list->devicecount; ++i) {
    ScannedDevice d;
    const char* serial = list->serial_numbers[i];
    d.uri = serial ? serial : "";
    d.description = hackrf_usb_board_id_name(list->usb_board_ids[i]);
    if (serial) d.description += std::string(" (") + serial + ")";
    result.push_back(std::move(d));
  }
  hackrf_device_list_free(list);
  return result;
}

} // namespace iqforge
