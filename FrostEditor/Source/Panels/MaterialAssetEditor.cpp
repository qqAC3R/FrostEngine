#include "frostpch.h"
#include "MaterialAssetEditor.h"

#include "Frost/Core/Application.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Utils/PlatformUtils.h"
#include "Frost/Asset/AssetManager.h"

#include "UserInterface/UIWidgets.h"
#include "Panels/ContentBrowser/ContentBrowser.h"
#include "Panels/ContentBrowser/ContentBrowserSelectionStack.h"

#include <imgui.h>
#include <imgui/imgui_internal.h>

namespace Frost
{
	void MaterialAssetEditor::Init(void* initArgs)
	{

	}

	void MaterialAssetEditor::OnEvent(Event& e)
	{

	}

	void MaterialAssetEditor::Render()
	{
		if (m_Visibility && m_ActiveMaterialAsset)
		{
			ImGui::Begin("Material Asset Editor", &m_Visibility);

			// PBR values
			{
				UserInterface::Text("Abledo");

				Ref<Texture2D> albedoTexture = m_ActiveMaterialAsset->GetAlbedoMap();

				ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
				ImTextureID imguiAlbedoTextureId = imguiLayer->GetImGuiTextureID(albedoTexture->GetImage2D());


				ImGui::PushID("Albedo_Texture_Button");
				ImVec2 cursourPosFromStart = ImGui::GetCursorPos();

				ImGui::ImageButton(imguiAlbedoTextureId, { 64, 64 });
				if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
				{
					std::string filepath = FileDialogs::OpenFile("");
					if (filepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						textureSpec.UseMips = true;
						textureSpec.FlipTexture = true;
						Ref<Texture2D> newAlbedoTex = AssetManager::LoadAsset<Texture2D>(filepath, (void*)&textureSpec);
						if (!newAlbedoTex)
							newAlbedoTex = AssetManager::CreateNewAsset<Texture2D>(filepath, (void*)&textureSpec);

						if (newAlbedoTex->Loaded())
						{
							Renderer::Submit([&, newAlbedoTex]() mutable {
								newAlbedoTex->GenerateMipMaps();
								m_ActiveMaterialAsset->SetAlbedoMap(newAlbedoTex);
							});
						}
						else
						{
							FROST_ERROR("[MaterialAssetEditor] Albedo Texture '{0}' cannot be loaded", filepath);
						}
					}
				}
				{
					ImVec2 imageButtonSize = ImGui::GetItemRectSize();
					ImGui::SetCursorPos(cursourPosFromStart);
					UserInterface::InvisibleButtonWithNoBehaivour("##AlbedoTextureInvisibleButton", imageButtonSize);

					// After setting up the button for the ALBEDO TEXTURE, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::Texture)
							{
								TextureSpecification textureSpec{};
								textureSpec.Format = ImageFormat::RGBA8;
								textureSpec.Usage = ImageUsage::ReadOnly;
								textureSpec.UseMips = true;
								textureSpec.FlipTexture = true;

								Ref<Texture2D> albedoTexture = AssetManager::LoadAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);
								if (!albedoTexture)
									albedoTexture = AssetManager::CreateNewAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);


								Renderer::Submit([&, albedoTexture]() mutable {
									albedoTexture->GenerateMipMaps();
									m_ActiveMaterialAsset->SetAlbedoMap(albedoTexture);
								});
							}
						}
					}
				}
				ImGui::PopID();




				ImGui::SameLine(0.0f, 5.0f);

				uint32_t startPosX = ImGui::GetCursorPosX();
				uint32_t startPosY = ImGui::GetCursorPosY();

				glm::vec4& albedoColor = m_ActiveMaterialAsset->GetAlbedoColor();
				UserInterface::DrawVec4ColorEdit("", albedoColor);

				ImGui::SetCursorPosX(startPosX);
				ImGui::SetCursorPosY(startPosY + 25);

				UserInterface::Text("Emissitivity");

				ImGui::SetCursorPosX(startPosX);
				ImGui::SetCursorPosY(startPosY + 45);

				float& emission = m_ActiveMaterialAsset->GetEmission();
				UserInterface::DragFloat(" ", emission, 0.1f, 0.0f, 1000.0f);
			}

			{
				UserInterface::Text("Roughness");

				Ref<Texture2D> roughnessTexture = m_ActiveMaterialAsset->GetRoughnessMap();

				ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
				ImTextureID imguiRoughnessTextureId = imguiLayer->GetImGuiTextureID(roughnessTexture->GetImage2D());

				ImVec2 cursourPosFromStart = ImGui::GetCursorPos();

				ImGui::PushID("Roughness_Texture_Button");
				ImGui::ImageButton(imguiRoughnessTextureId, { 64, 64 });
				if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
				{
					std::string filepath = FileDialogs::OpenFile("");
					if (filepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						textureSpec.UseMips = false;
						textureSpec.FlipTexture = true;

						Ref<Texture2D> newRoughnessTex = AssetManager::LoadAsset<Texture2D>(filepath, (void*)&textureSpec);
						if(!newRoughnessTex)
							newRoughnessTex = AssetManager::CreateNewAsset<Texture2D>(filepath, (void*)&textureSpec);

						if (newRoughnessTex->Loaded())
						{
							Renderer::Submit([&, newRoughnessTex]() mutable {
								m_ActiveMaterialAsset->SetRoughnessMap(newRoughnessTex);
							});
						}
						else
						{
							FROST_ERROR("[MaterialAssetEditor] Roughness Texture '{0}' cannot be loaded", filepath);
						}
					}
				}
				{
					ImVec2 imageButtonSize = ImGui::GetItemRectSize();
					ImGui::SetCursorPos(cursourPosFromStart);
					UserInterface::InvisibleButtonWithNoBehaivour("##RoughnessTextureInvisibleButton", imageButtonSize);

					// After setting up the button for the ROUGHNESS TEXTURE, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::Texture)
							{
								TextureSpecification textureSpec{};
								textureSpec.Format = ImageFormat::RGBA8;
								textureSpec.Usage = ImageUsage::ReadOnly;
								textureSpec.UseMips = false;
								textureSpec.FlipTexture = true;

								Ref<Texture2D> roughhnessTexture = AssetManager::LoadAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);
								if (!roughhnessTexture)
									roughhnessTexture = AssetManager::CreateNewAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);

								Renderer::Submit([&, roughhnessTexture]() mutable {
									m_ActiveMaterialAsset->SetRoughnessMap(roughhnessTexture);
								});
							}
						}
					}
				}
				ImGui::PopID();

				ImGui::SameLine(0.0f, 5.0f);
				float& roughness = m_ActiveMaterialAsset->GetRoughness();
				UserInterface::SliderFloat("  ", roughness, 0.0f, 1.0f);
			}

			{
				UserInterface::Text("Metalness");

				Ref<Texture2D> metalnessTexture = m_ActiveMaterialAsset->GetMetalnessMap();

				ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
				ImTextureID imguiMetalnesstextureId = imguiLayer->GetImGuiTextureID(metalnessTexture->GetImage2D());

				ImVec2 cursourPosFromStart = ImGui::GetCursorPos();

				ImGui::PushID("Metalness_Texture_Button");
				ImGui::ImageButton(imguiMetalnesstextureId, { 64, 64 });
				if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
				{
					std::string filepath = FileDialogs::OpenFile("");
					if (filepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						textureSpec.UseMips = false;
						textureSpec.FlipTexture = true;

						Ref<Texture2D> newMetalnessTex = AssetManager::LoadAsset<Texture2D>(filepath, (void*)&textureSpec);
						if(!newMetalnessTex)
							newMetalnessTex = AssetManager::CreateNewAsset<Texture2D>(filepath, (void*)&textureSpec);

						if (newMetalnessTex->Loaded())
						{
							Renderer::Submit([&, newMetalnessTex]() mutable {
								m_ActiveMaterialAsset->SetMetalnessMap(newMetalnessTex);
							});
						}
						else
						{
							FROST_ERROR("[MaterialAssetEditor] Metalness Texture '{0}' cannot be loaded", filepath);
						}
					}
				}
				{
					ImVec2 imageButtonSize = ImGui::GetItemRectSize();
					ImGui::SetCursorPos(cursourPosFromStart);
					UserInterface::InvisibleButtonWithNoBehaivour("##MetalnessTextureInvisibleButton", imageButtonSize);

					// After setting up the button for the METALNESS TEXTURE, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::Texture)
							{
								TextureSpecification textureSpec{};
								textureSpec.Format = ImageFormat::RGBA8;
								textureSpec.Usage = ImageUsage::ReadOnly;
								textureSpec.UseMips = false;
								textureSpec.FlipTexture = true;

								Ref<Texture2D> metalnesssTexture = AssetManager::LoadAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);
								if (!metalnesssTexture)
									metalnesssTexture = AssetManager::CreateNewAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);


								Renderer::Submit([&, metalnesssTexture]() mutable {
									m_ActiveMaterialAsset->SetMetalnessMap(metalnesssTexture);
								});
							}
						}
					}
				}
				ImGui::PopID();


				ImGui::SameLine(0.0f, 5.0f);
				float& metalness = m_ActiveMaterialAsset->GetMetalness();
				UserInterface::SliderFloat("   ", metalness, 0.0f, 1.0f);

			}


			{
				UserInterface::Text("Normal Map");

				Ref<Texture2D> normalTexture = m_ActiveMaterialAsset->GetNormalMap();
				ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
				ImTextureID imguiNormaltextureId = imguiLayer->GetImGuiTextureID(normalTexture->GetImage2D());

				ImVec2 cursourPosFromStart = ImGui::GetCursorPos();

				ImGui::PushID("Normal_Texture_Button");
				ImGui::ImageButton(imguiNormaltextureId, { 64, 64 });
				if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
				{
					std::string filepath = FileDialogs::OpenFile("");
					if (filepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						textureSpec.UseMips = false;
						textureSpec.FlipTexture = true;

						Ref<Texture2D> newNormalTex = AssetManager::LoadAsset<Texture2D>(filepath, (void*)&textureSpec);
						if (!newNormalTex)
							newNormalTex = AssetManager::CreateNewAsset<Texture2D>(filepath, (void*)&textureSpec);

						if (newNormalTex->Loaded())
						{
							Renderer::Submit([&, newNormalTex]() mutable {
								m_ActiveMaterialAsset->SetNormalMap(newNormalTex);
							});
						}
						else
						{
							FROST_ERROR("[MaterialAssetEditor] Normal Texture '{0}' cannot be loaded", filepath);
						}
					}
				}
				{
					ImVec2 imageButtonSize = ImGui::GetItemRectSize();
					ImGui::SetCursorPos(cursourPosFromStart);
					UserInterface::InvisibleButtonWithNoBehaivour("##NormalTextureInvisibleButton", imageButtonSize);

					// After setting up the button for the NORMAL TEXTURE, the DragDrop feature needs to be set
					if (ImGui::BeginDragDropTarget())
					{
						auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
						if (data)
						{
							SelectionData selectionData = *(SelectionData*)data->Data;

							if (selectionData.AssetType == AssetType::Texture)
							{
								TextureSpecification textureSpec{};
								textureSpec.Format = ImageFormat::RGBA8;
								textureSpec.Usage = ImageUsage::ReadOnly;
								textureSpec.UseMips = false;
								textureSpec.FlipTexture = true;

								Ref<Texture2D> normalTexture = AssetManager::LoadAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);
								if (!normalTexture)
									normalTexture = AssetManager::CreateNewAsset<Texture2D>(selectionData.FilePath.string(), (void*)&textureSpec);


								Renderer::Submit([&, normalTexture]() mutable {
									m_ActiveMaterialAsset->SetNormalMap(normalTexture);
								});
							}
						}
					}
				}
				ImGui::PopID();

				ImGui::SameLine(0.0f, 5.0f);
				bool useNormalMap_bool = bool(m_ActiveMaterialAsset->IsUsingNormalMap());
				UserInterface::CheckBox("UseNormalMap", useNormalMap_bool);
				m_ActiveMaterialAsset->SetUseNormalMap(uint32_t(useNormalMap_bool));
			}


			ImGui::End();
		}
	}

	void MaterialAssetEditor::Shutdown()
	{

	}
}