#pragma once

#include "generic.h"
#include "mem.h"
#include "pci.h"
#include "usb.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include "hub.h"

class DevXhci : public DevUsbController {
public:
  void Init();
  void Run() {
    pthread_t tid;
    if (pthread_create(&tid, NULL, AttachAll, this) != 0) {
      perror("pthread_create:");
      exit(1);
    }
    while(true) {
      _pci.WaitInterrupt();
      pthread_mutex_lock(&_mp);
      _interrupter.Handle();
      pthread_mutex_unlock(&_mp);
    }
  }
  virtual bool SendControlTransfer(UsbCtrl::DeviceRequest &request, Memory &mem, size_t data_size, int device_addr) override {
    assert(_device_list[device_addr] != nullptr);
    return _device_list[device_addr]->SendControlTransfer(request, mem, data_size);
  }
  virtual void InitHub(int number_of_ports, int ttt, int device_addr) override {
    assert(_device_list[device_addr] != nullptr);
    return _device_list[device_addr]->InitHub(number_of_ports, ttt);
  }
  virtual DevUsb *AttachDevice(Hub *hub, int hub_addr, int hub_port_id) override;
  virtual ReturnState SetupEndpoint(uint8_t endpt_address, int device_addr, int interval, UsbCtrl::TransferType type, UsbCtrl::PacketIdentification direction, int max_packetsize, RingBuffer<uint8_t *> *buf) override {
    assert(_device_list[device_addr] != nullptr);
    return _device_list[device_addr]->SetupEndpoint(endpt_address, interval, type, direction, max_packetsize, buf);
  }
private:
  static const int kCapRegOffsetCapLength = 0x00;
  static const int kCapRegOffsetHciVersion = 0x02;
  
  static const int kCapReg32OffsetHcsParams1 = 0x04 / sizeof(uint32_t);
  static const int kCapReg32OffsetHcsParams2 = 0x08 / sizeof(uint32_t);
  static const int kCapReg32OffsetHccParams1 = 0x10 / sizeof(uint32_t);
  static const int kCapReg32OffsetDboff = 0x14 / sizeof(uint32_t);
  static const int kCapReg32OffsetRtsoff = 0x18 / sizeof(uint32_t);
  
  static const int kOpRegOffsetUsbCmd = 0x00 / sizeof(uint32_t);
  static const int kOpRegOffsetUsbSts = 0x04 / sizeof(uint32_t);
  static const int kOpRegOffsetPageSize = 0x08 / sizeof(uint32_t);
  static const int kOpRegOffsetCrcr = 0x18 / sizeof(uint32_t);
  static const int kOpRegOffsetDcbaap = 0x30 / sizeof(uint32_t);
  static const int kOpRegOffsetConfig = 0x38 / sizeof(uint32_t);
  static const int kOpRegOffsetPortsc = 0x400 / sizeof(uint32_t);

  static const int kRunRegIntRegSet = 0x20 / sizeof(uint32_t);

  // Table 23: Host Controller Structural Parameters 1 (HCSPARAMS1)
  struct CapReg32HcsParams1MaxSlots {
    static const int kOffset = 0;
    static const int kLen = 8;
  };
  struct CapReg32HcsParams1MaxPorts {
    static const int kOffset = 24;
    static const int kLen = 8;
  };

  // Table 24: Host Controller Structural Parameters 2 (HCSPARAMS2)
  struct CapReg32HcsParams2MaxScratchpadHi {
    static const int kOffset = 21;
    static const int kLen = 5;
  };
  struct CapReg32HcsParams2MaxScratchpadLow {
    static const int kOffset = 27;
    static const int kLen = 5;
  };

  // Table 26: Host Controller Capability 1 Parameters (HCCPARAMS1)
  static const uint32_t kCapReg32HccParams1FlagContextSize = 1 << 2;
  static const uint32_t kCapReg32HccParams1FlagPortPowerControl = 1 << 3;
  struct CapReg32HccParams1Xecp {
    static const int kOffset = 16;
    static const int kLen = 16;
  };

  // Table 27: Doorbell Offset Register (DBOFF)
  struct CapReg32DboffDoorbellArrayOffset {
    static const int kOffset = 2;
    static const int kLen = 31 - 2 + 1;
  };

  // Table 28: Runtime Register Space Offset Register (RTSOFF)
  struct CapReg32TrsoffRuntimeSpaceOffset {
    static const int kOffset = 5;
    static const int kLen = 31 - 5 + 1;
  };
  
  // Table 32: USB Command Register Bit Definitions (USBCMD)
  static const uint32_t kOpRegUsbCmdFlagRunStop = 1 << 0;
  static const uint32_t kOpRegUsbCmdFlagReset = 1 << 1;
  static const uint32_t kOpRegUsbCmdFlagInterrupterEnable = 1 << 2;

  // Table 33: USB Status Register Bit Definitions (USBSTS)
  static const uint32_t kOpRegUsbStsFlagHchalted = 1 << 0;
  static const uint32_t kOpRegUsbStsFlagControllerNotReady = 1 << 11;

  // Table 34: Page Size Register Bit Definitions (PAGESIZE)
  static const uint32_t kOpRegPageSizeMask = (1 << 16) - 1;

  // Table 36: Command Ring Control Register Bit Definitions (CRCR)
  static const uint32_t kOpRegCrcrMaskCommandRingPointer = ~0 - ((1 << 6) - 1);
  static const uint32_t kOpRegCrcrFlagRingCycleStatus = 1 << 0;

  // Table 38: Configure Register Bit Definitions(CONFIG)
  struct OpRegConfigMaxSlotsEn {
    static const int kOffset = 0;
    static const int kLen = 8;
  };

  // Table 39: Port Status and Control Register Bit Definitions (PORTSC)
  static const uint32_t kOpRegPortscFlagCcs = 1 << 0;
  static const uint32_t kOpRegPortscFlagPortEnabled = 1 << 1;
  static const uint32_t kOpRegPortscFlagPortReset = 1 << 4;
  static const uint32_t kOpRegPortscFlagCsc = 1 << 17;
  static const uint32_t kOpRegPortscFlagPec = 1 << 18;
  static const uint32_t kOpRegPortscFlagWrc = 1 << 19;
  static const uint32_t kOpRegPortscFlagOcc = 1 << 20;
  static const uint32_t kOpRegPortscFlagPrc = 1 << 21;
  static const uint32_t kOpRegPortscFlagPlc = 1 << 22;
  static const uint32_t kOpRegPortscFlagCec = 1 << 23;
  static const uint32_t kOpRegPortscFlagsRwcBits = kOpRegPortscFlagCsc | kOpRegPortscFlagPec | kOpRegPortscFlagWrc | kOpRegPortscFlagOcc | kOpRegPortscFlagPrc | kOpRegPortscFlagPlc | kOpRegPortscFlagPortEnabled;
  struct OpRegPortscPortSpeed {
    static const int kOffset = 10;
    static const int kLen = 13 - 10 + 1;
  };

  // Table 53: Doorbell Register Bit Field Definitions
  struct DoorbellRegDbTarget {
    static const int kOffset = 0;
    static const int kLen = 8;
  };
  struct DoorbellRegDbStreamId {
    static const int kOffset = 16;
    static const int kLen = 16;
  };

  // Table 145: Format of xHCI Extended Capability Pointer Register
  static const int kExtCapRegOffsetCapabilityId = 0;
  static const int kExtCapRegLenCapabilityId = 0b11111111;
  static const int kExtCapRegOffsetNextPointer = 8;
  static const int kExtCapRegLenNextPointer = 8;

  // Table 146: xHCI Extended Capability Codes
  static const uint8_t kExCapCodeSupportedProtocol = 2;

  // Table 152: xHCI Supported Protocol Capability Field Definitions
  static const int kSupportedProtocolCap8OffsetCompatiblePortOffset = 0;
  static const int kSupportedProtocolCap8LenCompatiblePortOffset = 8;
  static const int kSupportedProtocolCap8OffsetCompatiblePortCount = 8;
  static const int kSupportedProtocolCap8LenCompatiblePortCount = 8;

  // Table 153: xHCI Supported Protocol Capability Field Definitions
  static const int kSupportedProtocolCapCOffsetProtocolSlotType = 0;
  static const int kSupportedProtocolCapCLenProtocolSlotType = 4;

  volatile uint8_t *_capreg_base_addr;
  volatile uint32_t *_capreg_base_addr32;
  volatile uint32_t *_opreg_base_addr;
  volatile uint32_t *_excapreg_base_addr;
  volatile uint32_t *_doorbell_array_base_addr;
  volatile uint32_t *_runtime_base_addr;

  class Device;
  class TrbRingBase {
  public:
    class Trb {
    public:
      enum class Direction : bool
        {
          kOut = false,
          kIn = true,
        };
      virtual void Set(uint32_t *addr, bool cycle_flag) = 0;
      static bool GetCycleBit(uint32_t *addr) {
        return ((addr[3] & kFlagCycleBit) != 0);
      }
      static void ToggleCycleBit(uint32_t *addr) {
        if (GetCycleBit(addr)) {
          addr[3] &= ~kFlagCycleBit;
        } else {
          addr[3] |= kFlagCycleBit;
        }
      }
    protected:
      void SetSub(uint32_t *addr, uint32_t type, bool cycle_flag) {
        addr[3]
          |= GenerateValue<TrbType, uint32_t>(type)
          | (cycle_flag ? kFlagCycleBit : 0);
      }
      // Table 75: Offset 0Ch – Normal TRB Field Definitions
      static const uint32_t kFlagCycleBit = 1 << 0;
      struct TrbType {
        static const int kOffset = 10;
        static const int kLen = 6;
      };
    };
  protected:
    void InitSub(DevXhci *hc) {
      _hc = hc;
    }

    DevXhci *_hc;
  };
  
  class TrbHandler {
  public:
    int index;
    int handle_index;
    bool cycle_flag;
    virtual void Handle() = 0;
  };

  class BlockingTrbHandler : public TrbHandler {
  public:
    BlockingTrbHandler() {
      pthread_cond_init(&_cond, NULL);
    }
    virtual void Handle() override {
      pthread_cond_broadcast(&_cond);
    }
    void Wait(pthread_mutex_t *mutex) {
      pthread_cond_wait(&_cond, mutex);
    }
    ~BlockingTrbHandler() {
      pthread_cond_destroy(&_cond);
    }
  private:
    pthread_cond_t _cond;
  };

  class InTransferRing;
  class BufferingNormalTrbHandler : public TrbHandler {
  public:
    BufferingNormalTrbHandler(InTransferRing *ring) : _ring(ring) {
    }
    void SetIndexOfRing(int index) {
      _index = index;
    }
    virtual void Handle() override;
  private:
    InTransferRing *_ring;
    int _index;
  };
  
  class DummyTrbHandler : public TrbHandler {
  public:
    DummyTrbHandler(BlockingTrbHandler *bhandler) : _bhandler(bhandler) {
    }
    virtual void Handle() override {
      _bhandler->Handle();
    }
  private:
    BlockingTrbHandler *_bhandler;
  };
    
  class TrbRing : public TrbRingBase {
  public:
    void Init(DevXhci *hc) {
      InitSub(hc);
      _mem = new Memory(kEntrySize * kEntryNum);
      _ring_address = _mem->GetVirtPtr<uint32_t>();
      _enqueue_index = 0;
      _cycle_flag = true;
      pthread_cond_init(&_cond, NULL);
      for (int i = 0; i < sizeof(_context) / sizeof(_context[0]); i++) {
        _context[i].status = ContextStatus::kOwnedBySoftware;
      }

      memset(_ring_address, 0, kEntrySize * kEntryNum);
      
      // set link TRB
      LinkTrb trb(_mem->GetPhysPtr());
      trb.Set(_ring_address + (kEntryNum - 1) * (kEntrySize / sizeof(uint32_t)), true);
    }
    Memory &GetMemory() {
      return *_mem;
    }
    int GetIndexFromEntryAddr(phys_addr addr) {
      assert(_mem->GetPhysPtr() <= addr);
      int index = (addr - _mem->GetPhysPtr()) / kEntrySize;
      assert(index < kEntryNum);
      return index;
    }
    static const int kEntrySize = 16;
    static const int kEntryNum = 256;
  protected:
    class LinkTrb : public Trb {
    public:
      LinkTrb(phys_addr ring_address) : _ring_address(ring_address) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = _ring_address;
        addr[1] = _ring_address >> 32;
        addr[2] = 0;
        addr[3] = kLinkTrbFlagToggleCycle;
        SetSub(addr, kValueTrbType, cycle_flag);
      }
      static void ToggleCycle(uint32_t *addr) {
      }
    private:
      // Table 134: Offset 0Ch – Link TRB Field Definitions
      static const uint32_t kLinkTrbFlagToggleCycle = 1 << 1;
      
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 6;

      const phys_addr _ring_address;
    };
    
    void AllocTrb(TrbHandler &handler, pthread_mutex_t *mutex) {
      while(_context[_enqueue_index].status == ContextStatus::kOwnedByHardware) {
        if (pthread_cond_wait(&_cond, mutex) < 0) {
          perror("pthread_cond_wait:");
        }
      }
      handler.index = _enqueue_index;
      handler.cycle_flag = _cycle_flag;
      _context[_enqueue_index].handler = &handler;
      _context[_enqueue_index].status = ContextStatus::kOwnedByHardware;
      _enqueue_index++;
      if (_enqueue_index == kEntryNum - 1) {
        _enqueue_index = 0;
        _cycle_flag = !_cycle_flag;
        LinkTrb trb(0); // dummy
        assert(_cycle_flag != trb.GetCycleBit(_ring_address + (kEntryNum - 1) * (kEntrySize / sizeof(uint32_t))));
        trb.ToggleCycleBit(_ring_address + (kEntryNum - 1) * (kEntrySize / sizeof(uint32_t)));
	uint32_t *tmp = _ring_address + (kEntryNum - 1) * (kEntrySize / sizeof(uint32_t));
      }
      return;
    }
    
    // release trb from consumer
    TrbHandler *ReleaseTrb(int index) {
      assert(index < kEntryNum);
      TrbContext *context = &_context[index];
      assert(context->status == ContextStatus::kOwnedByHardware);
      if (pthread_cond_signal(&_cond) < 0) {
        perror("pthread_cond_signal:");
      }
      context->handler->handle_index = index;
      context->status = ContextStatus::kOwnedBySoftware;
      context->handler->Handle();
      return context->handler;
    }

    uint32_t *_ring_address;
  private:
    enum class ContextStatus : bool
      {
        kOwnedByHardware,
        kOwnedBySoftware,
      };
    struct TrbContext {
      ContextStatus status;
      TrbHandler *handler;
    };

    Memory *_mem;
    int _enqueue_index;
    bool _cycle_flag;
    TrbContext _context[kEntryNum];
    pthread_cond_t _cond;
  };

  enum class TrbCompletionCode : uint8_t
    {
      kSuccess = 1,
    };
  static const char* const _completion_code_table[];
  static const char * const GetString(TrbCompletionCode code) {
    return _completion_code_table[static_cast<uint8_t>(code)];
  }

  class TransferRing : public TrbRing {
  public:
    struct CompletionInfo {
      int transfer_length;
      TrbCompletionCode completion_code;
      bool event_data;
      uint8_t endpoint_id;
      uint8_t slot_id;
    };
    class TransferTrb : public Trb {
    public:
      TransferTrb(bool chain, bool ioc, bool idt) : _chain(chain), _ioc(ioc), _idt(idt) {
      }
      bool GetIoc() {
        return _ioc;
      }
      void SetSub(uint32_t *addr, uint32_t type, bool cycle_flag) {
        addr[3]
          |= (_chain ? kFlagChainBit : 0)
          | (_ioc ? kFlagInterruptOnComplete : 0)
          | (_idt ? kFlagImmediateData : 0);
        Trb::SetSub(addr, type, cycle_flag);
      }
    public:
      // Table 79: Offset 0Ch – Setup Stage TRB Field Definitions
      static const int kFlagChainBit = 1 << 4;
      static const int kFlagInterruptOnComplete = 1 << 5;
      static const int kFlagImmediateData = 1 << 6;
      
      const bool _chain;
      const bool _ioc;
      const bool _idt;
    };

    class NormalTrb : public TransferTrb {
    public:
      NormalTrb() = delete;
      NormalTrb(phys_addr addr, int transfer_len, bool ioc, bool idt) : TransferTrb(false, ioc, idt), _addr(addr), _transfer_len(transfer_len) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = _addr;
        addr[1] = _addr >> 32;
        addr[2]
          = GenerateValue<TransferLength, uint32_t>(_transfer_len)
          | GenerateValue<TdSize, uint32_t>(0)
          | GenerateValue<InterruptTarget, uint32_t>(0);
        addr[3] = 0;
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 1;

      // Table 74: Offset 08h – Normal TRB Field Definitions Bits
      struct TransferLength {
        static const int kOffset = 0;
        static const int kLen = 16;
      };
      struct TdSize {
        static const int kOffset = 17;
        static const int kLen = 21 - 17 + 1;
      };
      struct InterruptTarget {
        static const int kOffset = 22;
        static const int kLen = 10;
      };

      const phys_addr _addr;
      const int _transfer_len;
    };

    class SetupStageTrb : public TransferTrb {
    public:
      enum class ValueTransferType : uint8_t
        {
          kNoDataStage = 0,
          kOutDataStage = 2,
          kInDataStage = 3,
        };
      SetupStageTrb() = delete;
      SetupStageTrb(ValueTransferType type, bool ioc, bool idt, UsbCtrl::DeviceRequest &request) : TransferTrb(false, ioc, idt), _request(request), _type(type) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        memcpy(addr, &_request, sizeof(UsbCtrl::DeviceRequest));
        addr[2]
          = GenerateValue<TrbTransferLength, uint32_t>(8)
          | GenerateValue<InterruptTarget, uint32_t>(0);
        addr[3]
          = GenerateValue<TransferType, uint32_t>(static_cast<uint8_t>(_type));
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 2;
      
      // Table 78: Offset 08h – Setup Stage TRB Field Definitions
      struct TrbTransferLength {
        static const int kOffset = 0;
        static const int kLen = 16;
      };
      struct InterruptTarget {
        static const int kOffset = 22;
        static const int kLen = 10;
      };

      // Table 79: Offset 0Ch – Setup Stage TRB Field Definitions
      struct TransferType {
        static const int kOffset = 16;
        static const int kLen = 2;
      };

      const ValueTransferType _type;
      const UsbCtrl::DeviceRequest _request;
    };
    
    class DataStageTrb : public TransferTrb {
    public:
      DataStageTrb() = delete;
      DataStageTrb(Direction dir, int transfer_len, bool chain, bool ioc, bool idt, phys_addr buf) : TransferTrb(chain, ioc, idt), _dir(dir), _transfer_len(transfer_len), _buf(buf) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = _buf;
        addr[1] = _buf >> 32;
        addr[2]
          = GenerateValue<TransferLength, uint32_t>(_transfer_len)
          | GenerateValue<TdSize, uint32_t>(0)
          | GenerateValue<InterruptTarget, uint32_t>(0);
        addr[3]
          = (static_cast<bool>(_dir) ? kFlagDirection : 0);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 3;

      // Table 81: Offset 08h – Data Stage TRB Field Definitions
      struct TransferLength {
        static const int kOffset = 0;
        static const int kLen = 17;
      };
      struct TdSize {
        static const int kOffset = 17;
        static const int kLen = 21 - 17 + 1;
      };
      struct InterruptTarget {
        static const int kOffset = 22;
        static const int kLen = 31 - 22 + 1;
      };

      // Table 82: Offset 0Ch – Data Stage TRB Field Definitions
      static const int kFlagEvaluateNextTrb = 1 << 1;
      static const int kFlagInterruptOnShortPacket = 1 << 2;
      static const int kFlagNoSnoop = 1 << 3;
      static const int kFlagDirection = 1 << 16;
    
      const Direction _dir;
      const int _transfer_len;
      const phys_addr _buf;
    };
    class StatusStageTrb : public TransferTrb {
    public:
      StatusStageTrb() = delete;
      StatusStageTrb(Direction dir, bool chain, bool ioc, bool idt) : TransferTrb(chain, ioc, idt), _dir(dir) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = 0;
        addr[1] = 0;
        addr[2]
          = GenerateValue<InterruptTarget, uint32_t>(0);
        addr[3]
          = (static_cast<bool>(_dir) ? kFlagDirection : 0);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 4;
      
      // Table 83: Offset 08h – Status Stage TRB Field Definitions
      struct InterruptTarget {
        static const int kOffset = 22;
        static const int kLen = 31 - 22 + 1;
      };

      // Table 84: Offset 0Ch – Status Stage TRB Field Definitions
      static const int kFlagDirection = 1 << 16;
      
      const Direction _dir;
    };
    void Init(DevXhci *hc) = delete;
    void Init(Device *device, int dci) {
      _device = device;
      _dci = dci;
      TrbRing::Init(device->GetHc());
    }
    void CompleteTransfer(int index, CompletionInfo &info) {
      _info[index] = info;
      ReleaseTrb(index);
    }
    // insert TRBs to the ring. get state from completion event.
    CompletionInfo Issue(TransferTrb *trb[], const int array_len, pthread_mutex_t *mutex) {
      BlockingTrbHandler bhandler;

      for(int i = 0; i < array_len - 1; i++) {
        assert(!trb[i]->GetIoc());
        DummyTrbHandler handler(&bhandler);
        AllocTrb(handler, mutex);

        trb[i]->Set(_ring_address + handler.index * (kEntrySize / sizeof(uint32_t)), handler.cycle_flag);
      }
      
      assert(trb[array_len - 1]->GetIoc());
      
      AllocTrb(bhandler, mutex);

      trb[array_len - 1]->Set(_ring_address + bhandler.index * (kEntrySize / sizeof(uint32_t)), bhandler.cycle_flag);
      _device->RingEndpointDoorbell(_dci);
      bhandler.Wait(mutex);
      return _info[bhandler.handle_index];
    }
  protected:
    CompletionInfo _info[kEntryNum];
    Device *_device;
    int _dci;
  };
  
  class OutTransferRing : public TransferRing {
  public:
  };

  class InTransferRing : public TransferRing {
  public:
    ~InTransferRing() {
      delete _mem;
    }
    void Fill(pthread_mutex_t *mutex, int max_packet_size, RingBuffer<uint8_t *> *buf) {
      _mutex = mutex;
      _max_packet_size = max_packet_size;
      _buf = buf;
      _mem = new Memory(max_packet_size * (kEntryNum - 1));
      for (int i = 0; i < kEntryNum - 1; i++) {
        TransferRing::NormalTrb trb(_mem->GetPhysPtr() + i * max_packet_size, max_packet_size, true, false);
        _handlers[i] = new BufferingNormalTrbHandler(this);
        _handlers[i]->SetIndexOfRing(i);
        
        AllocTrb(*_handlers[i], mutex);
	assert(i == _handlers[i]->index);

        trb.Set(_ring_address + _handlers[i]->index * (kEntrySize / sizeof(uint32_t)), _handlers[i]->cycle_flag);
      }
    }
    void Handle(int index) {
      uint8_t *data = new uint8_t[_max_packet_size];
      memcpy(data, _mem->GetVirtPtr<uint8_t>() + index * _max_packet_size, _max_packet_size);
      _buf->Push(data);

      TransferRing::NormalTrb trb(_mem->GetPhysPtr() + index * _max_packet_size, _max_packet_size, true, false);

      AllocTrb(*_handlers[index], _mutex);

      trb.Set(_ring_address + _handlers[index]->index * (kEntrySize / sizeof(uint32_t)), _handlers[index]->cycle_flag);
    }
  private:
    Memory *_mem = nullptr;
    BufferingNormalTrbHandler *_handlers[kEntryNum - 1];
    RingBuffer<uint8_t *> *_buf;
    int _max_packet_size;
    pthread_mutex_t *_mutex;
  };

  class CommandRing : public TrbRing {
  public:
    struct CompletionInfo {
      TrbCompletionCode completion_code;
      uint32_t completion_parameter;
      uint8_t slot_id;
    };
    class EnableSlotCommandTrb : public Trb {
    public:
      EnableSlotCommandTrb() = delete;
      EnableSlotCommandTrb(uint32_t slot_type) : _slot_type(slot_type) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = 0;
        addr[1] = 0;
        addr[2] = 0;
        addr[3]
          = GenerateValue<SlotType, uint32_t>(_slot_type);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:    
      // Table 112: Offset 0Ch – Enable Slot Command TRB Field Definitions
      struct SlotType {
        static const int kOffset = 16;
        static const int kLen = 5;
      };

      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 9;

      const uint32_t _slot_type;
    };
    class DisableSlotCommandTrb : public Trb {
    public:
      DisableSlotCommandTrb() = delete;
      DisableSlotCommandTrb(uint8_t slot_id) : _slot_id(slot_id) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = 0;
        addr[1] = 0;
        addr[2] = 0;
        addr[3]
          = GenerateValue<SlotId, uint32_t>(_slot_id);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:    
      // Table 113: Offset 0Ch – Disable Slot Command TRB Field Definitions
      struct SlotId {
        static const int kOffset = 24;
        static const int kLen = 31 - 24 + 1;
      };

      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 10;

      const uint8_t _slot_id;
    };
    class AddressDeviceCommandTrb : public Trb {
    public:
      AddressDeviceCommandTrb() = delete;
      AddressDeviceCommandTrb(phys_addr input_context, uint8_t slot_id, bool bsr) : _input_context(input_context), _slot_id(slot_id), _bsr(bsr) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = _input_context;
        addr[1] = _input_context >> 32;
        addr[2] = 0;
        addr[3]
          = GenerateValue<SlotId, uint32_t>(_slot_id)
          | (_bsr ? kFlagBlockSetAddressRequest : 0);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      
      // Table 115: Offset 0Ch – Address Device Command TRB Field Definitions
      struct SlotId {
        static const int kOffset = 24;
        static const int kLen = 8;
      };
      static const int kFlagBlockSetAddressRequest = 1 << 9;

      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 11;

      const phys_addr _input_context;
      const uint8_t _slot_id;
      const bool _bsr;
    };
    class ConfigureEndpointCommandTrb : public Trb {
    public:
      ConfigureEndpointCommandTrb() = delete;
      ConfigureEndpointCommandTrb(phys_addr input_context, uint8_t slot_id, bool deconfigure) : _input_context(input_context), _slot_id(slot_id), _deconfigure(deconfigure) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = _input_context;
        addr[1] = _input_context >> 32;
        addr[2] = 0;
        addr[3]
          = (_deconfigure ? kFlagDeconfigure : 0)
          | GenerateValue<SlotId, uint32_t>(_slot_id);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      // Table 117: Offset 0Ch – Configure Endpoint Command TRB Field Definitions
      static const uint32_t kFlagDeconfigure = 1 << 9;
      struct SlotId {
        static const int kOffset = 24;
        static const int kLen = 8;
      };

      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 12;

      const phys_addr _input_context;
      const uint8_t _slot_id;
      const bool _deconfigure;
    };
    class EvaluateContextCommandTrb : public Trb {
    public:
      EvaluateContextCommandTrb() = delete;
      EvaluateContextCommandTrb(phys_addr input_context, uint8_t slot_id) : _input_context(input_context), _slot_id(slot_id) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = _input_context;
        addr[1] = _input_context >> 32;
        addr[2] = 0;
        addr[3]
          = GenerateValue<SlotId, uint32_t>(_slot_id);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      // note: The Evaluate Context Command TRB uses the same format as the Address Device Command TRB
      
      // Table 115: Offset 0Ch – Address Device Command TRB Field Definitions
      struct SlotId {
        static const int kOffset = 24;
        static const int kLen = 8;
      };

      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 13;

      const phys_addr _input_context;
      const uint8_t _slot_id;
    };
    class ResetDeviceCommandTrb : public Trb {
    public:
      ResetDeviceCommandTrb() = delete;
      ResetDeviceCommandTrb(uint8_t slot_id) : _slot_id(slot_id) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        addr[0] = 0;
        addr[1] = 0;
        addr[2] = 0;
        addr[3]
          = GenerateValue<SlotId, uint32_t>(_slot_id);
        SetSub(addr, kValueTrbType, cycle_flag);
      }
    private:
      
      // Table 123: Offset 0Ch – Address Device Command TRB Field Definitions
      struct SlotId {
        static const int kOffset = 24;
        static const int kLen = 8;
      };

      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 17;

      const uint8_t _slot_id;
    };
    CompletionInfo Issue(Trb &trb, pthread_mutex_t *mutex) {
      BlockingTrbHandler handler;
      AllocTrb(handler, mutex);

      memset(_ring_address + handler.index * (kEntrySize / sizeof(uint32_t)), 0, kEntrySize);
      trb.Set(_ring_address + handler.index * (kEntrySize / sizeof(uint32_t)), handler.cycle_flag);
      _hc->RingCommandDoorbell();
      handler.Wait(mutex);
      return _completion_info[handler.handle_index];
    }
    void CompleteCommand(int index, CompletionInfo &completion_info) {
      _completion_info[index] = completion_info;
      ReleaseTrb(index);
    }
  private:    
    CompletionInfo _completion_info[kEntryNum];
  };

  struct ContainerForPortStatusChangeHandler {
    DevXhci *that;
    int root_port_id;
  };

  class EventRing : public TrbRingBase {
  public:
    void Init(DevXhci *hc) {
      InitSub(hc);
      _mem = new Memory(kEntrySize * kEntryNum);
      memset(_mem->GetVirtPtr<uint8_t>(), 0, kEntrySize * kEntryNum);
      _consumer_cycle_bit = true;
    }
    Memory &GetMemory() {
      return *_mem;
    }
    int GetEntryNum() {
      return kEntryNum;
    }
    // return value: dequeue_ptr is incremented or not
    bool Handle(phys_addr &dequeue_ptr);
  private:
    class EventTrb : public Trb {
    public:
      EventTrb() = delete;
      EventTrb(uint32_t *addr) : _addr(addr) {
      }
      virtual void Set(uint32_t *addr, bool cycle_flag) override {
        assert(false);
      }
      bool GetCycleBit() {
        return (_addr[3] & kFlagCycleBit) != 0;
      }
      uint32_t GetType() {
        return MaskValue<TrbType>(_addr[3]);
      }
      void ShowErrUnknown() {
        printf("warning: unknown event trb: %d\n", MaskValue<TrbType>(_addr[3]));
        fflush(stdout);
      }
    protected:
      uint32_t *_addr;
    };

    class TransferEventTrb : public EventTrb {
    public:
      TransferEventTrb() = delete;
      TransferEventTrb(uint32_t *addr) : EventTrb(addr) {
      }

      void SetContainer(TransferRing::CompletionInfo &info, phys_addr &pointer) {
        pointer = _addr[1];
        pointer = (pointer << 32)| _addr[0];
        info.transfer_length = MaskValue<TransferLength>(_addr[2]);
        info.completion_code = static_cast<TrbCompletionCode>(MaskValue<CompletionCode>(_addr[2]));
        info.event_data = (_addr[3] & kFlagEventData) ? true : false;
        info.endpoint_id = MaskValue<EndpointId>(_addr[3]);
        info.slot_id = MaskValue<SlotId>(_addr[3]);
      }
      
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 32;
    private:
      // Table 91: Offset 08h – Transfer Event TRB Field Definitions
      struct TransferLength {
        static const int kOffset = 0;
        static const int kLen = 23 - 0 + 1;
      };
      struct CompletionCode {
        static const int kOffset = 24;
        static const int kLen = 31 - 24 + 1;
      };

      // Table 92: Offset 0Ch – Transfer Event TRB Field Definitions
      static const uint32_t kFlagEventData = 1 << 2;
      struct EndpointId {
        static const int kOffset = 16;
        static const int kLen = 20 - 16 + 1;
      };
      struct SlotId {
        static const int kOffset = 24;
        static const int kLen = 31 - 24 + 1;
      };
    };
    
    class CommandCompletionEventTrb : public EventTrb {
    public:
      CommandCompletionEventTrb() = delete;
      CommandCompletionEventTrb(uint32_t *addr) : EventTrb(addr) {
      }

      void SetContainer(CommandRing::CompletionInfo &info, phys_addr &pointer) {
        pointer = _addr[1];
        pointer = (pointer << 32)| _addr[0];
        pointer &= GenerateMask<CommandTrbPointer, uint64_t>();
        info.completion_code = static_cast<TrbCompletionCode>(MaskValue<CompletionCode>(_addr[2]));
        info.completion_parameter = MaskValue<Parameter>(_addr[2]);
        info.slot_id = MaskValue<SlotId>(_addr[3]);
      }
      
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 33;
    private:
      // Table 93: Offset 00h and 04h – Command Completion Event TRB Field Definition
      struct CommandTrbPointer {
        static const int kOffset = 4;
        static const int kLen = 63 - 4 + 1;
      };
    
      // Table 94: Offset 08h – Command Completion Event TRB Field Definitions
      struct Parameter {
        static const int kOffset = 0;
        static const int kLen = 23 - 0 + 1;
      };
      struct CompletionCode {
        static const int kOffset = 24;
        static const int kLen = 31 - 24 + 1;
      };

      // Table 95: Offset 0Ch – Command Completion Event TRB Field Definitions Bits
      struct SlotId {
        static const int kOffset = 24;
        static const int kLen = 31 - 24 + 1;
      };
    };
    
    class PortStatusChangeEventTrb : public EventTrb {
    public:
      PortStatusChangeEventTrb() = delete;
      PortStatusChangeEventTrb(uint32_t *addr) : EventTrb(addr) {
      }
      void SetContainer(ContainerForPortStatusChangeHandler &container) {
        container.root_port_id = MaskValue<PortId>(_addr[0]);
      }
      
      // Table 139: TRB Type Definitions
      static const uint32_t kValueTrbType = 34;
    private:
      // Table 96: Offset 00h – Port Status Change Event TRB Field Definitions
      struct PortId {
        static const int kOffset = 24;
        static const int kLen = 31 - 24 + 1;
      };
    };
    
    Memory *_mem;
    static const int kEntrySize = 16;
    static const int kEntryNum = 256;
    bool _consumer_cycle_bit;
  };

  class EventRingSegmentTable {
  public:
    void Init(EventRing *event_ring) {
      _mem = new Memory(16); // only 1 entry
      uint32_t *ptr = _mem->GetVirtPtr<uint32_t>();
      phys_addr event_ring_addr = event_ring->GetMemory().GetPhysPtr();
      ptr[0] = event_ring_addr;
      ptr[1] = event_ring_addr >> 32;
      ptr[2] = event_ring->GetEntryNum();
      _event_ring = event_ring;
    }
    int GetSize() {
      return 1;
    }
    Memory &GetMemory() {
      return *_mem;
    }
    // return value: dequeue_ptr is incremented or not
    bool Handle(phys_addr &dequeue_ptr) {
      return _event_ring->Handle(dequeue_ptr);
    }
  private:
    Memory *_mem;
    EventRing *_event_ring = nullptr;
  };

  class Interrupter {
  public:
    void Init(volatile uint32_t *base_addr, EventRingSegmentTable *erst, EventRing *event_ring);
    void Handle() {
      if (IsFlagClear(_base_addr[kRegOffsetIman], kImanRegFlagPending)) {
        return;
      }

      _base_addr[0] |= kImanRegFlagPending;

      if (_erst->Handle(_dequeue_ptr)) {
        WriteDequeuePtr();
      } else {
        assert(IsFlagClear(_base_addr[kRegOffsetErdp], kErdpRegFlagEventHandlerBusy));
      }
    }
  private:
    static const int kRegOffsetIman = 0x0 / sizeof(uint32_t);
    static const int kRegOffsetImod = 0x4 / sizeof(uint32_t);
    static const int kRegOffsetErstsz = 0x8 / sizeof(uint32_t);
    static const int kRegOffsetErstba = 0x10 / sizeof(uint32_t);
    static const int kRegOffsetErdp = 0x18 / sizeof(uint32_t);

    // Table 48: Interrupter Management Register Bit Definitions (IMAN)
    static const uint32_t kImanRegFlagPending = 1 << 0;
    static const uint32_t kImanRegFlagEnable = 1 << 1;

    // Table 49: Interrupter Moderation Register (IMOD)
    struct ImodRegModerationInterval {
      static const int kOffset = 0;
      static const int kLen = 16;
    };
    struct ImodRegModerationCounter {
      static const int kOffset = 16;
      static const int kLen = 16;
    };
      
    // Table 50: Event Ring Segment Table Size Register Bit Definitions (ERSTSZ)
    struct ErstszRegEventRingSegmentTableSize {
      static const int kOffset = 0;
      static const int kLen = 16;
    };

    // Table 51: Event Ring Segment Table Base Address Register Bit Definitions (ERSTBA)
    struct ErstbaEventRingSegmentTableBaseAddress {
      static const int kOffset = 6;
      static const int kLen = 31 - 6 + 1;
    };

    // Table 52: Event Ring Dequeue Pointer Register Bit Definitions (ERDP)
    static const uint32_t kErdpRegFlagEventHandlerBusy = 1 << 3;
    struct ErdpRegEventRingDequeuePointer {
      static const int kOffset = 4;
      static const int kLen = 31 - 4 + 1;
    };
    
    void WriteDequeuePtr() {
      _base_addr[kRegOffsetErdp + 0]
        = (_dequeue_ptr & GenerateMask<ErdpRegEventRingDequeuePointer, uint32_t>())
        | kErdpRegFlagEventHandlerBusy;
      _base_addr[kRegOffsetErdp + 1] = _dequeue_ptr >> 32;
    }
    EventRingSegmentTable *_erst;
    phys_addr _dequeue_ptr;
    volatile uint32_t *_base_addr;
  };

  class Device {
  public:
    Device() = delete;
    Device(DevXhci *hc, const int root_port_id) : _hc(hc), _root_port_id(root_port_id) {
    }
    
    DevUsb *Init();

    void Release();

    DevXhci *GetHc() {
      return _hc;
    }
    
    void CompleteTransfer(phys_addr pointer, TransferRing::CompletionInfo &completion_info) {
      _input_context.CompleteTransfer(pointer, completion_info);
    }
   
    bool SendControlTransfer(UsbCtrl::DeviceRequest &request, Memory &mem, size_t data_size);
    ReturnState SetupEndpoint(uint8_t endpt_address, int interval, UsbCtrl::TransferType type, UsbCtrl::PacketIdentification direction, int max_packetsize, RingBuffer<uint8_t *> *buf) {
      RETURN_IF_ERR(_input_context.SetupEndpoint(endpt_address, interval, type, direction, max_packetsize, buf));
      do {
        CommandRing::ConfigureEndpointCommandTrb com(_input_context.GetPhysAddr(), _slot_id, false);
        CommandRing::CompletionInfo info = _hc->_command_ring.Issue(com, &_hc->_mp);
        if (info.completion_code != TrbCompletionCode::kSuccess) {
          return ReturnState::kErrUnknown;
        }
      } while(0);
      _input_context.RingEndpointDoorbell(endpt_address, direction);
      return ReturnState::kSuccess;
    }
    void InitHub(int number_of_ports, int ttt);
    void RegisterDevUsb(DevUsb *device) {
      _dev_usb = device;
    }
    void UnRegisterDevUsb() {
      if (_dev_usb != nullptr) {
        _dev_usb->Release();
      }
    }
    DevUsb *GetPointerOfDevUsb() {
      return _dev_usb;
    }
    int GetRootPortId() {
      return _root_port_id;
    }
    uint32_t GetRouteString() {
      return _route_string;
    }
    int GetSlotId() {
      return _slot_id;
    }
    void RingEndpointDoorbell(uint8_t target) {
      _hc->RingEndpointDoorbell(_slot_id, target);
    }
  protected:

    class DeviceContext {
    public:
      class SlotContext {
      public:
        void InitInput(Device *device, uint32_t *addr) {
          _addr = addr;
          int speed_value = 0;
          // Table 157: Default USB Speed ID Mapping
          switch(device->GetPortSpeed()) {
          case UsbCtrl::PortSpeed::kFullSpeed:
            speed_value = 1;
            break;
          case UsbCtrl::PortSpeed::kLowSpeed:
            speed_value = 2;
            break;
          case UsbCtrl::PortSpeed::kHighSpeed:
            speed_value = 3;
            break;
          case UsbCtrl::PortSpeed::kSuperSpeed:
            speed_value = 4;
            break;
          case UsbCtrl::PortSpeed::kSuperSpeedPlus:
            speed_value = 5;
            break;
          }
          _addr[0] = GenerateValue<RouteString, uint32_t>(device->GetRouteString())
            | GenerateValue<Speed, uint32_t>(speed_value)
            | GenerateValue<ContextEntries, uint32_t>(1);
          _addr[1] = GenerateValue<MaxExitLatency, uint32_t>(0)
            | GenerateValue<RootHubPortNumber, uint32_t>(device->GetRootPortId());
          _addr[2] = GenerateValue<InterrupterTarget, uint32_t>(0);
          _addr[3] = GenerateValue<DeviceAddress, uint32_t>(0);
          _addr[4] = 0;
          _addr[5] = 0;
          _addr[6] = 0;
          _addr[7] = 0;
        }
        void InitOutput(uint32_t *addr) {
          _addr = addr;
        }
        void InitHub(int number_of_ports, int ttt, UsbCtrl::PortSpeed speed) {
          _addr[0] |= kFlagHub;
          _addr[1] |= GenerateValue<NumberOfPorts, uint32_t>(number_of_ports);
          if (speed == UsbCtrl::PortSpeed::kHighSpeed) {
            _addr[2] |= GenerateValue<TtThinkTime, uint32_t>(ttt);
          }
        }
        void SetupEndpoint(int dci) {
          int current_last_dci = MaskValue<ContextEntries, uint32_t>(_addr[0]);
          if (dci > current_last_dci) {
            _addr[0] = (_addr[0] & ~GenerateMask<ContextEntries, uint32_t>())
              | GenerateValue<ContextEntries, uint32_t>(dci);
          }
        }
      private:
        // Table 57: Offset 00h – Slot Context Field Definitions
        struct RouteString {
          static const int kOffset = 0;
          static const int kLen = 20;
        };
        struct Speed {
          static const int kOffset = 20;
          static const int kLen = 23 - 20 + 1;
        };
        static const uint32_t kFlagHub = 1 << 26;
        struct ContextEntries {
          static const int kOffset = 27;
          static const int kLen = 31 - 27 + 1;
        };

        // Table 58: Offset 04h – Slot Context Field Definitions
        struct MaxExitLatency {
          static const int kOffset = 0;
          static const int kLen = 16;
        };
        struct RootHubPortNumber {
          static const int kOffset = 16;
          static const int kLen = 23 - 16 + 1;
        };
        struct NumberOfPorts {
          static const int kOffset = 24;
          static const int kLen = 31 - 24 + 1;
        };

        // Table 59: Offset 08h – Slot Context Field Definitions
        struct TtHubSlotId {
          static const int kOffset = 0;
          static const int kLen = 7 - 0 + 1;
        };
        struct TtPortNumber {
          static const int kOffset = 8;
          static const int kLen = 15 - 8 + 1;
        };
        struct TtThinkTime {
          static const int kOffset = 16;
          static const int kLen = 17 - 16 + 1;
        };
        struct InterrupterTarget {
          static const int kOffset = 22;
          static const int kLen = 31 - 22 + 1;
        };

        // Table 60: Offset 0Ch – Slot Context Field Definitions
        struct DeviceAddress {
          static const int kOffset = 0;
          static const int kLen = 8;
        };
	
        uint32_t *_addr;
      } _slot_context;

      class EndpointContext {
      public:
        void UpdateMaxPacketSize(uint8_t max_packet_size) {
          _addr[1] = (_addr[1] & ~GenerateMask<MaxPacketSize, uint32_t>()) | GenerateValue<MaxPacketSize, uint32_t>(max_packet_size);
        }
      protected:
        // Table 61: Offset 00h – Endpoint Context Field Definitions
        struct Mult {
          static const int kOffset = 8;
          static const int kLen = 9 - 8 + 1;
        };
        struct MaxPrimaryStreams {
          static const int kOffset = 10;
          static const int kLen = 14 - 10 + 1;
        };
        struct Interval {
          static const int kOffset = 16;
          static const int kLen = 23 - 16 + 1;
        };
	
        // Table 62: Offset 04h – Endpoint Context Field Definitions
        struct Cerr {
          static const int kOffset = 1;
          static const int kLen = 2 - 1 + 1;
        };
        struct EndpointType {
          static const int kOffset = 3;
          static const int kLen = 5 - 3 + 1;
        };
        struct MaxBurstSize {
          static const int kOffset = 8;
          static const int kLen = 15 - 8 + 1;
        };
        struct MaxPacketSize {
          static const int kOffset = 16;
          static const int kLen = 31 - 16 + 1;
        };

        // Table 63: Offset 08h – Endpoint Context Field Definitions
        static const int kFlagDequequeCycleState = 1 << 0;
        struct TrDequeuePointer {
          static const int kOffset = 4;
          static const int kLen = 31 - 4 + 1;
        };

        Device *_device;
        uint32_t *_addr;
        int _dci;

        void Init(Device *device, uint32_t *addr, int dci) {
          _device = device;
          _addr = addr;
          _dci = dci;
        }
      };

      class OutEndpointContext : public EndpointContext {
      public:
        // return value: error or not
        void Init(Device *device, uint32_t *addr, int dci, int interval, UsbCtrl::TransferType type, int max_packet_size);
        OutTransferRing &GetRing() {
          return _ring;
        }
      private:
        OutTransferRing _ring;
      } _out_endpoint_context[16];
      class InEndpointContext : public EndpointContext {
      public:
        // return value: error or not
        void Init(Device *device, uint32_t *addr, int dci, int interval, UsbCtrl::TransferType type, int max_packet_size, RingBuffer<uint8_t *> *buf);
        InTransferRing &GetRing() {
          return _ring;
        }
      private:
        RingBuffer<uint8_t *> *_buf;
        InTransferRing _ring;
      } _in_endpoint_context[16];
    };

    class OutputDeviceContext {
    public:
      void Init(Device *device) {
        _device = device;
        const int context_size = _device->_hc->_context_size * 32;
        _mem = new Memory(context_size);
        uint32_t *addr = _mem->GetVirtPtr<uint32_t>();
        memset(addr, 0, context_size);

        _dev_context._slot_context.InitOutput(addr + (0 * _device->_hc->_context_size) / sizeof(uint32_t));
      }
      phys_addr GetPhysAddr() {
        return _mem->GetPhysPtr();
      }
    private:
      DeviceContext _dev_context;
      Device *_device;
      Memory *_mem;
    } _output_context;
    
    class InputDeviceContext {
    public:
      // return value: error or not
      int Init(Device *device);
      void InitHub(int number_of_ports, int ttt) {
        _dev_context._slot_context.InitHub(number_of_ports, ttt, _device->GetPortSpeed());
      }
      void UpdateMaxPacketSizeOfEndpoint0(uint8_t max_packet_size) {
        _ed0_max_packet_size = max_packet_size; 
        _dev_context._in_endpoint_context[0].UpdateMaxPacketSize(max_packet_size);
      }
      phys_addr GetPhysAddr() {
        return _mem->GetPhysPtr();
      }
      TransferRing::CompletionInfo Issue(int dci, TransferRing::TransferTrb *trb[], const int array_len, pthread_mutex_t *mutex) {
        assert(dci >= 1 && dci <= 31);
        if ((dci % 2) == 1) {
          // IN
          return _dev_context._in_endpoint_context[dci / 2].GetRing().Issue(trb, array_len, mutex);
        } else {
          // OUT
          return _dev_context._out_endpoint_context[dci / 2].GetRing().Issue(trb, array_len, mutex);
        }
      }
      void CompleteTransfer(phys_addr pointer, TransferRing::CompletionInfo &completion_info) {
        int dci = completion_info.endpoint_id;
        assert(dci >= 1 && dci <= 31);
        if ((dci % 2) == 1) {
          // IN
          int index = _dev_context._in_endpoint_context[dci / 2].GetRing().GetIndexFromEntryAddr(pointer);
          _dev_context._in_endpoint_context[dci / 2].GetRing().CompleteTransfer(index, completion_info);
        } else {
          // OUT
          int index = _dev_context._out_endpoint_context[dci / 2].GetRing().GetIndexFromEntryAddr(pointer);
          _dev_context._out_endpoint_context[dci / 2].GetRing().CompleteTransfer(index, completion_info);
        }
      }
      void RingEndpointDoorbell(uint8_t endpt_address, UsbCtrl::PacketIdentification direction) {
        int dci = GetDciFromEndptAddress(endpt_address, direction);
        _device->RingEndpointDoorbell(dci);
      }
      ReturnState SetupEndpoint(uint8_t endpt_address, int interval, UsbCtrl::TransferType type, UsbCtrl::PacketIdentification direction, int max_packetsize, RingBuffer<uint8_t *> *buf) {
        int dci = GetDciFromEndptAddress(endpt_address, direction);
        uint32_t *addr = _mem->GetVirtPtr<uint32_t>();
        switch(direction) {
        case UsbCtrl::PacketIdentification::kOut:
          _dev_context._slot_context.SetupEndpoint(dci);
          _dev_context._out_endpoint_context[endpt_address].Init(_device, addr + ((dci + 1) * _device->_hc->_context_size) / sizeof(uint32_t), dci, interval, type, max_packetsize);
          break;
        case UsbCtrl::PacketIdentification::kIn:
          _dev_context._slot_context.SetupEndpoint(dci);
          _dev_context._in_endpoint_context[endpt_address].Init(_device, addr + ((dci + 1) * _device->_hc->_context_size) / sizeof(uint32_t), dci, interval, type, max_packetsize, buf);
          break;
        default:
          break;
        }
        _control_context.ClearAddContextFlag(1);
        _control_context.SetAddContextFlag(dci);
        return ReturnState::kSuccess;
      }
    private:
      class ControlContext {
      public:
        void Init(uint32_t *addr) {
          _addr = addr;
          // set A0 and A1
          _addr[1] = (1 << 0) | (1 << 1);
        }
        void ClearAddContextFlag(int dci) {
          _addr[1] &= ~(1 << dci);
        }
        void SetAddContextFlag(int dci) {
          _addr[1] |= (1 << dci);
        }
      private:
        uint32_t *_addr;
      } _control_context;

      DeviceContext _dev_context;
      Device *_device;
      Memory *_mem;
      int _ed0_max_packet_size;

      int GetDciFromEndptAddress(uint8_t endpt_address, UsbCtrl::PacketIdentification direction) {
        assert(endpt_address >= 1);
        int dci = (endpt_address - 1) * 2 + 2;
        if (direction == UsbCtrl::PacketIdentification::kIn) {
          dci += 1;
        }
        return dci;
      }
    } _input_context;

    DevXhci * const _hc;
    int _slot_id;
    const int _root_port_id;
    DevUsb *_dev_usb;
    uint32_t _route_string;

    virtual UsbCtrl::PortSpeed GetPortSpeed() = 0;
    virtual ReturnState SetRouteString() = 0;
    virtual void Reset() = 0;
  };

  class RootPortDevice : public Device {
  public:
    RootPortDevice() = delete;
    RootPortDevice(DevXhci *hc, const int root_port_id) : Device(hc, root_port_id) {
    }
    virtual UsbCtrl::PortSpeed GetPortSpeed() override {
      return _hc->GetPortSpeed(_root_port_id);
    }
  private:
    virtual ReturnState SetRouteString() override {
      _route_string = 0;
      return ReturnState::kSuccess;
    }
    virtual void Reset() override {
      _hc->Reset(_root_port_id);
    }
  };

  class HubPortDevice : public Device {
  public:
    HubPortDevice() = delete;
    HubPortDevice(DevXhci *hc, Device *parent, Hub *hub, int hub_port_id) : Device(hc, parent->GetRootPortId()), _parent(parent), _hub(hub), _hub_port_id(hub_port_id) {
    }
    virtual UsbCtrl::PortSpeed GetPortSpeed() override {
      return _hub->GetPortSpeed(_hub_port_id);
    }
  private:
    Device *_parent;
    Hub *_hub;
    int _hub_port_id;
    virtual ReturnState SetRouteString() override {
      uint32_t parent_string = _parent->GetRouteString();
      int offset = 0;
      while(offset < 32) {
        if (((parent_string >> 4) & 0xF) == 0) {
          break;
        }
        offset += 4;
      }
      if (offset == 32) {
        return ReturnState::kErrNoHwResource;
      }
      _route_string = parent_string | (((_hub_port_id > 0xF) ? 0xF : _hub_port_id) << offset);
      return ReturnState::kSuccess;
    }
    virtual void Reset() override {
      _hub->Reset(_hub_port_id);
    }
  };

  uint8_t GetSlotType(int root_port_id);
  void SetupScratchPad();

  void HandlePortStatusChange(int root_port_id) {
    int max_ports = MaskValue<CapReg32HcsParams1MaxPorts>(_capreg_base_addr32[kCapReg32OffsetHcsParams1]);
    assert(root_port_id <= max_ports);
    
    volatile uint32_t *portsc = &_opreg_base_addr[kOpRegOffsetPortsc + (root_port_id - 1) * 4];
    if (IsFlagSet(*portsc, kOpRegPortscFlagCsc)) {
      *portsc = (*portsc & ~kOpRegPortscFlagsRwcBits) | kOpRegPortscFlagCsc;
      if (IsFlagClear(*portsc, kOpRegPortscFlagCcs)) {
        Detach(root_port_id);
      } else {
        Attach(root_port_id);
      }
    }
  }

  static void *AttachAll(void *arg) {
    DevXhci *that = reinterpret_cast<DevXhci *>(arg);
    
    pthread_mutex_lock(&that->_mp);

    that->AttachAllSub();

    pthread_mutex_unlock(&that->_mp);

    return nullptr;
  }
  void AttachAllSub();
  void Attach(int root_port_id);
  void Reset(int root_port_id);

  UsbCtrl::PortSpeed GetPortSpeed(int root_port_id) {
    volatile uint32_t *portsc = &_opreg_base_addr[kOpRegOffsetPortsc + (root_port_id - 1) * 4];
    // Table 157: Default USB Speed ID Mapping
    switch (MaskValue<OpRegPortscPortSpeed>(*portsc)) {
    case 1: {
      return UsbCtrl::PortSpeed::kFullSpeed;
    }
    case 2: {
      return UsbCtrl::PortSpeed::kLowSpeed;
    }
    case 3: {
      return UsbCtrl::PortSpeed::kHighSpeed;
    }
    case 4: {
      return UsbCtrl::PortSpeed::kSuperSpeed;
    }
    case 5: {
      return UsbCtrl::PortSpeed::kSuperSpeedPlus;
    }
    default: {
      return UsbCtrl::PortSpeed::kUnknown;
    }
    }
  }

  void Detach(int root_port_id);

  void RingCommandDoorbell() {
    _doorbell_array_base_addr[0] = 0;
  }

  void RingEndpointDoorbell(int slot_id, uint8_t target) {
    _doorbell_array_base_addr[slot_id]
      = GenerateValue<DoorbellRegDbTarget, uint32_t>(target)
      | GenerateValue<DoorbellRegDbStreamId, uint32_t>(0);
  }

  void CompleteCommand(phys_addr pointer, CommandRing::CompletionInfo &info) {
    _command_ring.CompleteCommand(_command_ring.GetIndexFromEntryAddr(pointer), info);
  }

  void CompleteTransfer(phys_addr pointer, TransferRing::CompletionInfo &info) {
    assert(_device_list[info.slot_id] != nullptr);
    _device_list[info.slot_id]->CompleteTransfer(pointer, info);
  }

  void SetDcbaap(phys_addr pointer, uint8_t slot_id) {
    uint64_t *dcbaa_base = _dcbaa_mem->GetVirtPtr<uint64_t>();
    dcbaa_base[slot_id] = pointer;
  }

  void RegisterDevice(Device *device) {
    _device_list[device->GetSlotId()] = device;
  }
  
  void UnRegisterDevice(Device *device) {
    _device_list[device->GetSlotId()] = nullptr;
  }

  static void *HandlePortStatusChange(void *arg) {
    ContainerForPortStatusChangeHandler *container = reinterpret_cast<ContainerForPortStatusChangeHandler *>(arg);
    
    pthread_mutex_lock(&container->that->_mp);

    container->that->HandlePortStatusChange(container->root_port_id);

    pthread_mutex_unlock(&container->that->_mp);

    delete container;
  }

  DevPci _pci;
  int _context_size;
  Memory *_dcbaa_mem;
  Memory *_scratchpad_array_mem;
  Memory *_scratchpad_mem;
  CommandRing _command_ring;
  EventRing _event_ring;
  EventRingSegmentTable _event_ring_segment_table;
  Interrupter _interrupter;
  Device **_device_list;
  RootPortDevice **_root_hub_device_list;
  int _max_slots;

  pthread_mutex_t _mp;
};
