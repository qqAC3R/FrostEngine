#include "frostpch.h"
#include "PhysicsLayer.h"

namespace Frost
{
	template<typename T, typename ConditionFunction>
	static bool RemoveIfExists(std::vector<T>& vector, ConditionFunction condition)
	{
		for (std::vector<T>::iterator it = vector.begin(); it != vector.end(); ++it)
		{
			if (condition(*it))
			{
				vector.erase(it);
				return true;
			}
		}

		return false;
	}

	uint32_t PhysicsLayerManager::AddLayer(const std::string& name, bool setCollisions)
	{
		uint32_t layerId = GetNextLayerID();
		PhysicsLayer layer = { layerId, name, BIT(layerId), BIT(layerId) };
		s_Layers.insert(s_Layers.begin() + layerId, layer);
		s_LayerNames.insert(s_LayerNames.begin() + layerId, name);

		if (setCollisions)
		{
			for (const auto& layer2 : s_Layers)
			{
				SetLayerCollision(layer.LayerID, layer2.LayerID, true);
			}
		}

		return layer.LayerID;
	}

	void PhysicsLayerManager::RemoveLayer(uint32_t layerId)
	{
		PhysicsLayer& layerInfo = GetLayer(layerId);

		for (auto& otherLayer : s_Layers)
		{
			if (otherLayer.LayerID == layerId)
				continue;

			if (otherLayer.CollidesWith & layerInfo.BitValue)
			{
				otherLayer.CollidesWith &= ~layerInfo.BitValue;
			}
		}

		RemoveIfExists<std::string>(s_LayerNames, [&](const std::string& name) { return name == layerInfo.Name; });
		RemoveIfExists<PhysicsLayer>(s_Layers, [&](const PhysicsLayer& layer) { return layer.LayerID == layerId; });
	}

	void PhysicsLayerManager::UpdateLayerName(uint32_t layerId, const std::string& newName)
	{
		PhysicsLayer& layer = GetLayer(layerId);
		RemoveIfExists<std::string>(s_LayerNames, [&](const std::string& name) { return name == layer.Name; });
		s_LayerNames.insert(s_LayerNames.begin() + layerId, newName);
		layer.Name = newName;
	}

	void PhysicsLayerManager::SetLayerCollision(uint32_t layerId, uint32_t otherLayer, bool shouldCollide)
	{
		if (ShouldCollide(layerId, otherLayer) && shouldCollide)
			return;

		PhysicsLayer& layerInfo = GetLayer(layerId);
		PhysicsLayer& otherLayerInfo = GetLayer(otherLayer);

		if (shouldCollide)
		{
			layerInfo.CollidesWith |= otherLayerInfo.BitValue;
			otherLayerInfo.CollidesWith |= layerInfo.BitValue;
		}
		else
		{
			layerInfo.CollidesWith &= ~otherLayerInfo.BitValue;
			otherLayerInfo.CollidesWith &= ~layerInfo.BitValue;
		}
	}

	Vector<PhysicsLayer> PhysicsLayerManager::GetLayerCollisions(uint32_t layerId)
	{
		const PhysicsLayer& layer = GetLayer(layerId);

		std::vector<PhysicsLayer> layers;
		for (const auto& otherLayer : s_Layers)
		{
			if (otherLayer.LayerID == layerId)
				continue;

			if (layer.CollidesWith & otherLayer.BitValue)
				layers.push_back(otherLayer);
		}

		return layers;
	}

	bool PhysicsLayerManager::ShouldCollide(uint32_t layer1, uint32_t layer2)
	{
		return GetLayer(layer1).CollidesWith & GetLayer(layer2).BitValue;
	}

	bool PhysicsLayerManager::IsLayerValid(uint32_t layerId)
	{
		const PhysicsLayer& layer = GetLayer(layerId);
		return layer.LayerID != s_NullLayer.LayerID && layer.IsValid();
	}

	PhysicsLayer& PhysicsLayerManager::GetLayer(uint32_t layerId)
	{
		return layerId >= s_Layers.size() ? s_NullLayer : s_Layers[layerId];
	}

	PhysicsLayer& PhysicsLayerManager::GetLayer(const std::string& layerName)
	{
		for (auto& layer : s_Layers)
		{
			if (layer.Name == layerName)
			{
				return layer;
			}
		}

		return s_NullLayer;
	}

	void PhysicsLayerManager::ClearLayers()
	{
		s_Layers.clear();
		s_LayerNames.clear();
		AddLayer("Default");
	}

	uint32_t PhysicsLayerManager::GetNextLayerID()
	{
		int32_t lastId = -1;

		for (const auto& layer : s_Layers)
		{
			if (lastId != -1)
			{
				if (layer.LayerID != lastId + 1)
				{
					return lastId + 1;
				}
			}

			lastId = layer.LayerID;
		}

		return (uint32_t)s_Layers.size();
	}

	Vector<PhysicsLayer> PhysicsLayerManager::s_Layers;
	Vector<std::string> PhysicsLayerManager::s_LayerNames;
	PhysicsLayer PhysicsLayerManager::s_NullLayer = { 0, "NULL", 0, -1 };

}