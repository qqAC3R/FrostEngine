#pragma once

namespace Frost
{
	struct PhysicsLayer
	{
		uint32_t LayerID;
		std::string Name;
		uint32_t BitValue;
		int32_t CollidesWith = 0;

		bool IsValid() const
		{
			return !Name.empty() && BitValue > 0;
		}
	};

	class PhysicsLayerManager
	{
	public:
		static uint32_t AddLayer(const std::string& name, bool setCollisions = true);
		static void RemoveLayer(uint32_t layerId);

		static void UpdateLayerName(uint32_t layerId, const std::string& newName);

		static void SetLayerCollision(uint32_t layerId, uint32_t otherLayer, bool shouldCollide);
		static Vector<PhysicsLayer> GetLayerCollisions(uint32_t layerId);

		static const Vector<PhysicsLayer>& GetLayers() { return s_Layers; }
		static const Vector<std::string>& GetLayerNames() { return s_LayerNames; }

		static PhysicsLayer& GetLayer(uint32_t layerId);
		static PhysicsLayer& GetLayer(const std::string& layerName);
		static uint32_t GetLayerCount() { return uint32_t(s_Layers.size()); }

		static bool ShouldCollide(uint32_t layer1, uint32_t layer2);
		static bool IsLayerValid(uint32_t layerId);

		static void ClearLayers();

	private:
		static uint32_t GetNextLayerID();
	private:
		static Vector<PhysicsLayer> s_Layers;
		static Vector<std::string> s_LayerNames;
		static PhysicsLayer s_NullLayer;
	};

}