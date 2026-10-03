/**
 * @file  syscalls.c
 * @brief newlib 运行时桩。
 *
 * -nostartfiles 丢了 crt 里默认的 _init/_fini,而 newlib 的
 * __libc_init_array(全局构造的发起者)会调用它们,不给桩直接链接失败。
 * 以后 _sbrk/_write 之类的桩也放这里。
 */
void _init(void) {}
void _fini(void) {}
