#pragma once

#include "Frost/Asset/Asset.h"
#include "Frost/Renderer/Texture.h"
#include "Frost/Renderer/Material.h"
#include <glm/glm.hpp>

namespace Frost
{
	class MaterialAsset : public Asset
	{
	public:
		// Used mostly for `m_TextureAllocatorSlots`
		enum class TextureSlotIndex : uint32_t
		{
			AlbedoTexture = 0,
			RoughnessTexture = 1,
			MetalnessTexture = 2,
			NormalTexture = 3
		};

	public:
		MaterialAsset();
		virtual ~MaterialAsset();

		glm::vec4& GetAlbedoColor();
		void SetAlbedoColor(const glm::vec4& color);

		float& GetMetalness();
		void SetMetalness(float metalness);

		float& GetRoughness();
		void SetRoughness(float roughness);

		float& GetEmission();
		void SetEmission(float emission);

		Ref<Texture2D> GetAlbedoMap();
		void SetAlbedoMap(Ref<Texture2D> texture);
		void ClearAlbedoMap();

		Ref<Texture2D> GetNormalMap();
		void SetNormalMap(Ref<Texture2D> texture);
		uint32_t IsUsingNormalMap();
		void SetUseNormalMap(uint32_t value);
		void ClearNormalMap();

		Ref<Texture2D> GetMetalnessMap();
		void SetMetalnessMap(Ref<Texture2D> texture);
		void ClearMetalnessMap();

		Ref<Texture2D> GetRoughnessMap();
		void SetRoughnessMap(Ref<Texture2D> texture);
		void ClearRoughnessMap();

		Ref<Texture2D> GetTextureById(uint32_t textureId);
		void SetTextureById(uint32_t textureId, Ref<Texture2D> texture);
		Ref<DataStorage> GetMaterialInternalData() { return m_MaterialData; }

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }
	private:
		Ref<DataStorage> m_MaterialData;
		//glm::vec4 m_AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		//float m_Metalness = 0.0f;
		//float m_Roughness = 0.2f;
		//float m_Emission = 0.0f;
		//uint32_t m_UseNormalMap = 0;

		Vector<uint32_t> m_TextureAllocatorSlots; // Bindless

		Ref<Texture2D> m_AlbedoTexture;
		Ref<Texture2D> m_RoughnessTexture;
		Ref<Texture2D> m_MetalnessTexture;
		Ref<Texture2D> m_NormalTexture;

		friend class Mesh;
	};
}