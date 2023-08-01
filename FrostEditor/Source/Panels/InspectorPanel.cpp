#include "frostpch.h"
#include "InspectorPanel.h"

#include "Frost/Asset/AssetManager.h"
#include "Frost/Renderer/MaterialAsset.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Renderer/UserInterface/Font.h"
#include "Frost/Renderer/Animation/AnimationBlueprint.h"
#include "Frost/EntitySystem/Prefab.h"

#include "UserInterface/UIWidgets.h"
#include "Frost/ImGui/Utils/ScopedStyle.h"

#include "IconsFontAwesome.hpp"

#include "Frost/Physics/PhysX/CookingFactory.h"
#include "Frost/Physics/PhysicsLayer.h"
#include "Frost/Physics/PhysicsMaterial.h"
#include "Frost/Script/ScriptEngine.h"
#include "EditorLayer.h"

namespace Frost
{

	InspectorPanel::InspectorPanel(EditorLayer* editorLayer)
		: m_PanelNameImGui(ICON_PAINT_BRUSH + std::string(" Inspector")), m_Context(editorLayer)
	{

	}

	void InspectorPanel::Init(void* initArgs)
	{
	}

	void InspectorPanel::Render()
	{
		if (!m_Visibility)
			return;


		if (ImGui::Begin(m_PanelNameImGui.c_str(), &m_Visibility))
		{
			Entity& entity = m_SceneHierarchy->GetSelectedEntity();
			if (entity)
			{
				DrawComponents(entity);
			}
		}

		// Check if the window is focused (also including child window)
		m_IsPanelFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
		m_IsPanelHovered = ImGui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows);

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

			ImGui::Separator();

			// Tree Node
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

			//ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.20f, 0.23f, 1.0f));

			bool open = false;
			{

				ImGui::ScopedFontStyle fontStyle(ImGui::ScopedFontStyle::FontType::Bold);
				open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
			}
			ImGui::PopStyleVar();
			//ImGui::PopStyleColor();
			// End Tree Node


			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f - 10.0f);
			if (ImGui::Button(ICON_COG, ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("ComponentSettings");
			}
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();

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

	static std::string GetNameFromFilepath(const std::string& filepath)
	{
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind(".");
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;

		return filepath.substr(lastSlash, count);
	}

	void InspectorPanel::DrawComponents(Entity& entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			std::strncpy(buffer, tag.c_str(), sizeof(buffer));

			ImGui::Text(ICON_PENCIL);
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}

			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}

		ImGui::SameLine(ImGui::GetWindowWidth() - 70.0f);
		ImGui::PushItemWidth(-1);


		{
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

			std::string str = "ADD  " + std::string(ICON_PLUS);
			if (ImGui::Button(str.c_str()))
			{
				ImGui::OpenPopup("AddComponent");


			}
			ImGui::PopStyleVar();
		}

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

			// Animation component
			if (ImGui::MenuItem("Animation Component"))
			{
				if (!selectedEntity.HasComponent<MeshComponent>())
				{
					FROST_CORE_ERROR("This entity needs to have a Mesh Component for it to have an Animation Component!");
				}
				else
				{
					if (!selectedEntity.HasComponent<AnimationComponent>())
						selectedEntity.AddComponent<AnimationComponent>(&selectedEntity.GetComponent<MeshComponent>());
					else
						FROST_CORE_WARN("This entity already has the Animation Component!");
				}
			}

			ImGui::Separator();

			// Point light component
			if (ImGui::MenuItem("Point Light"))
			{
				if (!selectedEntity.HasComponent<PointLightComponent>())
					selectedEntity.AddComponent<PointLightComponent>();
				else
					FROST_CORE_WARN("This entity already has the Point Light Component!");
			}

			// Sky Light component
			if (ImGui::MenuItem("Sky Light Component"))
			{
				if (!selectedEntity.HasComponent<SkyLightComponent>())
					selectedEntity.AddComponent<SkyLightComponent>();
				else
					FROST_CORE_WARN("This entity already has the Sky Light Component");
			}

			// Rectangular Light component
			if (ImGui::MenuItem("Rectangular Light Component"))
			{
				if (!selectedEntity.HasComponent<RectangularLightComponent>())
					selectedEntity.AddComponent<RectangularLightComponent>();
				else
					FROST_CORE_WARN("This entity already has the Rectangular Light Component");
			}

			ImGui::Separator();

			// Rigid body component
			if (ImGui::MenuItem("Rigid body"))
			{
				if (!selectedEntity.HasComponent<RigidBodyComponent>())
					selectedEntity.AddComponent<RigidBodyComponent>();
				else
					FROST_CORE_WARN("This entity already has the Rigid Body Component!");
			}

			// Box Collider component
			if (ImGui::MenuItem("Box Collider"))
			{
				if (!selectedEntity.HasComponent<BoxColliderComponent>())
				{
					BoxColliderComponent& boxCollider = selectedEntity.AddComponent<BoxColliderComponent>();
					//boxCollider.DebugMesh = Renderer::GetDefaultMeshes().Cube;
				}
				else
					FROST_CORE_WARN("This entity already has the Box Collider Component!");
			}

			// Sphere Collider component
			if (ImGui::MenuItem("Sphere Collider"))
			{
				if (!selectedEntity.HasComponent<SphereColliderComponent>())
					selectedEntity.AddComponent<SphereColliderComponent>();
				else
					FROST_CORE_WARN("This entity already has the Sphere Collider Component!");
			}

			// Capsule Collider component
			if (ImGui::MenuItem("Capsule Collider"))
			{
				if (!selectedEntity.HasComponent<CapsuleColliderComponent>())
					selectedEntity.AddComponent<CapsuleColliderComponent>();
				else
					FROST_CORE_WARN("This entity already has the Capsule Collider Component!");
			}

			// Mesh Collider component
			if (ImGui::MenuItem("Mesh Collider"))
			{
				if (!selectedEntity.HasComponent<MeshColliderComponent>())
				{

					if (selectedEntity.HasComponent<MeshComponent>())
					{
						MeshComponent& meshComponent = selectedEntity.GetComponent<MeshComponent>();
						MeshColliderComponent& meshColliderComponent = selectedEntity.AddComponent<MeshColliderComponent>();

						if (meshComponent.IsMeshValid())
						{
							if (!meshComponent.Mesh->IsAnimated())
							{
								meshColliderComponent.CollisionMesh = meshComponent.Mesh->GetMeshAsset();
								CookingResult result = CookingFactory::CookMesh(meshColliderComponent, false);
							}
							else
							{
								meshColliderComponent.ResetMesh();
								FROST_CORE_WARN("[InspectorPanel] Mesh Colliders don't support dynamic meshes!");
							}
						}
					}
					else
					{
						FROST_CORE_WARN("This entity doesn't have the Mesh Component!");
					}
				}
				else
				{
					FROST_CORE_WARN("This entity already has the Mesh Collider Component!");
				}
			}

			ImGui::Separator();


			// Box Fog Volume component
			if (ImGui::MenuItem("Box Fog Volume"))
			{
				if (!selectedEntity.HasComponent<FogBoxVolumeComponent>())
					selectedEntity.AddComponent<FogBoxVolumeComponent>();
				else
					FROST_CORE_WARN("This entity already has the Box Fog Volume Component!");
			}

			// Cloud Volume component
			if (ImGui::MenuItem("Cloud Volume"))
			{
				if (!selectedEntity.HasComponent<CloudVolumeComponent>())
					selectedEntity.AddComponent<CloudVolumeComponent>();
				else
					FROST_CORE_WARN("This entity already has the Cloud Volume Component!");
			}

			ImGui::Separator();

			// Script component
			if (ImGui::MenuItem("Script Component"))
			{
				if (!selectedEntity.HasComponent<ScriptComponent>())
					selectedEntity.AddComponent<ScriptComponent>();
				else
					FROST_CORE_WARN("This entity already has the Script Component!");
			}

			

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();





		DrawComponent<TransformComponent>("TRANSFORM", entity, [](auto& component)
		{
			UserInterface::DrawVec3CoordsEdit("Translation", component.Translation);

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
			UserInterface::DrawVec3CoordsEdit("Rotation", component.Rotation);

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
			UserInterface::DrawVec3CoordsEdit("Scale", component.Scale, 1.0f);
		});


		AnimationComponent* animationComponent = nullptr;
		if (entity.HasComponent<AnimationComponent>())
		{
			animationComponent = &entity.GetComponent<AnimationComponent>();
		}
		MeshColliderComponent* meshColliderComponent = nullptr;
		if (entity.HasComponent<MeshColliderComponent>())
		{
			meshColliderComponent = &entity.GetComponent<MeshColliderComponent>();
		}
		DrawComponent<MeshComponent>("MESH", entity, [&, meshColliderComponent, animationComponent](auto& component)
		{
			std::string meshFilepath = "";
			if (component.Mesh.Raw() != nullptr)
			{
				if (component.Mesh->IsLoaded())
				{
					meshFilepath = AssetManager::GetRelativePathString(component.Mesh->GetMeshAsset()->GetFilepath());
					
				}
			}

			// Draw the mesh's filepath
			std::string path = UserInterface::DrawFilePath("File Path", meshFilepath, "fbx");
			if (!path.empty())
			{
				Ref<MeshAsset> meshAsset = AssetManager::GetOrLoadAsset<MeshAsset>(path);
				component.Mesh = Ref<Mesh>::Create(meshAsset);

				if (meshColliderComponent != nullptr)
				{
					if (component.IsMeshValid())
					{
						if (!component.Mesh->IsAnimated())
						{
							meshColliderComponent->ResetMesh();
							meshColliderComponent->CollisionMesh = component.Mesh;
							CookingResult result = CookingFactory::CookMesh(*meshColliderComponent, false);
						}
						else
						{
							meshColliderComponent->ResetMesh();
							FROST_CORE_WARN("[InspectorPanel] Mesh Colliders don't support dynamic meshes!");
						}
					}
				}


				if (animationComponent != nullptr)
				{
					animationComponent->Controller->SetAnimationBlueprint(Ref<AnimationBlueprint>::Create(meshAsset.Raw()));
				}
			}

			if (component.Mesh.Raw() != nullptr)
			{

				if (component.Mesh->IsLoaded())
				{

					uint32_t materialCount = component.Mesh->GetMaterialCount();

					const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap;

					const char* treeNodeName = "MeshMaterials";
					bool open = ImGui::TreeNodeEx((void*)treeNodeName, treeNodeFlags, "Materials");

					if (open)
					{
						ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
						constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
						if (ImGui::BeginTable("MeshMaterialsTable", 2, flags))
						{
							for (uint32_t materialIndex = 0; materialIndex < materialCount; materialIndex++)
							{
								ImGui::PushID(materialIndex);

								Ref<MaterialAsset> meshMaterialAsset = component.Mesh->GetMaterialAsset(materialIndex);

								ImGui::TableNextColumn();
								std::string materialIndexStr = "Material [" + std::to_string(materialIndex) + "]";
								ImGui::Text(materialIndexStr.c_str());

								ImGui::TableNextColumn();
								std::string materialName = meshMaterialAsset->GetMaterialName();
								if (materialName.empty())
									materialName = "Default";

								if (AssetManager::IsAssetHandleNonZero(meshMaterialAsset->Handle))
								{
									// We are setting -38.0f to make some room for the "X" button
									ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 38.0f, 20.0f });
								}
								else
								{
									ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });
								}

								// After setting up the button for the material, the DragDrop feature needs to be set
								if (ImGui::BeginDragDropTarget())
								{
									auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
									if (data)
									{
										SelectionData selectionData = *(SelectionData*)data->Data;

										if (selectionData.AssetType == AssetType::Material)
										{
											Ref<MaterialAsset> materialAsset = AssetManager::GetOrLoadAsset<MaterialAsset>(selectionData.FilePath.string());
											Ref<Mesh> mesh = component.Mesh;

											mesh->SetMaterialByAsset(materialIndex, materialAsset);
										}
									}
								}

								if (ImGui::BeginDragDropSource())
								{
									ContentBrowserDragDropData dragDropData(AssetType::Material, meshMaterialAsset.Raw(), sizeof(MaterialAsset));

									ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, &dragDropData, sizeof(ContentBrowserDragDropData));
									ImGui::EndDragDropSource();
								}



								// Opening a new Material
								if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
								{
									std::string materialPath = FileDialogs::OpenFile("");
									if (!materialPath.empty())
									{
										Ref<MaterialAsset> materialAsset = AssetManager::GetOrLoadAsset<MaterialAsset>(materialPath);
										Ref<Mesh> mesh = component.Mesh;

										mesh->SetMaterialByAsset(materialIndex, materialAsset);
									}
								}

								// Saving Material
								if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
								{
									ImGui::OpenPopup("SaveMaterial");
								}

								if (ImGui::BeginPopup("SaveMaterial"))
								{
									if (meshMaterialAsset->Handle != 0)
									{
										if (ImGui::MenuItem("Edit"))
										{
											m_MaterialAssetEditor->SetVisibility(true);
											m_MaterialAssetEditor->SetActiveMaterialAset(meshMaterialAsset);
										}
										if (ImGui::MenuItem("Save"))
										{
											AssetImporter::Serialize(meshMaterialAsset);
										}
									}
									if (ImGui::MenuItem("Save As"))
									{
										std::string materialPath = FileDialogs::SaveFile("");

										if (!materialPath.empty())
										{
											bool isAssetAlreadyLoaded = true;

											// Using `GetAsset` instead of `LoadOrGetAsset` because we need to check if it is loaded in memory or not
											Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(materialPath);
											if (!materialAsset)
											{
												// We are temporarily creating (or loading) a physics material asset.
												// The only purpose of using `LoadAsset` or `CreateNewAsset` is to register the asset into the registry file and set the parameters.
												// After that we serialize and delete the asset from memory.
												materialAsset = AssetManager::LoadAsset<MaterialAsset>(materialPath);

												if(!materialAsset)
													materialAsset = AssetManager::CreateNewAsset<MaterialAsset>(materialPath);
												isAssetAlreadyLoaded = false;
											}

											Ref<Mesh> mesh = component.Mesh;

											// Draw the selected material editor
											Ref<DataStorage> materialData = mesh->GetMaterialAsset(materialIndex)->GetMaterialInternalData();
											materialAsset->CopyFrom(mesh->GetMaterialAsset(materialIndex).Raw());

											if (!isAssetAlreadyLoaded)
											{
												// If the material asset doesn't exist (or not loaded in the engine), then we are temporarily creating one.
												// The only purpose of using `CreateNewAsset` is to register the asset into the registry file.
												// After that we delete the asset.
												AssetManager::RemoveAssetFromMemory(materialAsset->Handle);
											}
											else
											{
												// If the asset is already loaded, we have to set it to the current material's parameters (already has been done)
												// and seralize the material asset
												AssetImporter::Serialize(materialAsset);
											}
										}

									}





									ImGui::EndPopup();
								}

								// If the material asset is not default, there should be an option to delete it and set it to the default
								if (component.Mesh->GetMaterialAsset(materialIndex)->Handle != 0)
								{
									ImGui::SameLine();
									if (ImGui::Button("X", { 25.0f, 20.0f }))
									{
										component.Mesh->SetMaterialAssetToDefault(materialIndex);
									}
								}


								ImGui::PopID();
							}

							ImGui::EndTable();
						}
						ImGui::TreePop();

						ImGui::PopStyleVar();
					}

				}

			}
		});

		DrawComponent<AnimationComponent>("ANIMATION CONTROLLER", entity, [&](auto& component)
		{
			// Draw Animations
			if (component.MeshComponentPtr->Mesh.Raw() != nullptr)
			{
				const Mesh* mesh = component.MeshComponentPtr->Mesh.Raw();
				if (mesh->IsAnimated())
				{
					constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
					ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
					if (ImGui::BeginTable("PointLightProperties", 2, flags))
					{
						ImGui::PushID(0);

						ImGui::TableNextColumn();
						ImGui::Text("BluePrint");

						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-1);

						std::string buttonName = "Default";
						if (component.Controller->GetAnimationBlueprint()->Handle)
						{
							const AssetMetadata& metadata = AssetManager::GetMetadata(component.Controller->GetAnimationBlueprint());
							buttonName = GetNameFromFilepath(metadata.FilePath.string());
						}

						if (component.Controller->GetAnimationBlueprint())
						{
							if (AssetManager::IsAssetHandleNonZero(component.Controller->GetAnimationBlueprint()->Handle))
							{
								// We are setting -38.0f to make some room for the "X" button
								ImGui::Button(buttonName.c_str(), { ImGui::GetColumnWidth() - 38.0f, 20.0f });
							}
							else
							{
								ImGui::Button(buttonName.c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });
							}
						}

						//ImGui::Button(buttonName.c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
						{
							if (component.Controller->GetAnimationBlueprint())
							{
								m_Context->m_AnimationNodeEditor->SetVisibility(true);
								m_Context->m_AnimationNodeEditor->SetActiveAnimationBlueprint(component.Controller->GetAnimationBlueprint());
							}
						}

						// Saving Material
						if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
						{
							ImGui::OpenPopup("SaveBlueprint");
						}

						if (ImGui::BeginPopup("SaveBlueprint"))
						{
							if (ImGui::MenuItem("Save As"))
							{
								std::string materialPath = FileDialogs::SaveFile("");

								if (!materialPath.empty())
								{
									bool isAssetAlreadyLoaded = true;

									// Using `GetAsset` instead of `LoadOrGetAsset` because we need to check if it is loaded in memory or not
									Ref<AnimationBlueprint> animBlueprintAsset = AssetManager::GetAsset<AnimationBlueprint>(materialPath);
									if (!animBlueprintAsset)
									{
										// We are temporarily creating (or loading) a physics material asset.
										// The only purpose of using `LoadAsset` or `CreateNewAsset` is to register the asset into the registry file and set the parameters.
										// After that we serialize and delete the asset from memory.
										animBlueprintAsset = AssetManager::LoadAsset<AnimationBlueprint>(materialPath);
										if (!animBlueprintAsset)
										{
											animBlueprintAsset = AssetManager::CreateNewAsset<AnimationBlueprint>(materialPath, (void*)component.MeshComponentPtr->Mesh->GetMeshAsset().Raw());
										}

										isAssetAlreadyLoaded = false;
									}

									AssetImporter::Serialize(animBlueprintAsset);

									if (!isAssetAlreadyLoaded)
										AssetManager::RemoveAssetFromMemory(animBlueprintAsset->Handle);
								}
							}
							ImGui::EndPopup();
						}


						if (ImGui::BeginDragDropSource())
						{
							ContentBrowserDragDropData dragDropData(
								AssetType::AnimationBlueprint,
								component.Controller->GetAnimationBlueprint().Raw(), sizeof(AnimationBlueprint)
							);

							ImGui::TextUnformatted(buttonName.c_str());
							ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, &dragDropData, sizeof(ContentBrowserDragDropData));
							ImGui::EndDragDropSource();
						}
	
						// After setting up the button for the physics material, the DragDrop feature needs to be set
						if (ImGui::BeginDragDropTarget())
						{
							auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
							if (data)
							{
								SelectionData selectionData = *(SelectionData*)data->Data;

								if (selectionData.AssetType == AssetType::AnimationBlueprint)
								{
									Ref<AnimationBlueprint> animBlueprintAsset = AssetManager::GetOrLoadAsset<AnimationBlueprint>(selectionData.FilePath.string(), (void*)component.MeshComponentPtr->Mesh->GetMeshAsset().Raw());
									component.Controller->SetAnimationBlueprint(animBlueprintAsset);
									animBlueprintAsset->AnimationGraphNeedsToBeBaked();
								}
							}
						}

						// If the physics material asset is not default, there should be an option to delete it and set it to the default
						if (component.Controller->GetAnimationBlueprint())
						{
							if (AssetManager::IsAssetHandleNonZero(component.Controller->GetAnimationBlueprint()->Handle))
							{
								ImGui::SameLine();
								if (ImGui::Button("X", { 25.0f, 20.0f }))
								{
									component.ResetAnimationBlueprint();
								}
							}
						}
						

						ImGui::PopID();

						

						ImGui::EndTable();
					}
					ImGui::PopStyleVar();
					
					


				}
			}
		});

		DrawComponent<SkyLightComponent>("SKY LIGHT", entity, [&](auto& component)
		{
			std::string skyLightFilepath = "";
			if (component.IsValid())
			{
				skyLightFilepath = component.Filepath;
			}

			// Draw the mesh's filepath
			std::string path = UserInterface::DrawFilePath("File Path", skyLightFilepath, "hdr");
			if (!path.empty())
			{
				Renderer::GetSceneEnvironment()->ComputeEnvironmentMap(path, component.RadianceMap, component.PrefilteredMap, component.IrradianceMap);

				component.Filepath = path;
				component.IsActive = true;

				Renderer::GetSceneEnvironment()->SetHDREnvironmentMap(component.RadianceMap, component.PrefilteredMap, component.IrradianceMap);
			}

			if (component.IsValid())
			{
				ImGui::Checkbox("Active", &component.IsActive);
			}
		});

		DrawComponent<PointLightComponent>("POINT LIGHT", entity, [](auto& component)
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
					ImGui::PushItemWidth(-1);
					UserInterface::DrawVec3ColorEdit("", component.Color);

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Intensity");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Intensity, 0.055f);

					ImGui::PopID();
				}


				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Radius");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Radius, 0.055f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Falloff");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Falloff, 0.055f);

					ImGui::PopID();
				}


				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});

		DrawComponent<DirectionalLightComponent>("DIRECTIONAL LIGHT", entity, [](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("DirectionalLightProperties", 2, flags))
			{
				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Radiance");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DrawVec3ColorEdit("", component.Color);

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Intensity");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Intensity, 0.055f);

					ImGui::PopID();
				}


				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Size");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Size, 0.01f, 0.0f, 3.0f);

					ImGui::PopID();
				}

				ImGui::Separator();

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Volume Density");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.VolumeDensity, 0.01f, 0.0f, 1.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Volume Absorption");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Absorption, 0.01f, 0.0f, 100.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(5);

					ImGui::TableNextColumn();
					ImGui::Text("Phase Function");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Phase, 0.01f, -1.0f, 1.0f);

					ImGui::PopID();
				}



				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});

		DrawComponent<RectangularLightComponent>("RECTANGULAR LIGHT", entity, [](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("RectangularLightProperties", 2, flags))
			{
				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Radiance");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DrawVec3ColorEdit("", component.Radiance);

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Intensity");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Intensity, 0.055f, 0.0f, 1000.0f);

					ImGui::PopID();
				}


				{
					ImGui::PushID(2);
				
					ImGui::TableNextColumn();
					ImGui::Text("Radius");
				
					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Radius, 0.055f, 0.0f, 1000.0f);
				
					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Two Sided");

					ImGui::TableNextColumn();
					UserInterface::CheckBox("", component.TwoSided);

					ImGui::PopID();
				}


				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});



		DrawComponent<RigidBodyComponent>("RIGID BODY", entity, [&](auto& component)
		{
			constexpr ImGuiTableFlags flags{};
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("RigidBodyProperties", 1, flags))
			{
				ImGui::PushID(0);

				std::string componentBodyType = "Static";
				if (component.BodyType == RigidBodyComponent::Type::Dynamic)
					componentBodyType = "Dynamic";

				ImGui::TableNextColumn();


				ImGui::Text("Type");

				ImGui::SameLine();
				const float width = ImGui::GetWindowWidth();
				const float combo_width = width * 0.5f;

				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##rigidbodytypecombo", componentBodyType.c_str()))
				{

					bool isStaticSelected = (component.BodyType == RigidBodyComponent::Type::Static);

					if (ImGui::Selectable("Static", isStaticSelected))
						component.BodyType = RigidBodyComponent::Type::Static;

					if(isStaticSelected)
						ImGui::SetItemDefaultFocus();

					if (ImGui::Selectable("Dynamic", !isStaticSelected))
						component.BodyType = RigidBodyComponent::Type::Dynamic;

					if (!isStaticSelected)
						ImGui::SetItemDefaultFocus();

					ImGui::EndCombo();
				}

				ImGui::PopID();

				ImGui::EndTable();
			}


			// Rigid body properties (only for the dynamic type)
			if (component.BodyType == RigidBodyComponent::Type::Dynamic)
			{

				if (ImGui::BeginTable("RigidBodyProperties2", 2, flags))
				{

					{
						ImGui::PushID(1);

						ImGui::TableNextColumn();
						ImGui::Text("Mass");

						ImGui::TableNextColumn();
						ImGui::PushItemWidth(-1);
						UserInterface::DragFloat("", component.Mass, 0.3f, 0.0f, 1000000.0f);

						ImGui::PopID();
					}

					{
						ImGui::PushID(2);

						ImGui::TableNextColumn();
						ImGui::Text("Linear Drag");

						ImGui::TableNextColumn();
						ImGui::PushItemWidth(-1);
						UserInterface::DragFloat("", component.LinearDrag, 0.1f, 0.0f, 1000.0f);

						ImGui::PopID();
					}

					{
						ImGui::PushID(3);

						ImGui::TableNextColumn();
						ImGui::Text("Angular Drag");

						ImGui::TableNextColumn();
						ImGui::PushItemWidth(-1);
						UserInterface::DragFloat("", component.AngularDrag, 0.1f, 0.0f, 1000.0f);

						ImGui::PopID();
					}

					{
						ImGui::PushID(4);

						ImGui::TableNextColumn();
						ImGui::Text("Disable Gravity");

						ImGui::TableNextColumn();
						ImGui::Checkbox("", &component.DisableGravity);

						ImGui::PopID();
					}

					ImGui::EndTable();
				}





				// Constraints
				const char* treeNodeName = "RigidBodyConstraints";

				const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap;
				bool open = ImGui::TreeNodeEx((void*)treeNodeName, treeNodeFlags, "Constraints");


				if (open)
				{
					if (ImGui::BeginTable("RigidBodyConstraintsProperties", 2, flags))
					{

						{
							ImGui::PushID(5);

							ImGui::TableNextColumn();
							ImGui::Text("Lock Position X");

							ImGui::TableNextColumn();
							ImGui::Checkbox("", &component.LockPositionX);

							ImGui::PopID();
						}

						{
							ImGui::PushID(6);

							ImGui::TableNextColumn();
							ImGui::Text("Lock Position Y");

							ImGui::TableNextColumn();
							ImGui::Checkbox("", &component.LockPositionY);

							ImGui::PopID();
						}

						{
							ImGui::PushID(7);

							ImGui::TableNextColumn();
							ImGui::Text("Lock Position Z");

							ImGui::TableNextColumn();
							ImGui::Checkbox("", &component.LockPositionZ);

							ImGui::PopID();
						}

						{
							ImGui::PushID(8);

							ImGui::TableNextColumn();
							ImGui::Text("Lock Rotation X");

							ImGui::TableNextColumn();
							ImGui::Checkbox("", &component.LockRotationX);

							ImGui::PopID();
						}

						{
							ImGui::PushID(9);

							ImGui::TableNextColumn();
							ImGui::Text("Lock Rotation Y");

							ImGui::TableNextColumn();
							ImGui::Checkbox("", &component.LockRotationY);

							ImGui::PopID();
						}

						{
							ImGui::PushID(10);

							ImGui::TableNextColumn();
							ImGui::Text("Lock Rotation Z");

							ImGui::TableNextColumn();
							ImGui::Checkbox("", &component.LockRotationZ);

							ImGui::PopID();
						}

						ImGui::EndTable();
					}
					ImGui::TreePop();
				}
			}

			// Layer has been removed, set to Default layer
			if (!PhysicsLayerManager::IsLayerValid(component.Layer))
				component.Layer = 0;
			const PhysicsLayer& currentLayer = PhysicsLayerManager::GetLayer(component.Layer);

			ImGui::TableNextColumn();
			ImGui::Text("Layer");

			ImGui::SameLine();

			ImGui::PushItemWidth(-1);
			if (ImGui::BeginCombo("##layertypecombo", currentLayer.Name.c_str()))
			{
				int layerCount = PhysicsLayerManager::GetLayerCount();
				const auto& layerNames = PhysicsLayerManager::GetLayerNames();

				for (const auto& layerName : layerNames)
				{
					bool isSelected = currentLayer.Name ==  layerName;

					if (ImGui::Selectable(layerName.c_str(), isSelected))
					{
						component.Layer = PhysicsLayerManager::GetLayer(layerName).LayerID;
					}
				}

				ImGui::EndCombo();
			}


			

			ImGui::PopStyleVar();

				
		});


		DrawComponent<BoxColliderComponent>("BOX COLLIDER", entity, [&](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("BoxColliderComponent", 2, flags))
			{

				// Physics Material Editor
				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Physics Material");

					ImGui::TableNextColumn();
					std::string materialName = "Default";

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							const AssetMetadata& metadata = AssetManager::GetMetadata(component.MaterialHandle->Handle);
							materialName = GetNameFromFilepath(metadata.FilePath.string());
						}
					}

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							// We are setting -38.0f to make some room for the "X" button
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 38.0f, 20.0f });
						}
						else
						{
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });
						}
					}

					// After setting up the button for the physics material, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::PhysicsMat)
							{
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(selectionData.FilePath.string());
								component.MaterialHandle = physicsMaterialAsset;
							}
						}
					}
					if (ImGui::BeginDragDropSource())
					{
						ContentBrowserDragDropData dragDropData(AssetType::PhysicsMat, component.MaterialHandle.Raw(), sizeof(PhysicsMaterial));

						ImGui::TextUnformatted(component.MaterialHandle->m_MaterialName.c_str());
						ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, &dragDropData, sizeof(ContentBrowserDragDropData));
						ImGui::EndDragDropSource();
					}

					// Opening a new Material
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
					{
						std::string materialPath = FileDialogs::OpenFile("");
						if (!materialPath.empty())
						{
							Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialPath);
							component.MaterialHandle = physicsMaterialAsset;
						}
					}

					// Saving Material
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
					{
						ImGui::OpenPopup("SavePhysicsMaterial");
					}

					if (ImGui::BeginPopup("SavePhysicsMaterial"))
					{
						if (component.MaterialHandle)
						{
							if (component.MaterialHandle->Handle != 0)
							{
								if (ImGui::MenuItem("Edit"))
								{
									m_PhysicsMaterialEditor->SetVisibility(true);
									m_PhysicsMaterialEditor->SetActiveMaterialAset(component.MaterialHandle);
								}
								if (ImGui::MenuItem("Save To File"))
								{
									AssetImporter::Serialize(component.MaterialHandle);
								}
							}
						}
						if (ImGui::MenuItem("Save As"))
						{
							std::string materialPath = FileDialogs::SaveFile("");

							if (!materialPath.empty())
							{
								bool isAssetAlreadyLoaded = true;

								// Using `GetAsset` instead of `LoadOrGetAsset` because we need to check if it is loaded in memory or not
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetAsset<PhysicsMaterial>(materialPath);
								if (!physicsMaterialAsset)
								{
									// We are temporarily creating (or loading) a physics material asset.
									// The only purpose of using `LoadAsset` or `CreateNewAsset` is to register the asset into the registry file and set the parameters.
									// After that we serialize and delete the asset from memory.
									physicsMaterialAsset = AssetManager::LoadAsset<PhysicsMaterial>(materialPath);
									if (!physicsMaterialAsset)
										physicsMaterialAsset = AssetManager::CreateNewAsset<PhysicsMaterial>(materialPath);

									isAssetAlreadyLoaded = false;
								}

								// If the component has already a valid asset, we have to set to the current physics material's parameters
								if (component.MaterialHandle)
								{
									physicsMaterialAsset->CopyFrom(component.MaterialHandle.Raw());
								}
								else
								{
									// Creating a new temporary phyiscs material to just retrieve the default parameters
									Ref<PhysicsMaterial> physicsMaterialTemporay = Ref<PhysicsMaterial>::Create();
									physicsMaterialAsset->CopyFrom(physicsMaterialTemporay.Raw());
								}

								AssetImporter::Serialize(physicsMaterialAsset);

								if(!isAssetAlreadyLoaded)
									AssetManager::RemoveAssetFromMemory(physicsMaterialAsset->Handle);
							}
						}
						ImGui::EndPopup();
					}

					// If the physics material asset is not default, there should be an option to delete it and set it to the default
					if (component.MaterialHandle)
					{
						if (component.MaterialHandle->Handle != 0)
						{
							ImGui::SameLine();
							if (ImGui::Button("X", { 25.0f, 20.0f }))
							{
								component.ResetPhysicsMaterial();
							}
						}
					}

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Size");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("##boxColliderSize", &component.Size.x, 0.1, -1000.0f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Is Trigger");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::CheckBox("", component.IsTrigger);

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Offset");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("##boxColliderOffset", &component.Offset.x, 0.1f, -1000.0f, 1000.0f);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::PopStyleVar();
		});

		DrawComponent<SphereColliderComponent>("SPHERE COLLIDER", entity, [&](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("SphereColliderComponent", 2, flags))
			{

				// Physics Material Editor
				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Physics Material");

					ImGui::TableNextColumn();
					std::string materialName = "Default";

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							const AssetMetadata& metadata = AssetManager::GetMetadata(component.MaterialHandle->Handle);
							materialName = GetNameFromFilepath(metadata.FilePath.string());
						}
					}

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							// We are setting -38.0f to make some room for the "X" button
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 38.0f, 20.0f });
						}
						else
						{
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });
						}
					}

					// After setting up the button for the physics material, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::PhysicsMat)
							{
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(selectionData.FilePath.string());
								component.MaterialHandle = physicsMaterialAsset;
							}
						}
					}
					if (ImGui::BeginDragDropSource())
					{
						ContentBrowserDragDropData dragDropData(AssetType::PhysicsMat, component.MaterialHandle.Raw(), sizeof(PhysicsMaterial));

						ImGui::TextUnformatted(component.MaterialHandle->m_MaterialName.c_str());
						ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, &dragDropData, sizeof(ContentBrowserDragDropData));
						ImGui::EndDragDropSource();
					}

					// Opening a new Material
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
					{
						std::string materialPath = FileDialogs::OpenFile("");
						if (!materialPath.empty())
						{
							Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialPath);
							component.MaterialHandle = physicsMaterialAsset;
						}
					}

					// Saving Material
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
					{
						ImGui::OpenPopup("SavePhysicsMaterial");
					}

					if (ImGui::BeginPopup("SavePhysicsMaterial"))
					{
						if (component.MaterialHandle)
						{
							if (component.MaterialHandle->Handle != 0)
							{
								if (ImGui::MenuItem("Edit"))
								{
									m_PhysicsMaterialEditor->SetVisibility(true);
									m_PhysicsMaterialEditor->SetActiveMaterialAset(component.MaterialHandle);
								}
								if (ImGui::MenuItem("Save"))
								{
									AssetImporter::Serialize(component.MaterialHandle);
								}
							}
						}
						if (ImGui::MenuItem("Save As"))
						{
							std::string materialPath = FileDialogs::SaveFile("");

							if (!materialPath.empty())
							{
								bool isAssetAlreadyLoaded = true;

								// Using `GetAsset` instead of `LoadOrGetAsset` because we need to check if it is loaded in memory or not
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetAsset<PhysicsMaterial>(materialPath);
								if (!physicsMaterialAsset)
								{
									// We are temporarily creating (or loading) a physics material asset.
									// The only purpose of using `LoadAsset` or `CreateNewAsset` is to register the asset into the registry file and set the parameters.
									// After that we serialize and delete the asset from memory.
									physicsMaterialAsset = AssetManager::LoadAsset<PhysicsMaterial>(materialPath);
									if (!physicsMaterialAsset)
										physicsMaterialAsset = AssetManager::CreateNewAsset<PhysicsMaterial>(materialPath);

									isAssetAlreadyLoaded = false;
								}

								// If the component has already a valid asset, we have to set to the current physics material's parameters
								if (component.MaterialHandle)
								{
									physicsMaterialAsset->StaticFriction = component.MaterialHandle->StaticFriction;
									physicsMaterialAsset->DynamicFriction = component.MaterialHandle->DynamicFriction;
									physicsMaterialAsset->Bounciness = component.MaterialHandle->Bounciness;
								}
								else
								{
									// Creating a new temporary phyiscs material to just retrieve the default parameters
									Ref<PhysicsMaterial> physicsMaterialTemporay = Ref<PhysicsMaterial>::Create();
									physicsMaterialAsset->StaticFriction = physicsMaterialTemporay->StaticFriction;
									physicsMaterialAsset->DynamicFriction = physicsMaterialTemporay->DynamicFriction;
									physicsMaterialAsset->Bounciness = physicsMaterialTemporay->Bounciness;
								}

								AssetImporter::Serialize(physicsMaterialAsset);

								if (!isAssetAlreadyLoaded)
									AssetManager::RemoveAssetFromMemory(physicsMaterialAsset->Handle);
							}
						}
						ImGui::EndPopup();
					}

					// If the physics material asset is not default, there should be an option to delete it and set it to the default
					if (component.MaterialHandle)
					{
						if (component.MaterialHandle->Handle != 0)
						{
							ImGui::SameLine();
							if (ImGui::Button("X", { 25.0f, 20.0f }))
							{
								component.ResetPhysicsMaterial();
							}
						}
					}

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Radius");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Radius, 0.1f, 0.0f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Is Trigger");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::CheckBox("", component.IsTrigger);

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Offset");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("", &component.Offset.x, 0.1f, -1000.0f, 1000.0f);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::PopStyleVar();
		});


		DrawComponent<CapsuleColliderComponent>("CAPSULE COLLIDER", entity, [&](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("CapsuleColliderComponent", 2, flags))
			{

				// Physics Material Editor
				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Physics Material");

					ImGui::TableNextColumn();
					std::string materialName = "Default";

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							const AssetMetadata& metadata = AssetManager::GetMetadata(component.MaterialHandle->Handle);
							materialName = GetNameFromFilepath(metadata.FilePath.string());
						}
					}

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							// We are setting -38.0f to make some room for the "X" button
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 38.0f, 20.0f });
						}
						else
						{
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });
						}
					}

					// After setting up the button for the physics material, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::PhysicsMat)
							{
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(selectionData.FilePath.string());
								component.MaterialHandle = physicsMaterialAsset;
							}
						}
					}
					if (ImGui::BeginDragDropSource())
					{
						ContentBrowserDragDropData dragDropData(AssetType::PhysicsMat, component.MaterialHandle.Raw(), sizeof(PhysicsMaterial));

						ImGui::TextUnformatted(component.MaterialHandle->m_MaterialName.c_str());
						ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, &dragDropData, sizeof(ContentBrowserDragDropData));
						ImGui::EndDragDropSource();
					}

					// Opening a new Material
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
					{
						std::string materialPath = FileDialogs::OpenFile("");
						if (!materialPath.empty())
						{
							Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialPath);
							component.MaterialHandle = physicsMaterialAsset;
						}
					}

					// Saving Material
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
					{
						ImGui::OpenPopup("SavePhysicsMaterial");
					}

					if (ImGui::BeginPopup("SavePhysicsMaterial"))
					{
						if (component.MaterialHandle)
						{
							if (component.MaterialHandle->Handle != 0)
							{
								if (ImGui::MenuItem("Edit"))
								{
									m_PhysicsMaterialEditor->SetVisibility(true);
									m_PhysicsMaterialEditor->SetActiveMaterialAset(component.MaterialHandle);
								}
								if (ImGui::MenuItem("Save"))
								{
									AssetImporter::Serialize(component.MaterialHandle);
								}
							}
						}
						if (ImGui::MenuItem("Save As"))
						{
							std::string materialPath = FileDialogs::SaveFile("");

							if (!materialPath.empty())
							{
								bool isAssetAlreadyLoaded = true;

								// Using `GetAsset` instead of `LoadOrGetAsset` because we need to check if it is loaded in memory or not
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetAsset<PhysicsMaterial>(materialPath);
								if (!physicsMaterialAsset)
								{
									// We are temporarily creating (or loading) a physics material asset.
									// The only purpose of using `LoadAsset` or `CreateNewAsset` is to register the asset into the registry file and set the parameters.
									// After that we serialize and delete the asset from memory.
									physicsMaterialAsset = AssetManager::LoadAsset<PhysicsMaterial>(materialPath);
									if (!physicsMaterialAsset)
										physicsMaterialAsset = AssetManager::CreateNewAsset<PhysicsMaterial>(materialPath);

									isAssetAlreadyLoaded = false;
								}

								// If the component has already a valid asset, we have to set to the current physics material's parameters
								if (component.MaterialHandle)
								{
									physicsMaterialAsset->StaticFriction = component.MaterialHandle->StaticFriction;
									physicsMaterialAsset->DynamicFriction = component.MaterialHandle->DynamicFriction;
									physicsMaterialAsset->Bounciness = component.MaterialHandle->Bounciness;
								}
								else
								{
									// Creating a new temporary phyiscs material to just retrieve the default parameters
									Ref<PhysicsMaterial> physicsMaterialTemporay = Ref<PhysicsMaterial>::Create();
									physicsMaterialAsset->StaticFriction = physicsMaterialTemporay->StaticFriction;
									physicsMaterialAsset->DynamicFriction = physicsMaterialTemporay->DynamicFriction;
									physicsMaterialAsset->Bounciness = physicsMaterialTemporay->Bounciness;
								}

								AssetImporter::Serialize(physicsMaterialAsset);

								if (!isAssetAlreadyLoaded)
									AssetManager::RemoveAssetFromMemory(physicsMaterialAsset->Handle);
							}
						}
						ImGui::EndPopup();
					}

					// If the physics material asset is not default, there should be an option to delete it and set it to the default
					if (component.MaterialHandle)
					{
						if (component.MaterialHandle->Handle != 0)
						{
							ImGui::SameLine();
							if (ImGui::Button("X", { 25.0f, 20.0f }))
							{
								component.ResetPhysicsMaterial();
							}
						}
					}

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Radius");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Radius, 0.1f, 0.0f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Height");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Height, 0.1f, 0.0f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Is Trigger");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::CheckBox("", component.IsTrigger);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Offset");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("", &component.Offset.x, 0.1f, -1000.0f, 1000.0f);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::PopStyleVar();
		});

		DrawComponent<MeshColliderComponent>("MESH COLLIDER", entity, [&](auto& component)
		{
			constexpr ImGuiTableFlags flags{};
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("MeshColliderProperties", 2, flags))
			{

				// Physics Material Editor
				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Physics Material");

					ImGui::TableNextColumn();
					std::string materialName = "Default";

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							const AssetMetadata& metadata = AssetManager::GetMetadata(component.MaterialHandle->Handle);
							materialName = GetNameFromFilepath(metadata.FilePath.string());
						}
					}

					if (component.MaterialHandle)
					{
						if (AssetManager::IsAssetHandleNonZero(component.MaterialHandle->Handle))
						{
							// We are setting -38.0f to make some room for the "X" button
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 38.0f, 20.0f });
						}
						else
						{
							ImGui::Button(materialName.c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });
						}
					}

					// After setting up the button for the physics material, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::PhysicsMat)
							{
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(selectionData.FilePath.string());
								component.MaterialHandle = physicsMaterialAsset;
							}
						}
					}
					if (ImGui::BeginDragDropSource())
					{
						ContentBrowserDragDropData dragDropData(AssetType::PhysicsMat, component.MaterialHandle.Raw(), sizeof(PhysicsMaterial));

						ImGui::TextUnformatted(component.MaterialHandle->m_MaterialName.c_str());
						ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, &dragDropData, sizeof(ContentBrowserDragDropData));
						ImGui::EndDragDropSource();
					}

					// Opening a new Material
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
					{
						std::string materialPath = FileDialogs::OpenFile("");
						if (!materialPath.empty())
						{
							Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialPath);
							component.MaterialHandle = physicsMaterialAsset;
						}
					}

					// Saving Material
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
					{
						ImGui::OpenPopup("SavePhysicsMaterial");
					}

					if (ImGui::BeginPopup("SavePhysicsMaterial"))
					{
						if (component.MaterialHandle)
						{
							if (component.MaterialHandle->Handle != 0)
							{
								if (ImGui::MenuItem("Edit"))
								{
									m_PhysicsMaterialEditor->SetVisibility(true);
									m_PhysicsMaterialEditor->SetActiveMaterialAset(component.MaterialHandle);
								}
								if (ImGui::MenuItem("Save"))
								{
									AssetImporter::Serialize(component.MaterialHandle);
								}
							}
						}
						if (ImGui::MenuItem("Save As"))
						{
							std::string materialPath = FileDialogs::SaveFile("");

							if (!materialPath.empty())
							{
								bool isAssetAlreadyLoaded = true;

								// Using `GetAsset` instead of `LoadOrGetAsset` because we need to check if it is loaded in memory or not
								Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetAsset<PhysicsMaterial>(materialPath);
								if (!physicsMaterialAsset)
								{
									// We are temporarily creating (or loading) a physics material asset.
									// The only purpose of using `LoadAsset` or `CreateNewAsset` is to register the asset into the registry file and set the parameters.
									// After that we serialize and delete the asset from memory.
									physicsMaterialAsset = AssetManager::LoadAsset<PhysicsMaterial>(materialPath);
									if (!physicsMaterialAsset)
										physicsMaterialAsset = AssetManager::CreateNewAsset<PhysicsMaterial>(materialPath);

									isAssetAlreadyLoaded = false;
								}

								// If the component has already a valid asset, we have to set to the current physics material's parameters
								if (component.MaterialHandle)
								{
									physicsMaterialAsset->StaticFriction = component.MaterialHandle->StaticFriction;
									physicsMaterialAsset->DynamicFriction = component.MaterialHandle->DynamicFriction;
									physicsMaterialAsset->Bounciness = component.MaterialHandle->Bounciness;
								}
								else
								{
									// Creating a new temporary phyiscs material to just retrieve the default parameters
									Ref<PhysicsMaterial> physicsMaterialTemporay = Ref<PhysicsMaterial>::Create();
									physicsMaterialAsset->StaticFriction = physicsMaterialTemporay->StaticFriction;
									physicsMaterialAsset->DynamicFriction = physicsMaterialTemporay->DynamicFriction;
									physicsMaterialAsset->Bounciness = physicsMaterialTemporay->Bounciness;
								}

								AssetImporter::Serialize(physicsMaterialAsset);

								if (!isAssetAlreadyLoaded)
									AssetManager::RemoveAssetFromMemory(physicsMaterialAsset->Handle);
							}
						}
						ImGui::EndPopup();
					}

					// If the physics material asset is not default, there should be an option to delete it and set it to the default
					if (component.MaterialHandle)
					{
						if (component.MaterialHandle->Handle != 0)
						{
							ImGui::SameLine();
							if (ImGui::Button("X", { 25.0f, 20.0f }))
							{
								component.ResetPhysicsMaterial();
							}
						}
					}

					ImGui::PopID();
				}
				ImGui::EndTable();
			}


			{
				ImGui::PushID(1);

				std::string componentBodyType = "Triangle";
				if (component.IsConvex)
					componentBodyType = "Convex";

				ImGui::TableNextColumn();


				ImGui::Text("Collider Type");

				ImGui::SameLine();
				const float width = ImGui::GetWindowWidth();
				const float combo_width = width * 0.5f;

				ImGui::PushItemWidth(-1);
				if (ImGui::BeginCombo("##meshcollidertypecombo", componentBodyType.c_str()))
				{

					bool isStaticSelected = (component.IsConvex == true);

					if (ImGui::Selectable("Convex", isStaticSelected))
						component.IsConvex = true;

					if (isStaticSelected)
						ImGui::SetItemDefaultFocus();

					if (ImGui::Selectable("Triangle", !isStaticSelected))
						component.IsConvex = false;

					if (!isStaticSelected)
						ImGui::SetItemDefaultFocus();

					ImGui::EndCombo();
				}
				ImGui::PopID();
			}

			if (ImGui::BeginTable("MeshColliderProperties2", 2, flags))
			{

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Is Trigger");

					ImGui::TableNextColumn();
					UserInterface::CheckBox("", component.IsTrigger);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::PopStyleVar();
		});

		DrawComponent<FogBoxVolumeComponent>("BOX FOG VOLUME", entity, [](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("BoxFogVolumeProperties", 2, flags))
			{

				{
					ImGui::PushID(0);

					ImGui::TableNextColumn();
					ImGui::Text("Mie Scattering");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DrawVec3ColorEdit("", component.MieScattering);

					ImGui::PopID();
				}


				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Density");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Density, 0.01f, 0.005f, 10.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Emission");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DrawVec3ColorEdit("", component.Emission);

					ImGui::PopID();
				}


				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Phase");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.PhaseValue, 0.01f, 0.0f, 1.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Absorption");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Absorption, 0.01f, 0.0f, 100.0f);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});

		DrawComponent<CloudVolumeComponent>("BOX CLOUD VOLUME", entity, [](auto& component)
		{
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("CloudVolumeProperties", 2, flags))
			{
				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Scattering");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DrawVec3ColorEdit("", component.Scattering);

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Phase Function");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.PhaseFunction, 0.1f, 0.0f, 1.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Cloud Absorption");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.CloudAbsorption, 0.1f, 0.0f, 10.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Sun Absorption");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.SunAbsorption, 0.1f, 0.0f, 10.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Density");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.Density, 0.01f, 0.005f, 1000.0f);

					ImGui::PopID();
				}

				ImGui::Separator();

				{
					ImGui::PushID(5);

					ImGui::TableNextColumn();
					ImGui::Text("Cloud Scale");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.CloudScale, 0.01f, 0.005f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(6);

					ImGui::TableNextColumn();
					ImGui::Text("Density Offset");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.DensityOffset, 0.1f, 0.0f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(7);

					ImGui::TableNextColumn();
					ImGui::Text("Detail Offset");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					UserInterface::DragFloat("", component.DetailOffset, 0.1f, 0.0f, 10.0f);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});

		DrawComponent<CameraComponent>("CAMERA", entity, [&](auto& component)
		{
			constexpr ImGuiTableFlags flags{};
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("CameraProperties", 2, flags))
			{
				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Primary");

					ImGui::TableNextColumn();
					ImGui::Checkbox("", &component.Primary);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("FOV");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat("", &component.Camera->GetCameraFOV(), 0.1f, 0.0f, 180.0f))
						component.Camera->RecalculateProjectionMatrix();

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Near Clip");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat("", &component.Camera->GetNearClip(), 0.01f, 0.0f, 1.0f))
						component.Camera->RecalculateProjectionMatrix();

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Far Clip");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat("", &component.Camera->GetFarClip(), 2.0f, 1.0f, 10000.0f))
						component.Camera->RecalculateProjectionMatrix();

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});

		DrawComponent<TextComponent>("TEXT", entity, [&](auto& component)
		{
			constexpr ImGuiTableFlags flags{};
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
			if (ImGui::BeginTable("TextProperties", 2, flags))
			{
				{
					ImGui::PushID(1);
					UserInterface::PropertyMultiline("Text String", component.TextString);
					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Font");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Button(component.FontAsset->GetFontName().c_str(), { ImGui::GetColumnWidth() - 5.0f, 20.0f });
					
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
					{
						std::string filepath = FileDialogs::OpenFile("");
						if (!filepath.empty())
						{
							Ref<Font> newFontAsset = AssetManager::GetOrLoadAsset<Font>(filepath);
							if (newFontAsset)
							{
								component.FontAsset = newFontAsset;
							}
						}
					}

					// After setting up the button for the font, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::Font)
							{
								Ref<Font> fontAsset = AssetManager::GetOrLoadAsset<Font>(selectionData.FilePath.string());
								if (fontAsset)
								{
									component.FontAsset = fontAsset;
								}
							}
						}
					}

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Color");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::ColorEdit4("", &component.Color.x);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Line Spacing");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("", &component.LineSpacing, 0.01f, -100.0f, 100.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(5);

					ImGui::TableNextColumn();
					ImGui::Text("Kerning");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("", &component.Kerning, 0.01f, -100.0f, 100.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(6);

					ImGui::TableNextColumn();
					ImGui::Text("Max Width");

					ImGui::TableNextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("", &component.MaxWidth, 0.01f, 1.0f, 10000.0f);

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
			ImGui::PopStyleVar();
		});

		DrawComponent<ScriptComponent>("SCRIPT COMPONENT", entity, [&, entity](auto& component)
		{

			constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingMask_ };
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });

			bool isError = !ScriptEngine::ModuleExists(component.ModuleName);
			bool isRuntime = m_Context->m_SceneState == SceneState::Play;


			if (ImGui::BeginTable("ScriptProperties", 2, flags))
			{
				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Module Name");

					if (!isError)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
					}
					else
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));

					}

					ImGui::TableNextColumn();
					char buffer[256];
					memset(buffer, 0, sizeof(buffer));
					std::strncpy(buffer, component.ModuleName.c_str(), sizeof(buffer));
					ImGui::SetNextItemWidth(-1.0f);
					bool action = ImGui::InputText("##Tag", buffer, sizeof(buffer));

					
					ImGui::PopStyleColor();


					if (action)
					{
						if (!isError)
						{
							isError = true;
							ScriptEngine::ShutdownScriptEntity(entity, component.ModuleName);
						}

						if (ScriptEngine::ModuleExists(std::string(buffer)))
						{
							FROST_CORE_INFO("New module found! Old one: {0}, New one: {1}", component.ModuleName, std::string(buffer));

							component.ModuleName = std::string(buffer);

							if (isRuntime)
							{
								// In case it is run time, we need to create the entity
								ScriptEngine::InstantiateEntityClass(entity);
							}
							else
							{
								// In case it is on editor scene, we don't really care about the instance,
								// we just want to retrieve to values for the public fields
								UUID entityId = entity.GetComponent<IDComponent>().ID;
								ScriptEngine::GetEntityInstanceData(entity.GetSceneUUID(), entityId);
							}

						}
					}
					component.ModuleName = std::string(buffer);

					ImGui::PopID();
				}

				if (!isError)
				{
					int32_t pushId = 2;

					UUID entityId = entity.GetComponent<IDComponent>().ID;
					EntityInstanceData& entityInstanceData = ScriptEngine::GetEntityInstanceData(entity.GetSceneUUID(), entityId);
					ScriptModuleFieldMap& moduleFieldMap = component.ModuleFieldMap;

					isRuntime = m_Context->m_SceneState == SceneState::Play && entityInstanceData.Instance.IsRuntimeAvailable();
					
					if (moduleFieldMap.find(component.ModuleName) != moduleFieldMap.end())
					{
						
							
						auto& publicFields = moduleFieldMap.at(component.ModuleName);
						for (auto& [name, field] : publicFields)
						{
							ImGui::PushID(pushId);

							ImGui::TableNextColumn();
							ImGui::Text(name.c_str());

							ImGui::TableNextColumn();
							ImGui::PushItemWidth(-1.0f);
							
							switch (field.Type)
							{
								case FieldType::Int:
								{
									int32_t value = isRuntime ?
										field.GetRuntimeValue<int32_t>(entityInstanceData.Instance) :
										field.GetStoredValue<int32_t>();

									
									//ImGui::PushItemWidth(-1.0f);

									if (ImGui::DragInt("##dragInt", &value, 1.0f, INT_MIN, INT_MAX))
									{
										if (isRuntime)
											field.SetRuntimeValue(entityInstanceData.Instance, value);
										else
											field.SetStoredValue(value);
									}
									//ImGui::PopItemWidth();

									break;
								}
								

								case FieldType::Float:
								{
									float value = isRuntime ?
										field.GetRuntimeValue<float>(entityInstanceData.Instance) :
										field.GetStoredValue<float>();

									if (ImGui::DragFloat("##dragFloat", &value, 1.0f, FLT_MIN, FLT_MAX))
									{
										if (isRuntime)
											field.SetRuntimeValue(entityInstanceData.Instance, value);
										else
											field.SetStoredValue(value);
									}
									break;

								}


								case FieldType::UnsignedInt:
								{
									uint32_t value = isRuntime ?
										field.GetRuntimeValue<uint32_t>(entityInstanceData.Instance) :
										field.GetStoredValue<uint32_t>();

									if (ImGui::DragInt("##dragUint", (int32_t*)&value, 1.0f, 0, UINT_MAX))
									{
										if (isRuntime)
											field.SetRuntimeValue(entityInstanceData.Instance, value);
										else
											field.SetStoredValue(value);
									}
									break;
								}
								case FieldType::Vec2:
								{
									glm::vec2 value = isRuntime ?
										field.GetRuntimeValue<glm::vec2>(entityInstanceData.Instance) :
										field.GetStoredValue<glm::vec2>();

									if (ImGui::DragFloat2("##dragVec2", &value.x, 1.0f, FLT_MIN, FLT_MAX))
									{
										if (isRuntime)
											field.SetRuntimeValue(entityInstanceData.Instance, value);
										else
											field.SetStoredValue(value);
									}
									break;
								}
								case FieldType::Vec3:
								{
									glm::vec3 value = isRuntime ?
										field.GetRuntimeValue<glm::vec3>(entityInstanceData.Instance) :
										field.GetStoredValue<glm::vec3>();

									if (ImGui::DragFloat3("##dragVec3", &value.x, 1.0f, FLT_MIN, FLT_MAX))
									{
										if (isRuntime)
											field.SetRuntimeValue(entityInstanceData.Instance, value);
										else
											field.SetStoredValue(value);
									}
									break;
								}
								case FieldType::Vec4:
								{
									glm::vec4 value = isRuntime ?
										field.GetRuntimeValue<glm::vec4>(entityInstanceData.Instance) :
										field.GetStoredValue<glm::vec4>();

									if (ImGui::DragFloat4("##dragVec4", &value.x, 1.0f, FLT_MIN, FLT_MAX))
									{
										if (isRuntime)
											field.SetRuntimeValue(entityInstanceData.Instance, value);
										else
											field.SetStoredValue(value);
									}
									break;
								}
								case FieldType::String:
								{
									const std::string& value = isRuntime ? field.GetRuntimeValue<std::string>(entityInstanceData.Instance) : field.GetStoredValue<const std::string&>();
									char buffer[256];
									memset(buffer, 0, 256);
									memcpy(buffer, value.c_str(), value.length());
									if (ImGui::InputText("##fieldString", buffer, sizeof(buffer)))
									{
										if (isRuntime)
											field.SetRuntimeValue<const std::string&>(entityInstanceData.Instance, buffer);
										else
											field.SetStoredValue<const std::string&>(buffer);
									}
									break;
								}
								case FieldType::Asset:
								{
									// TODO: Add assets
									break;
								}
								case FieldType::Prefab:
								{
									UUID assetID = isRuntime ?
										field.GetRuntimeValue<UUID>(entityInstanceData.Instance) :
										field.GetStoredValue<UUID>();

									Entity entity = m_SceneHierarchy->m_SceneContext->FindEntityByUUID(assetID);

									std::string prefabName = "Null";
									if (AssetManager::IsAssetHandleValid(assetID))
									{
										const AssetMetadata& metadata = AssetManager::GetMetadata(assetID);//GetNameFromFilepath()
										prefabName = GetNameFromFilepath(metadata.FilePath.string());
									}
									
									if (UserInterface::PropertyPrefabReference("##prefabRef", prefabName.c_str()))
									{
										std::string filepath = FileDialogs::OpenFile("");

										if (!filepath.empty())
										{
											Ref<Prefab> prefab = AssetManager::GetOrLoadAsset<Prefab>(filepath);
											if (prefab)
											{
												if (isRuntime)
													field.SetRuntimeValue<const UUID&>(entityInstanceData.Instance, prefab->Handle);
												else
													field.SetStoredValue(prefab->Handle);
											}
											else
											{
												FROST_CORE_ERROR("Prefab is invalid!");
											}
										}
									}

									// After setting up the button for the material, the DragDrop feature needs to be set
									if (ImGui::BeginDragDropTarget())
									{
										auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
										if (data)
										{
											SelectionData selectionData = *(SelectionData*)data->Data;

											if (selectionData.AssetType == AssetType::Prefab)
											{
												Ref<Prefab> prefab = AssetManager::GetOrLoadAsset<Prefab>(selectionData.FilePath.string());

												if (prefab)
												{
													if (isRuntime)
														field.SetRuntimeValue<const UUID&>(entityInstanceData.Instance, prefab->Handle);
													else
														field.SetStoredValue(prefab->Handle);
												}
												else
												{
													FROST_CORE_ERROR("Prefab is invalid!");
												}
											}
										}
									}

									break;
								}
								case FieldType::Entity:
								{
									UUID uuid = isRuntime ?
										field.GetRuntimeValue<UUID>(entityInstanceData.Instance) :
										field.GetStoredValue<UUID>();

									Entity entity = m_SceneHierarchy->m_SceneContext->FindEntityByUUID(uuid);

									if (UserInterface::PropertyEntityReference("##entityRef", entity))
									{
										if (isRuntime)
											field.SetRuntimeValue<const UUID&>(entityInstanceData.Instance, entity.GetUUID());
										else
											field.SetStoredValue(entity.GetUUID());
									}
								}
							}

							ImGui::PopID();

							pushId++;

						}
						if (publicFields.size() > 0)
						{
							ImGui::PopItemWidth();
						}

					}
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