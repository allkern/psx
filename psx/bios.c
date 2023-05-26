#include "bios.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

psx_bios_t* psx_bios_create() {
    return (psx_bios_t*)malloc(sizeof(psx_bios_t));
}

void psx_bios_init(psx_bios_t* bios) {
    bios->buf = (uint8_t*)malloc(PSX_BIOS_SIZE);
}

void psx_bios_load(psx_bios_t* bios, const char* path) {
    FILE* file = fopen(path, "rb");

    fread(bios->buf, 1, PSX_BIOS_SIZE, file);

#ifdef PSXE_DEBUG_BIOS
    log_info("Loaded BIOS file \"%s\"", path);
#endif

    fclose(file);
}

uint32_t psx_bios_read32(psx_bios_t* bios, uint32_t offset) {
#ifdef PSXE_DEBUG_BIOS
    //log_debug("32-bit read at BIOS address %08x", offset);
#endif

    return *((uint32_t*)(bios->buf + offset));
}

uint16_t psx_bios_read16(psx_bios_t* bios, uint32_t offset) {
#ifdef PSXE_DEBUG_BIOS
    //log_debug("16-bit read at BIOS address %08x", offset);
#endif

    return *((uint16_t*)(bios->buf + offset));
}

uint8_t psx_bios_read8(psx_bios_t* bios, uint32_t offset) {
#ifdef PSXE_DEBUG_BIOS
    //log_debug("8-bit read at BIOS address %08x", offset);
#endif

    return bios->buf[offset];
}

void psx_bios_destroy(psx_bios_t* bios) {
    free(bios->buf);
    free(bios);
}