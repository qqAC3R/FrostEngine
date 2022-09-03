#include "frostpch.h"
#include "SceneHierarchyPanel.h"

#include "Frost/InputCodes/KeyCodes.h"
#include "Frost/Renderer/Renderer.h"
#include <imgui.h>

#include "IconsFontAwesome.hpp"

#define HIERARCHY_ENTITY_DND "HiEnT!"
#define NO_SUBMESH_SELECTED UINT32_MAX

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

		//ImVec4* colors = ImGui::GetStyle().Colors;
		//colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.12f, 0.15f, 1.0f));
		//0.12f, 0.14f, 0.17f, 1.00f
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		

		if (ImGui::Begin("Scene Hierarchy", &m_Visibility))
		{

			ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
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
				if (ImGui::MenuItem("Fog Box Volume"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Fog Box");
					m_SelectedEntity.AddComponent<FogBoxVolumeComponent>();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Camera"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Camera");
					m_SelectedEntity.AddComponent<CameraComponent>();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Sky Light"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Sky Light");
					m_SelectedEntity.AddComponent<SkyLightComponent>();
				}
				if (ImGui::MenuItem("Point Light"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Point Light");
					m_SelectedEntity.AddComponent<PointLightComponent>();
				}
				if (ImGui::MenuItem("Directional Light"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Drectional Light");
					m_SelectedEntity.AddComponent<DirectionalLightComponent>();
				}
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar();

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

					// Only draw the entities which are not parented, i.e. at top level
					if (entity.GetParent() == 0)
					{
						ImGui::TableNextColumn();

						DrawEntityNode(entity);
					}

					ImGui::PopID();
					idCounter++;
				});
				ImGui::EndTable();

			}
			ImGui::PopStyleVar();

			//ImGui::PopStyleColor();
		}


		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			m_SelectedEntity = {};
			m_SelectedSubmesh = NO_SUBMESH_SELECTED;
		}


		ExecuteLateFunctions();

		ImGui::End();
		ImGui::PopStyleColor(3);
		//ImGui::PopStyleColor(2);


		//colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);// RESET
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
		std::string& tag = entity.GetComponent<TagComponent>().Tag;
		//tag = tag;

		ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanFullWidth;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

		bool isEntitySelected = false;
		if (m_SelectedEntity == entity)
		{
			if (m_SelectedSubmesh == NO_SUBMESH_SELECTED)
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
			else
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

			isEntitySelected = true;

		}

		// Check if entity has submeshes, if it doesn't we should draw it as a leaf
		bool entityHasSubmeshes = false;
		if (entity.HasComponent<MeshComponent>())
		{
			const MeshComponent& entityMeshComponent = entity.GetComponent<MeshComponent>();

			if (entityMeshComponent.Mesh)
			{
				entityHasSubmeshes = entityMeshComponent.Mesh->GetSubMeshes().size() >= 1 ? true : false;
			}
		}

		// If the entity has 0 children, then it should have an arrow
		if (!entity.GetChildren().size() && !entityHasSubmeshes)
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		

		if (ImGui::IsItemClicked())
		{
			m_SelectedEntity = entity;
			m_SelectedSubmesh = NO_SUBMESH_SELECTED;
		}

		if (isEntitySelected)
		{
			ImGui::PopStyleColor();
		}


		// Drag and drop
		if (ImGui::BeginDragDropSource())
		{
			ImGui::Text(entity.GetComponent<TagComponent>().Tag.c_str());
			ImGui::SetDragDropPayload(HIERARCHY_ENTITY_DND, &entity, sizeof(Entity));
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HIERARCHY_ENTITY_DND);
			if (payload)
			{
				Entity& droppedEntity = *(Entity*)payload->Data;
				m_SceneContext->ParentEntity(entity, droppedEntity);
			}
			ImGui::EndDragDropTarget();
		}

		if (m_SelectedEntity && isEntitySelected)
		{
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32({ 0.15f, 0.15f, 0.15f, 1.0f }));
		}

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
			{
				entityDeleted = true;
			}
			if (entity.GetParent() != 0)
			{
				if (ImGui::MenuItem("UnParent"))
				{
					m_SceneContext->UnparentEntity(entity);
				}
			}

			ImGui::EndPopup();
		}





		if (opened)
		{

			if (entity.HasComponent<MeshComponent>())
			{
				const MeshComponent& entityMeshComponent = entity.GetComponent<MeshComponent>();

				if (entityMeshComponent.Mesh)
				{
					const Vector<Submesh>& submeshes = entityMeshComponent.Mesh->GetSubMeshes();

					uint32_t i = 0;
					uint32_t randomNumber = 0;
					for (auto submesh : submeshes)
					{
						//ImGuiTreeNodeFlags submeshFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth;
						ImGuiTreeNodeFlags submeshFlags = ImGuiTreeNodeFlags_Leaf;

						randomNumber = rand() % UINT32_MAX;

						bool isSubmeshSelected = false;
						if (m_SelectedSubmesh == i)
						{
							ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
							submeshFlags |= ImGuiTreeNodeFlags_Selected;
							isSubmeshSelected = true;
						}

						bool submeshOpened = false;
						if (submesh.MeshName == "")
						{
							std::string submeshDefaultName = "Submesh" + std::to_string(i);
							submeshOpened = ImGui::TreeNodeEx((void*)uint64_t(uint32_t(entity) + i), submeshFlags, submeshDefaultName.c_str());
						}
						else
						{
							submeshOpened = ImGui::TreeNodeEx((void*)uint64_t(uint32_t(entity) + i), submeshFlags, submesh.MeshName.c_str());
						}

						if (ImGui::IsItemClicked())
						{
							m_SelectedSubmesh = i;
							m_SelectedEntity = entity;
						}

						if (submeshOpened)
						{
							ImGui::TreePop();
						}


						if (isSubmeshSelected)
						{
							ImGui::PopStyleColor();
						}


						i++;
					}
				}
			}

			// Draw children, via recursive function call
			auto& k = entity.GetComponent<ParentChildComponent>().ChildIDs;
			
			for (auto& child : k)
			{
				Entity e = m_SceneContext->FindEntityByUUID(child);
				if (e)
				{
					DrawEntityNode(e);
				}
			}
			ImGui::TreePop();


			/*
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			bool opened = ImGui::TreeNodeEx((void*)9817239, flags, tag.c_str());
			if (opened)
				ImGui::TreePop();
			*/
			//ImGui::TreePop();
		}

		if (!entity.GetComponent<ParentChildComponent>().ParentID)
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Entity");
		}

		if (entityDeleted)
		{
			AddLateFunction([&, entity]()
			{
				if (m_SelectedEntity == entity)
				{
					m_SelectedEntity = {};
					m_SelectedSubmesh = NO_SUBMESH_SELECTED;
				}

				m_SceneContext->DestroyEntity(entity);
			});

		}
	}

	void SceneHierarchyPanel::AddLateFunction(const std::function<void()>& func)
	{
		m_LateDeletionFunctions.push_back(func);
	}

	void SceneHierarchyPanel::ExecuteLateFunctions()
	{
		for (auto& func : m_LateDeletionFunctions)
			func();

		m_LateDeletionFunctions.clear();
	}

	void SceneHierarchyPanel::Shutdown()
	{
	}
}