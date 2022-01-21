#include "frostpch.h"
#include "InspectorPanel.h"

#include "../UserInterface/UIWidgets.h"
#include <imgui.h>
#include <imgui/imgui_internal.h>

namespace Frost
{
	void InspectorPanel::Init(void* initArgs)
	{
	}

	void InspectorPanel::Render()
	{
		if (!m_Visibility)
			return;

		if (ImGui::Begin("Inspector", &m_Visibility))
		{
			Entity& entity = m_SceneHierarchy->GetSelectedEntity();
			if (entity)
			{
				DrawComponents(entity);
			}
		}
		ImGui::End();

	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
		if (entity.HasComponent<T>())
		{
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
			ImGui::PopStyleVar();
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("ComponentSettings");
			}

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				uiFunction(component);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponent<T>();
		}
	}

	void InspectorPanel::DrawComponents(Entity& entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			std::strncpy(buffer, tag.c_str(), sizeof(buffer));
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);


		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			Entity selectedEntity = m_SceneHierarchy->GetSelectedEntity();

			// Transform component
			if (ImGui::MenuItem("Transform"))
			{
				if (!selectedEntity.HasComponent<TransformComponent>())
					selectedEntity.AddComponent<TransformComponent>();
				else
					FROST_CORE_WARN("This entity already has the Transform Component!");
			}

			// Mesh component
			if (ImGui::MenuItem("Mesh"))
			{
				if (!selectedEntity.HasComponent<MeshComponent>())
					selectedEntity.AddComponent<MeshComponent>();
				else
					FROST_CORE_WARN("This entity already has the Mesh Component!");
			}

			// Point light component
			if (ImGui::MenuItem("Point Light"))
			{
				if (!selectedEntity.HasComponent<PointLightComponent>())
					selectedEntity.AddComponent<PointLightComponent>();
				else
					FROST_CORE_WARN("This entity already has the Point Light Component!");
			}

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();



		DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
		{
			UserInterface::DrawVec3CoordsEdit("Translation", component.Translation);

			glm::vec3 rotation = glm::degrees(component.Rotation);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
			UserInterface::DrawVec3CoordsEdit("Rotation", rotation);
			
			component.Rotation = glm::radians(rotation);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
			UserInterface::DrawVec3CoordsEdit("Scale", component.Scale, 1.0f);
		});

		DrawComponent<MeshComponent>("Mesh", entity, [](auto& component)
		{
			std::string meshFilepath = "";
			if (component.Mesh.Raw() != nullptr)
			{
				if (component.Mesh->IsLoaded())
				{
					meshFilepath = component.Mesh->GetFilepath();
				}
			}

			std::string path = UserInterface::DrawFilePath("File Path", meshFilepath, "fbx");
			if (!path.empty())
			{
				component.Mesh = Mesh::Load(path);
			}
		});


		DrawComponent<PointLightComponent>("Point Light", entity, [](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("PointLightProperties", 2, flags))
			{
				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Radiance");

					ImGui::TableNextColumn();
					UserInterface::DrawVec3ColorEdit("", component.Color);

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Intensity");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Intensity, 0.055f);

					ImGui::PopID();
				}


				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Radius");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Radius, 0.055f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Falloff");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Falloff, 0.055f);

					ImGui::PopID();
				}

				
				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});



	}

	void InspectorPanel::OnEvent(Event& e)
	{
	}

	void InspectorPanel::Shutdown()
	{

	}
}