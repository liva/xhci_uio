#include "xhci.h"
#include "hub.h"
#include "keyboard.h"

// Table 138: TRB Completion Code Definitions
const char* const DevXhci::_completion_code_table[] = {
  "Invalid",
  "Success",
  "Data Buffer Error",
  "Babble Detected Error",
  "USB Transaction Error",
  "TRB Error",
  "Stall Error",
  "Resource Error",
  "Bandwidth Error",
  "No Slots Available Error",
  "Invalid Stream Type Error",
  "Slot Not Enabled Error",
  "Endpoint Not Enabled Error",
  "Short Packet",
  "Ring Underrun",
  "Ring Overrun",
  "VF Event Ring Full Error",
  "Parameter Error",
  "Bandwidth Overrun Error",
  "Context State Error",
  "No Ping Response Error",
  "Event Ring Full Error",
  "Incompatible Device Error",
  "Missed Service Error",
  "Command Ring Stopped",
  "Command Aborted",
  "Stopped",
  "Stopped - Length Invalid",
  "Stopped - Short Packet",
  "Max Exit Latency Too Large Error",
  "Reserved",
  "Isoch Buffer Overrun",
  "Event Lost Error",
  "Undefined Error",
  "Invalid Stream ID Error",
  "Secondary Bandwidth Error",
  "Split Transaction Error",
};

void DevXhci::Init() {
  _pci.Init();
  uint16_t vid, did;
  _pci.ReadPciReg(DevPci::kVendorIDReg, vid);
  _pci.ReadPciReg(DevPci::kDeviceIDReg, did);
  uint8_t interface, sub, base;
  _pci.ReadPciReg(DevPci::kRegInterfaceClassCode, interface);
  _pci.ReadPciReg(DevPci::kRegSubClassCode, sub);
  _pci.ReadPciReg(DevPci::kRegBaseClassCode, base);
  uint16_t command;
  _pci.ReadPciReg(DevPci::kCommandReg, command);
  command |= DevPci::kCommandRegBusMasterEnableFlag;
  _pci.WritePciReg(DevPci::kCommandReg, command);

  uint32_t addr_bkup;
  _pci.ReadPciReg(DevPci::kBaseAddressReg0, addr_bkup);
  _pci.WritePciReg(DevPci::kBaseAddressReg0, 0xFFFFFFFF);
  uint32_t size;
  _pci.ReadPciReg(DevPci::kBaseAddressReg0, size);
  size = ~size + 1;
  _pci.WritePciReg(DevPci::kBaseAddressReg0, addr_bkup);

  int fd = open("/sys/class/uio/uio0/device/resource0", O_RDWR);
    
  _capreg_base_addr = reinterpret_cast<uint8_t *>(mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  close(fd);

  _capreg_base_addr32 = reinterpret_cast<volatile uint32_t *>(_capreg_base_addr);
  _opreg_base_addr = reinterpret_cast<volatile uint32_t *>(_capreg_base_addr + _capreg_base_addr[0]);
  // 4.2 Host Controller Initialization

  // wait until ready
  while (IsFlagSet(_opreg_base_addr[kOpRegOffsetUsbSts], kOpRegUsbStsFlagControllerNotReady)) {
    asm volatile("":::"memory");
  }

  if (IsFlagClear(_opreg_base_addr[kOpRegOffsetUsbSts], kOpRegUsbStsFlagHchalted)) {
    // halt controller
    _opreg_base_addr[kOpRegOffsetUsbCmd] &= ~kOpRegUsbCmdFlagRunStop;
  }

  while(IsFlagClear(_opreg_base_addr[kOpRegOffsetUsbSts], kOpRegUsbStsFlagHchalted)) {
    asm volatile("":::"memory");
  }

  // reset controller
  _opreg_base_addr[kOpRegOffsetUsbCmd] |= kOpRegUsbCmdFlagReset;

  while(IsFlagSet(_opreg_base_addr[kOpRegOffsetUsbCmd], kOpRegUsbCmdFlagReset)) {
    asm volatile("":::"memory");
  }

  // get information
  _max_slots = MaskValue<CapReg32HcsParams1MaxSlots>(_capreg_base_addr32[kCapReg32OffsetHcsParams1]);
  _excapreg_base_addr = _capreg_base_addr32 + MaskValue<CapReg32HccParams1Xecp>(_capreg_base_addr32[kCapReg32OffsetHccParams1]);
  _doorbell_array_base_addr = _capreg_base_addr32 + MaskValue<CapReg32DboffDoorbellArrayOffset>(_capreg_base_addr32[kCapReg32OffsetDboff]);
  _runtime_base_addr = _capreg_base_addr32 + MaskValue<CapReg32TrsoffRuntimeSpaceOffset>(_capreg_base_addr32[kCapReg32OffsetRtsoff]) * 8;

  // Program the Max Device Slots Enabled (MaxSlotsEn) field in the CONFIG register (5.4.7) to enable the device slots that system software is going to use.
  _opreg_base_addr[kOpRegOffsetConfig] &= ~GenerateMask<OpRegConfigMaxSlotsEn, uint32_t>();
  _opreg_base_addr[kOpRegOffsetConfig] |= GenerateValue<OpRegConfigMaxSlotsEn, uint32_t>(_max_slots);

  // Program the Device Context Base Address Array Pointer (DCBAAP) register (5.4.6) with a 64-bit address pointing to where the Device Context Base Address Array is located.
  _dcbaa_mem = new Memory((_max_slots + 1) * sizeof(uint64_t));
  uint64_t *dcbaa_base = _dcbaa_mem->GetVirtPtr<uint64_t>();
    
  _opreg_base_addr[kOpRegOffsetDcbaap] = _dcbaa_mem->GetPhysPtr() & 0xFFFFFFFF;
  _opreg_base_addr[kOpRegOffsetDcbaap + 1] = _dcbaa_mem->GetPhysPtr() >> 32;

  for (int i = 0; i < _max_slots + 1; i++) {
    dcbaa_base[i] = 0;
  }

  SetupScratchPad();

  // Define the Command Ring Dequeue Pointer by programming the Command Ring Control Register (5.4.5) with a 64-bit address pointing to the starting address of the first TRB of the Command Ring.
  _command_ring.Init(this);
  _opreg_base_addr[kOpRegOffsetCrcr] = (_command_ring.GetMemory().GetPhysPtr() & kOpRegCrcrMaskCommandRingPointer) | kOpRegCrcrFlagRingCycleStatus;
  _opreg_base_addr[kOpRegOffsetCrcr + 1] = _command_ring.GetMemory().GetPhysPtr() >> 32;

  // Initialize interrupts
  _event_ring.Init(this);
  _event_ring_segment_table.Init(&_event_ring);
    _opreg_base_addr[kOpRegOffsetUsbCmd] |= kOpRegUsbCmdFlagInterrupterEnable;
  _interrupter.Init(_runtime_base_addr + kRunRegIntRegSet, &_event_ring_segment_table, &_event_ring);
    
  // start controller
  _opreg_base_addr[kOpRegOffsetUsbCmd] |= kOpRegUsbCmdFlagRunStop;

  while((_opreg_base_addr[kOpRegOffsetUsbSts] & kOpRegUsbStsFlagHchalted) != 0) {
    asm volatile("":::"memory");
  }
    
  _device_list = new Device*[_max_slots + 1];
  for (int i = 0; i < _max_slots + 1; i++) {
    _device_list[i] = 0;
  }

  int max_ports = MaskValue<CapReg32HcsParams1MaxPorts>(_capreg_base_addr32[kCapReg32OffsetHcsParams1]);
  _root_hub_device_list = new RootPortDevice*[max_ports + 1];
  for (int i = 0; i <= max_ports; i++) {
    _root_hub_device_list[i] = nullptr;
  }

  if (pthread_mutex_init(&_mp, NULL) < 0) {
    perror("pthread_mutex_init:");
  }

  printf("xhci: info: successfully initialized!\n");
}

void DevXhci::AttachAllSub() {
  int max_ports = MaskValue<CapReg32HcsParams1MaxPorts>(_capreg_base_addr32[kCapReg32OffsetHcsParams1]);
  for (int i = 0; i < max_ports; i++) {
    int root_port_id = i + 1;
    volatile uint32_t *portsc = &_opreg_base_addr[kOpRegOffsetPortsc + (root_port_id - 1) * 4];
    if (IsFlagSet(*portsc, kOpRegPortscFlagCcs)) {
      Attach(root_port_id);
    }
  }
}

void DevXhci::Attach(int root_port_id) {
  if (_root_hub_device_list[root_port_id] == nullptr) {
    volatile uint32_t *portsc = &_opreg_base_addr[kOpRegOffsetPortsc + (root_port_id - 1) * 4];

    RootPortDevice *device = new RootPortDevice(this, root_port_id);
    _root_hub_device_list[root_port_id] = device;

    if (IsFlagClear(*portsc, kOpRegPortscFlagPortEnabled)) {
      Reset(root_port_id);
    }

    device->Init();
  }
}

void DevXhci::Detach(int root_port_id) {
  Device *device = _root_hub_device_list[root_port_id];
  if (device != nullptr) {
    _root_hub_device_list[root_port_id] = nullptr;
    device->Release();
    delete device;
  }
}

void DevXhci::Reset(int root_port_id) {
  volatile uint32_t *portsc = &_opreg_base_addr[kOpRegOffsetPortsc + (root_port_id - 1) * 4];
  
  // reset the port
  *portsc = (*portsc & ~kOpRegPortscFlagsRwcBits) | kOpRegPortscFlagPrc;
  while(IsFlagSet(*portsc, kOpRegPortscFlagPrc)) {
    asm volatile("":::"memory");
  }
  *portsc = (*portsc & ~kOpRegPortscFlagsRwcBits) | kOpRegPortscFlagPortReset;

  while(IsFlagClear(*portsc, kOpRegPortscFlagPrc)) {
    asm volatile("":::"memory");
  }
  *portsc = (*portsc & ~kOpRegPortscFlagsRwcBits) | kOpRegPortscFlagPrc;

  assert(IsFlagSet(*portsc, kOpRegPortscFlagPortEnabled));
}

DevUsb *DevXhci::Device::Init() {
  // 4.3.3 Device Slot Initialization
  do {
    CommandRing::EnableSlotCommandTrb com(_hc->GetSlotType(_root_port_id));
    CommandRing::CompletionInfo info = _hc->_command_ring.Issue(com, &_hc->_mp);
    if (info.completion_code != TrbCompletionCode::kSuccess) {
      return nullptr;
    }
    _slot_id = info.slot_id;
  } while(0);

  _hc->RegisterDevice(this);

  if (SetRouteString() != ReturnState::kSuccess) {
    return nullptr;
  }
  if (_input_context.Init(this)) {
    return nullptr;
  }
  _output_context.Init(this);
  _hc->SetDcbaap(_output_context.GetPhysAddr(), _slot_id);

  do {
    CommandRing::AddressDeviceCommandTrb com(_input_context.GetPhysAddr(), _slot_id, true);
    CommandRing::CompletionInfo info = _hc->_command_ring.Issue(com, &_hc->_mp);
    if (info.completion_code != TrbCompletionCode::kSuccess) {
      return nullptr;
    }
  } while(0);
  do {
    CommandRing::AddressDeviceCommandTrb com(_input_context.GetPhysAddr(), _slot_id, false);
    CommandRing::CompletionInfo info = _hc->_command_ring.Issue(com, &_hc->_mp);
    if (info.completion_code != TrbCompletionCode::kSuccess) {
      return nullptr;
    }
  } while(0);

  switch(GetPortSpeed()) {
  case UsbCtrl::PortSpeed::kFullSpeed: {
    do {
      Memory mem(8);
      UsbCtrl::DeviceRequest request;
      request.MakePacketOfGetDescriptorRequest(UsbCtrl::DescriptorType::kDevice, 0, 8);
      if (!SendControlTransfer(request, mem, 8)) {
        return nullptr;
      }

      _input_context.UpdateMaxPacketSizeOfEndpoint0(mem.GetVirtPtr<UsbCtrl::DeviceDescriptor>()->max_packet_size);
    } while(0);

    do {
      CommandRing::EvaluateContextCommandTrb com(_input_context.GetPhysAddr(), _slot_id);
      CommandRing::CompletionInfo info = _hc->_command_ring.Issue(com, &_hc->_mp);
      if (info.completion_code != TrbCompletionCode::kSuccess) {
        return nullptr;
      }
    } while(0);
    break;
  }
  }

  DevUsb *_dev_usb;

  if (_dev_usb = Hub::Init(_hc, _slot_id)) {
    RegisterDevUsb(_dev_usb);
    return _dev_usb;
  }
  
  if (_dev_usb = Keyboard::Init(_hc, _slot_id)) {
    RegisterDevUsb(_dev_usb);
    return _dev_usb;
  }

  printf("usb: info: unknown device\n");
      
  return nullptr;
}

void DevXhci::Device::Release() {
  UnRegisterDevUsb();
  
  _hc->UnRegisterDevice(this);
  
  do {
    CommandRing::DisableSlotCommandTrb com(_slot_id);
    CommandRing::CompletionInfo info = _hc->_command_ring.Issue(com, &_hc->_mp);
    if (info.completion_code != TrbCompletionCode::kSuccess) {
      return;
    }
  } while(0);
}

bool DevXhci::Device::SendControlTransfer(UsbCtrl::DeviceRequest &request, Memory &mem, size_t data_size) {
  if (request._length == 0) {
    TransferRing::TransferTrb *trb[2];
    TransferRing::SetupStageTrb trb1(TransferRing::SetupStageTrb::ValueTransferType::kNoDataStage, false, true, request);
    TransferRing::StatusStageTrb trb2(TrbRingBase::Trb::Direction::kIn, false, true, false);

    trb[0] = &trb1;
    trb[1] = &trb2;

    TransferRing::CompletionInfo info = _input_context.Issue(1, trb, 2, &_hc->_mp);
    if (info.completion_code != TrbCompletionCode::kSuccess) {
      return false;
    }
  } else {
    TransferRing::SetupStageTrb::ValueTransferType type;
    TrbRingBase::Trb::Direction dir1, dir2;
    if (request._request_type & 0b10000000) {
      type = TransferRing::SetupStageTrb::ValueTransferType::kInDataStage;
      dir1 = TrbRingBase::Trb::Direction::kIn;
      dir2 = TrbRingBase::Trb::Direction::kOut;
    } else {
      type = TransferRing::SetupStageTrb::ValueTransferType::kOutDataStage;
      dir1 = TrbRingBase::Trb::Direction::kOut;
      dir2 = TrbRingBase::Trb::Direction::kIn;
    }
    TransferRing::TransferTrb *trb[3];
    TransferRing::SetupStageTrb trb1(type, false, true, request);
    TransferRing::DataStageTrb trb2(dir1, data_size, false, false, false, mem.GetPhysPtr());
    TransferRing::StatusStageTrb trb3(dir2, false, true, false);

    trb[0] = &trb1;
    trb[1] = &trb2;
    trb[2] = &trb3;

    TransferRing::CompletionInfo info = _input_context.Issue(1, trb, 3, &_hc->_mp);
    if (info.completion_code != TrbCompletionCode::kSuccess) {
      return false;
    }
  }
  return true;
}

void DevXhci::Device::InitHub(int number_of_ports, int ttt) {
  _input_context.InitHub(number_of_ports, ttt);
  do {
    CommandRing::EvaluateContextCommandTrb com(_input_context.GetPhysAddr(), _slot_id);
    CommandRing::CompletionInfo info = _hc->_command_ring.Issue(com, &_hc->_mp);
    assert(info.completion_code == TrbCompletionCode::kSuccess);
  } while(0);
}

bool DevXhci::EventRing::Handle(phys_addr &dequeue_ptr) {
  int offset = dequeue_ptr - _mem->GetPhysPtr();
  uint32_t *ptr = _mem->GetVirtPtr<uint32_t>();
  assert(offset % sizeof(uint32_t) == 0);
  ptr += offset / sizeof(uint32_t);

  bool dequeu_ptr_incremented = false;

  while(true) {
    EventTrb trb(ptr);
    if (trb.GetCycleBit() != _consumer_cycle_bit) {
      if (dequeu_ptr_incremented) {
        dequeue_ptr = _mem->GetPhysPtr() + (ptr - _mem->GetVirtPtr<uint32_t>()) * sizeof(uint32_t);
      }
      return dequeu_ptr_incremented;
    }

    switch(trb.GetType()) {
    case TransferEventTrb::kValueTrbType: {
      TransferEventTrb trb2(ptr);
      phys_addr pointer;
      TransferRing::CompletionInfo info;
      trb2.SetContainer(info, pointer);
      _hc->CompleteTransfer(pointer, info);
      break;
    }
    case CommandCompletionEventTrb::kValueTrbType: {
      CommandCompletionEventTrb trb2(ptr);
      phys_addr pointer;
      CommandRing::CompletionInfo info;
      trb2.SetContainer(info, pointer);
      _hc->CompleteCommand(pointer, info);
      break;
    }
    case PortStatusChangeEventTrb::kValueTrbType: {
      auto container = new ContainerForPortStatusChangeHandler;
      container->that = _hc;
      PortStatusChangeEventTrb trb2(ptr);
      trb2.SetContainer(*container);
      pthread_t tid;
      if (pthread_create(&tid, NULL, HandlePortStatusChange, container) != 0) {
        perror("pthread_create:");
        exit(1);
      }
      break;
    }
    default: {
      trb.ShowErrUnknown();
      break;
    }
    }
	
    ptr += 4;
    if (ptr - _mem->GetVirtPtr<uint32_t>() == kEntryNum * kEntrySize / sizeof(uint32_t)) {
      ptr = _mem->GetVirtPtr<uint32_t>();
      _consumer_cycle_bit = !_consumer_cycle_bit;
    }
    dequeu_ptr_incremented = true;
  }
}

void DevXhci::Interrupter::Init(volatile uint32_t *base_addr, EventRingSegmentTable *erst, EventRing *event_ring) {
  phys_addr erst_addr = erst->GetMemory().GetPhysPtr();
  _base_addr = base_addr;
  // default Interrupt Moderation Interval is 4000(1ms)
  // refer to Table 49: Interrupter Moderation Register (IMOD)
  _base_addr[kRegOffsetImod]
    = GenerateValue<ImodRegModerationInterval, uint32_t>(40000)
    | GenerateValue<ImodRegModerationCounter, uint32_t>(0);
  _base_addr[kRegOffsetErstsz]
    = GenerateValue<ErstszRegEventRingSegmentTableSize, uint32_t>(erst->GetSize());
  _dequeue_ptr = event_ring->GetMemory().GetPhysPtr();
  WriteDequeuePtr();
  _base_addr[kRegOffsetErstba + 0] = erst_addr & GenerateMask<ErstbaEventRingSegmentTableBaseAddress, uint32_t>();
  _base_addr[kRegOffsetErstba + 1] = erst_addr >> 32;
  _base_addr[kRegOffsetIman] |= kImanRegFlagEnable;
  _erst = erst;
}

uint8_t DevXhci::GetSlotType(int root_port_id) {
  volatile uint32_t *base = _excapreg_base_addr;
  while(true) {
    uint8_t capid = MaskValue(*base, kExtCapRegLenCapabilityId, kExtCapRegOffsetCapabilityId);
    switch(capid) {
    case kExCapCodeSupportedProtocol: {
      int offset = MaskValue(base[2], kSupportedProtocolCap8LenCompatiblePortOffset, kSupportedProtocolCap8OffsetCompatiblePortOffset);
      int count = MaskValue(base[2], kSupportedProtocolCap8LenCompatiblePortCount, kSupportedProtocolCap8OffsetCompatiblePortCount);
      if (offset <= root_port_id && root_port_id < offset + count) {
        return MaskValue(base[3], kSupportedProtocolCapCLenProtocolSlotType, kSupportedProtocolCapCOffsetProtocolSlotType);
      }
      break;
    }
    }
    int next_offset = MaskValue(*base, kExtCapRegLenNextPointer, kExtCapRegOffsetNextPointer);
    if (next_offset == 0) {
      break;
    }
    base += next_offset;
  }
  assert(false);
}

void DevXhci::SetupScratchPad() {
  // setup scratchpad
  int max_scratchpad_bufs = (MaskValue<CapReg32HcsParams2MaxScratchpadHi>(_capreg_base_addr32[kCapReg32OffsetHcsParams2]) << 5)
    | MaskValue<CapReg32HcsParams2MaxScratchpadLow>(_capreg_base_addr32[kCapReg32OffsetHcsParams2]);
  _context_size = IsFlagSet(_capreg_base_addr32[kCapReg32OffsetHccParams1], kCapReg32HccParams1FlagContextSize) ? 64 : 32;
    
  if (max_scratchpad_bufs > 0) {
    size_t page_size = (_opreg_base_addr[kOpRegOffsetPageSize] & kOpRegPageSizeMask) << 12;
    if (page_size != 4096) {
      panic("page size is bigger than 4096!\n");
    }
    _scratchpad_array_mem = new Memory(max_scratchpad_bufs * sizeof(uint64_t));
    _scratchpad_mem = new Memory(page_size * max_scratchpad_bufs);
    uint64_t *dcbaa_base = _dcbaa_mem->GetVirtPtr<uint64_t>();
    dcbaa_base[0] = _scratchpad_array_mem->GetPhysPtr();
    uint64_t *scratchpad_array = _scratchpad_array_mem->GetVirtPtr<uint64_t>();
    for (int i = 0; i < max_scratchpad_bufs; i++) {
      scratchpad_array[i] = _scratchpad_mem->GetPhysPtr() + i * page_size;
    }
    for (int i = 0; i < page_size * max_scratchpad_bufs; i++) {
      _scratchpad_mem->GetVirtPtr<uint8_t>()[i] = 0;
    }
  }
}

DevUsb *DevXhci::AttachDevice(Hub *hub, int hub_addr, int hub_port_id) {
  HubPortDevice *device = new HubPortDevice(this, _device_list[hub_addr], hub, hub_port_id);
  return device->Init();
}

void DevXhci::Device::DeviceContext::OutEndpointContext::Init(Device *device, uint32_t *addr, int dci, int interval, UsbCtrl::TransferType type, int max_packet_size) {
  EndpointContext::Init(device, addr, dci);

  int ep_type;
  switch(type) {
  case UsbCtrl::TransferType::kControl:
    assert(false);
    break;
  case UsbCtrl::TransferType::kIsochronous:
    ep_type = 1;
    break;
  case UsbCtrl::TransferType::kBulk:
    ep_type = 2;
    break;
  case UsbCtrl::TransferType::kInterrupt:
    ep_type = 3;
    break;
  }
  int interval_ = 0;
  for (; interval_ < 256; interval_++) {
    int tmp = 2;
    for (int j = 0; j < interval_; j++) {
      tmp *= 2;
    }
    if (tmp * 125 >= interval * 1000) {
      break;
    }
  }
  _addr[0] = GenerateValue<Mult, uint32_t>(0)
    | GenerateValue<MaxPrimaryStreams, uint32_t>(0)
    | GenerateValue<Interval, uint32_t>(interval_);
  _addr[1] = GenerateValue<Cerr, uint32_t>(3)
    | GenerateValue<EndpointType, uint32_t>(ep_type)
    | GenerateValue<MaxBurstSize, uint32_t>(0)
    | GenerateValue<MaxPacketSize, uint32_t>(max_packet_size);
  _ring.Init(_device, _dci);
  phys_addr tr_ptr = _ring.GetMemory().GetPhysPtr();
  _addr[2] = kFlagDequequeCycleState
    | (tr_ptr & GenerateMask<TrDequeuePointer, uint32_t>());
  _addr[3] = tr_ptr >> 32;
  _addr[4] = 0;
  _addr[5] = 0;
  _addr[6] = 0;
  _addr[7] = 0;
}

void DevXhci::Device::DeviceContext::InEndpointContext::Init(Device *device, uint32_t *addr, int dci, int interval, UsbCtrl::TransferType type, int max_packet_size, RingBuffer<uint8_t *> *buf) {
  EndpointContext::Init(device, addr, dci);
  _buf = buf;

  int ep_type;
  switch(type) {
  case UsbCtrl::TransferType::kControl:
    ep_type = 0;
    break;
  case UsbCtrl::TransferType::kIsochronous:
    ep_type = 1;
    break;
  case UsbCtrl::TransferType::kBulk:
    ep_type = 2;
    break;
  case UsbCtrl::TransferType::kInterrupt:
    ep_type = 3;
    break;
  }
  ep_type += 4;
  int interval_ = 0;
  for (; interval_ < 256; interval_++) {
    int tmp = 2;
    for (int j = 0; j < interval_; j++) {
      tmp *= 2;
    }
    if (tmp * 125 >= interval * 1000) {
      break;
    }
  }
  _addr[0] = GenerateValue<Mult, uint32_t>(0)
    | GenerateValue<MaxPrimaryStreams, uint32_t>(0)
    | GenerateValue<Interval, uint32_t>(interval_);
  _addr[1] = GenerateValue<Cerr, uint32_t>(3)
    | GenerateValue<EndpointType, uint32_t>(ep_type)
    | GenerateValue<MaxBurstSize, uint32_t>(0)
    | GenerateValue<MaxPacketSize, uint32_t>(max_packet_size);
  _ring.Init(_device, _dci);
  phys_addr tr_ptr = _ring.GetMemory().GetPhysPtr();
  _addr[2] = kFlagDequequeCycleState
    | (tr_ptr & GenerateMask<TrDequeuePointer, uint32_t>());
  _addr[3] = tr_ptr >> 32;
  _addr[4] = 0;
  _addr[5] = 0;
  _addr[6] = 0;
  _addr[7] = 0;

  if (type != UsbCtrl::TransferType::kControl) {
    _ring.Fill(&_device->GetHc()->_mp, max_packet_size, buf);
  }
}

int DevXhci::Device::InputDeviceContext::Init(Device *device) {
  _device = device;
  const int context_size = _device->_hc->_context_size * 33;
  _mem = new Memory(context_size);
  uint32_t *addr = _mem->GetVirtPtr<uint32_t>();
  memset(addr, 0, context_size);

  _control_context.Init(addr + (0 * _device->_hc->_context_size) / sizeof(uint32_t));
  _dev_context._slot_context.InitInput(_device, addr + (1 * _device->_hc->_context_size) / sizeof(uint32_t));

  switch (device->GetPortSpeed()) {
  case UsbCtrl::PortSpeed::kLowSpeed: {
    _ed0_max_packet_size = 8;
    break;
  }
  case UsbCtrl::PortSpeed::kFullSpeed: {
    _ed0_max_packet_size = 64;
    break;
  }
  case UsbCtrl::PortSpeed::kHighSpeed: {
    _ed0_max_packet_size = 64;
    break;
  }
  case UsbCtrl::PortSpeed::kSuperSpeed: {
    _ed0_max_packet_size = 512;
    break;
  }
  case UsbCtrl::PortSpeed::kSuperSpeedPlus: {
    _ed0_max_packet_size = 512;
    break;
  }
  case UsbCtrl::PortSpeed::kUnknown: {
    return 1;
  }
  }

  _dev_context._in_endpoint_context[0].Init(_device, addr + (2 * _device->_hc->_context_size) / sizeof(uint32_t), 1, 0, UsbCtrl::TransferType::kControl, _ed0_max_packet_size, nullptr);

  return 0;
}

void DevXhci::BufferingNormalTrbHandler::Handle() {
  _ring->Handle(_index);
}

