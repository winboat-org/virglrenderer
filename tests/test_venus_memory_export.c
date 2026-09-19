/* SPDX-License-Identifier: MIT
 * Run the production allocation dispatch with a rejecting Vulkan allocator.
 * In particular, an explicit OPAQUE_FD must never acquire an extra DMA_BUF bit.
 */
#include "vkr_context.h"
#include "vkr_device.h"
#include "vkr_device_memory.h"
#include "vkr_physical_device.h"
#include "vn_protocol_renderer_device_memory.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); abort(); } } while (0)
#define HANDLE(type, ptr) ((type)(uintptr_t)(ptr))

static VkExternalMemoryHandleTypeFlags expected_types;
static VkDeviceSize expected_size;
static unsigned calls;

static VKAPI_ATTR VkResult VKAPI_CALL
allocate(VkDevice device, const VkMemoryAllocateInfo *info,
          const VkAllocationCallbacks *allocator, VkDeviceMemory *memory)
{
   (void)allocator; (void)memory;
   CHECK((uintptr_t)device == 109);
   CHECK(info->allocationSize == expected_size && info->memoryTypeIndex == 0);
   const VkExportMemoryAllocateInfo *export =
      vkr_find_struct(info->pNext, VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO);
   CHECK((export ? export->handleTypes : 0) == expected_types);
   const VkMemoryAllocateFlagsInfo *flags =
      vkr_find_struct(info->pNext, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
   CHECK(flags && flags->flags == VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
   calls++;
   return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

static void
run(struct vkr_context *ctx, struct vkr_device *dev,
     bool explicit, VkExternalMemoryHandleTypeFlags requested,
     VkExternalMemoryHandleTypeFlags expected, VkDeviceSize size)
{
   expected_types = expected;
   expected_size = size;
   const unsigned before = calls;
   VkMemoryAllocateFlagsInfo flags = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
   };
   VkExportMemoryAllocateInfo export = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      .pNext = &flags, .handleTypes = requested,
   };
   VkMemoryAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = explicit ? (void *)&export : (void *)&flags,
      .allocationSize = 4097,
   };
   VkDeviceMemory memory = HANDLE(VkDeviceMemory, 123);
   struct vn_command_vkAllocateMemory args = {
      .device = HANDLE(VkDevice, dev), .pAllocateInfo = &info, .pMemory = &memory,
   };
   ctx->dispatch.dispatch_vkAllocateMemory(&ctx->dispatch, &args);
   CHECK(args.ret == VK_ERROR_OUT_OF_DEVICE_MEMORY && calls == before + 1);
   if (requested)
      CHECK(export.handleTypes == requested);
}

int main(void)
{
   struct vkr_context ctx = {0};
   ctx.dispatch.data = &ctx;
   vkr_context_init_device_memory_dispatch(&ctx);
   struct vkr_physical_device physical = {
      .memory_properties = {
         .memoryTypeCount = 1,
         .memoryTypes = {{ .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT }},
      },
      .is_dma_buf_fd_export_supported = true,
      .is_opaque_fd_export_supported = true,
      .udmabuf_dev_fd = -1,
   };
   struct vkr_device dev = {
      .base = { .type = VK_OBJECT_TYPE_DEVICE, .id = 9, .handle.u64 = 109 },
      .physical_device = &physical,
      .proc_table = { .AllocateMemory = allocate },
   };
   const VkExternalMemoryHandleTypeFlags opaque = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
   const VkExternalMemoryHandleTypeFlags dma = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
   run(&ctx, &dev, true, opaque, opaque, 4097);
   run(&ctx, &dev, true, dma, dma, 4097);
   run(&ctx, &dev, true, dma | opaque, dma | opaque, 4097);
   run(&ctx, &dev, false, 0, dma, 4097);
   run(&ctx, &dev, true, 0, dma, 4097);
   physical.is_dma_buf_fd_export_supported = false;
   run(&ctx, &dev, false, 0, opaque, 8192);
   physical.memory_properties.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   run(&ctx, &dev, false, 0, 0, 4097);
   puts("PASS allocation dispatch: explicit opaque/dma, no added union, implicit fallback, error propagation");
   return 0;
}
