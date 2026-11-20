/* XIAO nRF52840: place the identity immediately after the 256-byte vectors.
 * An incompatible vector layout must fail at link time. */
ASSERT(. - __rom_start_address <= 0x100, "UF2 identity overlaps vectors");
. = . + (0x100 - (. - __rom_start_address));
LONG(0x31444956) /* VID1 */
LONG(0x535a424b) /* KBZS */
#if defined(CONFIG_SHIELD_MKB_L_ZS)
LONG(0x014c)
#elif defined(CONFIG_SHIELD_MKB_R_ZS)
LONG(0x0152)
#else
#error Missing UF2 volume target
#endif
