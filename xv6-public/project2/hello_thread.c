#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
  printf(1, "Hello, thread!\n");
  sleep(1000);
  exit();
}
