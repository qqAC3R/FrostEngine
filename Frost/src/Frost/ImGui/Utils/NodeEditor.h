#pragma once

#include "Frost/Core/UUID.h"
#include "Frost/Renderer/Animation/AnimationNode.h"

#include <imgui.h>
#include "imgui_node_editor.h"

namespace ImGui
{
	void SubmitPinWithText(const char* text, int32_t id, ax::NodeEditor::PinKind pinKind);


	struct NodeBuilder
	{
		NodeBuilder(Frost::UUID nodeID, Frost::AnimationNode::NodeType nodeType);
		~NodeBuilder();

		void RenderHeader(const char* name, const ImU32& color);

		Frost::UUID CurrentNodeID;
		Frost::AnimationNode::NodeType NodeType;
		ImU32 HeaderColor;
		ImVec2 HeaderMin;
		ImVec2 HeaderMax;
		float HeaderHeight;
	};

	class NodePin
	{
	public:
		NodePin(const std::string& name, Frost::UUID nodePinID, ax::NodeEditor::PinKind pinKind);
		virtual ~NodePin();

		Frost::UUID CurrentNodePinID;
	};

}