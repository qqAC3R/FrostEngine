#include "frostpch.h"
#include "MaterialAsset.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Renderer/BindlessAllocator.h"

namespace Frost
{
	MaterialAsset::MaterialAsset()
	{
		// Fill up the data in the correct order for us to copy it later
		m_MaterialData = Ref<DataStorage>::Create();

		// Albedo -         vec4        (16 bytes)
		// Roughness -      float       (4 bytes)
		// Metalness -      float       (4 bytes)
		// Emission -       float       (4 bytes)
		// UseNormalMap -   uint32_t    (4 bytes)
		// Texture IDs -    4 uint32_t  (16 bytes)
		// Total                         48 bytes
		m_MaterialData->Allocate(48);

		m_MaterialData->Add<glm::vec4>("AlbedoColor", glm::vec4(1.0f));
		m_MaterialData->Add<float>("EmissionFactor", 0.0f);
		m_MaterialData->Add<float>("RoughnessFactor", 0.0f);
		m_MaterialData->Add<float>("MetalnessFactor", 0.0f);

		m_MaterialData->Add<uint32_t>("UseNormalMap", 0);

		m_MaterialData->Add<uint32_t>("AlbedoTexture", 0);
		m_MaterialData->Add<uint32_t>("RoughnessTexture", 0);
		m_MaterialData->Add<uint32_t>("MetalnessTexture", 0);
		m_MaterialData->Add<uint32_t>("NormalTexture", 0);

		Ref<Texture2D> whiteTexture = Renderer::GetWhiteLUT();
		// Allocate texture slots, because the renderer is designed on bindless texture rendering
		// We are using 4 texture slots, because each mesh has an albedo, roughness, metalness and normal map
		m_TextureAllocatorSlots.resize(4);
		for (uint32_t i = 0; i < 4; i++)
		{
			uint32_t textureSlot = BindlessAllocator::AddTexture(whiteTexture);
			m_TextureAllocatorSlots[i] = textureSlot;
		}

		m_MaterialData->Set("AlbedoTexture", m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::AlbedoTexture]);
		m_MaterialData->Set("NormalTexture", m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::NormalTexture]);
		m_MaterialData->Set("RoughnessTexture", m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::RoughnessTexture]);
		m_MaterialData->Set("MetalnessTexture", m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::MetalnessTexture]);

		SetAlbedoMap(whiteTexture);
		SetRoughnessMap(whiteTexture);
		SetMetalnessMap(whiteTexture);
		SetNormalMap(whiteTexture);
#if 0
		m_AlbedoTexture = Renderer::GetWhiteLUT();
		m_NormalTexture = Renderer::GetWhiteLUT();
		m_MetalnessTexture = Renderer::GetWhiteLUT();
		m_RoughnessTexture = Renderer::GetWhiteLUT();
#endif
	}

	MaterialAsset::~MaterialAsset()
	{
		auto whiteTexture = Renderer::GetWhiteLUT();
		for (uint32_t textureSlot : m_TextureAllocatorSlots)
		{
			BindlessAllocator::AddTextureCustomSlot(whiteTexture, textureSlot);
			BindlessAllocator::RemoveTextureCustomSlot(textureSlot);
		}
	}

	glm::vec4& MaterialAsset::GetAlbedoColor()
	{
		return m_MaterialData->Get<glm::vec4>("AlbedoColor");
	}

	void MaterialAsset::SetAlbedoColor(const glm::vec4& color)
	{
		m_MaterialData->Set("AlbedoColor", color);
	}

	float& MaterialAsset::GetMetalness()
	{
		return m_MaterialData->Get<float>("MetalnessFactor");
	}

	void MaterialAsset::SetMetalness(float metalness)
	{
		m_MaterialData->Set("MetalnessFactor", metalness);
	}

	float& MaterialAsset::GetRoughness()
	{
		return m_MaterialData->Get<float>("RoughnessFactor");
	}

	void MaterialAsset::SetRoughness(float roughness)
	{
		m_MaterialData->Set("RoughnessFactor", roughness);
	}

	float& MaterialAsset::GetEmission()
	{
		return m_MaterialData->Get<float>("EmissionFactor");
	}

	void MaterialAsset::SetEmission(float emission)
	{
		m_MaterialData->Set("EmissionFactor", emission);
	}

	Ref<Texture2D> MaterialAsset::GetAlbedoMap()
	{
		return m_AlbedoTexture;
	}

	void MaterialAsset::SetAlbedoMap(Ref<Texture2D> texture)
	{
		if (texture->Loaded())
		{
			uint32_t textureId = m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::AlbedoTexture];
			BindlessAllocator::AddTextureCustomSlot(texture, textureId);
			m_AlbedoTexture = texture;
		}
	}

	void MaterialAsset::ClearAlbedoMap()
	{
		SetAlbedoMap(Renderer::GetWhiteLUT());
	}

	Ref<Texture2D> MaterialAsset::GetNormalMap()
	{
		return m_NormalTexture;
	}

	void MaterialAsset::SetNormalMap(Ref<Texture2D> texture)
	{
		if (texture->Loaded())
		{
			uint32_t textureId = m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::NormalTexture];
			BindlessAllocator::AddTextureCustomSlot(texture, textureId);
			m_NormalTexture = texture;
		}
	}

	uint32_t MaterialAsset::IsUsingNormalMap()
	{
		return m_MaterialData->Get<uint32_t>("UseNormalMap");
	}

	void MaterialAsset::SetUseNormalMap(uint32_t value)
	{
		m_MaterialData->Set("UseNormalMap", value);
	}

	void MaterialAsset::ClearNormalMap()
	{
		SetNormalMap(Renderer::GetWhiteLUT());
	}

	Ref<Texture2D> MaterialAsset::GetMetalnessMap()
	{
		return m_MetalnessTexture;
	}

	void MaterialAsset::SetMetalnessMap(Ref<Texture2D> texture)
	{
		if (texture->Loaded())
		{
			uint32_t textureId = m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::MetalnessTexture];
			BindlessAllocator::AddTextureCustomSlot(texture, textureId);
			m_MetalnessTexture = texture;
		}
	}

	void MaterialAsset::ClearMetalnessMap()
	{
		SetMetalnessMap(Renderer::GetWhiteLUT());
	}

	Ref<Texture2D> MaterialAsset::GetRoughnessMap()
	{
		return m_RoughnessTexture;
	}

	void MaterialAsset::SetRoughnessMap(Ref<Texture2D> texture)
	{
		if (texture->Loaded())
		{
			uint32_t textureId = m_TextureAllocatorSlots[(uint32_t)TextureSlotIndex::RoughnessTexture];
			BindlessAllocator::AddTextureCustomSlot(texture, textureId);
			m_RoughnessTexture = texture;
		}
	}

	void MaterialAsset::ClearRoughnessMap()
	{
		SetRoughnessMap(Renderer::GetWhiteLUT());
	}

	Ref<Texture2D> MaterialAsset::GetTextureById(uint32_t textureId)
	{
		TextureSlotIndex textureSlotIndex = (TextureSlotIndex)UINT32_MAX;

		// Get the index of the texture from the List of textures.
		// This will give us a hint to what type of texture we are trying to get
		for(uint32_t i = 0; i < 4; i++)
		{
			if (m_TextureAllocatorSlots[i] == textureId)
			{
				textureSlotIndex = (TextureSlotIndex)i;
				break;
			}
		}

		if ((uint32_t)textureSlotIndex == UINT32_MAX)
		{
			FROST_ASSERT_MSG("Texture slot not found!");
			return nullptr;
		}

		switch (textureSlotIndex)
		{
			case MaterialAsset::TextureSlotIndex::AlbedoTexture:     return GetAlbedoMap();
			case MaterialAsset::TextureSlotIndex::RoughnessTexture:  return GetRoughnessMap();
			case MaterialAsset::TextureSlotIndex::MetalnessTexture:  return GetMetalnessMap();
			case MaterialAsset::TextureSlotIndex::NormalTexture:     return GetNormalMap();
		}

		FROST_ASSERT_MSG("Texture slot index not valid!");
		return nullptr;
	}

	void MaterialAsset::SetTextureById(uint32_t textureId, Ref<Texture2D> texture)
	{
		TextureSlotIndex textureSlotIndex = (TextureSlotIndex)UINT32_MAX;

		if (!texture->Loaded()) return;

		// Get the index of the texture from the List of textures.
		// This will give us a hint to what type of texture we are trying to set
		for (uint32_t i = 0; i < 4; i++)
		{
			if (m_TextureAllocatorSlots[i] == textureId)
			{
				textureSlotIndex = (TextureSlotIndex)i;
				break;
			}
		}

		if ((uint32_t)textureSlotIndex == UINT32_MAX)
		{
			FROST_ASSERT_MSG("Texture slot not found!");
			return;
		}

		switch (textureSlotIndex)
		{
			case MaterialAsset::TextureSlotIndex::AlbedoTexture:     return SetAlbedoMap(texture);
			case MaterialAsset::TextureSlotIndex::RoughnessTexture:  return SetRoughnessMap(texture);
			case MaterialAsset::TextureSlotIndex::MetalnessTexture:  return SetMetalnessMap(texture);
			case MaterialAsset::TextureSlotIndex::NormalTexture:     return SetNormalMap(texture);
		}

		FROST_ASSERT_MSG("Texture slot index not valid!");
	}

}