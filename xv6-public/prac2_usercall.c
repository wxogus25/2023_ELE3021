#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    // 직접 interrupt 128번 호출 (user mode)
    __asm__("int $128");
    exit();
}