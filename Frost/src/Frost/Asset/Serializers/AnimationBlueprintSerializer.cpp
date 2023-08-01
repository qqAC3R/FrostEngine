#include "frostpch.h"
#include "AnimationBlueprintSerializer.h"

#include "Frost/Asset/AssetManager.h"

#include "Frost/Renderer/Animation.h"
#include "Frost/Renderer/Mesh.h"

#include <json/nlohmann/json.hpp>

namespace Frost
{

	void AnimationBlueprintSerializer::Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const
	{
		SerializeBlueprint(AssetManager::GetFileSystemPathString(metadata), asset.As<AnimationBlueprint>());
	}

	bool AnimationBlueprintSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const
	{
		if (!FileSystem::Exists(AssetManager::GetFileSystemPathString(metadata)))
			return false;

		const MeshAsset* meshAsset = (const MeshAsset*)pNext;
		Ref<AnimationBlueprint> animationBlueprint = Ref<AnimationBlueprint>::Create(meshAsset);
		
		bool isDerivedFromSameAssetMesh = DeserializeBlueprint(AssetManager::GetFileSystemPathString(metadata), animationBlueprint);
		if (!isDerivedFromSameAssetMesh)
			return false;

		asset = animationBlueprint;
		asset->Handle = metadata.Handle;

		return true;
	}

	Ref<Asset> AnimationBlueprintSerializer::CreateAssetRef(const AssetMetadata& metadata, void* pNext) const
	{
		const MeshAsset* meshAsset = (const MeshAsset*)pNext;
		Ref<AnimationBlueprint> animationBlueprint = Ref<AnimationBlueprint>::Create(meshAsset);
		return animationBlueprint.As<Asset>();
	}

	void AnimationBlueprintSerializer::SerializeBlueprint(const std::string& filepath, Ref<AnimationBlueprint> animationBlueprint)
	{
		nlohmann::ordered_json out = nlohmann::ordered_json();

		out["MeshAsset"] = animationBlueprint->m_MeshAsset->Handle.Get();

		nlohmann::ordered_json& inputs = out["Inputs"];
		for (auto& [inputHandle, inputInfo] : animationBlueprint->m_InputMap)
		{
			nlohmann::ordered_json input = nlohmann::ordered_json();
			input["Handle"] = inputHandle.Get();
			input["Type"] = AnimationInput::GetStringFromInputType(inputInfo.InputType);
			input["Name"] = inputInfo.Name;
			if (inputInfo.InputType == AnimationInput::Type::Animation)
			{
				input["Data"] = inputInfo.Data.As<Animation>()->GetName();
			}
			else
			{
				switch (inputInfo.InputType)
				{
					case AnimationInput::Type::Float: input["Data"] = *inputInfo.Data.As<float>().Raw(); break;
					case AnimationInput::Type::Int:	  input["Data"] = *inputInfo.Data.As<int>().Raw(); break;
					case AnimationInput::Type::Bool:  input["Data"] = *inputInfo.Data.As<bool>().Raw(); break;
				}
			}

			inputs.push_back(input);
		}

		nlohmann::ordered_json& nodes = out["Nodes"];
		for (auto& [nodeHandle, animationNode] : animationBlueprint->m_NodeMap)
		{
			nlohmann::ordered_json node = nlohmann::ordered_json();

			node["Handle"] = nodeHandle.Get();
			node["Type"] = AnimationNode::GetStringFromNodeType(animationNode->Type);

			// Store the curret position of the node in the grid
			glm::vec2 nodePositionInGrid = animationBlueprint->m_NodePositionsMap[nodeHandle];
			node["NodePositionInGrid"] = { nodePositionInGrid.x, nodePositionInGrid.y };
			
			// Input node requires some additional parameters to add
			if(animationNode->Type == AnimationNode::NodeType::Input)
			{
				Ref<InputAnimationNode> inputAnimationNode = animationNode.As<InputAnimationNode>();
				node["InputType"] = AnimationInput::GetStringFromInputType(inputAnimationNode->InputType);
				node["InputHandle"] = inputAnimationNode->InputHandle.Get();
			}

			// Store the input pins information
			{
				nlohmann::ordered_json& inputPins = node["InputPins"];
				
				for (auto& [pinName, pinHandle] : animationNode->Inputs)
				{
					nlohmann::ordered_json inputPin = nlohmann::ordered_json();
					inputPin["PinName"] = pinName;
					inputPin["Handle"] = pinHandle.Get();

					Ref<AnimationPinInfo> pinInfo = animationNode->InputData[pinHandle];
					inputPin["AnimationParentNode"] = pinInfo->AnimationParentNode.Get();

					inputPin["PinType"] = AnimationInput::GetStringFromInputType(pinInfo->PinType);
					inputPin["PinColor"] = { pinInfo->PinColor.x, pinInfo->PinColor.y, pinInfo->PinColor.z, pinInfo->PinColor.w };

					inputPins.push_back(inputPin);
				}
			}

			// Store the output pins information
			{
				nlohmann::ordered_json& outputPins = node["OutputPins"];

				for (auto& [pinName, pinHandle] : animationNode->Outputs)
				{
					nlohmann::ordered_json outputPin = nlohmann::ordered_json();
					outputPin["PinName"] = pinName;
					outputPin["Handle"] = pinHandle.Get();

					Ref<AnimationPinInfo> pinInfo = animationNode->OutputData[pinHandle];
					outputPin["AnimationParentNode"] = pinInfo->AnimationParentNode.Get();

					outputPin["PinType"] = AnimationInput::GetStringFromInputType(pinInfo->PinType);
					outputPin["PinColor"] = { pinInfo->PinColor.x, pinInfo->PinColor.y, pinInfo->PinColor.z, pinInfo->PinColor.w };

					outputPins.push_back(outputPin);
				}
			}

			nodes.push_back(node);
		}

		nlohmann::ordered_json& pinLinks = out["PinLinks"];
		for (auto& pinkLink : animationBlueprint->m_PinLinks)
		{
			nlohmann::ordered_json pinLink = nlohmann::ordered_json();
			pinLink["LinkID"] = pinkLink.LinkID.Get();
			
			pinLink["ParentNodeA"] = pinkLink.PinA->AnimationParentNode.Get();
			pinLink["PinA"] = pinkLink.PinA->PinHandle.Get();

			pinLink["ParentNodeB"] = pinkLink.PinB->AnimationParentNode.Get();
			pinLink["PinB"] = pinkLink.PinB->PinHandle.Get();

			pinLinks.push_back(pinLink);
		}

		std::ofstream istream(filepath);
		istream.clear();
		istream << out.dump(4);
		istream.close();
	}

	bool AnimationBlueprintSerializer::DeserializeBlueprint(const std::string& filepath, Ref<AnimationBlueprint>& animationBlueprint)
	{
		std::string content;
		std::ifstream instream(filepath);

		// Get the scene name from filepath
		instream.seekg(0, std::ios::end);
		size_t size = instream.tellg();
		content.resize(size);

		instream.seekg(0, std::ios::beg);
		instream.read(&content[0], size);
		instream.close();

		// Parse the json file
		nlohmann::json in = nlohmann::json::parse(content);

		// If the animation blueprint derives from a different mesh asset, we shouldnt even consider loading it
		if (in["MeshAsset"] != animationBlueprint->m_MeshAsset->Handle.Get())
			return false;

		animationBlueprint->m_NodeMap.clear();
		animationBlueprint->m_InputMap.clear();
		animationBlueprint->m_PinMap.clear();
		animationBlueprint->m_OutputNode = nullptr;

		// Store inputs
		uint32_t inputSize = in["Inputs"].size();
		for (uint32_t i = 0; i < inputSize; i++)
		{
			nlohmann::json input = in["Inputs"][i];

			AnimationInput& animationInput = animationBlueprint->m_InputMap[InputNodeHandle(input["Handle"])];
			animationInput.Name = input["Name"];
			animationInput.InputType = AnimationInput::GetInputTypeFromString(input["Type"]);

			switch (animationInput.InputType)
			{
				case AnimationInput::Type::Float:      animationInput.Data = Ref<float>::Create(input["Data"]).As<void*>(); break;
				case AnimationInput::Type::Int:        animationInput.Data = Ref<int32_t>::Create(input["Data"]).As<void*>(); break;
				case AnimationInput::Type::Bool:       animationInput.Data = Ref<bool>::Create(input["Data"]).As<void*>(); break;
				case AnimationInput::Type::Animation: 
				{
					animationInput.Data = nullptr;
					for (auto& animation : animationBlueprint->GetAppropiateMeshAsset()->GetAnimations())
					{
						if (animation->GetName() == animationInput.Name)
						{
							animationInput.Data = animation.As<void*>();
							break;
						}
					}
					break;
				}
			}
		}

		// Store nodes
		uint32_t nodesSize = in["Nodes"].size();
		for (uint32_t i = 0; i < nodesSize; i++)
		{
			nlohmann::json node = in["Nodes"][i];

			Ref<AnimationNode> animationNode = nullptr;

			AnimationNode::NodeType animationNodeType = AnimationNode::GetNodeTypeFromString(node["Type"]);
			switch (animationNodeType)
			{
				case AnimationNode::NodeType::Blend:   	 animationNode = Ref<BlendAnimationNode>::Create();	    break;
				case AnimationNode::NodeType::Output:    animationNode = Ref<OutputAnimationNode>::Create();    break;
				case AnimationNode::NodeType::Condition: animationNode = Ref<ConditionAnimationNode>::Create(); break;
				case AnimationNode::NodeType::Input:
				{
					Ref<InputAnimationNode> inputAnimationNode = Ref<InputAnimationNode>::Create(
						animationBlueprint->m_InputMap[InputNodeHandle(node["InputHandle"])].Name,
						AnimationInput::GetInputTypeFromString(node["InputType"])
					);
					inputAnimationNode->InputHandle = InputNodeHandle(node["InputHandle"]);
					inputAnimationNode->TypeColor = InputAnimationNode::GetColorByInputType(inputAnimationNode->InputType);

					animationNode = inputAnimationNode;
					break;
				}
			}
			animationNode->NodeID = NodeHandle(node["Handle"]);
			animationNode->Type = animationNodeType;

			animationNode->Inputs.clear();
			animationNode->InputData.clear();
			animationNode->Outputs.clear();
			animationNode->OutputData.clear();


			uint32_t inputPinSize = node["InputPins"].size();
			for (uint32_t j = 0; j < inputPinSize; j++)
			{
				nlohmann::json inputPin = node["InputPins"][j];

				std::string pinName = inputPin["PinName"];
				InputNodeHandle pinHandle = inputPin["Handle"];
				AnimationInput::Type pinType = AnimationInput::GetInputTypeFromString(inputPin["PinType"]);

				Ref<AnimationPinInfo> customPin = Ref<AnimationPinInfo>::Create(
					pinType,
					AnimationPinKind::Input,
					animationNode->NodeID,
					pinHandle,
					glm::vec4(inputPin["PinColor"][0], inputPin["PinColor"][1], inputPin["PinColor"][2], inputPin["PinColor"][3])
				);

				animationNode->AddCustomInput(pinName, customPin);
			}

			uint32_t outputPinSize = node["OutputPins"].size();
			for (uint32_t j = 0; j < outputPinSize; j++)
			{
				nlohmann::json outputPin = node["OutputPins"][j];

				std::string pinName = outputPin["PinName"];
				InputNodeHandle pinHandle = outputPin["Handle"];
				AnimationInput::Type pinType = AnimationInput::GetInputTypeFromString(outputPin["PinType"]);

				Ref<AnimationPinInfo> customPin = Ref<AnimationPinInfo>::Create(
					pinType,
					AnimationPinKind::Input,
					animationNode->NodeID,
					pinHandle,
					glm::vec4(outputPin["PinColor"][0], outputPin["PinColor"][1], outputPin["PinColor"][2], outputPin["PinColor"][3])
				);

				animationNode->AddCustomOutput(pinName, customPin);
			}


			switch (animationNodeType)
			{
				case AnimationNode::NodeType::Blend:   	 animationBlueprint->AddCustomBlendNode(animationNode); break;
				case AnimationNode::NodeType::Output:    animationBlueprint->AddCustomOutputNode(animationNode); break;
				case AnimationNode::NodeType::Condition: animationBlueprint->AddCustomConditionNode(animationNode); break;
				case AnimationNode::NodeType::Input:	 animationBlueprint->AddCustomInputNode(animationNode); break;
			}

			animationBlueprint->m_NodePositionsMap[animationNode->NodeID] = { 
				node["NodePositionInGrid"][0], node["NodePositionInGrid"][1] 
			};
		}

		uint32_t pinLinksSize = in["PinLinks"].size();
		for (uint32_t j = 0; j < pinLinksSize; j++)
		{
			nlohmann::json pinLink = in["PinLinks"][j];

			NodeHandle nodePinA = NodeHandle(pinLink["ParentNodeA"]);
			NodeHandle nodePinB = NodeHandle(pinLink["ParentNodeB"]);
			PinHandle pinA = PinHandle(pinLink["PinA"]);
			PinHandle pinB = PinHandle(pinLink["PinB"]);
			LinkHandle linkID = PinHandle(pinLink["LinkID"]);

			animationBlueprint->AddPinLinkWithCustomID(linkID, animationBlueprint->m_PinMap[nodePinA][pinA].Raw(), animationBlueprint->m_PinMap[nodePinB][pinB].Raw());
		}
	}

}