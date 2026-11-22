/*
 * Earth And Legend (PS Vita) - network initialisation.
 *
 * The ShiVa engine uses plain BSD sockets (TCP) for LAN multiplayer and the
 * optional online services. The Vita requires the NET sysmodule and a memory
 * pool to be initialised before any socket call works.
 */

#ifndef EAL_NET_H
#define EAL_NET_H

#ifdef __cplusplus
extern "C" {
#endif

/* Loads the NET module and initialises sceNet/sceNetCtl.
 * Safe to call more than once. Returns 1 on success. */
int eal_net_init(void);

/* Writes the console's current IPv4 address into buf.
 * Returns 1 when a usable address was obtained. */
int eal_net_local_ip(char *buf, int size);

#ifdef __cplusplus
}
#endif

#endif // EAL_NET_H
