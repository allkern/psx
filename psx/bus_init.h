#ifndef BUS_INIT_H
#define BUS_INIT_H

#include "dev/bios.h"
#include "dev/ram.h"
#include "dev/dma.h"
#include "dev/exp1.h"
#include "dev/mc1.h"
#include "dev/mc2.h"
#include "dev/mc3.h"
#include "dev/ic.h"
#include "dev/scratchpad.h"
#include "dev/gpu.h"
#include "dev/spu.h"

struct psx_bus_t {
    psx_bios_t* bios;
    psx_ram_t* ram;
    psx_dma_t* dma;
    psx_exp1_t* exp1;
    psx_mc1_t* mc1;
    psx_mc2_t* mc2;
    psx_mc3_t* mc3;
    psx_ic_t* ic;
    psx_scratchpad_t* scratchpad;
    psx_gpu_t* gpu;
    psx_spu_t* spu;
};

void psx_bus_init_bios(psx_bus_t*, psx_bios_t*);
void psx_bus_init_ram(psx_bus_t*, psx_ram_t*);
void psx_bus_init_dma(psx_bus_t*, psx_dma_t*);
void psx_bus_init_exp1(psx_bus_t*, psx_exp1_t*);
void psx_bus_init_mc1(psx_bus_t*, psx_mc1_t*);
void psx_bus_init_mc2(psx_bus_t*, psx_mc2_t*);
void psx_bus_init_mc3(psx_bus_t*, psx_mc3_t*);
void psx_bus_init_ic(psx_bus_t*, psx_ic_t*);
void psx_bus_init_scratchpad(psx_bus_t*, psx_scratchpad_t*);
void psx_bus_init_gpu(psx_bus_t*, psx_gpu_t*);
void psx_bus_init_spu(psx_bus_t*, psx_spu_t*);

#endif