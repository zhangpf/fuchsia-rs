// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "advertising_report_parser.h"

#include "lib/ftl/logging.h"

namespace bluetooth {
namespace hci {

AdvertisingReportParser::AdvertisingReportParser(const EventPacket& event)
    : encountered_error_(false) {
  FTL_DCHECK(event.event_code() == kLEMetaEventCode);
  auto params = event.GetPayload<LEMetaEventParams>();
  FTL_DCHECK(params->subevent_code == kLEAdvertisingReportSubeventCode);

  LEAdvertisingReportSubeventParams* subevent_params =
      reinterpret_cast<LEAdvertisingReportSubeventParams*>(
          params->subevent_parameters);

  remaining_reports_ = subevent_params->num_reports;
  remaining_bytes_ = event.GetPayloadSize() - sizeof(LEMetaEventParams) -
                     sizeof(LEAdvertisingReportSubeventParams);
  ptr_ = subevent_params->reports;
}

bool AdvertisingReportParser::GetNextReport(LEAdvertisingReportData** out_data,
                                            int8_t* out_rssi) {
  FTL_DCHECK(out_data);
  FTL_DCHECK(out_rssi);

  if (encountered_error_ || !HasMoreReports()) return false;

  LEAdvertisingReportData* data =
      reinterpret_cast<LEAdvertisingReportData*>(ptr_);

  // Each report contains the all the report data, followed by the advertising
  // payload, followed by a single octet for the RSSI.
  size_t report_size = sizeof(*data) + data->length_data + 1;
  if (report_size > remaining_bytes_) {
    // Report exceeds the bounds of the packet.
    encountered_error_ = true;
    return false;
  }

  remaining_bytes_ -= report_size;
  remaining_reports_--;
  ptr_ += report_size;

  *out_data = data;
  *out_rssi = *(ptr_ - 1);

  return true;
}

bool AdvertisingReportParser::HasMoreReports() {
  if (encountered_error_) return false;

  if (!!remaining_reports_ != !!remaining_bytes_) {
    // There should be no bytes remaining if there are no reports left to parse.
    encountered_error_ = true;
    return false;
  }
  return !!remaining_reports_;
}

}  // namespace hci
}  // namespace bluetooth
