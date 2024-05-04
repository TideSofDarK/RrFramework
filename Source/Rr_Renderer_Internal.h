#pragma once

#include "Rr_Vulkan.h"

typedef struct Rr_Renderer Rr_Renderer;

VkCommandBuffer Rr_BeginImmediate(const Rr_Renderer* Renderer);
void Rr_EndImmediate(const Rr_Renderer* Renderer);
