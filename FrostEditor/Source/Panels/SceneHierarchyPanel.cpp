#include "frostpch.h"
#include "SceneHierarchyPanel.h"

#include "Frost/InputCodes/KeyCodes.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/EntitySystem/Prefab.h"
#include "Frost/Utils/PlatformUtils.h"
#include "Frost/Asset/AssetManager.h"
#include "Panels/ContentBrowser/ContentBrowser.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "IconsFontAwesome.hpp"

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

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.12f, 0.15f, 1.0f));
		//0.12f, 0.14f, 0.17f, 1.00f
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 1.0f, 2.0f });

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
				if (ImGui::MenuItem("Text"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Text");
					TextComponent& textComponent = m_SelectedEntity.AddComponent<TextComponent>();
					textComponent.FontAsset = Renderer::GetDefaultFont();
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
					TransformComponent& ts = m_SelectedEntity.GetComponent<TransformComponent>();
					ts.Rotation = { -90.0f, 0.0f, 0.0f};
				}
				if (ImGui::MenuItem("Rectangular Light"))
				{
					m_SelectedEntity = m_SceneContext->CreateEntity("Rectangular Light");
					m_SelectedEntity.AddComponent<RectangularLightComponent>();
					TransformComponent& ts = m_SelectedEntity.GetComponent<TransformComponent>();
				}
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(3, 161, 252, 255)); // Prefab blue color
				if (ImGui::MenuItem("Import Prefab"))
				{
					std::string path = FileDialogs::OpenFile("");
					
					if (!path.empty())
					{
						Ref<Prefab> prefab = AssetManager::GetOrLoadAsset<Prefab>(path);
						if (prefab)
						{
							m_SceneContext->Instantiate(prefab);
						}
						else
						{
							FROST_CORE_ERROR("Prefab cannot be initialized or is not valid!");
						}
					}
				}
				ImGui::PopStyleColor();

				ImGui::EndPopup();
			}
			ImGui::PopStyleVar(); // ImGuiStyleVar_PopupRounding

#if 0
			ImRect windowRect = { ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax() };
			if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetCurrentWindow()->ID))
			{
				const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP, ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
				ContentBrowserDragDropData* dragDropData = (ContentBrowserDragDropData*)payload->Data;

				if (payload)
				{
					// "Prefab type" might also be used as entity for the scene hierarchy panel. In this way, we dont have to add another AssetType "Entity"
					if (dragDropData->AssetType == AssetType::Prefab)
					{
						Entity& entity = *(Entity*)dragDropData->Data;
						m_SceneContext->UnparentEntity(entity);
					}
					delete dragDropData->Data;
				}


				ImGui::EndDragDropTarget();
			}
#endif


			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("HierarchyTable", 2, flags))
			{
				ImGui::TableSetupColumn("  Name", ImGuiTableColumnFlags_NoHide);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 12.0f);
				ImGui::TableHeadersRow();

				uint32_t idCounter = 0;
				m_SceneContext->GetRegistry().each([&](auto entityID)
				{
					ImGui::PushID(idCounter);
					Entity entity{ entityID , m_SceneContext.Raw() };

					if (entity.HasComponent<PrefabComponent>())
						ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(3, 161, 252, 255)); // Prefab Blue Color

					// Only draw the entities which are not parented, i.e. at top level
					if (entity.GetParent() == 0)
					{
						ImGui::TableNextColumn();

						DrawEntityNode(entity);
					}

					if (entity.HasComponent<PrefabComponent>())
						ImGui::PopStyleColor();

					ImGui::PopID();
					idCounter++;
				});
				ImGui::EndTable();

			}
			ImGui::PopStyleVar(); // ImGuiStyleVar_CellPadding
		}
		


		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			m_SelectedEntity = {};
			m_SelectedSubmesh = NO_SUBMESH_SELECTED;
		}


		ExecuteLateFunctions();

		ImGui::End();
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(); // ImGuiStyleVar_WindowPadding
	}

	void SceneHierarchyPanel::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& event) -> bool
		{
			if (event.GetKeyCode() == Key::Delete)
			{
				if (m_SelectedEntity)
				{
					m_SceneContext->DestroyEntity(m_SelectedEntity);
					m_SelectedEntity = {};
				}
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
				entityHasSubmeshes = entityMeshComponent.Mesh->GetMeshAsset()->GetSubMeshes().size() >= 1 ? true : false;
			}
		}

		// If the entity has 0 children, then it should have an arrow
		if (!entity.GetChildren().size() && !entityHasSubmeshes)
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		
		bool isClicked = ImGui::IsItemClicked();

		// Drag and drop
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
		{
			ImGui::Text(entity.GetComponent<TagComponent>().Tag.c_str());

			ContentBrowserDragDropData dragDropData(AssetType::Prefab, &entity, sizeof(Entity));

			ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, &dragDropData, sizeof(ContentBrowserDragDropData));
			ImGui::EndDragDropSource();

			if (m_SelectedEntity)
			{
				m_SelectedEntity = m_PreviousEntity;
				m_SelectedSubmesh = NO_SUBMESH_SELECTED;
			}
		}
		else
		{
			if (isClicked)
			{
				m_PreviousEntity = m_SelectedEntity;
				m_SelectedEntity = entity;
				m_SelectedSubmesh = NO_SUBMESH_SELECTED;
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
			if (payload)
			{
#if 0
				Entity& droppedEntity = *(Entity*)payload->Data;
				m_SceneContext->ParentEntity(entity, droppedEntity);
#endif
				ContentBrowserDragDropData dragDropData = *(ContentBrowserDragDropData*)payload->Data;

				// "Prefab type" might also be used as entity for the scene hierarchy panel. In this way, we dont have to add another AssetType "Entity"
				if (dragDropData.AssetType == AssetType::Prefab)
				{
					Entity& droppedEntity = *(Entity*)dragDropData.Data;
					m_SceneContext->ParentEntity(entity, droppedEntity);
				}

				//delete dragDropData.Data;

			}
			ImGui::EndDragDropTarget();
		}


		if (isEntitySelected)
		{
			ImGui::PopStyleColor();
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

			
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(3, 161, 252, 255)); // Set blue color text, specific for prefabs
			if (ImGui::MenuItem("Save As Prefab"))
			{
				std::string path = FileDialogs::SaveFile("");

				if (!path.empty())
				{
					bool isAssetAlreadyLoaded = true;


					// Using `GetAsset` instead of `LoadOrGetAsset` because we need to check if it is loaded in memory or not
					Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(path);
					if (!prefab)
					{
						// We are temporarily creating (or loading) a physics material asset.
						// The only purpose of using `LoadAsset` or `CreateNewAsset` is to register the asset into the registry file and set the parameters.
						// After that we serialize and delete the asset from memory.
						prefab = AssetManager::LoadAsset<Prefab>(path);
						if (!prefab)
							prefab = AssetManager::CreateNewAsset<Prefab>(path);

						isAssetAlreadyLoaded = false;
					}
					prefab->Create(entity);

					AssetImporter::Serialize(prefab);
					if (!isAssetAlreadyLoaded)
						AssetManager::RemoveAssetFromMemory(prefab->Handle);
				}
				//entity
			}
			ImGui::PopStyleColor();


			ImGui::EndPopup();
		}





		if (opened)
		{

			if (entity.HasComponent<MeshComponent>())
			{
				const MeshComponent& entityMeshComponent = entity.GetComponent<MeshComponent>();

				if (entityMeshComponent.Mesh)
				{
					const Vector<Submesh>& submeshes = entityMeshComponent.Mesh->GetMeshAsset()->GetSubMeshes();

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
		}

		if (!entity.GetComponent<ParentChildComponent>().ParentID)
		{
			ImGui::TableNextColumn();

			if (entity.HasComponent<PrefabComponent>())
				ImGui::TextUnformatted("Prefab");
			else
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

				Renderer::SubmitDeletion([&, entity]()
				{
					m_SceneContext->DestroyEntity(entity);
				});
			});

		}
	}

	void SceneHierarchyPanel::SetSelectedEntityByID(uint32_t entityID)
	{
		if (entityID == UINT32_MAX)
		{
			m_SelectedEntity = {};
			return;
		}

		bool entityFound = false;

		m_SceneContext->GetRegistry().each([&, entityID](auto entity)
		{
			Entity ent = { entity, m_SceneContext.Raw() };
			if ((uint32_t)entity == entityID)
			{
				m_SelectedEntity = ent;
				entityFound = true;
				return;
			}
		});

		if(!entityFound)
			m_SelectedEntity = {};
	}

	void SceneHierarchyPanel::AddLateFunction(const std::function<void()>& func)
	{
		m_LateDeletionFunctions.AddFunction(func);
	}

	void SceneHierarchyPanel::ExecuteLateFunctions()
	{
		m_LateDeletionFunctions.Execute();
	}

	void SceneHierarchyPanel::Shutdown()
	{
	}

}