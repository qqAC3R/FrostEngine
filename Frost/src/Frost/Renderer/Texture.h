#pragma once

#if 0
/* Vulkan Structs */
struct VkImage_T;
using VkImage = VkImage_T*;

struct VkImageView_T;
using VkImageView = VkImageView_T*;

struct VkSampler_T;
using VkSampler = VkSampler_T*;

enum VkImageLayout;
#endif

namespace Frost
{
	enum class ShaderType;
	class VulkanDescriptorLayout;

	struct TextureSpecs
	{
		enum class FormatSpec
		{
			None = 0,
			
			// Color
			RGBA8 = 1, RGBA16F = 2,

			// Depth/Stencil
			DEPTH32 = 3,
		};

		enum class FilteringSpec
		{
			NEAREST, LINEAR
		};

		enum class UsageSpec
		{
			ColorAttachment, DepthStencilAttachment, Storage
		};

		FormatSpec Format = FormatSpec::RGBA8;
		FilteringSpec Filtering = FilteringSpec::LINEAR;
		bool UseMipMaps = false;

		Vector<UsageSpec> Usage;

		uint32_t Width, Height;
	};

	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual const TextureSpecs& GetTextureSpecification() const = 0;

		/* OpenGL Specific API */
		virtual uint32_t GetRendererID() const = 0;
		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void Unbind(uint32_t slot = 0) const = 0;
		
		/* Vulkan Specific API */
		//virtual VkSampler GetVulkanSampler() const = 0;
		//virtual VkImage GetVulkanImage() const = 0;
		//virtual VkImageView GetVulkanImageView() const = 0;
		//virtual VkImageLayout GetVulkanImageLayout() const = 0;

		virtual void Destroy() const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(const std::string& path, TextureSpecs spec = {});
	};

	class TextureCubeMap : public Texture
	{
	public:
		static Ref<TextureCubeMap> Create(TextureSpecs spec = {});
	};

	class Image2D : public Texture
	{
	public:
		static Ref<Image2D> Create(TextureSpecs spec = {});
	};


}