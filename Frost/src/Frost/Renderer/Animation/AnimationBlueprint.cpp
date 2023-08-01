#include "frostpch.h"
#include "AnimationBlueprint.h"

#include "Frost/Core/FunctionQueue.h"

#include "Frost/Renderer/Animation.h"
#include "Frost/Renderer/Mesh.h"

namespace Frost
{
	AnimationBlueprint::AnimationBlueprint(const MeshAsset* meshAsset)
		: m_MeshAsset(meshAsset), m_IsAnimationGraphBaked(false)
	{
		// Set up the initial output node
		Ref<OutputAnimationNode> outputAnimationNode = Ref<OutputAnimationNode>::Create();

		m_NodeMap[outputAnimationNode->NodeID] = outputAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : outputAnimationNode->InputData)
			m_PinMap[outputAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : outputAnimationNode->OutputData)
			m_PinMap[outputAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		m_OutputNode = outputAnimationNode.As<AnimationNode>();

		// After that, we should add all the animation inputs
		const Vector<Ref<Animation>>& animations = meshAsset->GetAnimations();
		for (auto animation : animations)
		{
			AddAnimationInput(animation->GetName(), animation);
		}
	}

	void AnimationBlueprint::AddInput(const std::string& varName, AnimationInput::Type varType)
	{
		InputNodeHandle inputHandle = InputNodeHandle();
		AnimationInput& animationInput = m_InputMap[inputHandle];
		animationInput.Name = varName;
		animationInput.InputType = varType;

		switch (varType)
		{
			case AnimationInput::Type::Float:      animationInput.Data = Ref<float>::Create().As<void*>(); break;
			case AnimationInput::Type::Int:        animationInput.Data = Ref<int32_t>::Create().As<void*>(); break;
			case AnimationInput::Type::Bool:       animationInput.Data = Ref<bool>::Create().As<void*>(); break;
			case AnimationInput::Type::Animation:  FROST_ASSERT_MSG("Cannot add input as type Animation. An Animation pointer needs to be passed! (Try using `AddAnimationInput`)");
		}
		//inputAnimationNode->InputHandle = inputHandle;
	}

	void AnimationBlueprint::AddAnimationInput(const std::string& varName, Ref<Animation> animation)
	{
		InputNodeHandle inputHandle = InputNodeHandle();

		AnimationInput& animationInput = m_InputMap[inputHandle];
		animationInput.Name = varName;
		animationInput.InputType = AnimationInput::Type::Animation;
		animationInput.Data = animation.As<Animation>();
	}

	void AnimationBlueprint::DeleteInput(const InputNodeHandle& inputHandle)
	{
		FunctionQueue lateFunction;

		// We firstly delete all the input nodes that are directly linked to the input data
		for (auto& [nodeHandle, animationNode] : m_NodeMap)
		{
			if (animationNode->Type == AnimationNode::NodeType::Input)
			{
				if (animationNode.As<InputAnimationNode>()->InputHandle == inputHandle)
				{
					NodeHandle nodeHandleCopy = nodeHandle; // Copying because the lambda cannot capture the `nodeHandle`
					lateFunction.AddFunction([this, nodeHandleCopy]() {
						DeleteNode(nodeHandleCopy);
					});
				}
			}
		}

		lateFunction.Execute();

		// After all the deletions, we can finally delete the input data
		m_InputMap.erase(inputHandle);
	}

	Ref<AnimationNode> AnimationBlueprint::AddInputNode(const InputNodeHandle& inputHandle)
	{
		AnimationInput& animationInput = m_InputMap[inputHandle];

		Ref<InputAnimationNode> inputAnimationNode = Ref<InputAnimationNode>::Create(animationInput.Name, animationInput.InputType);
		inputAnimationNode->InputHandle = inputHandle;

		m_NodeMap[inputAnimationNode->NodeID] = inputAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : inputAnimationNode->InputData)
			m_PinMap[inputAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : inputAnimationNode->OutputData)
			m_PinMap[inputAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		FROST_CORE_INFO("Added Input Node (ID: {0})", inputAnimationNode->NodeID);

		return inputAnimationNode.As<AnimationNode>();
	}

	Ref<AnimationNode> AnimationBlueprint::AddBlendNode()
	{
		Ref<BlendAnimationNode> blendAnimationNode = Ref<BlendAnimationNode>::Create();
		m_NodeMap[blendAnimationNode->NodeID] = blendAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : blendAnimationNode->InputData)
			m_PinMap[blendAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : blendAnimationNode->OutputData)
			m_PinMap[blendAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		FROST_CORE_INFO("Added Blend Node (ID: {0})", blendAnimationNode->NodeID);

		return blendAnimationNode.As<AnimationNode>();
	}

	Ref<AnimationNode> AnimationBlueprint::AddConditionNode()
	{
		Ref<ConditionAnimationNode> conditionAnimationNode = Ref<ConditionAnimationNode>::Create();
		m_NodeMap[conditionAnimationNode->NodeID] = conditionAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : conditionAnimationNode->InputData)
			m_PinMap[conditionAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : conditionAnimationNode->OutputData)
			m_PinMap[conditionAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		FROST_CORE_INFO("Added Condition Node (ID: {0})", conditionAnimationNode->NodeID);

		return conditionAnimationNode.As<AnimationNode>();
	}

	void AnimationBlueprint::SetFloatInput(const std::string& inputName, float input)
	{
		// This is very inefficient, however due to its simplicity and relying on the fact that blueprints shouldn't be really complex,
		// this should cause any big performance issues.
		for (auto& [inputHandle, inputInfo] : m_InputMap)
		{
			if (inputInfo.Name == inputName)
			{
				Ref<float> data = inputInfo.Data.As<float>();
				memcpy(data.Raw(), &input, sizeof(float));
			}
		}
	}

	void AnimationBlueprint::SetIntInput(const std::string& inputName, int32_t input)
	{
		// This is very inefficient, however due to its simplicity and relying on the fact that blueprints shouldn't be really complex,
		// this should cause any big performance issues.
		for (auto& [inputHandle, inputInfo] : m_InputMap)
		{
			if (inputInfo.Name == inputName)
			{
				Ref<int32_t> data = inputInfo.Data.As<int32_t>();
				memcpy(data.Raw(), &input, sizeof(int32_t));
			}
		}
	}

	void AnimationBlueprint::SetBoolInput(const std::string& inputName, bool input)
	{
		// This is very inefficient, however due to its simplicity and relying on the fact that blueprints shouldn't be really complex,
		// this should cause any big performance issues.
		for (auto& [inputHandle, inputInfo] : m_InputMap)
		{
			if (inputInfo.Name == inputName)
			{
				Ref<bool> data = inputInfo.Data.As<bool>();
				memcpy(data.Raw(), &input, sizeof(bool));
			}
		}
	}

	Ref<AnimationNode> AnimationBlueprint::GetAnimationNodeFromPin(AnimationPinInfo* pinInfo)
	{
		return m_NodeMap[pinInfo->AnimationParentNode];
	}

	void AnimationBlueprint::DeleteNode(const NodeHandle& nodeHandle)
	{
		Ref<AnimationNode> animationNode = m_NodeMap[nodeHandle];

		// Firstly, delete the pin links with this node
		uint32_t pinkLinkIndex = 0;
		for(auto& pinLink : m_PinLinks)
		{
			if (pinLink.PinA->AnimationParentNode == nodeHandle || pinLink.PinB->AnimationParentNode == nodeHandle)
			{
				ErasePinLink(pinkLinkIndex);
			}
			pinkLinkIndex++;
		}

		// After that, delete the pins
		for (auto& [inputPinUUID, inputPinInfo] : animationNode->InputData)
			m_PinMap[animationNode->NodeID].erase(inputPinUUID);
		for (auto& [outputPinUUID, outputPinInfo] : animationNode->OutputData)
			m_PinMap[animationNode->NodeID].erase(outputPinUUID);

		m_PinMap.erase(animationNode->NodeID);

		m_NodeMap.erase(nodeHandle);
	}

	void AnimationBlueprint::AddPinLink(AnimationPinInfo* inputPin, AnimationPinInfo* outputPin)
	{
		AnimationPinLink animationPinkLink = AnimationPinLink(inputPin, outputPin);
		m_PinLinks.push_back(animationPinkLink);

		inputPin->ConnectedPin = outputPin;
		outputPin->ConnectedPin = inputPin;
	}

	void AnimationBlueprint::AddPinLinkWithCustomID(LinkHandle customLinkID, AnimationPinInfo* inputPin, AnimationPinInfo* outputPin)
	{
		AnimationPinLink animationPinkLink = AnimationPinLink(inputPin, outputPin);
		animationPinkLink.LinkID = customLinkID;
		m_PinLinks.push_back(animationPinkLink);

		inputPin->ConnectedPin = outputPin;
		outputPin->ConnectedPin = inputPin;
	}

	void AnimationBlueprint::ErasePinLink(uint32_t index)
	{
		AnimationPinLink* pinkLink = (m_PinLinks.begin() + index)._Ptr;
		pinkLink->PinA->ConnectedPin = nullptr;
		pinkLink->PinB->ConnectedPin = nullptr;
		m_PinLinks.erase(m_PinLinks.begin() + index);
	}

	void AnimationBlueprint::Copy(AnimationBlueprint* animationBlueprint)
	{
		m_IsAnimationGraphBaked = false;

		m_InputMap.clear();
		m_NodeMap.clear();
		m_PinMap.clear();
		m_OutputNode = nullptr;
		m_PinLinks.clear();
		m_NodePositionsMap.clear();

		// Copy node positions
		m_NodePositionsMap = animationBlueprint->m_NodePositionsMap;

		// Copy Inputs
		for (auto& [inputHandle, customAnimationInput] : animationBlueprint->m_InputMap)
		{
			AnimationInput& animationInput = m_InputMap[inputHandle];
			animationInput.Name = customAnimationInput.Name;
			animationInput.InputType = customAnimationInput.InputType;

			void* customInputData = customAnimationInput.Data.Raw();

			switch (animationInput.InputType)
			{
				case AnimationInput::Type::Float:      animationInput.Data = Ref<float>::Create(*(float*)customInputData).As<void*>(); break;
				case AnimationInput::Type::Int:        animationInput.Data = Ref<int32_t>::Create(*(int32_t*)customInputData).As<void*>(); break;
				case AnimationInput::Type::Bool:       animationInput.Data = Ref<bool>::Create(*(bool*)customInputData).As<void*>(); break;
				case AnimationInput::Type::Animation:  animationInput.Data = customAnimationInput.Data.As<void*>();
			}
		}

		// Copy nodes
		for (auto& [nodeHandle, animationNodePointer] : animationBlueprint->m_NodeMap)
		{
			switch (animationNodePointer->Type)
			{
				case AnimationNode::NodeType::Input:
					AddCustomInputNode(animationNodePointer);
					break;
				case AnimationNode::NodeType::Blend:
					AddCustomBlendNode(animationNodePointer);
					break;
				case AnimationNode::NodeType::Condition:
					AddCustomConditionNode(animationNodePointer);
					break;
				case AnimationNode::NodeType::Output:
					AddCustomOutputNode(animationNodePointer);
					break;
			}
		}

		// Copy node links
		for (auto& pinkLink : animationBlueprint->m_PinLinks)
		{
			Ref<AnimationPinInfo> pinA = m_PinMap.at(pinkLink.PinA->AnimationParentNode)[pinkLink.PinA->PinHandle];
			Ref<AnimationPinInfo> pinB = m_PinMap.at(pinkLink.PinB->AnimationParentNode)[pinkLink.PinB->PinHandle];
			AddPinLinkWithCustomID(pinkLink.LinkID, pinA.Raw(), pinB.Raw());
		}
	}

	Ref<AnimationNode> AnimationBlueprint::AddCustomInputNode(Ref<AnimationNode> customAnimationNode)
	{
		InputNodeHandle inputHandle = customAnimationNode.As<InputAnimationNode>()->InputHandle;
		AnimationInput& animationInput = m_InputMap[inputHandle];

		Ref<InputAnimationNode> inputAnimationNode = Ref<InputAnimationNode>::Create(customAnimationNode.As<InputAnimationNode>().Raw());
		inputAnimationNode->InputHandle = inputHandle;

		m_NodeMap[inputAnimationNode->NodeID] = inputAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : inputAnimationNode->InputData)
			m_PinMap[inputAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : inputAnimationNode->OutputData)
			m_PinMap[inputAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		FROST_CORE_INFO("Added Custom Input Node (ID: {0})", inputAnimationNode->NodeID);

		return inputAnimationNode.As<AnimationNode>();
	}

	Ref<AnimationNode> AnimationBlueprint::AddCustomOutputNode(Ref<AnimationNode> customAnimationNode)
	{
		// Set up the initial output node
		Ref<OutputAnimationNode> outputAnimationNode = Ref<OutputAnimationNode>::Create(customAnimationNode.As<OutputAnimationNode>().Raw());

		m_NodeMap[outputAnimationNode->NodeID] = outputAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : outputAnimationNode->InputData)
			m_PinMap[outputAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : outputAnimationNode->OutputData)
			m_PinMap[outputAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		m_OutputNode = outputAnimationNode.As<AnimationNode>();
		return m_OutputNode;
	}

	Ref<AnimationNode> AnimationBlueprint::AddCustomBlendNode(Ref<AnimationNode> customAnimationNode)
	{
		Ref<BlendAnimationNode> blendAnimationNode = Ref<BlendAnimationNode>::Create(customAnimationNode.As<BlendAnimationNode>().Raw());
		m_NodeMap[blendAnimationNode->NodeID] = blendAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : blendAnimationNode->InputData)
			m_PinMap[blendAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : blendAnimationNode->OutputData)
			m_PinMap[blendAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		FROST_CORE_INFO("Added Custom Blend Node (ID: {0})", blendAnimationNode->NodeID);

		return blendAnimationNode.As<AnimationNode>();
	}

	Ref<AnimationNode> AnimationBlueprint::AddCustomConditionNode(Ref<AnimationNode> customAnimationNode)
	{
		Ref<ConditionAnimationNode> conditionAnimationNode = Ref<ConditionAnimationNode>::Create(customAnimationNode.As<ConditionAnimationNode>().Raw());
		m_NodeMap[conditionAnimationNode->NodeID] = conditionAnimationNode.As<AnimationNode>();

		for (auto& [inputPinUUID, inputPinInfo] : conditionAnimationNode->InputData)
			m_PinMap[conditionAnimationNode->NodeID][inputPinUUID] = inputPinInfo;
		for (auto& [outputPinUUID, outputPinInfo] : conditionAnimationNode->OutputData)
			m_PinMap[conditionAnimationNode->NodeID][outputPinUUID] = outputPinInfo;

		FROST_CORE_INFO("Added Custom Condition Node (ID: {0})", conditionAnimationNode->NodeID);

		return conditionAnimationNode.As<AnimationNode>();
	}

	bool AnimationBlueprint::ReloadData(const std::string& filepath)
	{
		return false;
	}
}