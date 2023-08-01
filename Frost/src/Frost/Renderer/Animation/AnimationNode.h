#pragma once

#include "Frost/Core/UUID.h"
#include "Frost/Core/Engine.h"
#include <glm/glm.hpp>

namespace Frost
{
	using NodeHandle = UUID;
	using PinHandle = UUID;
	using LinkHandle = UUID;
	using InputNodeHandle = UUID;

	/*enum class AnimationPinType
	{
		Float, Int, Bool, Animation
	};*/

	struct AnimationInput
	{
		enum class Type
		{
			None, Float, Int, Bool, Animation
		};

		static std::string GetStringFromInputType(AnimationInput::Type type)
		{
			switch (type)
			{
				case AnimationInput::Type::Float:      return "Float";
				case AnimationInput::Type::Int:        return "Int";
				case AnimationInput::Type::Bool:       return "Bool";
				case AnimationInput::Type::Animation:  return "Animation";
				case AnimationInput::Type::None:       return "None";
			}
		}
		
		static AnimationInput::Type GetInputTypeFromString(const std::string& type)
		{
			if(type == "Float") return AnimationInput::Type::Float;
			else if(type == "Int") return AnimationInput::Type::Int;
			else if(type == "Bool") return AnimationInput::Type::Bool;
			else if (type == "Animation") return AnimationInput::Type::Animation;
			
			return AnimationInput::Type::None;
		}

		std::string Name;
		Type InputType;
		Ref<void*> Data;
	};

	enum class AnimationPinKind
	{
		Input, Output
	};

	struct AnimationPinInfo
	{
		AnimationPinInfo(AnimationInput::Type pinType, AnimationPinKind pinKind, const NodeHandle& parentNode, const PinHandle& pinHandle, const glm::vec4& pinColor)
		{
			PinType = pinType;
			AnimationParentNode = parentNode;
			PinHandle = pinHandle;
			ConnectedPin = nullptr;
			PinColor = pinColor;
			PinKind = pinKind;
		}

		bool IsPinColored() const
		{
			return PinColor != glm::vec4(1.0f);
		}

		bool IsConnectedPinValid() const
		{
			return ConnectedPin != nullptr;
		}

		NodeHandle AnimationParentNode;
		PinHandle PinHandle;
		glm::vec4 PinColor = glm::vec4(1.0f);
		AnimationInput::Type PinType;
		AnimationPinKind PinKind;

		AnimationPinInfo* ConnectedPin = nullptr;
	};

	struct AnimationPinLink
	{
		AnimationPinLink(AnimationPinInfo* pinA, AnimationPinInfo* pinB)
		{
			LinkID = LinkHandle();
			PinA = pinA;
			PinB = pinB;
		}

		LinkHandle LinkID;
		AnimationPinInfo* PinA = nullptr;
		AnimationPinInfo* PinB = nullptr;
	};

	class AnimationNode
	{
	public:
		enum class NodeType
		{
			Blend, Input, Output, Condition, None
		};

		NodeHandle NodeID = NodeHandle();
		AnimationNode::NodeType Type;

		void AddInput(AnimationInput::Type pinType, const std::string& name, const glm::vec4& pinColor = glm::vec4(1.0f))
		{
			PinHandle pinHandle = PinHandle();
			Inputs[name] = pinHandle;
			InputData[pinHandle] = Ref<AnimationPinInfo>::Create(pinType, AnimationPinKind::Input, NodeID, pinHandle, pinColor);
		}

		void AddCustomInput(const std::string& name, Ref<AnimationPinInfo> customPin)
		{
			Inputs[name] = customPin->PinHandle;
			InputData[customPin->PinHandle] = Ref<AnimationPinInfo>::Create(customPin->PinType, AnimationPinKind::Input, NodeID, customPin->PinHandle, customPin->PinColor);
		}

		void AddOutput(AnimationInput::Type pinType, const std::string& name, const glm::vec4& pinColor = glm::vec4(1.0f))
		{
			PinHandle pinHandle = PinHandle();
			Outputs[name] = pinHandle;
			OutputData[pinHandle] = Ref<AnimationPinInfo>::Create(pinType, AnimationPinKind::Output, NodeID, pinHandle, pinColor);
		}

		void AddCustomOutput(const std::string& name, Ref<AnimationPinInfo> customPin)
		{
			Outputs[name] = customPin->PinHandle;
			OutputData[customPin->PinHandle] = Ref<AnimationPinInfo>::Create(customPin->PinType, AnimationPinKind::Output, NodeID, customPin->PinHandle, customPin->PinColor);
		}

		void SetConnectedInputPin(const PinHandle& pinID, AnimationPinInfo* connectedPin)
		{
			InputData[pinID]->ConnectedPin = connectedPin;
		}

		void SetConnectedInputPinByName(const std::string& name, AnimationPinInfo* connectedPin)
		{
			SetConnectedInputPin(Inputs[name], connectedPin);
		}

		void SetConnectedOutputPin(const PinHandle& pinID, AnimationPinInfo* connectedPin)
		{
			OutputData[pinID]->ConnectedPin = connectedPin;
		}

		void SetConnectedOutputPinByName(const std::string& name, AnimationPinInfo* connectedPin)
		{
			SetConnectedOutputPin(Outputs[name], connectedPin);
		}

		Ref<AnimationPinInfo> GetInputPinByName(const std::string& name)
		{
			return InputData[Inputs[name]];
		}

		Ref<AnimationPinInfo> GetOutputPinByName(const std::string& name)
		{
			return OutputData[Outputs[name]];
		}

		HashMap<std::string, PinHandle> Inputs;
		HashMap<PinHandle, Ref<AnimationPinInfo>> InputData; // Have to make Ref because we also have this in the AnimationBlueprint

		HashMap<std::string, PinHandle> Outputs;
		HashMap<PinHandle, Ref<AnimationPinInfo>> OutputData; // Have to make Ref because we also have this in the AnimationBlueprint

	public:
		static std::string GetStringFromNodeType(NodeType type)
		{
			switch (type)
			{
			case AnimationNode::NodeType::Blend:     return "Blend";
			case AnimationNode::NodeType::Input:     return "Input";
			case AnimationNode::NodeType::Output:    return "Output";
			case AnimationNode::NodeType::Condition: return "Condition";
			case AnimationNode::NodeType::None:      return "None";
			}
			return "None";
		}

		static NodeType GetNodeTypeFromString(const std::string& type)
		{
			if (type == "Blend")         return NodeType::Blend;
			else if(type == "Input")     return NodeType::Input;
			else if(type == "Output")    return NodeType::Output;
			else if(type == "Condition") return NodeType::Condition;
			
			return NodeType::None;
		}
	};

	class InputAnimationNode : public AnimationNode
	{
	public:
		InputAnimationNode(const std::string& varName, AnimationInput::Type varType)
		{
			Type = AnimationNode::NodeType::Input;

			glm::vec4 typeColor = GetColorByInputType(varType);

			InputType = varType;
			TypeColor = typeColor;

			AddOutput(varType, varName, typeColor);
		}

		InputAnimationNode(InputAnimationNode* customInputAnimationNode)
		{
			Type = AnimationNode::NodeType::Input;
			NodeID = customInputAnimationNode->NodeID;

			InputType = customInputAnimationNode->InputType;
			TypeColor = customInputAnimationNode->TypeColor;

			std::string outputPinName = (*customInputAnimationNode->Outputs.begin()).first;
			Ref<AnimationPinInfo> outputPinInfo = (*customInputAnimationNode->OutputData.begin()).second;

			AddCustomOutput(outputPinName, outputPinInfo);
		}

		static glm::vec4 GetColorByInputType(AnimationInput::Type type)
		{
			switch (type)
			{
			case AnimationInput::Type::Float:
				return glm::vec4(0.5f, 0.67f, 0.36f, 1.0f); break; // Green color
			case AnimationInput::Type::Int:
				return glm::vec4(0.85f, 0.4f, 1.0f, 1.0f); break; // Pink-violet color
			case AnimationInput::Type::Bool:
				return glm::vec4(1.0f, 0.40f, 0.53f, 1.0f); break; // Red color
			case AnimationInput::Type::Animation:
				return glm::vec4(0.38f, 0.57f, 0.91f, 1.0f); break; // Blue color
			}
			return glm::vec4(1.0f);
		}

		AnimationInput::Type InputType;
		glm::vec4 TypeColor;

		InputNodeHandle InputHandle;
	};

	class BlendAnimationNode : public AnimationNode
	{
	public:
		BlendAnimationNode()
		{
			Type = AnimationNode::NodeType::Blend;

			AddInput(AnimationInput::Type::Animation, "A");
			AddInput(AnimationInput::Type::Float, "Blend Factor");
			AddInput(AnimationInput::Type::Animation, "B");
			
			AddOutput(AnimationInput::Type::Animation, "Pose");
		}

		// Copy constructor
		BlendAnimationNode(BlendAnimationNode* customBlendAnimationNode)
		{
			Type = AnimationNode::NodeType::Blend;
			NodeID = customBlendAnimationNode->NodeID;

			AddCustomInput("A", customBlendAnimationNode->GetInputPinByName("A"));
			AddCustomInput("Blend Factor", customBlendAnimationNode->GetInputPinByName("Blend Factor"));
			AddCustomInput("B", customBlendAnimationNode->GetInputPinByName("B"));

			AddCustomOutput("Pose", customBlendAnimationNode->GetOutputPinByName("Pose"));
		}
	};

	class OutputAnimationNode : public AnimationNode
	{
	public:
		OutputAnimationNode()
		{
			Type = AnimationNode::NodeType::Output;
		
			AddInput(AnimationInput::Type::Float, "Skeleton", glm::vec4(1.0f, 0.38f, 0.38f, 1.0f));
			AddInput(AnimationInput::Type::Animation, "Pose");
		}

		// Copy constructor
		OutputAnimationNode(OutputAnimationNode* customOutputAnimationNode)
		{
			Type = AnimationNode::NodeType::Output;
			NodeID = customOutputAnimationNode->NodeID;

			AddCustomInput("Skeleton", customOutputAnimationNode->GetInputPinByName("Skeleton"));
			AddCustomInput("Pose", customOutputAnimationNode->GetInputPinByName("Pose"));
		}
	};

	class ConditionAnimationNode : public AnimationNode
	{
	public:
		ConditionAnimationNode()
		{
			Type = AnimationNode::NodeType::Condition;
	
			AddInput(AnimationInput::Type::Bool, "Condition");
			AddInput(AnimationInput::Type::Animation, "True", glm::vec4(0.38f, 1.0f, 0.38f, 1.0f));
			AddInput(AnimationInput::Type::Animation, "False", glm::vec4(1.0f, 0.38f, 0.38f, 1.0f));

			AddOutput(AnimationInput::Type::Animation, "Result");
		}

		// Copy constructor
		ConditionAnimationNode(ConditionAnimationNode* customConditionAnimationNode)
		{
			Type = AnimationNode::NodeType::Condition;
			NodeID = customConditionAnimationNode->NodeID;

			AddCustomInput("Condition", customConditionAnimationNode->GetInputPinByName("Condition"));
			AddCustomInput("True", customConditionAnimationNode->GetInputPinByName("True"));
			AddCustomInput("False", customConditionAnimationNode->GetInputPinByName("False"));

			AddCustomOutput("Result", customConditionAnimationNode->GetOutputPinByName("Result"));
		}
	};

}