#pragma once
#include <stdio.h>

class DevPci {
public:
  DevPci() {
  }
  void Init() {
    _uiofd = open("/dev/uio0", O_RDONLY);
    if (_uiofd < 0) {
      perror("uio open:");
      panic("");
    }
    _configfd = open("/sys/class/uio/uio0/device/config", O_RDWR);
    if (_configfd < 0) {
      perror("config open:");
      panic("");
    }
  }
  void ReadPciReg(uint16_t reg, uint8_t &val) {
    pread(_configfd, &val, 1, reg);
  }
  void ReadPciReg(uint16_t reg, uint16_t &val) {
    pread(_configfd, &val, 2, reg);
  }
  void ReadPciReg(uint16_t reg, uint32_t &val) {
    pread(_configfd, &val, 4, reg);
  }
  void WritePciReg(uint16_t reg, uint8_t val) {
    pwrite(_configfd, &val, 1, reg);
  }
  void WritePciReg(uint16_t reg, uint16_t val) {
    pwrite(_configfd, &val, 2, reg);
  }
  void WritePciReg(uint16_t reg, uint32_t val) {
    pwrite(_configfd, &val, 4, reg);
  }
  void WaitInterrupt() {
    uint16_t command;
    ReadPciReg(kCommandReg, command);
    command &= ~kCommandRegInterruptDisableFlag;
    WritePciReg(kCommandReg, command);

    int icount;
    /* Wait for next interrupt. */
    if (read(_uiofd, &icount, 4) != 4) {
      perror("uio read:");
      exit(1);
    }
  }

  static const uint16_t kVendorIDReg = 0x00;
  static const uint16_t kDeviceIDReg = 0x02;
  static const uint16_t kCommandReg = 0x04;
  static const uint16_t kStatusReg = 0x06;
  static const uint16_t kRegRevisionId = 0x08;
  static const uint16_t kRegInterfaceClassCode = 0x09;
  static const uint16_t kRegSubClassCode = 0x0A;
  static const uint16_t kRegBaseClassCode = 0x0B;
  static const uint16_t kHeaderTypeReg = 0x0E;
  static const uint16_t kBaseAddressReg0 = 0x10;
  static const uint16_t kBaseAddressReg1 = 0x14;
  static const uint16_t kSubsystemVendorIdReg = 0x2c;
  static const uint16_t kSubsystemIdReg = 0x2e;
  static const uint16_t kCapPtrReg = 0x34;
  static const uint16_t kIntPinReg = 0x3D;

  // Capability Registers
  static const uint16_t kCapRegId = 0x0;
  static const uint16_t kCapRegNext = 0x1;

  // MSI Capability Registers
  // see PCI Local Bus Specification Figure 6-9
  static const uint16_t kMsiCapRegControl = 0x2;
  static const uint16_t kMsiCapRegMsgAddr = 0x4;
  // 32bit
  static const uint16_t kMsiCapReg32MsgData = 0x8;
  // 64bit
  static const uint16_t kMsiCapReg64MsgUpperAddr = 0x8;
  static const uint16_t kMsiCapReg64MsgData = 0xC;

  // Message Control for MSI
  // see PCI Local Bus Specification 6.8.1.3
  static const uint16_t kMsiCapRegControlMsiEnableFlag = 1 << 0;
  static const uint16_t kMsiCapRegControlMultiMsgCapOffset = 1;
  static const uint16_t kMsiCapRegControlMultiMsgCapMask = 7 << kMsiCapRegControlMultiMsgCapOffset;
  static const uint16_t kMsiCapRegControlMultiMsgEnableOffset = 4;
  static const uint16_t kMsiCapRegControlAddr64Flag = 1 << 7;

  static const uint16_t kCommandRegBusMasterEnableFlag = 1 << 2;
  static const uint16_t kCommandRegMemWriteInvalidateFlag = 1 << 4;
  static const uint16_t kCommandRegInterruptDisableFlag = 1 << 10;

  static const uint8_t kHeaderTypeRegFlagMultiFunction = 1 << 7;
  static const uint8_t kHeaderTypeRegMaskDeviceType = (1 << 7) - 1;
  static const uint8_t kHeaderTypeRegValueDeviceTypeNormal = 0x00;
  static const uint8_t kHeaderTypeRegValueDeviceTypeBridge = 0x01;
  static const uint8_t kHeaderTypeRegValueDeviceTypeCardbus = 0x02;

  static const uint16_t kStatusRegFlagCapListAvailable = 1 << 4;

  static const uint32_t kRegBaseAddrFlagIo = 1 << 0;
  static const uint32_t kRegBaseAddrMaskMemType = 3 << 1;
  static const uint32_t kRegBaseAddrValueMemType32 = 0 << 1;
  static const uint32_t kRegBaseAddrValueMemType64 = 2 << 1;
  static const uint32_t kRegBaseAddrIsPrefetchable = 1 << 3;
  static const uint32_t kRegBaseAddrMaskMemAddr = 0xFFFFFFF0;
  static const uint32_t kRegBaseAddrMaskIoAddr = 0xFFFFFFFC;

private:
  int _uiofd;
  int _configfd;
};

