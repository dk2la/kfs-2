#ifndef PMM_H
#define PMM_H

#include "types.h"

/*
 * Physical memory manager: which 4 KiB frames of RAM are free.
 *
 * One bit per frame in a bitmap. Only RAM between 1 MiB and DIRECT_MAP_MAX
 * is handed out; the kernel image is reserved at start-up.
 */

struct multiboot_info;

/* Read the memory map from the loader and build the bitmap. */
void		pmm_init(const struct multiboot_info *info);

/*
 * Take a free frame, the highest one available, and return its physical
 * address. Returns 0 when no frame is left (0 is never a usable frame).
 */
uintptr_t	pmm_alloc_frame(void);

/* Take the frame at `addr` if it is free. Returns 1 if taken, 0 if not. */
int		pmm_alloc_frame_at(uintptr_t addr);

/* Give a frame back. A frame that is not in use is reported, not freed. */
void		pmm_free_frame(uintptr_t addr);

size_t		pmm_total_frames(void);	/* usable RAM, in frames */
size_t		pmm_free_frames(void);
uintptr_t	pmm_top(void);		/* first address after usable RAM */

#endif
