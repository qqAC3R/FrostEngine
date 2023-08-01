#include "frostpch.h"
#include "NodeEditor.h"

#include "Frost/Renderer/Texture.h"
#include "Frost/Core/Application.h"
#include "Frost/ImGui/ImGuiLayer.h"
#include "Frost/ImGui/Utils/ScopedStyle.h"

namespace ImGui
{
	namespace ed = ax::NodeEditor;

	void SubmitPinWithText(const char* text, int32_t id, ax::NodeEditor::PinKind pinKind)
	{
		ed::BeginPin(id, pinKind);
		ImGui::Text(text);
		ed::EndPin();
	}

	//  TODO: This is never deleted
	static Frost::Ref<Frost::Texture2D> s_BlueprintBackground = nullptr;
	static Frost::Ref<Frost::Texture2D> s_BlueprintInputBackground = nullptr;

	NodeBuilder::NodeBuilder(Frost::UUID nodeID, Frost::AnimationNode::NodeType nodeType)
	{
		CurrentNodeID = nodeID;
		NodeType = nodeType;
		HeaderHeight = 22.0f;

		if (!s_BlueprintBackground)
			s_BlueprintBackground = Frost::Texture2D::Create("Resources/Editor/NodeEditor/BlueprintBackground.png");
		if(!s_BlueprintInputBackground)
			s_BlueprintInputBackground = Frost::Texture2D::Create("Resources/Editor/NodeEditor/BlueprintInputBackground.png");

		ImGui::PushID(nodeID.Get());

		ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));
		ed::BeginNode(nodeID.Get());

		std::string nodeIDString = "node_" + std::to_string(nodeID.Get());
		ImGui::BeginVertical(nodeIDString.c_str());
	}

	NodeBuilder::~NodeBuilder()
	{
		ImGui::EndVertical();

		HeaderMin = ImGui::GetItemRectMin();
		HeaderMax = ImGui::GetItemRectMax();
		HeaderMax.y = HeaderMin.y + HeaderHeight;

		ed::EndNode();
		ed::PopStyleVar();
		ImGui::PopID();

		if (ImGui::IsItemVisible())
		{
			auto drawList = ed::GetNodeBackgroundDrawList(CurrentNodeID.Get());

			//const auto halfBorderWidth = ed::GetStyle().NodeBorderWidth * 0.5f;

			auto alpha = static_cast<int>(255 * ImGui::GetStyle().Alpha);
			auto headerColor = IM_COL32(0, 0, 0, alpha) | (HeaderColor & IM_COL32(255, 255, 255, 0));

			if ((HeaderMax.x > HeaderMin.x) && (HeaderMax.y > HeaderMin.y) && s_BlueprintBackground)
			{
				ImVec2 uv = ImVec2(0.9f, 0.9f);

				Frost::ImGuiLayer* imguiLayer = Frost::Application::Get().GetImGuiLayer();

				ImTextureID textureID = 0;
				if (NodeType == Frost::AnimationNode::NodeType::Input)
					textureID = imguiLayer->GetImGuiTextureID(s_BlueprintInputBackground->GetImage2D());
				else
					textureID = imguiLayer->GetImGuiTextureID(s_BlueprintBackground->GetImage2D());

				drawList->AddImageRounded(textureID,
					ImVec2(HeaderMin.x - 7.0f, HeaderMin.y - 3.0f),
					ImVec2(HeaderMax.x + 7.0f, HeaderMax.y + 3.0f),
					ImVec2(0.1f, 0.0f), uv,
					headerColor, ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop
				);

			}
		}
	}

	void NodeBuilder::RenderHeader(const char* name, const ImU32& color)
	{
		HeaderColor = color;
		ImGui::ScopedFontStyle fontStyle(ScopedFontStyle::FontType::Bold);

		ImGui::BeginHorizontal("header");

		ImGui::Spring(0);
		ImGui::TextUnformatted(name);
		ImGui::Spring(1);
		ImGui::Dummy(ImVec2(0, HeaderHeight));

		ImGui::EndHorizontal();
	}

	NodePin::NodePin(const std::string& name, Frost::UUID nodePinID, ax::NodeEditor::PinKind pinKind)
	{
		ed::BeginPin(nodePinID.Get(), pinKind);

		CurrentNodePinID = nodePinID;

		std::string nodePinNameID = "NodePin_" + std::to_string(nodePinID.Get());
		ImGui::BeginHorizontal(nodePinNameID.c_str());


		// TODO: This if for selecting the appropriate nodes (have to change alpha if the input cannot create a link)
		auto alpha = ImGui::GetStyle().Alpha;
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

		//ImGui::Spring(0);
		ImGui::TextUnformatted(name.c_str());
		//ImGui::Spring(0);
	}

	NodePin::~NodePin()
	{
		ImGui::PopStyleVar();

		ImGui::EndHorizontal();

		ed::EndPin();
	}

}
