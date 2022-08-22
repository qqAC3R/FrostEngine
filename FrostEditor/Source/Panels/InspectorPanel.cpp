#include "frostpch.h"
#include "InspectorPanel.h"

#include "../UserInterface/UIWidgets.h"
#include <imgui.h>
#include <imgui/imgui_internal.h>

#include "IconsFontAwesome.hpp"

namespace Frost
{

	InspectorPanel::InspectorPanel()
		: m_PanelNameImGui(ICON_PAINT_BRUSH + std::string(" Inspector"))
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

			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
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

			//ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.25f, 1.0f));
			//ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.21f, 0.23f, 0.26f, 1.0f));
			//ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.27f, 0.3f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);


			std::string str = "ADD  " + std::string(ICON_PLUS);
			if (ImGui::Button(str.c_str()))
			{
				ImGui::OpenPopup("AddComponent");


			}
			ImGui::PopStyleVar();
			//ImGui::PopStyleColor(3);
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

			// Point light component
			if (ImGui::MenuItem("Point Light"))
			{
				if (!selectedEntity.HasComponent<PointLightComponent>())
					selectedEntity.AddComponent<PointLightComponent>();
				else
					FROST_CORE_WARN("This entity already has the Point Light Component!");
			}

			// Box Fog Volume component
			if (ImGui::MenuItem("Box Fog Volume"))
			{
				if (!selectedEntity.HasComponent<FogBoxVolumeComponent>())
					selectedEntity.AddComponent<FogBoxVolumeComponent>();
				else
					FROST_CORE_WARN("This entity already has the Box Fog Volume Component!");
			}

			// Box Fog Volume component
			if (ImGui::MenuItem("Cloud Volume"))
			{
				if (!selectedEntity.HasComponent<CloudVolumeComponent>())
					selectedEntity.AddComponent<CloudVolumeComponent>();
				else
					FROST_CORE_WARN("This entity already has the Cloud Volume Component!");
			}

			// Point light component
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

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();


		AnimationComponent* animationComponent = nullptr;
		if (entity.HasComponent<AnimationComponent>())
		{
			animationComponent = &entity.GetComponent<AnimationComponent>();
		}


		DrawComponent<TransformComponent>("TRANSFORM", entity, [](auto& component)
		{
			UserInterface::DrawVec3CoordsEdit("Translation", component.Translation);

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
			UserInterface::DrawVec3CoordsEdit("Rotation", component.Rotation);

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
			UserInterface::DrawVec3CoordsEdit("Scale", component.Scale, 1.0f);
		});

		DrawComponent<MeshComponent>("MESH", entity, [animationComponent](auto& component)
		{
			std::string meshFilepath = "";
			if (component.Mesh.Raw() != nullptr)
			{
				if (component.Mesh->IsLoaded())
				{
					meshFilepath = component.Mesh->GetFilepath();
				}
			}

			// Draw the mesh's filepath
			std::string path = UserInterface::DrawFilePath("File Path", meshFilepath, "fbx");
			if (!path.empty())
			{
				component.Mesh = Mesh::Load(path);

				if (animationComponent != nullptr)
				{
					animationComponent->Controller->SetActiveAnimation(nullptr);
				}
			}
		});

		DrawComponent<AnimationComponent>("ANIMATION CONTROLLER", entity, [](auto& component)
		{
			// Draw Animations
			if (component.MeshComponentPtr->Mesh.Raw() != nullptr)
			{
				const Mesh* mesh = component.MeshComponentPtr->Mesh.Raw();
				if (mesh->IsAnimated())
				{
					const Vector<Ref<Animation>>& animations = mesh->GetAnimations();
					Ref<Animation> activeAnimation = component.Controller->GetActiveAnimation();

					std::string activeAnimationName = "-";
					if (activeAnimation)
					{
						activeAnimationName = activeAnimation->GetName();
					}

					if (ImGui::BeginCombo("##animator", activeAnimationName.c_str()))
					{
						if (ImGui::Selectable("-"))
							component.Controller->SetActiveAnimation(nullptr);

						for (auto animation : animations)
						{
							bool isSelected = (animation.Raw() == activeAnimation.Raw());

							if (ImGui::Selectable(animation->GetName().c_str(), isSelected))
								component.Controller->SetActiveAnimation(animation);

							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					// Play/Stop Icons
					{
						if (ImGui::Button(ICON_PLAY, { 30, 30 }))
							if (activeAnimationName != "-")
								component.Controller->StartAnimation();

						ImGui::SameLine(0.0f, 2.0f);

						if (ImGui::Button(ICON_PAUSE, { 30, 30 }))
							if (activeAnimationName != "-")
								component.Controller->StopAnimation();

						ImGui::SliderFloat("##time_slider", component.Controller->GetAnimationTime(), 0.0f, 1.0f);
					}
					

				}
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
					ImGui::Text("Size");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Size, 0.01f, 0.0f, 1.0f);

					ImGui::PopID();
				}

				ImGui::Separator();

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Volume Density");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.VolumeDensity, 0.01f, 0.0f, 1.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Volume Absorption");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Absorption, 0.01f, 0.0f, 100.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(5);

					ImGui::TableNextColumn();
					ImGui::Text("Phase Function");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Phase, 0.01f, -1.0f, 1.0f);

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
					UserInterface::DrawVec3ColorEdit("", component.MieScattering);

					ImGui::PopID();
				}


				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Density");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Density, 0.01f, 0.005f, 10.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Emission");

					ImGui::TableNextColumn();
					UserInterface::DrawVec3ColorEdit("", component.Emission);

					ImGui::PopID();
				}


				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Phase");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.PhaseValue, 0.01f, 0.0f, 1.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Absorption");

					ImGui::TableNextColumn();
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
					UserInterface::DrawVec3ColorEdit("", component.Scattering);

					ImGui::PopID();
				}

				{
					ImGui::PushID(1);

					ImGui::TableNextColumn();
					ImGui::Text("Phase Function");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.PhaseFunction, 0.1f, 0.0f, 1.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(2);

					ImGui::TableNextColumn();
					ImGui::Text("Cloud Absorption");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.CloudAbsorption, 0.1f, 0.0f, 10.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(3);

					ImGui::TableNextColumn();
					ImGui::Text("Sun Absorption");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.SunAbsorption, 0.1f, 0.0f, 10.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(4);

					ImGui::TableNextColumn();
					ImGui::Text("Density");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.Density, 0.01f, 0.005f, 1000.0f);

					ImGui::PopID();
				}

				ImGui::Separator();

				{
					ImGui::PushID(5);

					ImGui::TableNextColumn();
					ImGui::Text("Cloud Scale");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.CloudScale, 0.01f, 0.005f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(6);

					ImGui::TableNextColumn();
					ImGui::Text("Density Offset");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.DensityOffset, 0.1f, 0.0f, 1000.0f);

					ImGui::PopID();
				}

				{
					ImGui::PushID(7);

					ImGui::TableNextColumn();
					ImGui::Text("Detail Offset");

					ImGui::TableNextColumn();
					UserInterface::DragFloat("", component.DetailOffset, 0.1f, 0.0f, 10.0f);

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