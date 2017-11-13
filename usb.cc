#include "usb.h"

void DevUsb::LoadDeviceDescriptor() {
  Memory mem(sizeof(UsbCtrl::DeviceDescriptor));
  UsbCtrl::DeviceRequest request;
  request.MakePacketOfGetDescriptorRequest(UsbCtrl::DescriptorType::kDevice, 0, sizeof(UsbCtrl::DeviceDescriptor));
  assert(SendControlTransfer(request, mem, sizeof(UsbCtrl::DeviceDescriptor)));
  memcpy(&_device_desc, mem.GetVirtPtr<uint8_t>(), sizeof(UsbCtrl::DeviceDescriptor));
}

void DevUsb::LoadCombinedDescriptors() {
  int length;
  do {
    UsbCtrl::ConfigurationDescriptor config_desc;
    
    Memory mem(sizeof(UsbCtrl::ConfigurationDescriptor));
    UsbCtrl::DeviceRequest request;
    request.MakePacketOfGetDescriptorRequest(UsbCtrl::DescriptorType::kConfiguration, 0, sizeof(UsbCtrl::ConfigurationDescriptor));
    assert(SendControlTransfer(request, mem, sizeof(UsbCtrl::ConfigurationDescriptor)));
    memcpy(&config_desc, mem.GetVirtPtr<uint8_t>(), sizeof(UsbCtrl::ConfigurationDescriptor));

    length = config_desc.total_length;
  } while(0);

  _combined_desc = new uint8_t[length];

  do {
    Memory mem(sizeof(UsbCtrl::ConfigurationDescriptor));
    UsbCtrl::DeviceRequest request;
    request.MakePacketOfGetDescriptorRequest(UsbCtrl::DescriptorType::kConfiguration, 0, length);
    assert(SendControlTransfer(request, mem, length));
    memcpy(_combined_desc, mem.GetVirtPtr<uint8_t>(), length);
  } while(0);
}

UsbCtrl::DummyDescriptor *DevUsb::GetDescriptorInCombinedDescriptors(UsbCtrl::DescriptorType type, int desc_index) {
  assert(desc_index >= 0);

  UsbCtrl::ConfigurationDescriptor *config_desc = reinterpret_cast<UsbCtrl::ConfigurationDescriptor *>(_combined_desc);

  for (uint16_t index = 0; index < config_desc->total_length;) {
    UsbCtrl::DummyDescriptor *dummy_desc = reinterpret_cast<UsbCtrl::DummyDescriptor *>(_combined_desc + index);
    assert(dummy_desc->length != 0);
    if (static_cast<UsbCtrl::DescriptorType>(dummy_desc->type) == type) {
      if (desc_index == 0) {
        return dummy_desc;
      } else {
        desc_index--;
      }
    }
    index += dummy_desc->length;
  }

  assert(false);
}
