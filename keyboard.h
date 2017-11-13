#pragma once

#include <stdint.h>
#include "ringbuffer.h"
#include "usb.h"

class Keyboard : public DevUsb {
public:
  Keyboard() = delete;
  Keyboard(DevUsbController *hc, int addr) : DevUsb(hc, addr), _buf(64) {
  }
  static Keyboard *Init(DevUsbController *hc, int addr);
  virtual void Release() override {
    printf("keyboard: info: detached\n");
  }
private:
  static const int kMaxPacketSize = 8;
  RingBuffer<uint8_t *> _buf;
  
  void InitSub();
  static void *Handle(void *arg) {
    reinterpret_cast<Keyboard *>(arg)->HandleSub();
    return nullptr;
  }
  void HandleSub() {
    while(true) {
      uint8_t *data = _buf.Pop();
      for (int i = 0; i < kMaxPacketSize; i++) {
        printf("%02x ", data[i]);
      }
      printf("\n");
      delete data;
    }
  }
};
