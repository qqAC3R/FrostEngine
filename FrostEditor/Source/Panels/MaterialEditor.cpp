#include "frostpch.h"
#include "MaterialEditor.h"

#include "Frost/ImGui/ImGuiLayer.h"
#include "Frost/Utils/PlatformUtils.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Core/Application.h"

#include "../UserInterface/UIWidgets.h"

#include <imgui.h>
#include <imgui/imgui_internal.h>

namespace Frost
{
	void MaterialEditor::Init(void* initArgs)
	{

	}

	void MaterialEditor::OnEvent(Event& e)
	{

	}

	void MaterialEditor::Render()
	{
		//ImGui::Begin("Material Editor");

		if (m_SelectedEntity && m_SelectedEntity.HasComponent<MeshComponent>())
		{
			MeshComponent& selectedEntityMesh = m_SelectedEntity.GetComponent<MeshComponent>();

			if (selectedEntityMesh.Mesh)
			{
				if (m_SelectedSubmesh != UINT32_MAX)
				{
					const Submesh& selectedSubmesh = selectedEntityMesh.Mesh->GetMeshAsset()->GetSubMeshes()[m_SelectedSubmesh];
					m_SelectedMaterialIndex = selectedSubmesh.MaterialIndex;
				}

				/*
				
				uint32_t idCounter = 0;
				for (uint32_t i = 0; i < selectedEntityMesh.Mesh->GetMaterialCount(); i++)
				{
					//ImGui::Text("Material %d", i);

					ImGui::PushID(idCounter);
					ImGui::TableNextColumn();

					ImGuiTreeNodeFlags flags = ((m_SelectedMaterialIndex == i) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
					if (m_SelectedMaterialIndex == i)
					{
						flags = ImGuiTreeNodeFlags_Selected;
						ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
					}

					flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
					bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)m_SelectedEntity, flags, "Material %d", i);
					if (m_SelectedMaterialIndex == i)
					{
						ImGui::PopStyleColor();
					}

					if (ImGui::IsItemClicked())
					{
						m_SelectedMaterialIndex = i;
					}

					if (opened)
					{
						ImGui::TreePop();
					}
					ImGui::TableNextColumn();


					ImGui::PopID();
					*/
			}
		}
		//ImGui::End();


#if 0
		ImGui::Begin("Material Editor");
		if (m_SelectedEntity && m_SelectedEntity.HasComponent<MeshComponent>())
		{
			MeshComponent& selectedEntityMesh = m_SelectedEntity.GetComponent<MeshComponent>();

			if (m_SelectedMaterialIndex != UINT32_MAX)
			{

				// Draw the selected material editor
				Ref<DataStorage> materialData = selectedEntityMesh.Mesh->GetMaterialData(m_SelectedMaterialIndex);

				// Textures
				uint32_t albedoTextureID = materialData->Get<uint32_t>("AlbedoTexture");
				uint32_t roughnessTextureID = materialData->Get<uint32_t>("RoughnessTexture");
				uint32_t metalnessTextureID = materialData->Get<uint32_t>("MetalnessTexture");
				uint32_t normalTextureID = materialData->Get<uint32_t>("NormalTexture");

				uint32_t& useNormalMap = materialData->Get<uint32_t>("UseNormalMap");

				// PBR values
				{
					UserInterface::Text("Abledo");

					Ref<Texture2D> albedoTexture = selectedEntityMesh.Mesh->GetTexture(albedoTextureID);

					ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
					ImTextureID imguiAlbedoTextureId = imguiLayer->GetImGuiTextureID(albedoTexture->GetImage2D());

					ImGui::PushID("Albedo_Texture_Button");
					ImGui::ImageButton(imguiAlbedoTextureId, { 64, 64 });
					if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
					{
						std::string filepath = FileDialogs::OpenFile("");
						if (filepath != "")
						{
							Ref<Texture2D> newAlbedoTex = Texture2D::Create(filepath);
							if (newAlbedoTex->Loaded())
							{
								Ref<Mesh> mesh = selectedEntityMesh.Mesh;
								Renderer::Submit([mesh, newAlbedoTex, albedoTextureID]() mutable {
									mesh->SetNewTexture(albedoTextureID, newAlbedoTex);
								});
							}
							else
							{
								FROST_ERROR("Texture '{0}' can't be loaded", filepath);
							}
						}
					}

					ImGui::PopID();


					ImGui::SameLine(0.0f, 5.0f);

					uint32_t startPosX = ImGui::GetCursorPosX();
					uint32_t startPosY = ImGui::GetCursorPosY();

					glm::vec3& albedoColor = materialData->Get<glm::vec3>("AlbedoColor");
					UserInterface::DrawVec3ColorEdit("", albedoColor);

					ImGui::SetCursorPosX(startPosX);
					ImGui::SetCursorPosY(startPosY + 25);

					UserInterface::Text("Emissitivity");

					ImGui::SetCursorPosX(startPosX);
					ImGui::SetCursorPosY(startPosY + 45);

					float& emission = materialData->Get<float>("EmissionFactor");
					UserInterface::DragFloat(" ", emission, 0.1f, 0.0f, 1000.0f);
				}

				{
					UserInterface::Text("Roughness");

					Ref<Texture2D> roughnessTexture = selectedEntityMesh.Mesh->GetTexture(roughnessTextureID);

					ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
					ImTextureID imguiRoughnessTextureId = imguiLayer->GetImGuiTextureID(roughnessTexture->GetImage2D());

					ImGui::PushID("Roughness_Texture_Button");
					ImGui::ImageButton(imguiRoughnessTextureId, { 64, 64 });
					if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
					{
						std::string filepath = FileDialogs::OpenFile("");
						if (filepath != "")
						{
							Ref<Texture2D> newRoughnessTex = Texture2D::Create(filepath);
							if (newRoughnessTex->Loaded())
							{
								Ref<Mesh> mesh = selectedEntityMesh.Mesh;
								Renderer::Submit([mesh, newRoughnessTex, roughnessTextureID]() mutable {
									mesh->SetNewTexture(roughnessTextureID, newRoughnessTex);
								});
							}
							else
							{
								FROST_ERROR("Texture '{0}' can't be loaded", filepath);
							}
						}
					}
					ImGui::PopID();

					ImGui::SameLine(0.0f, 5.0f);
					float& roughness = materialData->Get<float>("RoughnessFactor");
					UserInterface::SliderFloat("  ", roughness, 0.0f, 1.0f);
				}

				{
					UserInterface::Text("Metalness");

					Ref<Texture2D> metalnessTexture = selectedEntityMesh.Mesh->GetTexture(metalnessTextureID);

					ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
					ImTextureID imguiMetalnesstextureId = imguiLayer->GetImGuiTextureID(metalnessTexture->GetImage2D());
					
					ImGui::PushID("Metalness_Texture_Button");
					ImGui::ImageButton(imguiMetalnesstextureId, { 64, 64 });
					if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
					{
						std::string filepath = FileDialogs::OpenFile("");
						if (filepath != "")
						{
							Ref<Texture2D> newMetalnessTex = Texture2D::Create(filepath);
							if (newMetalnessTex->Loaded())
							{
								Ref<Mesh> mesh = selectedEntityMesh.Mesh;
								Renderer::Submit([mesh, newMetalnessTex, metalnessTextureID]() mutable {
									mesh->SetNewTexture(metalnessTextureID, newMetalnessTex);
								});
							}
							else
							{
								FROST_ERROR("Texture '{0}' can't be loaded", filepath);
							}
						}
					}
					ImGui::PopID();


					ImGui::SameLine(0.0f, 5.0f);
					float& metalness = materialData->Get<float>("MetalnessFactor");
					UserInterface::SliderFloat("   ", metalness, 0.0f, 1.0f);

				}


				{
					UserInterface::Text("Normal Map");

					Ref<Texture2D> normalTexture = selectedEntityMesh.Mesh->GetTexture(normalTextureID);
					//ImTextureID imguiNormaltextureId = ImGuiLayer::GetTextureIDFromVulkanTexture(normalTexture->GetImage2D());
					ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
					ImTextureID imguiNormaltextureId = imguiLayer->GetImGuiTextureID(normalTexture->GetImage2D());

					ImGui::PushID("Normal_Texture_Button");
					ImGui::ImageButton(imguiNormaltextureId, { 64, 64 });
					if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
					{
						std::string filepath = FileDialogs::OpenFile("");
						if (filepath != "")
						{
							Ref<Texture2D> newNormalTex = Texture2D::Create(filepath);
							if (newNormalTex->Loaded())
							{
								Ref<Mesh> mesh = selectedEntityMesh.Mesh;
								Renderer::Submit([mesh, newNormalTex, normalTextureID]() mutable {
									mesh->SetNewTexture(normalTextureID, newNormalTex);
								});
							}
							else
							{
								FROST_ERROR("Texture '{0}' can't be loaded", filepath);
							}
						}
					}
					ImGui::PopID();

					ImGui::SameLine(0.0f, 5.0f);
					bool useNormalMap_bool = useNormalMap;
					UserInterface::CheckBox("UseNormalMap", useNormalMap_bool);
					useNormalMap = uint32_t(useNormalMap_bool);
				}


			}
		}
		ImGui::End();
#endif
	}

	void MaterialEditor::Shutdown()
	{

	}
}