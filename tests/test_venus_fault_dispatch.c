/* SPDX-License-Identifier: MIT
 * Drive the production decoded-command dispatch against fake Vulkan entrypoints.
 * Diagnostics must preserve handles, ranges, dependencies and failure results.
 * No GPU or display server is used. */
#include "vkr_buffer.h"
#include "vkr_command_buffer.h"
#include "vkr_device.h"
#include "vkr_queue.h"
#include "virgl_util.h"
#include "vn_protocol_renderer_command_buffer.h"
#include "vn_protocol_renderer_queue.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); abort(); } } while (0)
#define HANDLE(type, ptr) ((type)(uintptr_t)(ptr))

static char messages[32768];
static size_t message_size;
static unsigned copies, executions, submissions, waits;

static void
capture(enum virgl_log_level_flags level, const char *message, void *data)
{
   (void)level; (void)data;
   const size_t size = strlen(message);
   CHECK(message_size + size + 1 < sizeof(messages));
   memcpy(messages + message_size, message, size + 1);
   message_size += size;
}

static VKAPI_ATTR void VKAPI_CALL
copy_buffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst,
             uint32_t count, const VkBufferCopy *regions)
{
   CHECK((uintptr_t)cmd == 101 && (uintptr_t)src == 102 && (uintptr_t)dst == 103);
   CHECK(count == 2);
   CHECK(regions[0].srcOffset == 4294967300ull && regions[0].dstOffset == 12);
   CHECK(regions[0].size == 4294967312ull);
   CHECK(regions[1].srcOffset == 24 && regions[1].dstOffset == 48 && regions[1].size == 64);
   copies++;
}

static VKAPI_ATTR void VKAPI_CALL
copy_buffer2(VkCommandBuffer cmd, const VkCopyBufferInfo2 *info)
{
   CHECK((uintptr_t)cmd == 101);
   CHECK((uintptr_t)info->srcBuffer == 102 && (uintptr_t)info->dstBuffer == 103);
   CHECK(info->regionCount == 1 && info->pRegions[0].srcOffset == 4294967300ull);
   CHECK(info->pRegions[0].dstOffset == 12 && info->pRegions[0].size == 4294967312ull);
   copies++;
}

static VKAPI_ATTR void VKAPI_CALL
execute(VkCommandBuffer cmd, uint32_t count, const VkCommandBuffer *secondary)
{
   CHECK((uintptr_t)cmd == 101 && count == 1 && (uintptr_t)secondary[0] == 104);
   executions++;
}

static VKAPI_ATTR VkResult VKAPI_CALL
submit(VkQueue queue, uint32_t count, const VkSubmitInfo *info, VkFence fence)
{
   CHECK((uintptr_t)queue == 105 && count == 1 && (uintptr_t)fence == 108);
   CHECK(info->commandBufferCount == 1 && (uintptr_t)info->pCommandBuffers[0] == 101);
   CHECK(info->waitSemaphoreCount == 1 && info->signalSemaphoreCount == 1);
   CHECK((uintptr_t)info->pWaitSemaphores[0] == 106 && (uintptr_t)info->pSignalSemaphores[0] == 107);
   CHECK(info->pWaitDstStageMask[0] == VK_PIPELINE_STAGE_TRANSFER_BIT);
   const VkTimelineSemaphoreSubmitInfo *values = info->pNext;
   CHECK(values && values->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
   CHECK(values->waitSemaphoreValueCount == 1 && values->signalSemaphoreValueCount == 1);
   CHECK(values->pWaitSemaphoreValues[0] == 4294967298ull);
   CHECK(values->pSignalSemaphoreValues[0] == 4294967299ull);
   submissions++;
   return VK_ERROR_DEVICE_LOST;
}

static VKAPI_ATTR VkResult VKAPI_CALL
submit2(VkQueue queue, uint32_t count, const VkSubmitInfo2 *info, VkFence fence)
{
   CHECK((uintptr_t)queue == 105 && count == 1 && (uintptr_t)fence == 108);
   CHECK(info->commandBufferInfoCount == 1);
   CHECK((uintptr_t)info->pCommandBufferInfos[0].commandBuffer == 101);
   CHECK(info->waitSemaphoreInfoCount == 1 && info->signalSemaphoreInfoCount == 1);
   CHECK((uintptr_t)info->pWaitSemaphoreInfos[0].semaphore == 106);
   CHECK((uintptr_t)info->pSignalSemaphoreInfos[0].semaphore == 107);
   CHECK(info->pWaitSemaphoreInfos[0].value == 4294967298ull);
   CHECK(info->pSignalSemaphoreInfos[0].value == 4294967299ull);
   CHECK(info->pWaitSemaphoreInfos[0].stageMask == VK_PIPELINE_STAGE_2_COPY_BIT);
   submissions++;
   return VK_ERROR_DEVICE_LOST;
}

static VKAPI_ATTR VkResult VKAPI_CALL
wait_fences(VkDevice dev, uint32_t count, const VkFence *fences,
             VkBool32 all, uint64_t timeout)
{
   CHECK((uintptr_t)dev == 109 && count == 1 && (uintptr_t)fences[0] == 108);
   CHECK(all && timeout == 123456789ull);
   waits++;
   return VK_TIMEOUT;
}

int main(void)
{
   struct vkr_context ctx = { .ctx_id = 7 };
   ctx.dispatch.data = &ctx;
   atomic_init(&ctx.fault_trace_seq, 0);
   vkr_context_init_command_buffer_dispatch(&ctx);
   vkr_context_init_queue_dispatch(&ctx);
   vkr_context_init_fence_dispatch(&ctx);
   struct vkr_device dev = {
      .base = { .type = VK_OBJECT_TYPE_DEVICE, .id = 9, .handle.u64 = 109 },
      .proc_table = { .CmdCopyBuffer = copy_buffer, .CmdCopyBuffer2 = copy_buffer2,
         .CmdExecuteCommands = execute, .QueueSubmit = submit,
         .QueueSubmit2 = submit2, .WaitForFences = wait_fences },
   };
   struct vkr_command_buffer cmd = {
      .base = { .type = VK_OBJECT_TYPE_COMMAND_BUFFER, .id = 1, .handle.u64 = 101 },
      .device = &dev, .context = &ctx,
   };
   struct vkr_command_buffer secondary = {
      .base = { .type = VK_OBJECT_TYPE_COMMAND_BUFFER, .id = 4, .handle.u64 = 104 },
      .device = &dev, .context = &ctx,
   };
   struct vkr_buffer src = { .base = { .type = VK_OBJECT_TYPE_BUFFER, .id = 2, .handle.u64 = 102 } };
   struct vkr_buffer dst = { .base = { .type = VK_OBJECT_TYPE_BUFFER, .id = 3, .handle.u64 = 103 } };
   struct vkr_queue queue = {
      .base = { .type = VK_OBJECT_TYPE_QUEUE, .id = 5, .handle.u64 = 105 },
      .device = &dev, .context = &ctx,
   };
   CHECK(mtx_init(&queue.vk_mutex, mtx_plain) == thrd_success);
   struct vkr_semaphore sem_wait = { .base = { .type = VK_OBJECT_TYPE_SEMAPHORE, .id = 6, .handle.u64 = 106 } };
   struct vkr_semaphore sem_signal = { .base = { .type = VK_OBJECT_TYPE_SEMAPHORE, .id = 7, .handle.u64 = 107 } };
   struct vkr_fence fence = { .base = { .type = VK_OBJECT_TYPE_FENCE, .id = 8, .handle.u64 = 108 } };
   virgl_log_set_handler(capture, NULL, NULL);

   for (unsigned enabled = 0; enabled < 2; enabled++) {
      vkr_debug_flags = enabled ? VKR_DEBUG_FAULT : 0;
      VkBufferCopy regions[] = { {4294967300ull, 12, 4294967312ull}, {24, 48, 64} };
      struct vn_command_vkCmdCopyBuffer copy = {
         .commandBuffer = HANDLE(VkCommandBuffer, &cmd), .srcBuffer = HANDLE(VkBuffer, &src),
         .dstBuffer = HANDLE(VkBuffer, &dst), .regionCount = 2, .pRegions = regions,
      };
      ctx.dispatch.dispatch_vkCmdCopyBuffer(&ctx.dispatch, &copy);
      VkBufferCopy2 region2 = { .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
         .srcOffset = 4294967300ull, .dstOffset = 12, .size = 4294967312ull };
      VkCopyBufferInfo2 info2 = { .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
         .srcBuffer = HANDLE(VkBuffer, &src), .dstBuffer = HANDLE(VkBuffer, &dst),
         .regionCount = 1, .pRegions = &region2 };
      struct vn_command_vkCmdCopyBuffer2 copy2 = {
         .commandBuffer = HANDLE(VkCommandBuffer, &cmd), .pCopyBufferInfo = &info2 };
      ctx.dispatch.dispatch_vkCmdCopyBuffer2(&ctx.dispatch, &copy2);
      VkCommandBuffer secondary_handle = HANDLE(VkCommandBuffer, &secondary);
      struct vn_command_vkCmdExecuteCommands execution = {
         .commandBuffer = HANDLE(VkCommandBuffer, &cmd), .commandBufferCount = 1,
         .pCommandBuffers = &secondary_handle };
      ctx.dispatch.dispatch_vkCmdExecuteCommands(&ctx.dispatch, &execution);

      VkSemaphoreSubmitInfo wait = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
         .semaphore = HANDLE(VkSemaphore, &sem_wait), .value = 4294967298ull,
         .stageMask = VK_PIPELINE_STAGE_2_COPY_BIT };
      VkSemaphoreSubmitInfo signal = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
         .semaphore = HANDLE(VkSemaphore, &sem_signal), .value = 4294967299ull };
      VkCommandBufferSubmitInfo command = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
         .commandBuffer = HANDLE(VkCommandBuffer, &cmd) };
      VkSubmitInfo2 info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
         .waitSemaphoreInfoCount = 1, .pWaitSemaphoreInfos = &wait,
         .commandBufferInfoCount = 1, .pCommandBufferInfos = &command,
         .signalSemaphoreInfoCount = 1, .pSignalSemaphoreInfos = &signal };
      struct vn_command_vkQueueSubmit2 submit = { .queue = HANDLE(VkQueue, &queue),
         .submitCount = 1, .pSubmits = &info, .fence = HANDLE(VkFence, &fence) };
      ctx.dispatch.dispatch_vkQueueSubmit2(&ctx.dispatch, &submit);
      CHECK(submit.ret == VK_ERROR_DEVICE_LOST);
      VkCommandBuffer primary_handle = HANDLE(VkCommandBuffer, &cmd);
      VkSemaphore wait_handle = HANDLE(VkSemaphore, &sem_wait);
      VkSemaphore signal_handle = HANDLE(VkSemaphore, &sem_signal);
      const uint64_t wait_value = 4294967298ull, signal_value = 4294967299ull;
      const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      VkTimelineSemaphoreSubmitInfo values = {.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
         .waitSemaphoreValueCount = 1, .pWaitSemaphoreValues = &wait_value,
         .signalSemaphoreValueCount = 1, .pSignalSemaphoreValues = &signal_value};
      VkSubmitInfo legacy_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = &values,
         .waitSemaphoreCount = 1, .pWaitSemaphores = &wait_handle, .pWaitDstStageMask = &stage,
         .signalSemaphoreCount = 1, .pSignalSemaphores = &signal_handle,
         .commandBufferCount = 1, .pCommandBuffers = &primary_handle};
      struct vn_command_vkQueueSubmit legacy = {.queue = HANDLE(VkQueue, &queue),
         .submitCount = 1, .pSubmits = &legacy_info, .fence = HANDLE(VkFence, &fence)};
      ctx.dispatch.dispatch_vkQueueSubmit(&ctx.dispatch, &legacy);
      CHECK(legacy.ret == VK_ERROR_DEVICE_LOST);
      VkFence fence_handle = HANDLE(VkFence, &fence);
      struct vn_command_vkWaitForFences waiting = { .device = HANDLE(VkDevice, &dev),
         .fenceCount = 1, .pFences = &fence_handle, .waitAll = VK_TRUE, .timeout = 123456789ull };
      ctx.dispatch.dispatch_vkWaitForFences(&ctx.dispatch, &waiting);
      CHECK(waiting.ret == VK_TIMEOUT);
      if (!enabled)
         CHECK(!message_size && !atomic_load(&ctx.fault_trace_seq));
   }
   CHECK(copies == 4 && executions == 2 && submissions == 4 && waits == 2);
   CHECK(strstr(messages, "copy_buffer cmd=1 src=2 dst=3 index=0 src_offset=4294967300 dst_offset=12 size=4294967312"));
   CHECK(strstr(messages, "copy_buffer2 cmd=1 src=2 dst=3 index=0 src_offset=4294967300"));
   CHECK(strstr(messages, "cmd_op cmd=1 op=vkCmdCopyBuffer"));
   CHECK(strstr(messages, "execute_secondary cmd=1 index=0 secondary=4"));
   CHECK(strstr(messages, "submit2_wait batch=0 sem=6 value=4294967298 stages=0x100000000"));
   CHECK(strstr(messages, "submit2_signal batch=0 sem=7 value=4294967299"));
   CHECK(strstr(messages, "submit2_result queue=5 result=-4"));
   CHECK(strstr(messages, "submit_wait batch=0 sem=6 value=4294967298 stages=0x1000"));
   CHECK(strstr(messages, "submit_signal batch=0 sem=7 value=4294967299"));
   CHECK(strstr(messages, "submit_result queue=5 result=-4"));
   CHECK(strstr(messages, "wait_fence index=0 fence=8 all=1 timeout=123456789"));
   CHECK(strstr(messages, "wait_fences_result result=2"));
   virgl_log_set_handler(NULL, NULL, NULL);
   mtx_destroy(&queue.vk_mutex);
   vkr_debug_flags = 0;
   puts("PASS: production fault dispatch preserves native calls, 64-bit ranges, dependencies and errors");
   return 0;
}
