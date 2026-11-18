/* nv2a_vk.h -- Vulkan renderer for the pushbuffer executor (see nv2a_vk.c). */
#ifndef NV2A_VK_H
#define NV2A_VK_H

/* Register the Vulkan back end with the executor (nv2a_backend.h). */
void nv2a_vk_install(void);

#include <stdint.h>
void nv2a_vk_queue_texture(uint32_t payload, uint32_t format, uint32_t available);

#endif
