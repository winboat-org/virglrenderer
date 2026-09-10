/*
 * Copyright 2026 Helios contributors
 * SPDX-License-Identifier: MIT
 */

#include "vkr_device_generated_commands.h"

#include "vn_protocol_renderer_device_generated_commands.h"

#include "vkr_context.h"
#include "vkr_device.h"
#include "vkr_device_generated_commands_gen.h"

static void
vkr_dispatch_vkCreateIndirectCommandsLayoutEXT(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkCreateIndirectCommandsLayoutEXT *args)
{
   vkr_indirect_commands_layout_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyIndirectCommandsLayoutEXT(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkDestroyIndirectCommandsLayoutEXT *args)
{
   vkr_indirect_commands_layout_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkCreateIndirectExecutionSetEXT(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkCreateIndirectExecutionSetEXT *args)
{
   vkr_indirect_execution_set_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyIndirectExecutionSetEXT(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkDestroyIndirectExecutionSetEXT *args)
{
   vkr_indirect_execution_set_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetGeneratedCommandsMemoryRequirementsEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetGeneratedCommandsMemoryRequirementsEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetGeneratedCommandsMemoryRequirementsEXT_args_handle(args);
   vk->GetGeneratedCommandsMemoryRequirementsEXT(args->device, args->pInfo, args->pMemoryRequirements);
}

static void
vkr_dispatch_vkUpdateIndirectExecutionSetPipelineEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkUpdateIndirectExecutionSetPipelineEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkUpdateIndirectExecutionSetPipelineEXT_args_handle(args);
   vk->UpdateIndirectExecutionSetPipelineEXT(args->device, args->indirectExecutionSet, args->executionSetWriteCount,
      args->pExecutionSetWrites);
}

static void
vkr_dispatch_vkUpdateIndirectExecutionSetShaderEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkUpdateIndirectExecutionSetShaderEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkUpdateIndirectExecutionSetShaderEXT_args_handle(args);
   vk->UpdateIndirectExecutionSetShaderEXT(args->device, args->indirectExecutionSet, args->executionSetWriteCount,
      args->pExecutionSetWrites);
}

void
vkr_context_init_device_generated_commands_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateIndirectCommandsLayoutEXT = vkr_dispatch_vkCreateIndirectCommandsLayoutEXT;
   dispatch->dispatch_vkDestroyIndirectCommandsLayoutEXT = vkr_dispatch_vkDestroyIndirectCommandsLayoutEXT;
   dispatch->dispatch_vkCreateIndirectExecutionSetEXT = vkr_dispatch_vkCreateIndirectExecutionSetEXT;
   dispatch->dispatch_vkDestroyIndirectExecutionSetEXT = vkr_dispatch_vkDestroyIndirectExecutionSetEXT;
   dispatch->dispatch_vkGetGeneratedCommandsMemoryRequirementsEXT = vkr_dispatch_vkGetGeneratedCommandsMemoryRequirementsEXT;
   dispatch->dispatch_vkUpdateIndirectExecutionSetPipelineEXT = vkr_dispatch_vkUpdateIndirectExecutionSetPipelineEXT;
   dispatch->dispatch_vkUpdateIndirectExecutionSetShaderEXT = vkr_dispatch_vkUpdateIndirectExecutionSetShaderEXT;
}
