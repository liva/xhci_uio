#include "keyboard.h"

Keyboard *Keyboard::Init(DevUsbController *hc, int addr) {
  Keyboard *dev = new Keyboard(hc, addr);
  dev->LoadDeviceDescriptor();
  dev->LoadCombinedDescriptors();
  UsbCtrl::InterfaceDescriptor *interface_desc = dev->GetInterfaceDescriptorInCombinedDescriptors(0);
  if (interface_desc->class_code == 3 && interface_desc->subclass_code == 1 && interface_desc->protocol_code == 1) {
    printf("keyboard: info: attched\n");
    dev->InitSub();
    return dev;
  }
  delete dev;
  return nullptr;
}

void Keyboard::InitSub() {
  UsbCtrl::EndpointDescriptor *ed0 = GetEndpointDescriptorInCombinedDescriptors(0);
  assert(ed0->GetMaxPacketSize() == kMaxPacketSize);
  assert(ed0->GetTransferType() == UsbCtrl::TransferType::kInterrupt);
  assert(ed0->GetDirection() == UsbCtrl::PacketIdentification::kIn);

  if (SetupEndpoint(ed0->GetEndpointNumber(), ed0->GetInterval(), UsbCtrl::TransferType::kInterrupt, ed0->GetDirection(), ed0->GetMaxPacketSize(), &_buf) != ReturnState::kSuccess) {
    printf("usb keyboard: error: failed to init endpoint\n");
    return;
  }
  
  do {
    // Set Protocol
    // see HID1_11 7.2.6 Set_Protocol Request 
    
    Memory mem(0);
    UsbCtrl::DeviceRequest request;
    request.MakePacket(0b00100001, 0x0B, 0, 0, 0);
    assert(SendControlTransfer(request, mem, 0));
  } while(0);
  pthread_t tid;
  if (pthread_create(&tid, NULL, Handle, this) != 0) {
    perror("pthread_create:");
    exit(1);
  }
}
