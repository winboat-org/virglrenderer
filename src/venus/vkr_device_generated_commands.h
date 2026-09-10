/*
 * Copyright 2026 Helios contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef VKR_DEVICE_GENERATED_COMMANDS_H
#define VKR_DEVICE_GENERATED_COMMANDS_H

#include "vkr_common.h"

struct vkr_indirect_commands_layout {
   struct vkr_object base;
};
VKR_DEFINE_OBJECT_CAST(indirect_commands_layout, VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT, VkIndirectCommandsLayoutEXT)

struct vkr_indirect_execution_set {
   struct vkr_object base;
};
VKR_DEFINE_OBJECT_CAST(indirect_execution_set, VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT, VkIndirectExecutionSetEXT)

void
vkr_context_init_device_generated_commands_dispatch(struct vkr_context *ctx);

#endif /* VKR_DEVICE_GENERATED_COMMANDS_H */
