#include <unistd.h>

#include "server/webserver.h"

int main() {
  WebServer server(10000, 3, 60000, false, 0, "root", "12345678", "webserver",
                   12, 4, true, 3, 1024);
  server.start();

  return 0;
}