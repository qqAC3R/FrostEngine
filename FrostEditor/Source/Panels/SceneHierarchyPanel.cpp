#include "frostpch.h"
#include "SceneHierarchyPanel.h"

#include "Frost/InputCodes/KeyCodes.h"
#include "Frost/Renderer/Renderer.h"
#include <imgui.h>

namespace Frost
{
	void SceneHierarchyPanel::Init(void* initArgs)
	{
		m_SelectedEntity = {};
		m_SceneContext = nullptr;
	}

	void SceneHierarchyPanel::Render()
	{
		if (!m_Visibility)
		{
			return;
		}

		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
		if (ImGui::Begin("Scene Hierarchy", &m_Visibility))
		{

			if (ImGui::BeginPopup("Add Entity") || (ImGui::BeginPopupContextWindow(nullptr, 1, false)))
			{
				if (ImGui::MenuItem("Empty Entity"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Entity");
				}
				if (ImGui::MenuItem("Mesh"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Mesh");
					m_SelectedEntity.AddComponent<MeshComponent>();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Point Light"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Point Light");
					m_SelectedEntity.AddComponent<PointLightComponent>();
				}
				ImGui::EndPopup();
			}

			
			
			
			
			

			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("HierarchyTable", 2, flags))
			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 12.0f);
				ImGui::TableHeadersRow();

				uint32_t idCounter = 0;
				m_SceneContext->GetRegistry().each([&](auto entityID)
				{
					ImGui::PushID(idCounter);
					Entity entity{ entityID , m_SceneContext.Raw() };

					ImGui::TableNextColumn();
					DrawEntityNode(entity);

					ImGui::PopID();
					idCounter++;
				});
				ImGui::EndTable();

			}
			ImGui::PopStyleVar();

		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			m_SelectedEntity = {};


		ImGui::PopStyleColor(2);
		ImGui::End();
	}

	void SceneHierarchyPanel::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& event) -> bool
		{
			if (event.GetKeyCode() == Key::Delete)
			{
				m_SceneContext->DestroyEntity(m_SelectedEntity);
				m_SelectedEntity = {};
			}
			return false;
		});
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity& entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = 0;
		if (m_SelectedEntity == entity)
		{
			flags = ImGuiTreeNodeFlags_Selected;
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
		}

		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (m_SelectedEntity == entity)
		{
			ImGui::PopStyleColor();
		}

		if (ImGui::IsItemClicked())
		{
			m_SelectedEntity = entity;
		}



		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (opened)
		{
			//ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			//bool opened = ImGui::TreeNodeEx((void*)9817239, flags, tag.c_str());
			//if (opened)
			//	ImGui::TreePop();
			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			m_SceneContext->DestroyEntity(entity);
			if (m_SelectedEntity == entity)
				m_SelectedEntity = {};
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Entity");
	}

	void SceneHierarchyPanel::Shutdown()
	{
	}
}