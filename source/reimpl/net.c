#include "reimpl/net.h"

#include <string.h>

#include <psp2/kernel/sysmem.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>

#include "utils/trace.h"

#define NET_MEM_SIZE (1024 * 1024)

static SceUID net_mem = -1;
static int net_ready = 0;

int eal_net_init(void) {
    if (net_ready)
        return 1;

    int res = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    eal_trace("net: load NET module -> 0x%08X", (unsigned) res);

    net_mem = sceKernelAllocMemBlock("eal_net", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                                     NET_MEM_SIZE, NULL);
    if (net_mem < 0) {
        eal_trace("net: memblock failed (0x%08X)", (unsigned) net_mem);
        return 0;
    }

    void *mem = NULL;
    if (sceKernelGetMemBlockBase(net_mem, &mem) < 0 || !mem) {
        eal_trace("net: memblock base failed");
        return 0;
    }

    SceNetInitParam param;
    param.memory = mem;
    param.size = NET_MEM_SIZE;
    param.flags = 0;

    res = sceNetInit(&param);
    if (res < 0) {
        eal_trace("net: sceNetInit failed (0x%08X)", (unsigned) res);
        return 0;
    }

    res = sceNetCtlInit();
    if (res < 0)
        eal_trace("net: sceNetCtlInit failed (0x%08X)", (unsigned) res);

    net_ready = 1;
    eal_trace("net: initialised (%d KB pool)", NET_MEM_SIZE / 1024);
    return 1;
}

int eal_net_local_ip(char *buf, int size) {
    if (!buf || size <= 0)
        return 0;

    buf[0] = '\0';

    if (!net_ready)
        return 0;

    SceNetCtlInfo info;
    memset(&info, 0, sizeof(info));

    if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) < 0) {
        eal_trace("net: could not read local IP");
        return 0;
    }

    strncpy(buf, info.ip_address, (size_t) size - 1);
    buf[size - 1] = '\0';

    if (buf[0] == '\0' || strcmp(buf, "0.0.0.0") == 0)
        return 0;

    return 1;
}
