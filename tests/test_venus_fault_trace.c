/* SPDX-License-Identifier: MIT
 * CPU-only evidence integrity checks. Does not create a Vulkan device. */
#include "vkr_context.h"
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); abort(); } } while (0)

static unsigned
file_count(const char *path)
{
   DIR *dir = opendir(path);
   CHECK(dir);
   unsigned count = 0;
   struct dirent *entry;
   while ((entry = readdir(dir)))
      count += entry->d_name[0] != '.';
   closedir(dir);
   return count;
}

int main(void)
{
   char dir[] = "/tmp/venus-fault-trace-XXXXXX";
   CHECK(mkdtemp(dir));
   CHECK(setenv("VKR_FAULT_TRACE_DIR", dir, 1) == 0);
   struct vkr_context ctx = { .ctx_id = 7 };
   atomic_init(&ctx.fault_trace_seq, 0);
   atomic_init(&ctx.fault_trace_shader_bytes, 0);
   const uint32_t code[] = { 0x07230203, 0x00010000, 0x12345678, 1, 0 };
   const VkShaderModuleCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = sizeof(code), .pCode = code,
   };
   vkr_debug_flags = 0;
   vkr_fault_trace_shader(&ctx, 42, &info);
   CHECK(file_count(dir) == 0 && atomic_load(&ctx.fault_trace_seq) == 0);
   vkr_debug_flags = VKR_DEBUG_FAULT;
   vkr_fault_trace_shader(&ctx, 42, &info);
   CHECK(file_count(dir) == 1);
   char path[256];
   snprintf(path, sizeof(path), "%s/shader-%ld-7-0-42.spv", dir, (long)getpid());
   FILE *f = fopen(path, "rb");
   CHECK(f);
   uint32_t actual[ARRAY_SIZE(code)];
   CHECK(fread(actual, 1, sizeof(actual), f) == sizeof(actual));
   CHECK(fgetc(f) == EOF && !memcmp(code, actual, sizeof(code)));
   CHECK(fclose(f) == 0);
   /* Simulated PID/context/sequence reuse cannot overwrite earlier evidence. */
   atomic_store(&ctx.fault_trace_seq, 0);
   vkr_fault_trace_shader(&ctx, 42, &info);
   CHECK(file_count(dir) == 1);
   atomic_store(&ctx.fault_trace_shader_bytes, 128ull * 1024 * 1024);
   vkr_fault_trace_shader(&ctx, 43, &info);
   CHECK(file_count(dir) == 1);
   CHECK(unlink(path) == 0 && rmdir(dir) == 0);
   vkr_debug_flags = 0;
   puts("PASS: opt-in host shader bytes, exclusive evidence, bounded capture");
   return 0;
}
