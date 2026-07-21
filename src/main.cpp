#include <cstdio>

#include "gui/app.h"

int main() {
  iqforge::App app;
  if (!app.init()) {
    std::fprintf(stderr, "Failed to initialize IQ Forge (window/GL context)\n");
    return 1;
  }
  app.run();
  return 0;
}
