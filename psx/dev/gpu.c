#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gpu.h"
#include "../log.h"

int g_psx_gpu_dither_kernel[] = {
    -4, +0, -3, +1,
    +2, -2, +3, -1,
    -3, +1, -4, +0,
    +3, -1, +2, -2,
};

uint16_t gpu_to_bgr555(uint32_t color) {
    return ((color & 0x0000f8) >> 3) |
           ((color & 0x00f800) >> 6) |
           ((color & 0xf80000) >> 9);
}

psx_gpu_t* psx_gpu_create() {
    return (psx_gpu_t*)malloc(sizeof(psx_gpu_t));
}

void psx_gpu_init(psx_gpu_t* gpu, psx_ic_t* ic) {
    memset(gpu, 0, sizeof(psx_gpu_t));

    gpu->io_base = PSX_GPU_BEGIN;
    gpu->io_size = PSX_GPU_SIZE;

    gpu->vram = (uint16_t*)malloc(PSX_GPU_VRAM_SIZE);
    gpu->state = GPU_STATE_RECV_CMD;

    gpu->ic = ic;
}

uint32_t psx_gpu_read32(psx_gpu_t* gpu, uint32_t offset) {
    switch (offset) {
        case 0x00: {
            uint32_t data = 0x0;

            if (gpu->c0_tsiz) {
                data |= gpu->vram[gpu->c0_addr + (gpu->c0_xcnt + (gpu->c0_ycnt * 1024))];

                gpu->c0_xcnt += 1;

                if (gpu->c0_xcnt == gpu->c0_xsiz) {
                    gpu->c0_ycnt += 1;
                    gpu->c0_xcnt = 0;
                }

                data |= gpu->vram[gpu->c0_addr + (gpu->c0_xcnt + (gpu->c0_ycnt * 1024))] << 16;

                gpu->c0_xcnt += 1;

                if (gpu->c0_xcnt == gpu->c0_xsiz) {
                    gpu->c0_ycnt += 1;
                    gpu->c0_xcnt = 0;
                }

                gpu->c0_tsiz -= 2;
            }

            return data;
        } break;
        case 0x04: return gpu->gpustat | 0x1c000000;
    }

    log_warn("Unhandled 32-bit GPU read at offset %08x", offset);

    return 0x0;
}

uint16_t psx_gpu_read16(psx_gpu_t* gpu, uint32_t offset) {
    switch (offset) {
        case 0x00: return 0xaaaa;
        case 0x04: return gpu->gpustat;
    }

    log_warn("Unhandled 16-bit GPU read at offset %08x", offset);

    return 0x0;
}

uint8_t psx_gpu_read8(psx_gpu_t* gpu, uint32_t offset) {
    switch (offset) {
        case 0x00: return gpu->gpuread;
        case 0x04: return gpu->gpustat;
    }

    log_warn("Unhandled 8-bit GPU read at offset %08x", offset);

    return 0x0;
}

int min(int x0, int x1) {
    return (x0 < x1) ? x0 : x1;
}

int max(int x0, int x1) {
    return (x0 > x1) ? x0 : x1;
}

#define EDGE(a, b, c) ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x))

uint16_t gpu_fetch_texel(psx_gpu_t* gpu, uint16_t tx, uint16_t ty, uint32_t tpx, uint32_t tpy, int depth) {
    tx = (tx & ~gpu->texw_mx) | (gpu->texw_ox & gpu->texw_mx);
    ty = (ty & ~gpu->texw_my) | (gpu->texw_oy & gpu->texw_my);

    switch (depth) {
        // 4-bit
        case 0: {
            uint16_t texel = gpu->vram[(tpx + (tx >> 2)) + ((tpy + ty) * 1024)];

            int index = (texel >> ((tx & 0x3) << 2)) & 0xf;

            return gpu->vram[(gpu->clut_x + index) + (gpu->clut_y * 1024)];
        } break;

        // 8-bit
        case 1: {
            uint16_t texel = gpu->vram[(tpx + (tx >> 1)) + ((tpy + ty) * 1024)];

            int index = (texel >> ((tx & 0x1) << 3)) & 0xff;

            return gpu->vram[(gpu->clut_x + index) + (gpu->clut_y * 1024)];
        } break;

        // 15-bit
        default: {
            return gpu->vram[(tpx + tx) + ((tpy + ty) * 1024)];
        } break;
    }
}

void plotLineLow(psx_gpu_t* gpu, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int yi = 1;
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }
    int d = (2 * dy) - dx;
    int y = y0;

    for (int x = x0; x < x1; x++) {
        if ((x < 1024) && (y < 512))
            gpu->vram[x + (y * 1024)] = color;

        if (d > 0) {
            y += yi;
            d += (2 * (dy - dx));
        } else {
            d += 2*dy;
        }
    }
}

void plotLineHigh(psx_gpu_t* gpu, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int xi = 1;
    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }
    int d = (2 * dx) - dy;
    int x = x0;

    for (int y = y0; y < y1; y++) {
        if ((x < 1024) && (y < 512))
            gpu->vram[x + (y * 1024)] = color;
        if (d > 0) {
            x = x + xi;
            d += (2 * (dx - dy));
        } else {
            d += 2*dx;
        }
    }
}

void plotLine(psx_gpu_t* gpu, int x0, int y0, int x1, int y1, uint16_t color) {
    if (abs(y1 - y0) < abs(x1 - x0)) {
        if (x0 > x1) {
            plotLineLow(gpu, x1, y1, x0, y0, color);
        } else {
            plotLineLow(gpu, x0, y0, x1, y1, color);
        }
    } else {
        if (y0 > y1) {
            plotLineHigh(gpu, x1, y1, x0, y0, color);
        } else {
            plotLineHigh(gpu, x0, y0, x1, y1, color);
        }
    }
}

void gpu_render_flat_line(psx_gpu_t* gpu, vertex_t v0, vertex_t v1, uint32_t color) {
    plotLine(gpu, v0.x, v0.y, v1.x, v1.y, color);
    // double l = 640.0;

    // for (double p = 0.0; p < 1.0; p += 1.0 / l) {
    //     unsigned int ax = (v0.x * p) + (v1.x * (1.0f - p));
    //     unsigned int ay = (v0.y * p) + (v1.y * (1.0f - p));

    //     if ((ax > 1024) || (ay > 512))
    //         continue;

    //     gpu->vram[ax + (ay * 1024)] = color;
    // }
}

void gpu_render_flat_rectangle(psx_gpu_t* gpu, vertex_t v, uint32_t w, uint32_t h, uint32_t color) {
    /* Offset coordinates */
    v.x += gpu->off_x;
    v.y += gpu->off_y;

    /* Calculate bounding box */
    int xmin = max(v.x, gpu->draw_x1);
    int ymin = max(v.y, gpu->draw_y1);
    int xmax = min(xmin + w, gpu->draw_x2);
    int ymax = min(ymin + h, gpu->draw_y2);

    for (uint32_t y = ymin; y < ymax; y++) {
        for (uint32_t x = xmin; x < xmax; x++) {
            gpu->vram[x + (y * 1024)] = color;
        }
    }
}

void gpu_render_textured_rectangle(psx_gpu_t* gpu, vertex_t v, uint32_t w, uint32_t h, uint32_t tpx, uint32_t tpy) {
    vertex_t a = v;

    a.x += gpu->off_x;
    a.y += gpu->off_y;

    int xmin = max(a.x, gpu->draw_x1);
    int ymin = max(a.y, gpu->draw_y1);
    int xmax = min(xmin + w, gpu->draw_x2);
    int ymax = min(ymin + h, gpu->draw_y2);

    uint32_t xc = 0, yc = 0;

    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            uint16_t color = gpu_fetch_texel(gpu, a.tx + xc, a.ty + yc, tpx, tpy, gpu->texp_d);

            ++xc;

            if (!color) continue;

            gpu->vram[x + (y * 1024)] = color;
        }

        xc = 0;

        ++yc;
    }
}

void gpu_render_flat_triangle(psx_gpu_t* gpu, vertex_t v0, vertex_t v1, vertex_t v2, uint32_t color) {
    vertex_t a, b, c;

    a = v0;

    /* Ensure the winding order is correct */
    if (EDGE(v0, v1, v2) < 0) {
        b = v2;
        c = v1;
    } else {
        b = v1;
        c = v2;
    }

    a.x += gpu->off_x;
    a.y += gpu->off_y;
    b.x += gpu->off_x;
    b.y += gpu->off_y;
    c.x += gpu->off_x;
    c.y += gpu->off_y;

    int xmin = max(min(min(a.x, b.x), c.x), gpu->draw_x1);
    int ymin = max(min(min(a.y, b.y), c.y), gpu->draw_y1);
    int xmax = min(max(max(a.x, b.x), c.x), gpu->draw_x2); 
    int ymax = min(max(max(a.y, b.y), c.y), gpu->draw_y2);

    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            int z0 = ((b.x - a.x) * (y - a.y)) - ((b.y - a.y) * (x - a.x));
            int z1 = ((c.x - b.x) * (y - b.y)) - ((c.y - b.y) * (x - b.x));
            int z2 = ((a.x - c.x) * (y - c.y)) - ((a.y - c.y) * (x - c.x));

            if ((z0 >= 0) && (z1 >= 0) && (z2 >= 0)) {
                gpu->vram[x + (y * 1024)] = gpu_to_bgr555(color);
            }
        }
    }
}

void gpu_render_shaded_triangle(psx_gpu_t* gpu, vertex_t v0, vertex_t v1, vertex_t v2) {
    vertex_t a, b, c, p;

    a = v0;

    /* Ensure the winding order is correct */
    if (EDGE(v0, v1, v2) < 0) {
        b = v2;
        c = v1;
    } else {
        b = v1;
        c = v2;
    }

    a.x += gpu->off_x;
    a.y += gpu->off_y;
    b.x += gpu->off_x;
    b.y += gpu->off_y;
    c.x += gpu->off_x;
    c.y += gpu->off_y;

    int xmin = max(min(min(a.x, b.x), c.x), gpu->draw_x1);
    int ymin = max(min(min(a.y, b.y), c.y), gpu->draw_y1);
    int xmax = min(max(max(a.x, b.x), c.x), gpu->draw_x2); 
    int ymax = min(max(max(a.y, b.y), c.y), gpu->draw_y2);

    int area = EDGE(a, b, c);

    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            p.x = x;
            p.y = y;

            float z0 = EDGE((float)b, (float)c, (float)p);
            float z1 = EDGE((float)c, (float)a, (float)p);
            float z2 = EDGE((float)a, (float)b, (float)p);

            if ((z0 >= 0) && (z1 >= 0) && (z2 >= 0)) {
                int cr = (z0 * ((a.c >>  0) & 0xff) + z1 * ((b.c >>  0) & 0xff) + z2 * ((c.c >>  0) & 0xff)) / area;
                int cg = (z0 * ((a.c >>  8) & 0xff) + z1 * ((b.c >>  8) & 0xff) + z2 * ((c.c >>  8) & 0xff)) / area;
                int cb = (z0 * ((a.c >> 16) & 0xff) + z1 * ((b.c >> 16) & 0xff) + z2 * ((c.c >> 16) & 0xff)) / area;

                // Calculate positions within our 4x4 dither
                // kernel
                int dy = (y - ymin) % 4;
                int dx = (x - xmin) % 4;

                // Shift two pixels horizontally on the last
                // two scanlines?
                // if (dy > 1) {
                //     dx = ((x + 2) - xmin) % 4;
                // }

                int dither = g_psx_gpu_dither_kernel[dx + (dy * 4)];

                // Add to the original 8-bit color values
                cr += dither;
                cg += dither;
                cb += dither;

                // Saturate (clamp) to 00-ff
                cr = (cr >= 0xff) ? 0xff : ((cr <= 0) ? 0 : cr);
                cg = (cg >= 0xff) ? 0xff : ((cg <= 0) ? 0 : cg);
                cb = (cb >= 0xff) ? 0xff : ((cb <= 0) ? 0 : cb);

                uint32_t color = (cb << 16) | (cg << 8) | cr;

                gpu->vram[x + (y * 1024)] = gpu_to_bgr555(color);
            }
        }
    }
}

void gpu_render_textured_triangle(psx_gpu_t* gpu, vertex_t v0, vertex_t v1, vertex_t v2, uint32_t tpx, uint32_t tpy, int depth) {
    vertex_t a, b, c;

    a = v0;

    /* Ensure the winding order is correct */
    if (EDGE(v0, v1, v2) < 0) {
        b = v2;
        c = v1;
    } else {
        b = v1;
        c = v2;
    }

    a.x += gpu->off_x;
    a.y += gpu->off_y;
    b.x += gpu->off_x;
    b.y += gpu->off_y;
    c.x += gpu->off_x;
    c.y += gpu->off_y;

    int xmin = max(min(min(a.x, b.x), c.x), gpu->draw_x1);
    int ymin = max(min(min(a.y, b.y), c.y), gpu->draw_y1);
    int xmax = min(max(max(a.x, b.x), c.x), gpu->draw_x2); 
    int ymax = min(max(max(a.y, b.y), c.y), gpu->draw_y2);

    uint32_t area = EDGE(a, b, c);

    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            vertex_t p;

            p.x = x;
            p.y = y;

            float z0 = EDGE((float)b, (float)c, (float)p);
            float z1 = EDGE((float)c, (float)a, (float)p);
            float z2 = EDGE((float)a, (float)b, (float)p);

            if ((z0 >= 0) && (z1 >= 0) && (z2 >= 0)) {
                uint32_t tx = ((z0 * a.tx) + (z1 * b.tx) + (z2 * c.tx)) / area;
                uint32_t ty = ((z0 * a.ty) + (z1 * b.ty) + (z2 * c.ty)) / area;

                uint16_t color = gpu_fetch_texel(gpu, tx, ty, tpx, tpy, depth);

                if (!color) continue;

                gpu->vram[x + (y * 1024)] = color;
            }
        }
    }
}

void gpu_cmd_a0(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 2;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                // Save static data
                gpu->xpos = gpu->buf[1] & 0x3ff;
                gpu->ypos = (gpu->buf[1] >> 16) & 0x1ff;
                gpu->xsiz = gpu->buf[2] & 0xffff;
                gpu->ysiz = gpu->buf[2] >> 16;
                gpu->xsiz = ((gpu->xsiz - 1) & 0x3ff) + 1;
                gpu->ysiz = ((gpu->ysiz - 1) & 0x1ff) + 1;
                gpu->tsiz = ((gpu->xsiz * gpu->ysiz) + 1) & 0xfffffffe;
                gpu->addr = gpu->xpos + (gpu->ypos * 1024);
            }
        } break;

        case GPU_STATE_RECV_DATA: {
            gpu->vram[gpu->addr + (gpu->xcnt + (gpu->ycnt * 1024))] = gpu->recv_data & 0xffff;

            gpu->xcnt += 1;

            if (gpu->xcnt == gpu->xsiz) {
                gpu->ycnt += 1;
                gpu->xcnt = 0;
            }

            gpu->vram[gpu->addr + (gpu->xcnt + (gpu->ycnt * 1024))] = gpu->recv_data >> 16;

            gpu->xcnt += 1;

            if (gpu->xcnt == gpu->xsiz) {
                gpu->ycnt += 1;
                gpu->xcnt = 0;
            }

            gpu->tsiz -= 2;

            if (!gpu->tsiz) {
                gpu->xcnt = 0;
                gpu->ycnt = 0;
                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

// Monochrome Opaque Quadrilateral
void gpu_cmd_28(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 4;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                gpu->v0.x = gpu->buf[1] & 0xffff;
                gpu->v0.y = gpu->buf[1] >> 16;
                gpu->v1.x = gpu->buf[2] & 0xffff;
                gpu->v1.y = gpu->buf[2] >> 16;
                gpu->v2.x = gpu->buf[3] & 0xffff;
                gpu->v2.y = gpu->buf[3] >> 16;
                gpu->v3.x = gpu->buf[4] & 0xffff;
                gpu->v3.y = gpu->buf[4] >> 16;
                gpu->color = gpu->buf[0] & 0xffffff;

                gpu_render_flat_triangle(gpu, gpu->v0, gpu->v1, gpu->v2, gpu->color);
                gpu_render_flat_triangle(gpu, gpu->v1, gpu->v2, gpu->v3, gpu->color);

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

// Monochrome Opaque Quadrilateral
void gpu_cmd_30(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 5;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                gpu->v0.c = gpu->buf[0] & 0xffffff;
                gpu->v0.x = gpu->buf[1] & 0xffff;
                gpu->v0.y = gpu->buf[1] >> 16;
                gpu->v1.c = gpu->buf[2] & 0xffffff;
                gpu->v1.x = gpu->buf[3] & 0xffff;
                gpu->v1.y = gpu->buf[3] >> 16;
                gpu->v2.c = gpu->buf[4] & 0xffffff;
                gpu->v2.x = gpu->buf[5] & 0xffff;
                gpu->v2.y = gpu->buf[5] >> 16;

                gpu_render_shaded_triangle(gpu, gpu->v0, gpu->v1, gpu->v2);

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

// Monochrome Opaque Quadrilateral
void gpu_cmd_38(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 7;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                gpu->v0.c = gpu->buf[0] & 0xffffff;
                gpu->v0.x = gpu->buf[1] & 0xffff;
                gpu->v0.y = gpu->buf[1] >> 16;
                gpu->v1.c = gpu->buf[2] & 0xffffff;
                gpu->v1.x = gpu->buf[3] & 0xffff;
                gpu->v1.y = gpu->buf[3] >> 16;
                gpu->v2.c = gpu->buf[4] & 0xffffff;
                gpu->v2.x = gpu->buf[5] & 0xffff;
                gpu->v2.y = gpu->buf[5] >> 16;
                gpu->v3.c = gpu->buf[6] & 0xffffff;
                gpu->v3.x = gpu->buf[7] & 0xffff;
                gpu->v3.y = gpu->buf[7] >> 16;

                gpu_render_shaded_triangle(gpu, gpu->v0, gpu->v1, gpu->v2);
                gpu_render_shaded_triangle(gpu, gpu->v1, gpu->v2, gpu->v3);

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

// Monochrome Opaque Quadrilateral
void gpu_cmd_2c(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 8;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                uint32_t texp = gpu->buf[4] >> 16;
                gpu->color = gpu->buf[0] & 0xffffff;
                gpu->pal   = gpu->buf[2] >> 16;
                gpu->v0.tx = gpu->buf[2] & 0xff;
                gpu->v0.ty = (gpu->buf[2] >> 8) & 0xff;
                gpu->v1.tx = gpu->buf[4] & 0xff;
                gpu->v1.ty = (gpu->buf[4] >> 8) & 0xff;
                gpu->v2.tx = gpu->buf[6] & 0xff;
                gpu->v2.ty = (gpu->buf[6] >> 8) & 0xff;
                gpu->v3.tx = gpu->buf[8] & 0xff;
                gpu->v3.ty = (gpu->buf[8] >> 8) & 0xff;
                gpu->v0.x = gpu->buf[1] & 0xffff;
                gpu->v0.y = gpu->buf[1] >> 16;
                gpu->v1.x = gpu->buf[3] & 0xffff;
                gpu->v1.y = gpu->buf[3] >> 16;
                gpu->v2.x = gpu->buf[5] & 0xffff;
                gpu->v2.y = gpu->buf[5] >> 16;
                gpu->v3.x = gpu->buf[7] & 0xffff;
                gpu->v3.y = gpu->buf[7] >> 16;

                gpu->clut_x = (gpu->pal & 0x3f) << 4;
                gpu->clut_y = (gpu->pal >> 6) & 0x1ff;

                uint32_t tpx = (texp & 0xf) << 6;
                uint32_t tpy = (texp & 0x10) << 4;
                uint32_t depth = (texp >> 7) & 0x3;

                gpu_render_textured_triangle(gpu, gpu->v0, gpu->v1, gpu->v2, tpx, tpy, depth);
                gpu_render_textured_triangle(gpu, gpu->v1, gpu->v2, gpu->v3, tpx, tpy, depth);

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

// Monochrome Opaque Quadrilateral
void gpu_cmd_2d(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 8;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                uint32_t texp = gpu->buf[4] >> 16;
                gpu->color = gpu->buf[0] & 0xffffff;
                gpu->pal   = gpu->buf[2] >> 16;
                gpu->v0.tx = gpu->buf[2] & 0xff;
                gpu->v0.ty = (gpu->buf[2] >> 8) & 0xff;
                gpu->v1.tx = gpu->buf[4] & 0xff;
                gpu->v1.ty = (gpu->buf[4] >> 8) & 0xff;
                gpu->v2.tx = gpu->buf[6] & 0xff;
                gpu->v2.ty = (gpu->buf[6] >> 8) & 0xff;
                gpu->v3.tx = gpu->buf[8] & 0xff;
                gpu->v3.ty = (gpu->buf[8] >> 8) & 0xff;
                gpu->v0.x = gpu->buf[1] & 0xffff;
                gpu->v0.y = gpu->buf[1] >> 16;
                gpu->v1.x = gpu->buf[3] & 0xffff;
                gpu->v1.y = gpu->buf[3] >> 16;
                gpu->v2.x = gpu->buf[5] & 0xffff;
                gpu->v2.y = gpu->buf[5] >> 16;
                gpu->v3.x = gpu->buf[7] & 0xffff;
                gpu->v3.y = gpu->buf[7] >> 16;

                gpu->clut_x = (gpu->pal & 0x3f) << 4;
                gpu->clut_y = (gpu->pal >> 6) & 0x1ff;

                uint32_t tpx = (texp & 0xf) << 6;
                uint32_t tpy = (texp & 0x10) << 4;
                int depth = (texp >> 7) & 0x3;

                gpu_render_textured_triangle(gpu, gpu->v0, gpu->v1, gpu->v2, tpx, tpy, depth);
                gpu_render_textured_triangle(gpu, gpu->v1, gpu->v2, gpu->v3, tpx, tpy, depth);

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

void gpu_cmd_64(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 3;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                gpu->color = gpu->buf[0] & 0xffffff;
                gpu->v0.x  = gpu->buf[1] & 0xffff;
                gpu->v0.y  = gpu->buf[1] >> 16;
                gpu->v0.tx = gpu->buf[2] & 0xff;
                gpu->v0.ty = (gpu->buf[2] >> 8) & 0xff;
                gpu->pal   = gpu->buf[2] >> 16;

                uint32_t w = gpu->buf[3] & 0xffff;
                uint32_t h = gpu->buf[3] >> 16;

                gpu->clut_x = (gpu->pal & 0x3f) << 4;
                gpu->clut_y = (gpu->pal >> 6) & 0x1ff;

                uint32_t tpx = gpu->texp_x;
                uint32_t tpy = gpu->texp_y;

                gpu_render_textured_rectangle(gpu, gpu->v0, w, h, tpx, tpy);

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

void gpu_cmd_68(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 1;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                gpu->color = gpu->buf[0] & 0xffffff;
                gpu->v0.x  = gpu->buf[1] & 0xffff;
                gpu->v0.y  = gpu->buf[1] >> 16;

                gpu->v0.x += gpu->off_x;
                gpu->v0.y += gpu->off_y;

                gpu->vram[gpu->v0.x + (gpu->v0.y * 1024)] = gpu_to_bgr555(gpu->color);


                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

void gpu_cmd_40(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 2;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->color = gpu->buf[0] & 0xffffff;
                gpu->v0.x  = gpu->buf[1] & 0xffff;
                gpu->v0.y  = gpu->buf[1] >> 16;
                gpu->v1.x  = gpu->buf[2] & 0xffff;
                gpu->v1.y  = gpu->buf[2] >> 16;

                gpu_render_flat_line(gpu, gpu->v0, gpu->v1, gpu_to_bgr555(gpu->color));

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

void gpu_cmd_c0(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 2;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->c0_xcnt = 0;
                gpu->c0_ycnt = 0;
                uint32_t c0_xpos = gpu->buf[1] & 0xffff;
                uint32_t c0_ypos = gpu->buf[1] >> 16;
                gpu->c0_xsiz = gpu->buf[2] & 0xffff;
                gpu->c0_ysiz = gpu->buf[2] >> 16;
                c0_xpos = c0_xpos & 0x3ff;
                c0_ypos = c0_ypos & 0x1ff;
                gpu->c0_xsiz = ((gpu->c0_xsiz - 1) & 0x3ff) + 1;
                gpu->c0_ysiz = ((gpu->c0_ysiz - 1) & 0x1ff) + 1;
                gpu->c0_tsiz = ((gpu->c0_xsiz * gpu->c0_ysiz) + 1) & 0xfffffffe;
                gpu->c0_addr = c0_xpos + (c0_ypos * 1024);

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

void gpu_cmd_02(psx_gpu_t* gpu) {
    switch (gpu->state) {
        case GPU_STATE_RECV_CMD: {
            gpu->state = GPU_STATE_RECV_ARGS;
            gpu->cmd_args_remaining = 2;
        } break;

        case GPU_STATE_RECV_ARGS: {
            if (!gpu->cmd_args_remaining) {
                gpu->state = GPU_STATE_RECV_DATA;

                gpu->color = gpu->buf[0] & 0xffffff;
                gpu->v0.x  = gpu->buf[1] & 0xffff;
                gpu->v0.y  = gpu->buf[1] >> 16;
                gpu->xsiz  = gpu->buf[2] & 0xffff;
                gpu->ysiz  = gpu->buf[2] >> 16;

                gpu->v0.x = (gpu->v0.x & 0x3f0);
                gpu->v0.y = gpu->v0.y & 0x1ff;
                gpu->xsiz = (((gpu->xsiz & 0x3ff) + 0x0f) & 0xfffffff0);
                gpu->ysiz = gpu->ysiz & 0x1ff;

                gpu->v0.x += gpu->off_x;
                gpu->v0.y += gpu->off_y;

                gpu_render_flat_rectangle(gpu, gpu->v0, gpu->xsiz, gpu->ysiz, gpu_to_bgr555(gpu->color));

                gpu->state = GPU_STATE_RECV_CMD;
            }
        } break;
    }
}

void psx_gpu_update_cmd(psx_gpu_t* gpu) {
    switch (gpu->buf[0] >> 24) {
        case 0x00: /* nop */ break;
        case 0x01: /* Cache clear */ break;
        case 0x02: gpu_cmd_02(gpu); break;
        case 0x28: gpu_cmd_28(gpu); break;
        case 0x2c: gpu_cmd_2d(gpu); break;
        case 0x2d: gpu_cmd_2d(gpu); break;
        case 0x2e: gpu_cmd_2d(gpu); break;
        case 0x2f: gpu_cmd_2d(gpu); break;
        case 0x30: gpu_cmd_30(gpu); break;
        case 0x32: gpu_cmd_30(gpu); break;
        case 0x38: gpu_cmd_38(gpu); break;
        case 0x40: gpu_cmd_40(gpu); break;
        case 0x64: gpu_cmd_64(gpu); break;
        case 0x65: gpu_cmd_64(gpu); break;
        case 0x68: gpu_cmd_68(gpu); break;
        case 0xa0: gpu_cmd_a0(gpu); break;
        case 0xc0: gpu_cmd_c0(gpu); break;
        case 0xe1: {
            gpu->gpustat &= 0xfffff800;
            gpu->gpustat |= gpu->buf[0] & 0x7ff;
            gpu->texp_x = (gpu->gpustat & 0xf) << 6;
            gpu->texp_y = (gpu->gpustat & 0x10) << 4;
            gpu->texp_d = (gpu->gpustat >> 7) & 0x3;
        } break;
        case 0xe2: {
            gpu->texw_mx = (gpu->buf[0] >> 0 ) & 0x1f;
            gpu->texw_my = (gpu->buf[0] >> 5 ) & 0x1f;
            gpu->texw_ox = (gpu->buf[0] >> 10) & 0x1f;
            gpu->texw_oy = (gpu->buf[0] >> 15) & 0x1f;
        } break;
        case 0xe3: {
            gpu->draw_x1 = (gpu->buf[0] >> 0 ) & 0x3ff;
            gpu->draw_y1 = (gpu->buf[0] >> 10) & 0x1ff;
        } break;
        case 0xe4: {
            gpu->draw_x2 = (gpu->buf[0] >> 0 ) & 0x3ff;
            gpu->draw_y2 = (gpu->buf[0] >> 10) & 0x1ff;
        } break;
        case 0xe5: {
            gpu->off_x = (gpu->buf[0] >> 0 ) & 0x7ff;
            gpu->off_y = (gpu->buf[0] >> 11) & 0x7ff;
        } break;
        case 0xe6: {
            /* To-do: Implement mask bit thing */
        } break;
        default: log_fatal("Unhandled GP0(%02Xh)", gpu->buf[0] >> 24); break;
    }
}

void psx_gpu_write32(psx_gpu_t* gpu, uint32_t offset, uint32_t value) {
    switch (offset) {
        // GP0
        case 0x00: {
            switch (gpu->state) {
                case GPU_STATE_RECV_CMD: {
                    gpu->buf_index = 0;
                    gpu->buf[gpu->buf_index++] = value;

                    psx_gpu_update_cmd(gpu);
                } break;

                case GPU_STATE_RECV_ARGS: {
                    gpu->buf[gpu->buf_index++] = value;
                    gpu->cmd_args_remaining--;

                    psx_gpu_update_cmd(gpu);
                } break;

                case GPU_STATE_RECV_DATA: {
                    gpu->recv_data = value;

                    psx_gpu_update_cmd(gpu);
                } break;
            }

            return;
        } break;

        // GP1
        case 0x04: {
            uint8_t cmd = value >> 24;

            switch (cmd) {
                case 0x08:
                    gpu->display_mode = value & 0xffffff;

                    if (gpu->event_cb_table[GPU_EVENT_DMODE])
                        gpu->event_cb_table[GPU_EVENT_DMODE](gpu);
                break;

                case 0x10:
                     switch (value & 7) {
                        case 2:
                            gpu->gpuread = ((gpu->texw_oy / 8) << 15) | ((gpu->texw_ox / 8) << 10) | ((gpu->texw_my / 8) << 5) | (gpu->texw_mx / 8);
                            break;
                        case 3:
                            gpu->gpuread = (gpu->draw_y1 << 10) | gpu->draw_x1;
                            break;
                        case 4:
                            gpu->gpuread = (gpu->draw_y2 << 10) | gpu->draw_x2;
                            break;
                        case 5: // Drawing Offset
                            gpu->gpuread = (gpu->off_y << 10) | gpu->off_x;
                            break;
                        default:
                            break;
                     }
            }

            log_error("GP1(%02Xh) args=%06x", value >> 24, value & 0xffffff);

            return;
        } break;
    }

    log_warn("Unhandled 32-bit GPU write at offset %08x (%08x)", offset, value);
}

void psx_gpu_write16(psx_gpu_t* gpu, uint32_t offset, uint16_t value) {
    log_warn("Unhandled 16-bit GPU write at offset %08x (%04x)", offset, value);
}

void psx_gpu_write8(psx_gpu_t* gpu, uint32_t offset, uint8_t value) {
    log_warn("Unhandled 8-bit GPU write at offset %08x (%02x)", offset, value);
}

void psx_gpu_set_event_callback(psx_gpu_t* gpu, int event, psx_gpu_event_callback_t cb) {
    gpu->event_cb_table[event] = cb;
}

void psx_gpu_set_udata(psx_gpu_t* gpu, int index, void* udata) {
    gpu->udata[index] = udata;
}

#define GPU_CYCLES_PER_HDRAW_NTSC 2560.0f
#define GPU_CYCLES_PER_SCANL_NTSC 3413.0f
#define GPU_SCANS_PER_VDRAW_NTSC 240
#define GPU_SCANS_PER_FRAME_NTSC 263

void gpu_hblank_event(psx_gpu_t* gpu) {
    gpu->line++;

    if (gpu->line < GPU_SCANS_PER_VDRAW_NTSC) {
        if (gpu->line & 1) {
            gpu->gpustat |= 1 << 31;
        } else {
            gpu->gpustat &= ~(1 << 31);
        }
    } else {
        gpu->gpustat &= ~(1 << 31);
    }

    if (gpu->line == GPU_SCANS_PER_VDRAW_NTSC) {
        if (gpu->event_cb_table[GPU_EVENT_VBLANK])
            gpu->event_cb_table[GPU_EVENT_VBLANK](gpu);

        psx_ic_irq(gpu->ic, IC_VBLANK);
    } else if (gpu->line == GPU_SCANS_PER_FRAME_NTSC) {
        if (gpu->event_cb_table[GPU_EVENT_VBLANK_END])
            gpu->event_cb_table[GPU_EVENT_VBLANK_END](gpu);

        gpu->line = 0;
    }
}

void psx_gpu_update(psx_gpu_t* gpu, int cyc) {
    int prev_hblank = (gpu->cycles >= GPU_CYCLES_PER_HDRAW_NTSC) &&
                      (gpu->cycles <= GPU_CYCLES_PER_SCANL_NTSC);

    // Convert CPU (~33.8 MHz) cycles to GPU (~53.7 MHz) cycles
    gpu->cycles += (float)cyc * (PSX_GPU_CLOCK_FREQ_NTSC / PSX_CPU_CLOCK_FREQ);

    int curr_hblank = (gpu->cycles >= GPU_CYCLES_PER_HDRAW_NTSC) &&
                      (gpu->cycles <= GPU_CYCLES_PER_SCANL_NTSC);
    
    if (curr_hblank && !prev_hblank) {
        gpu_hblank_event(gpu);
    } else if (prev_hblank && !curr_hblank) {
        if (gpu->event_cb_table[GPU_EVENT_HBLANK_END])
            gpu->event_cb_table[GPU_EVENT_HBLANK_END](gpu);
        
        gpu->cycles -= (float)GPU_CYCLES_PER_SCANL_NTSC;
    }
}

void* psx_gpu_get_display_buffer(psx_gpu_t* gpu) {
    return gpu->vram + (gpu->disp_x + (gpu->disp_y * 1024));
}

void psx_gpu_destroy(psx_gpu_t* gpu) {
    free(gpu->vram);
    free(gpu);
}