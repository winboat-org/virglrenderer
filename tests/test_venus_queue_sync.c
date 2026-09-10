/* Copyright 2026 Helios contributors
 * SPDX-License-Identifier: MIT
 * Exercise the production queue-marker allocator and worker with a controlled
 * Vulkan implementation. No GPU or display server is needed.
 */
#include "vkr_context.h"
#include "vkr_device.h"
#include "vkr_physical_device.h"
#include "vkr_queue.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); abort(); } } while (0)

static mtx_t test_mutex;
static cnd_t test_cond;
static bool release_wait;
static unsigned wait_calls, retired, created, destroyed;
static uint64_t retired_id;
static VkResult create_result = VK_SUCCESS, reset_result = VK_SUCCESS;
static VkResult submit_result = VK_SUCCESS, wait_result = VK_SUCCESS;

static VKAPI_ATTR VkResult VKAPI_CALL
create_fence(VkDevice dev, const VkFenceCreateInfo *info,
             const VkAllocationCallbacks *alloc, VkFence *fence)
{
   (void)dev; (void)alloc;
   CHECK(info->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
   CHECK(!info->pNext && !info->flags);
   if (create_result != VK_SUCCESS)
      return create_result;
   *fence = (VkFence)(uintptr_t)++created;
   return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL
destroy_fence(VkDevice dev, VkFence fence, const VkAllocationCallbacks *alloc)
{
   (void)dev; (void)alloc;
   CHECK(fence != VK_NULL_HANDLE);
   destroyed++;
}

static VKAPI_ATTR VkResult VKAPI_CALL
reset_fences(VkDevice dev, uint32_t count, const VkFence *fences)
{
   (void)dev;
   CHECK(count == 1 && *fences != VK_NULL_HANDLE);
   return reset_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL
submit(VkQueue queue, uint32_t count, const VkSubmitInfo *info, VkFence fence)
{
   (void)queue;
   CHECK(count == 0 && !info && fence != VK_NULL_HANDLE);
   return submit_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL
wait_fences(VkDevice dev, uint32_t count, const VkFence *fences,
            VkBool32 all, uint64_t timeout)
{
   (void)dev;
   CHECK(count == 1 && *fences != VK_NULL_HANDLE && all && timeout);
   mtx_lock(&test_mutex);
   wait_calls++;
   cnd_broadcast(&test_cond);
   while (!release_wait)
      cnd_wait(&test_cond, &test_mutex);
   VkResult result = wait_result;
   mtx_unlock(&test_mutex);
   return result;
}

static VKAPI_ATTR VkResult VKAPI_CALL
fence_status(VkDevice dev, VkFence fence)
{
   (void)dev; (void)fence;
   return VK_ERROR_DEVICE_LOST;
}

static void
retire(uint32_t ctx_id, uint32_t ring_idx, uint64_t fence_id)
{
   CHECK(ctx_id == 7 && ring_idx == 3);
   mtx_lock(&test_mutex);
   retired++;
   retired_id = fence_id;
   cnd_broadcast(&test_cond);
   mtx_unlock(&test_mutex);
}

static void
wait_count(unsigned *counter, unsigned count)
{
   struct timespec deadline;
   timespec_get(&deadline, TIME_UTC);
   deadline.tv_sec += 5;
   mtx_lock(&test_mutex);
   while (*counter < count)
      CHECK(cnd_timedwait(&test_cond, &test_mutex, &deadline) == thrd_success);
   mtx_unlock(&test_mutex);
}

static void
wait_pool_count(struct vkr_device *dev, struct list_head *pool, unsigned count)
{
   for (unsigned i = 0; i < 5000; i++) {
      mtx_lock(&dev->free_sync_mutex);
      unsigned found = 0;
      list_for_each_entry (struct vkr_queue_sync, sync, pool, head) {
         (void)sync;
         found++;
      }
      bool ready = found == count;
      mtx_unlock(&dev->free_sync_mutex);
      if (ready)
         return;
      thrd_sleep(&(struct timespec){ .tv_nsec = 1000000 }, NULL);
   }
   CHECK(false);
}

int main(void)
{
   struct vkr_context ctx = { .ctx_id = 7, .retire_fence = retire };
   struct vkr_physical_device physical = { .KHR_external_fence_fd = true };
   struct vkr_device dev = { .physical_device = &physical };
   CHECK(mtx_init(&test_mutex, mtx_plain) == thrd_success);
   CHECK(cnd_init(&test_cond) == thrd_success);
   CHECK(mtx_init(&dev.free_sync_mutex, mtx_plain) == thrd_success);
   list_inithead(&dev.free_syncs);
   list_inithead(&dev.failed_syncs);
   dev.proc_table.CreateFence = create_fence;
   dev.proc_table.DestroyFence = destroy_fence;
   dev.proc_table.ResetFences = reset_fences;
   dev.proc_table.QueueSubmit = submit;
   dev.proc_table.WaitForFences = wait_fences;
   dev.proc_table.GetFenceStatus = fence_status;

   struct vkr_queue *queue = vkr_queue_create(&ctx, &dev, 0, 0, 0, VK_NULL_HANDLE);
   CHECK(queue);
   CHECK(vkr_queue_sync_submit(queue, 0, 3, 100));
   wait_count(&wait_calls, 1);
   mtx_lock(&test_mutex);
   CHECK(retired == 0); /* Submission alone is not completion. */
   release_wait = true;
   cnd_broadcast(&test_cond);
   mtx_unlock(&test_mutex);
   wait_count(&retired, 1);
   wait_pool_count(&dev, &dev.free_syncs, 1);
   CHECK(retired_id == 100 && created == 1);

   reset_result = VK_ERROR_DEVICE_LOST;
   CHECK(!vkr_queue_sync_submit(queue, 0, 3, 101));
   CHECK(destroyed == 1);
   reset_result = VK_SUCCESS;
   create_result = VK_ERROR_OUT_OF_HOST_MEMORY;
   CHECK(!vkr_queue_sync_submit(queue, 0, 3, 102));
   create_result = VK_SUCCESS;

   mtx_lock(&test_mutex);
   wait_result = VK_ERROR_DEVICE_LOST;
   mtx_unlock(&test_mutex);
   CHECK(vkr_queue_sync_submit(queue, 0, 3, 103));
   wait_pool_count(&dev, &dev.failed_syncs, 1);
   CHECK(list_is_empty(&dev.free_syncs));
   mtx_lock(&test_mutex);
   CHECK(retired == 1);
   mtx_unlock(&test_mutex);

   submit_result = VK_ERROR_DEVICE_LOST;
   CHECK(vkr_queue_sync_submit(queue, 0, 3, 104));
   wait_pool_count(&dev, &dev.failed_syncs, 2);
   CHECK(list_is_empty(&dev.free_syncs));
   submit_result = VK_SUCCESS;
   mtx_lock(&test_mutex);
   wait_result = VK_ERROR_OUT_OF_HOST_MEMORY;
   mtx_unlock(&test_mutex);
   CHECK(vkr_queue_sync_submit(queue, 0, 3, 105));
   wait_pool_count(&dev, &dev.failed_syncs, 3);
   CHECK(list_is_empty(&dev.free_syncs));
   vkr_queue_destroy(&ctx, queue);
   CHECK(retired == 1); /* Teardown must not acknowledge failed markers. */
   list_for_each_entry_safe (struct vkr_queue_sync, sync, &dev.failed_syncs, head) {
      list_del(&sync->head);
      destroy_fence(VK_NULL_HANDLE, sync->fence, NULL);
      free(sync);
   }
   CHECK(created == destroyed);
   mtx_destroy(&dev.free_sync_mutex);
   cnd_destroy(&test_cond);
   mtx_destroy(&test_mutex);
   puts("PASS: ordinary queue fences, authenticated completion, allocation/reset/wait/submit failures");
   return 0;
}
