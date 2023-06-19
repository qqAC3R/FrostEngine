#pragma once

#include "Frost/Renderer/Texture.h"

#include <filesystem>

namespace Frost
{
	struct MSDFData;
	struct FontConfiguration;
	struct FontInput;

	class Font : public Asset
	{
	public:
		Font(const std::filesystem::path& filepath);
		virtual ~Font();

		Ref<Texture2D> GetFontAtlas() const { return m_TextureAtlas; }
		const MSDFData* GetMSDFData() const { return m_MSDFData; }

		const std::string& GetFontName() const { return m_Name; }

		//static void InitDefaultFont();
		//static Ref<Font> GetDefaultFont();

		static AssetType GetStaticType() { return AssetType::Font; }
		virtual AssetType GetAssetType() const override { return AssetType::Font; }
	private:
		void GenerateAtlasDimesions(FontConfiguration& fontConfig);
		void LoadGlyphs(FontInput& fontInput);
		void GenerateFontEdgeColoring(const FontConfiguration& fontConfig);

	private:
		std::filesystem::path m_Filepath;
		Ref<Texture2D> m_TextureAtlas;
		MSDFData* m_MSDFData = nullptr;
		std::string m_Name;
	};


}