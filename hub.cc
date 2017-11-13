#include "hub.h"

Hub *Hub::Init(DevUsbController *hc, int addr) {
  Hub *dev = new Hub(hc, addr);
  dev->LoadDeviceDescriptor();
  dev->LoadCombinedDescriptors();
  UsbCtrl::InterfaceDescriptor *interface_desc = dev->GetInterfaceDescriptorInCombinedDescriptors(0);
  if (interface_desc->class_code == 9 && interface_desc->subclass_code == 0 && interface_desc->protocol_code == 0) {
    printf("hub: info: full-/low-speed hub attached\n");
    dev->InitSub();
    return dev;
  }
  delete dev;
  return nullptr;
}

void Hub::InitSub() {
  do {
    // Get Hub Descriptor
    
    Memory mem(sizeof(HubDescriptor));
    UsbCtrl::DeviceRequest request;
    request.MakePacket(0b10100000, static_cast<uint8_t>(UsbCtrl::RequestCode::kGetDescriptor), (0x29 << 8) + 0, 0, sizeof(HubDescriptor));
    assert(SendControlTransfer(request, mem, sizeof(HubDescriptor)));

    _desc = mem.GetVirtPtr<HubDescriptor>();

    InitHub(_desc->num_of_ports, MaskValue<HubDescriptor::TtThinkTime>(_desc->characteristics));
  } while(0);
  do {
    // Set Configuration
    
    Memory mem(0);
    UsbCtrl::DeviceRequest request;
    request.MakePacket(0b00000000, static_cast<uint8_t>(UsbCtrl::RequestCode::kSetConfiguration), GetConfigurationDescriptorInCombinedDescriptors()->configuration_value, 0, 0);
    assert(SendControlTransfer(request, mem, 0));
  } while(0);
  do {
    // Get Hub Status
    
    Memory mem(4);
    UsbCtrl::DeviceRequest request;
    request.MakePacket(0b10100000, static_cast<uint8_t>(UsbCtrl::RequestCode::kGetStatus), 0, 0, 4);
    assert(SendControlTransfer(request, mem, 4));
    assert((mem.GetVirtPtr<uint16_t>()[0] & HubStatus::kFlagLocalPowerSource) == 0);
  } while(0);

  UsbCtrl::EndpointDescriptor *ed0 = GetEndpointDescriptorInCombinedDescriptors(0);
  assert(ed0->GetTransferType() == UsbCtrl::TransferType::kInterrupt);
  assert(ed0->GetDirection() == UsbCtrl::PacketIdentification::kIn);

  RingBuffer<uint8_t *> buf(64);
  if (SetupEndpoint(ed0->GetEndpointNumber(), ed0->GetInterval(), UsbCtrl::TransferType::kInterrupt, ed0->GetDirection(), ed0->GetMaxPacketSize(), &buf) != ReturnState::kSuccess) {
    printf("hub: error: failed to init endpoint\n");
    return;
  }

  for (int i = 1; i <= _desc->num_of_ports; i++) {
    SetPortFeature(i, HubClassFeatureSelector::kPortPower);
    usleep(_desc->power_on_to_power_good * 2000);
    if (GetPortStatus(i) & PortStatus::kFlagCurrentConnectStatus) {
      ClearPortFeature(i, HubClassFeatureSelector::kChangePortConnection);
      printf("hub: info: new present port\n");
      SetPortFeature(i, HubClassFeatureSelector::kPortReset);
      usleep(10 * 1000);
      uint16_t status = GetPortStatus(i);
      assert((status & PortStatus::kFlagPortEnabled) != 0);
      assert((status & PortStatus::kFlagReset) == 0);
      ClearPortFeature(i, HubClassFeatureSelector::kChangePortReset);
      if (AttachDeviceToHostController(i) == nullptr) {
        printf("hub: error: failed to attach device\n");
      }
    }
  }
}

uint16_t Hub::GetPortStatus(int port_id) {
  Memory mem(4);
  UsbCtrl::DeviceRequest request;
  request.MakePacket(0b10100011, static_cast<uint8_t>(UsbCtrl::RequestCode::kGetStatus), 0, port_id, 4);
  assert(SendControlTransfer(request, mem, 4));
  return mem.GetVirtPtr<uint16_t>()[0];
}

void Hub::SetPortFeature(int port_id, HubClassFeatureSelector selector) {
  Memory mem(0);
  UsbCtrl::DeviceRequest request;
  request.MakePacket(0b00100011, static_cast<uint8_t>(UsbCtrl::RequestCode::kSetFeature), static_cast<uint16_t>(selector), port_id, 0);
  assert(SendControlTransfer(request, mem, 0));
}

void Hub::ClearPortFeature(int port_id, HubClassFeatureSelector selector) {
  Memory mem(0);
  UsbCtrl::DeviceRequest request;
  request.MakePacket(0b00100011, static_cast<uint8_t>(UsbCtrl::RequestCode::kClearFeature), static_cast<uint16_t>(selector), port_id, 0);
  assert(SendControlTransfer(request, mem, 0));
}

UsbCtrl::PortSpeed Hub::GetPortSpeed(int port_id) {
  if ((GetPortStatus(port_id) & PortStatus::kFlagLowSpeed) != 0) {
    return UsbCtrl::PortSpeed::kLowSpeed;
  } else {
    return UsbCtrl::PortSpeed::kFullSpeed;
  }
}

void Hub::Reset(int port_id) {
  SetPortFeature(port_id, HubClassFeatureSelector::kPortReset);
  usleep(20 * 1000);
}
