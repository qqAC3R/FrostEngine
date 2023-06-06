#include "frostpch.h"
#include "AssetSerializer.h"

#include "Frost/Asset/AssetManager.h"
#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Renderer/MaterialAsset.h"

#include <json/nlohmann/json.hpp>

namespace Frost
{

	bool MeshAssetSeralizer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const
	{
		std::string filepath = AssetManager::GetFileSystemPathString(metadata);
		asset = MeshAsset::Load(filepath).As<Asset>();

		if (asset.As<MeshAsset>()->IsLoaded())
		{
			asset->SetFlag(AssetFlag::None);
			return true;
		}

		asset = nullptr;
		//asset->SetFlag(AssetFlag::Missing);
		return false;
	}

	void MaterialAssetSerializer::Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const
	{
		std::string filepath = AssetManager::GetFileSystemPathString(metadata);

		std::ofstream istream(filepath);
		istream.clear();

		nlohmann::ordered_json out = nlohmann::ordered_json();

		Ref<MaterialAsset> materialAsset = asset.As<MaterialAsset>();

		glm::vec4 albedoColor = materialAsset->GetAlbedoColor();
		float roughness = materialAsset->GetRoughness();
		float metalness = materialAsset->GetMetalness();
		float emission = materialAsset->GetEmission();
		uint32_t useNormalMap = materialAsset->IsUsingNormalMap();

		out["AlbedoColor"] = { albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a };
		out["RoughnessValue"] = roughness;
		out["MetalnessValue"] = metalness;
		out["EmissionValue"] = emission;
		out["UseNormalMap"] = useNormalMap;

		out["AlbedoTexture"] = materialAsset->GetAlbedoMap()->Handle.Get();
		out["NormalTexture"] = materialAsset->GetNormalMap()->Handle.Get();
		out["RoughnessTexture"] = materialAsset->GetRoughnessMap()->Handle.Get();
		out["MetalnessTexture"] = materialAsset->GetMetalnessMap()->Handle.Get();

		istream << out.dump(4);
		istream.close();
	}

	bool MaterialAssetSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const
	{
		Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create();
		Ref<Texture2D> whiteTexture = Renderer::GetWhiteLUT();
		std::string filepath = AssetManager::GetFileSystemPathString(metadata);

		std::string content;
		std::ifstream instream(filepath);

		if (!instream.is_open())
		{
			asset = nullptr;
			return false;
		}
		asset = materialAsset.As<Asset>();


		// Get the scene name from filepath
		instream.seekg(0, std::ios::end);
		size_t size = instream.tellg();
		content.resize(size);

		instream.seekg(0, std::ios::beg);
		instream.read(&content[0], size);
		instream.close();

		// Parse the json file
		nlohmann::json in = nlohmann::json::parse(content);

		materialAsset->SetAlbedoColor({ in["AlbedoColor"][0], in["AlbedoColor"][1], in["AlbedoColor"][2], in["AlbedoColor"][3] });
		materialAsset->SetRoughness(in["RoughnessValue"]);
		materialAsset->SetMetalness(in["MetalnessValue"]);
		materialAsset->SetEmission(in["EmissionValue"]);
		materialAsset->SetUseNormalMap(in["UseNormalMap"]);

		// Setting up Albedo Texture
		if (in["AlbedoTexture"] != 0)
		{
			TextureSpecification textureSpec{};
			textureSpec.Format = ImageFormat::RGBA8;
			textureSpec.Usage = ImageUsage::ReadOnly;
			textureSpec.UseMips = true;

			AssetMetadata metadata = AssetManager::GetMetadata(AssetHandle(in["AlbedoTexture"]));
			Ref<Texture2D> albedoTexture = AssetManager::GetOrLoadAsset<Texture2D>(metadata.FilePath.string(), (void*)&textureSpec);

			if (albedoTexture)
			{
				materialAsset->SetAlbedoMap(albedoTexture);
			}
			else
			{
				FROST_CORE_ERROR("Albedo texture from Material Asset with filepath '{0}' and UUID {1} was not found!", metadata.FilePath.string(), metadata.Handle);
				materialAsset->SetAlbedoMap(whiteTexture);
			}
		}
		else
		{
			materialAsset->SetAlbedoMap(whiteTexture);
		}

		// Setting up Normal Texture
		if (in["NormalTexture"] != 0)
		{
			TextureSpecification textureSpec{};
			textureSpec.Format = ImageFormat::RGBA8;
			textureSpec.Usage = ImageUsage::ReadOnly;

			AssetMetadata metadata = AssetManager::GetMetadata(AssetHandle(in["NormalTexture"]));
			Ref<Texture2D> normalTexture = AssetManager::GetOrLoadAsset<Texture2D>(metadata.FilePath.string(), (void*)&textureSpec);

			if (normalTexture)
			{
				materialAsset->SetNormalMap(normalTexture);
			}
			else
			{
				FROST_CORE_ERROR("Normal texture from Material Asset with filepath '{0}' and UUID {1} was not found!", metadata.FilePath.string(), metadata.Handle);
				materialAsset->SetNormalMap(whiteTexture);
			}
		}
		else
		{
			materialAsset->SetNormalMap(whiteTexture);
		}

		// Setting up Roughness Texture
		if (in["RoughnessTexture"] != 0)
		{
			TextureSpecification textureSpec{};
			textureSpec.Format = ImageFormat::RGBA8;
			textureSpec.Usage = ImageUsage::ReadOnly;

			AssetMetadata metadata = AssetManager::GetMetadata(AssetHandle(in["RoughnessTexture"]));
			Ref<Texture2D> roughnessTexture = AssetManager::GetOrLoadAsset<Texture2D>(metadata.FilePath.string(), (void*)&textureSpec);

			if (roughnessTexture)
			{
				materialAsset->SetRoughnessMap(roughnessTexture);
			}
			else
			{
				FROST_CORE_ERROR("Roughness texture from Material Asset with filepath '{0}' and UUID {1} was not found!", metadata.FilePath.string(), metadata.Handle);
				materialAsset->SetRoughnessMap(whiteTexture);
			}
		}
		else
		{
			materialAsset->SetRoughnessMap(whiteTexture);
		}

		// Setting up Roughness Texture
		if (in["MetalnessTexture"] != 0)
		{
			TextureSpecification textureSpec{};
			textureSpec.Format = ImageFormat::RGBA8;
			textureSpec.Usage = ImageUsage::ReadOnly;

			AssetMetadata metadata = AssetManager::GetMetadata(AssetHandle(in["MetalnessTexture"]));
			Ref<Texture2D> metalnessTexture = AssetManager::GetOrLoadAsset<Texture2D>(metadata.FilePath.string(), (void*)&textureSpec);

			if (metalnessTexture)
			{
				materialAsset->SetMetalnessMap(metalnessTexture);
			}
			else
			{
				FROST_CORE_ERROR("Metalness texture from Material Asset with filepath '{0}' and UUID {1} was not found!", metadata.FilePath.string(), metadata.Handle);
				materialAsset->SetMetalnessMap(whiteTexture);
			}
		}
		else
		{
			materialAsset->SetMetalnessMap(whiteTexture);
		}

		return true;
	}

	Ref<Asset> MaterialAssetSerializer::CreateAssetRef(const AssetMetadata& metadata, void* pNext) const
	{
		Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create();
		materialAsset->SetFlag(AssetFlag::None);
		return materialAsset.As<Asset>();
	}

	bool TextureAssetSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const
	{
		std::string filepath = AssetManager::GetFileSystemPathString(metadata);
		TextureSpecification textureSpec = *(TextureSpecification*)pNext;
		//textureSpec.Format = ImageFormat::RGBA8;
		//textureSpec.Usage = ImageUsage::ReadOnly; // Technically speaking, all texture assets should be `ReadOnly`
		//textureSpec.UseMips = false;

		asset = Texture2D::Create(filepath, textureSpec).As<Asset>();

		if (asset.As<Texture2D>()->Loaded())
		{
			asset->SetFlag(AssetFlag::None);
			return true;
		}

		asset = nullptr;
		return false;
	}

	Ref<Asset> TextureAssetSerializer::CreateAssetRef(const AssetMetadata& metadata, void* pNext) const
	{
#if 0
		std::string filepath = AssetManager::GetFileSystemPathString(metadata);
		TextureSpecification textureSpec = *(TextureSpecification*)pNext;

		Ref<Texture2D> textureAsset = Texture2D::Create(filepath, textureSpec);

		if (textureAsset->Loaded())
		{
			textureAsset->SetFlag(AssetFlag::None);
			return textureAsset;
		}

		return nullptr;
#endif
		return nullptr;
	}

}