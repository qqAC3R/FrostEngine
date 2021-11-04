#pragma once

/* ---------------------- Core -------------------------- */
#include "Frost/Core/Application.h"
#include "Frost/Core/Memory.h"
#include "Frost/Core/Layer.h"
#include "Frost/Core/Log.h"

#include "Frost/Core/Timestep.h"
#include "Frost/Utils/PlatformUtils.h"
#include "Frost/Utils/Timer.h"
/* ------------------------------------------------------ */



/* ------------------------ ECS ------------------------- */
/* ------------------------------------------------------ */

/* ---------------------- Renderer ---------------------- */
#include "Frost/Renderer/Framebuffer.h"
#include "Frost/Renderer/Shader.h"
#include "Frost/Renderer/Pipeline.h"

#include "Frost/Renderer/RenderPass.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Renderer/Buffers/BufferDevice.h"
#include "Frost/Renderer/Buffers/VertexBuffer.h"
#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "Frost/Renderer/Buffers/UniformBuffer.h"

#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/Texture.h"
#include "Frost/Renderer/EditorCamera.h"
#include "Frost/Renderer/Material.h"

#include "Frost/ImGui/ImGuiLayer.h"
/* ------------------------------------------------------ */

/* ---------------------- Math ---------------------- */
#include "Frost/Math/FrustumCamera.h"
/* ------------------------------------------------------ */

/* -------------------- Ray Tracing --------------------- */
#include "Frost/Renderer/RayTracing/RayTracingPipeline.h"
#include "Frost/Renderer/RayTracing/ShaderBindingTable.h"
#include "Frost/Renderer/RayTracing/AccelerationStructures.h"
/* ------------------------------------------------------ */




/* -------------------- Entry Point --------------------- */
#include "Frost/Core/EntryPoint.h"
/* ------------------------------------------------------ */