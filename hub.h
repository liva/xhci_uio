// reference: Universal Serial Bus Specification Revision 1.1

#pragma once

#include "usb.h"

class Hub : public DevUsb {
public:
  Hub() = delete;
  Hub(DevUsbController *hc, int addr) : DevUsb(hc, addr) {
  }
  static Hub *Init(DevUsbController *hc, int addr);
  UsbCtrl::PortSpeed GetPortSpeed(int port_id);
  void Reset(int port_id);
  virtual void Release() override {
    printf("hub: info: detached\n");
    assert(false);
  }
private:
  // Table 11-8. Hub Descriptor
  class HubDescriptor {
  public:
    uint8_t length;
    uint8_t type;
    uint8_t num_of_ports;
    uint16_t characteristics;
    uint8_t power_on_to_power_good;
    uint8_t hub_controller_current;

    struct TtThinkTime {
      static const int kOffset = 5;
      static const int kLen = 6 - 5 + 1;
    };
  } __attribute__((__packed__));
  static_assert(sizeof(HubDescriptor) == 7, "");

  // Table 11-12. Hub Class Feature Selectors
  enum class HubClassFeatureSelector : uint16_t {
    kChangeHubLocalPower = 0,
    kChangeHubOverCurrent = 1,
    kPortConnection = 0,
    kPortEnable = 1,
    kPortSuspend = 2,
    kPortOverCurrent = 3,
    kPortReset = 4,
    kPortPower = 8,
    kPortLowSpeed = 9,
    kChangePortConnection = 16,
    kChangePortEnable = 17,
    kChangePortSuspend = 18,
    kChangePortOverCurrent = 19,
    kChangePortReset = 20,
  };

  // Table 11-13. Hub Status Field, wHubStatus
  struct HubStatus {
    static const uint16_t kFlagLocalPowerSource = 1 << 0;
  };

  // Table 11-15. Port Status Field, wPortStatus
  struct PortStatus {
    static const uint16_t kFlagCurrentConnectStatus = 1 << 0;
    static const uint16_t kFlagPortEnabled = 1 << 1;
    static const uint16_t kFlagSuspend = 1 << 2;
    static const uint16_t kFlagOverCurrent = 1 << 3;
    static const uint16_t kFlagReset = 1 << 4;
    static const uint16_t kFlagPower = 1 << 8;
    static const uint16_t kFlagLowSpeed = 1 << 9;
  };

  int _num_of_ports = 0;
  HubDescriptor *_desc;
  
  void InitSub();
  uint16_t GetPortStatus(int port_id);
  void SetPortFeature(int port_id, HubClassFeatureSelector selector);
  void ClearPortFeature(int port_id, HubClassFeatureSelector selector);
  DevUsb *AttachDeviceToHostController(int port_id) {
    return GetHostController()->AttachDevice(this, GetAddr(), port_id);
  }
};
