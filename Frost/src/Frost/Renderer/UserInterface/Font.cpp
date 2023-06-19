#include "frostpch.h"
#include "Font.h"

#include "Frost/Project/Project.h"
#include "Frost/Utils/FileSystem.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"

#include "MSDFData.h"

namespace Frost
{
	struct FontInput
	{
		const char* fontFilename;
		msdf_atlas::GlyphIdentifierType glyphIdentifierType;
		const char* charsetFilename;
		double fontScale;
		const char* fontName;
	};

	struct FontConfiguration
	{
		msdf_atlas::ImageType imageType;
		msdf_atlas::ImageFormat imageFormat;
		msdf_atlas::YDirection yDirection;
		int width, height;
		double emSize;
		double pxRange;
		double angleThreshold;
		double miterLimit;
		void (*edgeColoring)(msdfgen::Shape&, double, unsigned long long);
		bool expensiveColoring;
		unsigned long long coloringSeed;
		msdf_atlas::GeneratorAttributes generatorAttributes;
	};

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define DEFAULT_MITER_LIMIT 1.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREADS 8

	namespace Utils
	{
		static std::filesystem::path GetFontCacheDirectory()
		{
			return Project::GetProjectDirectory() / std::filesystem::path("Resources/Cache/Fonts");
		}

		static void CreateCacheDirectoryIfNeeded()
		{
			std::filesystem::path cacheDirectory = GetFontCacheDirectory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}
	}

	struct CachedAtlasText
	{
		uint32_t AtlasWidth;
		uint32_t AtlasHeight;
		void* AtlasPixels;
	};

	template <typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GEN_FN>
	static Ref<Texture2D> CreateAndCacheAtlas(const Vector<msdf_atlas::GlyphGeometry>& glyphs,
		const msdf_atlas::FontGeometry& fontGeometry,
		const FontConfiguration& config,
		const std::filesystem::path& filepath)
	{
		Utils::CreateCacheDirectoryIfNeeded();

		std::string fileExtension = filepath.extension().string();
		fileExtension = fileExtension.substr(1, fileExtension.size() - 1);

		std::filesystem::path cachedFilepath = Utils::GetFontCacheDirectory() / (filepath.stem().string() + "_" + fileExtension + ".fct"); // fct - Frost Cached Text

		if (std::filesystem::exists(cachedFilepath))
		{
			CachedAtlasText cachedAtlas;

			Buffer cachedAtlasBuffer = FileSystem::ReadBytes(cachedFilepath);
			
			cachedAtlas.AtlasWidth = cachedAtlasBuffer.Read<uint32_t>(0);
			cachedAtlas.AtlasHeight = cachedAtlasBuffer.Read<uint32_t>(sizeof(uint32_t)); // Read the next uint, that will be the height
			cachedAtlas.AtlasPixels = cachedAtlasBuffer.ReadBytes(
				cachedAtlas.AtlasWidth * cachedAtlas.AtlasHeight * 4 * sizeof(float),
				sizeof(uint32_t) * 2
			);

			TextureSpecification textureSpec{};
			textureSpec.UseMips = false;
			textureSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			textureSpec.Format = ImageFormat::RGBA32F;
			Ref<Texture2D> imageAtlas = Texture2D::Create(cachedAtlas.AtlasWidth, cachedAtlas.AtlasHeight, textureSpec, cachedAtlas.AtlasPixels);

			return imageAtlas;
		}

		msdf_atlas::ImmediateAtlasGenerator<S, N, GEN_FN, msdf_atlas::BitmapAtlasStorage<T, N> > generator(config.width, config.height);
		generator.setAttributes(config.generatorAttributes);
		generator.setThreadCount(THREADS);
		generator.generate(glyphs.data(), glyphs.size());

		msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>) generator.atlasStorage();



		TextureSpecification textureSpec{};
		textureSpec.UseMips = false;
		textureSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
		textureSpec.Format = ImageFormat::RGBA32F;
		Ref<Texture2D> imageAtlas = Texture2D::Create(bitmap.width, bitmap.height, textureSpec, bitmap.pixels);

		Buffer cachedAtlasBuffer;
		cachedAtlasBuffer.Allocate(bitmap.width * bitmap.height * 4 * sizeof(float) + 2 * sizeof(uint32_t));
		cachedAtlasBuffer.Write((void*)&bitmap.width, sizeof(uint32_t), 0);
		cachedAtlasBuffer.Write((void*)&bitmap.height, sizeof(uint32_t), sizeof(uint32_t));
		cachedAtlasBuffer.Write((void*)bitmap.pixels, bitmap.width * bitmap.height * 4 * sizeof(float), 2 * sizeof(uint32_t));
		FileSystem::WriteBytes(cachedFilepath, cachedAtlasBuffer);

		return imageAtlas;
	}


	Font::Font(const std::filesystem::path& filepath)
		: m_Filepath(filepath), m_Name(filepath.stem().string()), m_MSDFData(new MSDFData())
	{
		int result = 0;
		FontConfiguration config = { };
		
		config.imageType = msdf_atlas::ImageType::MSDF;
		config.imageFormat = msdf_atlas::ImageFormat::BINARY_FLOAT;
		config.yDirection = msdf_atlas::YDirection::BOTTOM_UP;
		config.edgeColoring = msdfgen::edgeColoringInkTrap;
		const char* imageFormatName = nullptr;
		config.generatorAttributes.config.overlapSupport = true;
		config.generatorAttributes.scanlinePass = true;
		
		config.angleThreshold = DEFAULT_ANGLE_THRESHOLD;
		config.miterLimit = DEFAULT_MITER_LIMIT;
		config.imageType = msdf_atlas::ImageType::MTSDF;
		config.emSize = 40;

		FontInput fontInput = { };
		fontInput.glyphIdentifierType = msdf_atlas::GlyphIdentifierType::UNICODE_CODEPOINT;
		fontInput.fontScale = -1;
		std::string fontFilepath = m_Filepath.string();
		fontInput.fontFilename = fontFilepath.c_str();



		LoadGlyphs(fontInput);

		// Determine final atlas dimensions, scale and range, pack glyphs
		GenerateAtlasDimesions(config);

		GenerateFontEdgeColoring(config);


		Ref<Texture2D> texture;
		switch (config.imageType)
		{
			case msdf_atlas::ImageType::MSDF:
				texture = CreateAndCacheAtlas<float, float, 3, msdf_atlas::msdfGenerator>(m_MSDFData->Glyphs, m_MSDFData->FontGeometry, config, m_Filepath);
				break;
			case msdf_atlas::ImageType::MTSDF:
				texture = CreateAndCacheAtlas<float, float, 4, msdf_atlas::mtsdfGenerator>(m_MSDFData->Glyphs, m_MSDFData->FontGeometry, config, m_Filepath);
				break;
		}

		m_TextureAtlas = texture;
	}

	void Font::GenerateAtlasDimesions(FontConfiguration& fontConfig)
	{
		double rangeValue = 2.0;
		int fixedWidth = -1, fixedHeight = -1;
		msdf_atlas::TightAtlasPacker::DimensionsConstraint atlasSizeConstraint = msdf_atlas::TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
		double minEmSize = 0;


		double pxRange = rangeValue;
		bool fixedDimensions = fixedWidth >= 0 && fixedHeight >= 0;
		bool fixedScale = fontConfig.emSize > 0;

		msdf_atlas::TightAtlasPacker atlasPacker;
		if (fixedDimensions)
			atlasPacker.setDimensions(fixedWidth, fixedHeight);
		else
			atlasPacker.setDimensionsConstraint(atlasSizeConstraint);

		atlasPacker.setPadding(
			fontConfig.imageType == msdf_atlas::ImageType::MSDF ||
			fontConfig.imageType == msdf_atlas::ImageType::MTSDF ? 0 : -1
		);

		// TODO: In this case (if padding is -1), the border pixels of each glyph are black, but still computed. For floating-point output, this may play a role.
		if (fixedScale)
			atlasPacker.setScale(fontConfig.emSize);
		else
			atlasPacker.setMinimumScale(minEmSize);

		atlasPacker.setPixelRange(pxRange);
		atlasPacker.setMiterLimit(fontConfig.miterLimit);

		int remaining = atlasPacker.pack(m_MSDFData->Glyphs.data(), m_MSDFData->Glyphs.size());
		if (remaining != 0)
		{
			FROST_CORE_ERROR("Error: Could not fit {0} out of {1} glyphs into the atlas.", remaining, (int)m_MSDFData->Glyphs.size());
			FROST_ASSERT_INTERNAL(false);
		}

		atlasPacker.getDimensions(fontConfig.width, fontConfig.height);
		FROST_ASSERT_INTERNAL(fontConfig.width > 0 && fontConfig.height > 0);
		fontConfig.emSize = atlasPacker.getScale();
		fontConfig.pxRange = atlasPacker.getPixelRange();

		if (!fixedScale)
			FROST_CORE_TRACE("[MSDF_GEN] Glyph size: {0} pixels/EM", fontConfig.emSize);
		if (!fixedDimensions)
			FROST_CORE_TRACE("[MSDF_GEN] Atlas dimensions: {0} x {1}", fontConfig.width, fontConfig.height);
	}

	void Font::LoadGlyphs(FontInput& fontInput)
	{
		class FontHolder {
			msdfgen::FreetypeHandle* ft;
			msdfgen::FontHandle* font;
			const char* fontFilename;
		public:
			FontHolder() : ft(msdfgen::initializeFreetype()), font(nullptr), fontFilename(nullptr) { }
			~FontHolder() {
				if (ft) {
					if (font)
						msdfgen::destroyFont(font);
					msdfgen::deinitializeFreetype(ft);
				}
			}
			bool load(const char* fontFilename) {
				if (ft && fontFilename) {
					if (this->fontFilename && !strcmp(this->fontFilename, fontFilename))
						return true;
					if (font)
						msdfgen::destroyFont(font);
					if ((font = msdfgen::loadFont(ft, fontFilename))) {
						this->fontFilename = fontFilename;
						return true;
					}
					this->fontFilename = nullptr;
				}
				return false;
			}
			operator msdfgen::FontHandle* () const {
				return font;
			}
		};

		FontHolder font;
		if (!font.load(fontInput.fontFilename))
			FROST_ASSERT_INTERNAL(false);
		if (fontInput.fontScale <= 0)
			fontInput.fontScale = 1;

		// Load character set
		fontInput.glyphIdentifierType = msdf_atlas::GlyphIdentifierType::UNICODE_CODEPOINT;
		msdf_atlas::Charset charset;

		// From ImGui
		static const uint32_t charsetRanges[] =
		{
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
			0x2DE0, 0x2DFF, // Cyrillic Extended-A
			0xA640, 0xA69F, // Cyrillic Extended-B
			0,
		};

		for (int range = 0; range < 8; range += 2)
		{
			for (int c = charsetRanges[range]; c <= charsetRanges[range + 1]; c++)
				charset.add(c);
		}

		// Load glyphs
		m_MSDFData->FontGeometry = msdf_atlas::FontGeometry(&m_MSDFData->Glyphs);
		int glyphsLoaded = -1;
		switch (fontInput.glyphIdentifierType)
		{
			case msdf_atlas::GlyphIdentifierType::GLYPH_INDEX:
			{
				glyphsLoaded = m_MSDFData->FontGeometry.loadGlyphset(font, fontInput.fontScale, charset);
				break;
			}
			case msdf_atlas::GlyphIdentifierType::UNICODE_CODEPOINT:
			{
				glyphsLoaded = m_MSDFData->FontGeometry.loadCharset(font, fontInput.fontScale, charset);
				break;
			}
		}

		if (glyphsLoaded < 0)
			FROST_ASSERT_INTERNAL(false);

		FROST_CORE_TRACE("[MSDF_GEN] Loaded geometry of {0} out of {1} glyphs", glyphsLoaded, (int)charset.size());

		// List missing glyphs
		if (glyphsLoaded < (int)charset.size())
		{
			FROST_CORE_WARN("[MSDF_GEN] Missing {0} {1}", (int)charset.size() - glyphsLoaded, fontInput.glyphIdentifierType == msdf_atlas::GlyphIdentifierType::UNICODE_CODEPOINT ? "codepoints" : "glyphs");
		}

		if (fontInput.fontName)
			m_MSDFData->FontGeometry.setName(fontInput.fontName);
	}

	void Font::GenerateFontEdgeColoring(const FontConfiguration& fontConfig)
	{
		// Edge coloring
		if (fontConfig.imageType == msdf_atlas::ImageType::MSDF ||
			fontConfig.imageType == msdf_atlas::ImageType::MTSDF)
		{
			if (fontConfig.expensiveColoring)
			{
				msdf_atlas::Workload([&glyphs = m_MSDFData->Glyphs, &fontConfig](int i, int threadNo) -> bool
					{
						unsigned long long glyphSeed = (LCG_MULTIPLIER * (fontConfig.coloringSeed ^ i) + LCG_INCREMENT) * !!fontConfig.coloringSeed;
						glyphs[i].edgeColoring(fontConfig.edgeColoring, fontConfig.angleThreshold, glyphSeed);
						return true;

					}, m_MSDFData->Glyphs.size()).finish(THREADS);
			}
			else
			{
				unsigned long long glyphSeed = fontConfig.coloringSeed;
				for (msdf_atlas::GlyphGeometry& glyph : m_MSDFData->Glyphs)
				{
					glyphSeed *= LCG_MULTIPLIER;
					glyph.edgeColoring(fontConfig.edgeColoring, fontConfig.angleThreshold, glyphSeed);
				}
			}
		}
	}

	Font::~Font()
	{
		delete m_MSDFData;
	}

}