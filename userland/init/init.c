#include <runtime.h>
#include <debug.h>
#include <malloc.h>
#include <string.h>
#include <crc32.h>
#include <port.h>
#include "vga_client.h"

static const char* const strings[] = {
    "This was a triumph\n",
    "I'm making a note here: \"HUGE SUCCESS\"\n",
    "It's hard to overstate my satisfaction\n",
    "Aperture Science\n",
    "We do what we must because we can\n",
    "For the good of all of us, except the ones who are dead\n",
    "\n",
    "But there's no sense crying over every mistake\n",
    "You just keep on trying 'til you run out of cake\n",
    "And the Science gets done\n",
    "And you make a neat gun\n",
    "For the people who are still alive\n",
    "\n",
    "I'm not even angry\n",
    "I'm being so sincere right now\n",
    "Even though you broke my heart\n",
    "And killed me and tore me to pieces\n",
    "And threw every piece into a fire\n",
    "As they burned it hurt because I was so happy for you\n",
    "\n",
    "Now these points of data make a beautiful line\n",
    "And we're out of beta, we're releasing on time\n",
    "So I'm GLaD I got burned\n",
    "Think of all the things we learned\n",
    "For the people who are still alive\n",
    "\n",
    "Go ahead and leave me\n",
    "I think I prefer to stay inside\n",
    "Maybe you'll find someone else to help you\n",
    "Maybe Black Mesa\n",
    "That was a joke, haha, fat chance\n",
    "Anyway, this cake is great, it's so delicious and moist\n",
    "\n",
    "Look at me still talking when there's Science to do. When I look out there, it makes me GLaD I'm not you\n",
    "I've experiments to run\n",
    "There is research to be done\n",
    "On the people who are still alive\n",
    "\n",
    "And believe me I am still alive\n",
    "I'm doing science and I'm still alive\n",
    "I feel fantastic and I'm still alive\n",
    "While you're dying I'll be still alive\n",
    "And when you're dead I will be still alive\n",
    "Still alive, still alive\n"
};


static void test_fat_read()
{
    trace("Starting FAT read tests");

    int fd = open("/init.c", O_RDONLY, 0);
    trace("fd: %d", fd);

    if(fd != -1) {
        char buffer[512];
        uint32_t crc = crc_init();
        while(true) {
            int read_bytes = read(fd, buffer, sizeof(buffer));
            if(read_bytes == -1) {
                panic("I/O error");
            } else if(read_bytes == 0) {
                break;
            } else {
                crc = crc_update(crc, buffer, read_bytes);
            }
        }
        int ret = close(fd);
        assert(ret == 0);

        crc = crc_finalize(crc);
        trace("init.c CRC32: 0x%04X", crc);
    }
}

static void puts(const char* str)
{
    int rpc_ret = vga_write_string(VGAPort, pcb.ack_port, str);
    handle_rpc_ret(rpc_ret);
}

static void test_log()
{
    for(int line = 0; line < sizeof(strings) / sizeof(strings[0]); line++) {
        puts(strings[line]);
    }
    while(1);
}

static void run_tests()
{
#if 0
    test_fat_read();
#else
    test_log();
#endif
}

void main()
{
#if 1
    /* Start block driver */
    int blockdrv_pid = fork();
    if(!blockdrv_pid) {
        exec("blk.elf");
        invalid_code_path();
    }

    /* Start vfs */
    int vfs_pid = fork();
    if(!vfs_pid) {
        exec("vfs.elf");
        invalid_code_path();
    }
#endif

#if 0
    /* Start svga driver */
    int svga_pid = fork();
    if(!svga_pid) {
        exec("svga.elf");
        invalid_code_path();
    }
#else
    /* Start vga driver */
    int vga_pid = fork();
    if(!vga_pid) {
        exec("vga.elf");
        invalid_code_path();
    }
#endif

#if 1
    /* Run tests */
    run_tests();
#endif
}


