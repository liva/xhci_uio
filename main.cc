#include "xhci.h"

int main(int argc, const char **argv)
{
  auto dev = new DevXhci;
  dev->Init();
  dev->Run();
  return 0;
}
