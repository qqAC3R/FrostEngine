#include "frostpch.h"
#include "AnimationNodeEditor.h"

#include "Frost/ImGui/Utils/NodeEditor.h"
#include "Frost/ImGui/Utils/ScopedStyle.h"
#include "Frost/ImGui/ImGuiLayer.h"

#include "Frost/Core/Application.h"
#include "Frost/Core/Input.h"
#include "Frost/Core/FunctionQueue.h"
#include "Frost/InputCodes/KeyCodes.h"

#include "UserInterface/UIWidgets.h"
#include "IconsFontAwesome.hpp"

namespace ed = ax::NodeEditor;

#define ANIMATION_NODE_EDITOR_DRAG_DROP "animation_node_editor_drag_drop"

namespace Frost
{
	static ed::EditorContext* m_Context = nullptr;
	static bool s_ShowBoundingBoxes = false;

	static AnimationPinInfo* s_StartLinkPin = nullptr;


	AnimationNoteEditor::AnimationNoteEditor()
	{
		m_PinConnectedTex = Texture2D::Create("Resources/Editor/NodeEditor/PinConnected.png");
		m_PinDisconnectedTex = Texture2D::Create("Resources/Editor/NodeEditor/PinDisconnected.png");
		m_AnimationPoseTex = Texture2D::Create("Resources/Editor/NodeEditor/AnimationPose.png");
		m_AnimationPoseDisconnectedTex = Texture2D::Create("Resources/Editor/NodeEditor/AnimationPoseDisconnected.png");
		m_SaveIconTex = Texture2D::Create("Resources/Editor/NodeEditor/SaveIcon.png");
	}

	AnimationNoteEditor::~AnimationNoteEditor()
	{

	}

	void AnimationNoteEditor::Init(void* initArgs)
	{
		ed::Config config;
		config.UserPointer = this;
		//config.SettingsFile = "Blueprints.json";

		m_Context = ed::CreateEditor(&config);
		ed::SetCurrentEditor(m_Context);

		auto& editorStyle = ed::GetStyle();
		editorStyle = ed::Style();

		editorStyle.Colors[ed::StyleColor_Bg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(24, 27, 28, 255));
		editorStyle.Colors[ed::StyleColor_Grid] = ImGui::ColorConvertU32ToFloat4(IM_COL32(36, 40, 41, 200));
		editorStyle.Colors[ed::StyleColor_NodeBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(32, 32, 38, 255));
		editorStyle.Colors[ed::StyleColor_NodeBorder] = ImGui::ColorConvertU32ToFloat4(IM_COL32(39, 38, 45, 255));
		editorStyle.Colors[ed::StyleColor_HovNodeBorder] = ImGui::ColorConvertU32ToFloat4(IM_COL32(94, 92, 108, 255));
		//editorStyle.Colors[ed::StyleColor_LinkSelRect] = ImGui::ColorConvertU32ToFloat4(IM_COL32(94, 92, 108, 255));
	}

	void AnimationNoteEditor::OnEvent(Event& e)
	{

	}


	void AnimationNoteEditor::Render()
	{
		if (!m_CurrentAnimationBlueprint) return;

		if (m_Visibility)
		{
			ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{ 0, 0 });
			ImGui::Begin("Animation Node Editor", &m_Visibility, ImGuiWindowFlags_NoCollapse);
			{
				//ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
				//	| ImGuiTableFlags_SizingFixedFit
				//	| ImGuiTableFlags_BordersInnerV;

				//if (ImGui::BeginTable("AnimationNodeTable", 2, tableFlags, ImVec2(0.0f, 0.0f)))
				{
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.12f, 0.15f, 1.0f));
					ImGui::Begin("Animation Node Inputs");
					{
						
						FunctionQueue lateDeleteFunctions;

						constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
						ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2.0f, 2.8f });
						if (ImGui::BeginTable("HierarchyTable", 2, flags))
						{
							ImGui::TableSetupColumn("  Name", ImGuiTableColumnFlags_NoHide);
							ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 12.0f);
							ImGui::TableHeadersRow();

							ImGui::ScopedStyle colorHeaderActive(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
							ImGui::ScopedStyle colorHeaderHovered(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

							uint32_t idCounter = 0;
							for (auto& [inputHandle, inputInfo] : m_CurrentAnimationBlueprint->m_InputMap)
							{
								ImGui::PushID(idCounter);
								ImGui::TableNextColumn();

								ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Leaf;
								treeFlags |= ImGuiTreeNodeFlags_SpanFullWidth;
								treeFlags |= ImGuiTreeNodeFlags_SpanAvailWidth;

								bool isInputSeleted = m_SelectedInput == inputHandle;
								if (isInputSeleted)
								{
									ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
									ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32({ 0.15f, 0.15f, 0.15f, 1.0f }));
									treeFlags |= ImGuiTreeNodeFlags_Selected;
								}

								ImVec4 typeColor;
								switch (inputInfo.InputType)
								{
									case AnimationInput::Type::Float:
										typeColor = ImVec4(0.5f, 0.67f, 0.36f, 1.0f); break; // Green color
									case AnimationInput::Type::Int:
										typeColor = ImVec4(0.85f, 0.4f, 1.0f, 1.0f); break; // Pink-violet color
									case AnimationInput::Type::Bool:
										typeColor = ImVec4(1.0f, 0.40f, 0.53f, 1.0f); break; // Red color
									case AnimationInput::Type::Animation:
										typeColor = ImVec4(0.38f, 0.57f, 0.91f, 1.0f); break; // Blue color
								}
								ImGui::PushStyleColor(ImGuiCol_Text, typeColor);


								bool opened = ImGui::TreeNodeEx((void*)inputHandle.Get(), treeFlags, inputInfo.Name.c_str());
								bool isClicked = ImGui::IsItemClicked();

								if (opened)
								{
									ImGui::TreePop();
								}

								if (isClicked)
								{
									m_SelectedInput = inputHandle;
								}

								if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
								{
									ImGui::Text(inputInfo.Name.c_str());

									ImGui::SetDragDropPayload(ANIMATION_NODE_EDITOR_DRAG_DROP, &inputHandle, sizeof(UUID));
									ImGui::EndDragDropSource();
								}



								ImGui::TableNextColumn();

								float beforeCursorPosX = ImGui::GetCursorPosX();
								float beforeCursorPosY = ImGui::GetCursorPosY();
								float columnWidth = ImGui::GetColumnWidth();
								ImGui::TextUnformatted(AnimationInput::GetStringFromInputType(inputInfo.InputType).c_str());

								ImGui::PopStyleColor(); // ImGuiCol_Text

								ImGui::SetCursorPosX(beforeCursorPosX + columnWidth - 30.0f);
								ImGui::SetCursorPosY(beforeCursorPosY);
								bool isDeleted = ImGui::Button("X", { 25.0f, 17.0f });

								if (isDeleted)
								{
									InputNodeHandle inputHandleCopy = inputHandle; // Copying because the lambda cannot capture the `inputHandle`
									lateDeleteFunctions.AddFunction([&, inputHandleCopy]() {
										m_CurrentAnimationBlueprint->DeleteInput(inputHandleCopy);
									});
								}

								if (isInputSeleted)
									ImGui::PopStyleColor();


								ImGui::PopID();
								idCounter++;
							}
							lateDeleteFunctions.Execute();


							ImGui::EndTable();
						}
						ImGui::PopStyleVar(); // ImGuiStyleVar_CellPadding

						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
						if (ImGui::BeginPopup("Add Input") || (ImGui::BeginPopupContextWindow(nullptr, 1, false)))
						{
							if (ImGui::MenuItem("Add Float"))
								m_CurrentAnimationBlueprint->AddInput("New float", AnimationInput::Type::Float);
							if (ImGui::MenuItem("Add Int"))
								m_CurrentAnimationBlueprint->AddInput("New int", AnimationInput::Type::Int);
							if (ImGui::MenuItem("Add Boolean"))
								m_CurrentAnimationBlueprint->AddInput("New boolean", AnimationInput::Type::Bool);
							
							ImGui::EndPopup();
						}
						ImGui::PopStyleVar(); // ImGuiStyleVar_ItemSpacing

					}
					ImGui::End();
					ImGui::PopStyleColor();


					ImGui::Begin("Animation Node Input Inspector");
					{
						if (m_SelectedInput)
						{
							AnimationInput& animationInput = m_CurrentAnimationBlueprint->m_InputMap[m_SelectedInput];

							if (animationInput.InputType != AnimationInput::Type::Animation)
							{

								char buffer[256];
								memset(buffer, 0, sizeof(buffer));
								std::strncpy(buffer, animationInput.Name.c_str(), sizeof(buffer));

								UserInterface::ShiftCursorX(10.0f);
								UserInterface::ShiftCursorY(2.0f);
								ImGui::Text(ICON_PENCIL);

								ImGui::SameLine();

								ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
								ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
								if (ImGui::InputText("##animationNameInputText", buffer, 256))
								{
									animationInput.Name = (std::string)buffer;
								}
								ImGui::PopStyleVar();
								ImGui::PopStyleColor();

								ImGui::Separator();



								ImGui::Text("Value");
								ImGui::SameLine();

								switch (animationInput.InputType)
								{
									case AnimationInput::Type::Int:
										ImGui::DragInt("##animationInputInt", animationInput.Data.As<int32_t>().Raw(), 1.0f, INT32_MIN, INT32_MAX);
										break;
									case AnimationInput::Type::Float:
										ImGui::DragFloat("##animationInputInt", animationInput.Data.As<float>().Raw(), 0.01f, INT32_MIN, INT32_MAX);
										break;
									case AnimationInput::Type::Bool:
									{
										Ref<bool> animationInputPtr = animationInput.Data.As<bool>();
										int32_t boolAsInt = (int32_t)*animationInputPtr.Raw();
										ImGui::DragInt("##animationInputInt", &boolAsInt, 1, 0, 1);
										memcpy(animationInputPtr.Raw(), (bool*)&boolAsInt, sizeof(bool));
										break;
									}
								}

							}
						}
					}
					ImGui::End();





					//ImGui::Begin("Animation Node Viewport");
					//ImGui::TableSetColumnIndex(1);
					ImGui::BeginChild("NodeGraphContent");
					{
						ed::SetCurrentEditor(m_Context);
						ed::Begin("My Editor", ImVec2(0.0, 0.0f));

						{
							for (auto& [nodeHandle, animationNode] : m_CurrentAnimationBlueprint->m_NodeMap)
							{
								// Start drawing nodes.
								{
									ImGui::NodeBuilder nodeBuilder(nodeHandle, animationNode->Type);

									std::string animationNodeName = "";
									ImU32 headerColor = IM_COL32(255, 255, 255, 255);
									ImVec4 color = {};
									switch (animationNode->Type)
									{
									case AnimationNode::NodeType::Input:
										animationNodeName = "Input";
										color = *(ImVec4*)&animationNode.As<InputAnimationNode>()->TypeColor;
										headerColor = IM_COL32(color.x * 255, color.y * 255, color.z * 255, color.w * 255);

										break;
									case AnimationNode::NodeType::Blend:
										animationNodeName = "Blend";
										headerColor = IM_COL32(128, 255, 255, 255);
										break;
									case AnimationNode::NodeType::Output:
										animationNodeName = "Ouput";
										headerColor = IM_COL32(255, 128, 128, 255);
										break;
									case AnimationNode::NodeType::Condition:
										animationNodeName = "Condition";
										headerColor = IM_COL32(86, 135, 200, 255);
										break;
									}


									nodeBuilder.RenderHeader(animationNodeName.c_str(), headerColor);

									ImGui::BeginHorizontal("content");
									{

										ImGui::BeginVertical("inputs", ImVec2(0, 0), 0.0f);
										for (auto& [inputPinName, inputPinHandle] : animationNode->Inputs)
										{
											//ImGui::NodePin nodePin(inputName, nodeHandle, ed::PinKind::Input);

											auto alpha = ImGui::GetStyle().Alpha;
											ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);


											Ref<AnimationPinInfo> pinInfo = animationNode->InputData[inputPinHandle];

											ed::PinId pindID(pinInfo.Raw());
											ed::BeginPin(pindID, ed::PinKind::Input);

											{
												std::string nodePinNameID = "NodePin_" + std::to_string(inputPinHandle);
												ImGui::BeginHorizontal(nodePinNameID.c_str());
												{
													// Get the pin color
													glm::vec4 finalPinColor = glm::vec4(1.0f);
													if (pinInfo->IsPinColored())
														finalPinColor = pinInfo->PinColor;

													if (pinInfo->IsConnectedPinValid())
													{
														if (pinInfo->ConnectedPin->IsPinColored())
															finalPinColor = pinInfo->ConnectedPin->PinColor;
													}

													// Apply alpha decrease after getting all the colors required
													if (s_StartLinkPin)
													{
														if (s_StartLinkPin->PinType != pinInfo->PinType || s_StartLinkPin->PinKind == pinInfo->PinKind || pinInfo->ConnectedPin != nullptr)
															finalPinColor.a = 0.2f;
													}

													// Get the pin required texture
													Ref<Texture2D> texture = GetTextureByPinType(pinInfo->PinType, pinInfo->IsConnectedPinValid());

													// Render the texture
													ImGui::Image(
														(ImTextureID)imguiLayer->GetImGuiTextureID(texture->GetImage2D()),
														{ texture->GetWidth() / 1.5f, texture->GetHeight() / 1.5f },
														{ 0.01f, 0.01f },
														{ 0.99f, 0.99f },
														*(ImVec4*)&finalPinColor
													);


													ImGui::Spring(0.0f, 1.0f);
													ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);

													{
														ImGui::ScopedStyle textColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, finalPinColor.a));
														ImGui::TextUnformatted(inputPinName.c_str());
													}
												}
												ImGui::EndHorizontal();

											}

											ed::EndPin();

											ImGui::PopStyleVar();
										}

										ImGui::EndVertical();

										if (s_ShowBoundingBoxes)
										{
											ImGui::GetWindowDrawList()->AddRect(
												ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255)
											);
										}




										if (animationNode->Outputs.size() > 0)
											ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);


										//ImGui::Spring(1);

										ImGui::BeginVertical("outputs", ImVec2(0, 0), 1.0f);
										for (auto& [outputPinName, outputPinHandle] : animationNode->Outputs)
										{
											auto alpha = ImGui::GetStyle().Alpha;
											ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);


											Ref<AnimationPinInfo> pinInfo = animationNode->OutputData[outputPinHandle];

											ed::PinId pindID(pinInfo.Raw());
											ed::BeginPin(pindID, ed::PinKind::Output);

											{
												std::string pinNameID = "NodePin_" + std::to_string(outputPinHandle);
												ImGui::BeginHorizontal(pinNameID.c_str());
												{
													// Get the pin color
													glm::vec4 finalPinColor = glm::vec4(1.0f);
													if (pinInfo->IsPinColored())
														finalPinColor = pinInfo->PinColor;

													if (pinInfo->IsConnectedPinValid())
													{
														if (pinInfo->ConnectedPin->IsPinColored())
															finalPinColor = pinInfo->ConnectedPin->PinColor;
													}

													// Apply alpha decrease after getting all the colors required
													if (s_StartLinkPin)
													{
														if (s_StartLinkPin->PinType != pinInfo->PinType || s_StartLinkPin->PinKind == pinInfo->PinKind || pinInfo->ConnectedPin != nullptr)
															finalPinColor.a = 0.2f;
													}

													// Get the pin required texture
													Ref<Texture2D> texture = GetTextureByPinType(pinInfo->PinType, pinInfo->IsConnectedPinValid());

													{
														ImGui::ScopedStyle textColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, finalPinColor.a));

														// For the input node, the name of the variables might change, so we request the name from the inputMap
														// This could've not been possible before because for other nodes,
														// the input/output pins are set in stone and don't need any updation everyframe.
														if (animationNode->Type == AnimationNode::NodeType::Input)
														{
															Ref<InputAnimationNode> inputAnimationNode = animationNode.As<InputAnimationNode>();
															std::string inputName = m_CurrentAnimationBlueprint->m_InputMap[inputAnimationNode->InputHandle].Name;
															ImGui::TextUnformatted(inputName.c_str());
														}
														else
														{
															ImGui::TextUnformatted(outputPinName.c_str());
														}
													}

													ImGui::Spring(0.0f, 1.0f);
													ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);


													// Render the texture
													ImGui::Image(
														(ImTextureID)imguiLayer->GetImGuiTextureID(texture->GetImage2D()),
														{ texture->GetWidth() / 1.5f, texture->GetHeight() / 1.5f },
														{ 0.01f, 0.01f },
														{ 0.99f, 0.99f },
														*(ImVec4*)&finalPinColor
													);
												}
												ImGui::EndHorizontal();
											}
											ed::EndPin();

											ImGui::PopStyleVar();
										}
										ImGui::EndVertical();

										if (s_ShowBoundingBoxes)
										{
											ImGui::GetWindowDrawList()->AddRect(
												ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255)
											);
										}


									}

									ImGui::EndHorizontal();
								}


								for (auto& linkInfo : m_CurrentAnimationBlueprint->m_PinLinks)
								{
									ed::PinId inputPinA((void*)linkInfo.PinA);
									ed::PinId outputPinB((void*)linkInfo.PinB);

									glm::vec4 linkColor = glm::vec4(1.0f);
									if (linkInfo.PinA->IsPinColored())
										linkColor = linkInfo.PinA->PinColor;
									if (linkInfo.PinB->IsPinColored())
										linkColor = linkInfo.PinB->PinColor;

									ed::Link(linkInfo.LinkID.Get(), inputPinA, outputPinB, *(ImVec4*)&linkColor, 2.0f);
								}

#if 0
								if (Input::IsKeyPressed(Key::X))
								{
									ed::LinkId linkId = ed::GetHoveredLink();

									if (linkId)
									{
										for (auto& link : m_PinLinks)
										{
											uint32_t index = 0;
											if (link.LinkID == linkId.Get())
											{
												link.PinA->ConnectedPin = nullptr;
												link.PinB->ConnectedPin = nullptr;
												m_PinLinks.erase(m_PinLinks.begin() + index);
												break;
											}
											index++;
										}
									}
								}
#endif

								

								// Handle creation action, returns true if editor want to create new object (node or link)
								if (ed::BeginCreate())
								{
									ed::PinId inputPinId, outputPinId;
									if (ed::QueryNewLink(&inputPinId, &outputPinId))
									{
										// Set the active pin if the query link has begun
										{
											AnimationPinInfo* inputPinUUID = inputPinId.AsPointer<AnimationPinInfo>();
											AnimationPinInfo* outputPinUUID = outputPinId.AsPointer<AnimationPinInfo>();
											s_StartLinkPin = inputPinUUID ? inputPinUUID : outputPinUUID;
										}

										auto showLabel = [](const char* label, ImColor color)
										{
											ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
											auto size = ImGui::CalcTextSize(label);

											auto padding = ImGui::GetStyle().FramePadding;
											auto spacing = ImGui::GetStyle().ItemSpacing;

											ImGui::SetCursorPos(
												ImVec2(ImGui::GetCursorPos().x + spacing.x, ImGui::GetCursorPos().y - spacing.y)
											);

											auto rectMin = ImVec2(ImGui::GetCursorScreenPos().x - padding.x, ImGui::GetCursorScreenPos().y - padding.y);
											auto rectMax = ImVec2(ImGui::GetCursorScreenPos().x + size.x + padding.x, ImGui::GetCursorScreenPos().y + size.y + padding.y);

											auto drawList = ImGui::GetWindowDrawList();
											drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
											ImGui::TextUnformatted(label);
										};


										if (inputPinId && outputPinId) // both are valid, let's accept link
										{
											// NOTE: InputPIn and OutputPin is not respected in the ImGuiEditorNode library, so the order doesn't matter 
											AnimationPinInfo* inputPinUUID = inputPinId.AsPointer<AnimationPinInfo>();
											AnimationPinInfo* outputPinUUID = outputPinId.AsPointer<AnimationPinInfo>();

											if (inputPinUUID->PinType != outputPinUUID->PinType)
											{
												showLabel("x Incompatible Pin Type", ImColor(20, 20, 20, 180));
												ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
											}
											else if (inputPinUUID->PinKind == outputPinUUID->PinKind)
											{
												showLabel("x Incompatible Pin Kind", ImColor(20, 20, 20, 180));
												ed::RejectNewItem(ImColor(255, 0, 0), 1.0f);
											}
											else if (inputPinUUID->ConnectedPin != nullptr || outputPinUUID->ConnectedPin != nullptr)
											{
												showLabel("x Pin occupied", ImColor(20, 20, 20, 180));
												ed::RejectNewItem(ImColor(255, 0, 0), 1.0f);
											}
											else if (ed::AcceptNewItem())
											{
												
												if (!((inputPinUUID->ConnectedPin && !outputPinUUID->ConnectedPin) || (!inputPinUUID->ConnectedPin && outputPinUUID->ConnectedPin)))
												{
													if (inputPinUUID->ConnectedPin || outputPinUUID->ConnectedPin)
													{
														uint32_t index = 0;
														for (auto& link : m_CurrentAnimationBlueprint->m_PinLinks)
														{
															if (
																(link.PinA == inputPinUUID->ConnectedPin || link.PinB == outputPinUUID->ConnectedPin) ||
																(link.PinA == outputPinUUID->ConnectedPin || link.PinB == inputPinUUID->ConnectedPin)
																)
															{
																//link.PinA->ConnectedPin = nullptr;
																//link.PinB->ConnectedPin = nullptr;
																m_CurrentAnimationBlueprint->ErasePinLink(index);
																break;
															}
															index++;
														}
													}

													//inputPinUUID->ConnectedPin = outputPinUUID;
													//outputPinUUID->ConnectedPin = inputPinUUID;
													m_CurrentAnimationBlueprint->AddPinLink(inputPinUUID, outputPinUUID);

													ed::Link(m_CurrentAnimationBlueprint->m_PinLinks.back().LinkID.Get(), inputPinId, outputPinId);
												}
											}



										}
									}
								}
								else
									s_StartLinkPin = nullptr;
								ed::EndCreate(); // Wraps up object creation action handling.


								// Handle deletion action
								if (ed::BeginDelete())
								{
									// There may be many links marked for deletion, let's loop over them.
									ed::LinkId deletedLinkId;
									while (ed::QueryDeletedLink(&deletedLinkId))
									{
										// If you agree that link can be deleted, accept deletion.
										if (ed::AcceptDeletedItem())
										{
											// Then remove link from your data.
											uint32_t index = 0;
											for (auto& link : m_CurrentAnimationBlueprint->m_PinLinks)
											{
												if (link.LinkID == deletedLinkId.Get())
												{
													m_CurrentAnimationBlueprint->ErasePinLink(index);
													break;
												}
												index++;
											}
										}

									}
								}
								ed::EndDelete(); // Wrap up deletion action

							}
						}

						ImVec2 mousePos = ImGui::GetMousePos();
						static ed::NodeId contextNodeId = 0;

						ed::Suspend();
						{
							if (ed::ShowNodeContextMenu(&contextNodeId))
								ImGui::OpenPopup("Node Context Menu");
							if (ed::ShowBackgroundContextMenu())
								ImGui::OpenPopup("Create New Node");
						}
						ed::Resume();


						ed::Suspend();
						{
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
							if (ImGui::BeginPopup("Create New Node") || (ImGui::BeginPopupContextWindow(nullptr, 1, false)))
							{
								if (ImGui::MenuItem("Blend Node"))
								{
									Ref<BlendAnimationNode> blendAnimationNode = m_CurrentAnimationBlueprint->AddBlendNode();
									ed::SetNodePosition(blendAnimationNode->NodeID.Get(), mousePos);
								}
								if (ImGui::MenuItem("Condition Node"))
								{
									Ref<ConditionAnimationNode> conditionAnimationNode = m_CurrentAnimationBlueprint->AddConditionNode();
									ed::SetNodePosition(conditionAnimationNode->NodeID.Get(), mousePos);
								}

								ImGui::EndPopup();
							}
							else if (ImGui::BeginPopup("Node Context Menu") || (ImGui::BeginPopupContextWindow(nullptr, 1, false)))
							{
								if (ImGui::MenuItem("Delete"))
								{
									
									Vector<AnimationPinLink> remainedAnimationPinLinks;

									// Then remove link from your data.
									uint32_t index = 0;
									for (auto& link : m_CurrentAnimationBlueprint->m_PinLinks)
									{

										if (link.PinA->AnimationParentNode == contextNodeId.Get() || link.PinB->AnimationParentNode == contextNodeId.Get())
										{
											link.PinA->ConnectedPin = nullptr;
											link.PinB->ConnectedPin = nullptr;
										}
										else
										{
											remainedAnimationPinLinks.push_back(m_CurrentAnimationBlueprint->m_PinLinks[index]);
										}
										index++;
									}
									m_CurrentAnimationBlueprint->m_PinLinks = remainedAnimationPinLinks;


									ed::DeleteNode(contextNodeId);
									m_CurrentAnimationBlueprint->DeleteNode(contextNodeId.Get());


								}
								ImGui::EndPopup();
							}
							ImGui::PopStyleVar(); // ImGuiStyleVar_ItemSpacing
						}
						ed::Resume();



						ed::End();


						{
							ImGui::SetCursorPos({ ImGui::GetWindowWidth() - 60.0f, 5 });

							ImGuiLayer* imguilayer = Application::Get().GetImGuiLayer();
							ImGui::ImageButton(imguilayer->GetImGuiTextureID(m_SaveIconTex->GetImage2D()), { 30, 40 }, { 0.1f, 0.0f }, { 0.9, 1.0f });
							if(ImGui::IsItemClicked())
							{
								for (auto& [nodeHandle, animationNode] : m_CurrentAnimationBlueprint->m_NodeMap)
									m_CurrentAnimationBlueprint->m_NodePositionsMap[nodeHandle] = *(glm::vec2*)&ed::GetNodePosition(nodeHandle.Get());

								m_AnimationControllerBluePrint->Copy(m_CurrentAnimationBlueprint.Raw());
							}
						}

						{
							// Offset a bit to see the outlined border of the drag drop
							ImGui::SetCursorPos({ 5, 5 });

							ImGui::ScopedStyle buttonColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
							ImGui::ScopedStyle buttonColorActive(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
							ImGui::ScopedStyle buttonColorHovered(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
							ImGui::ScopedStyle borderColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

							ImVec2 viewportSize = ImGui::GetWindowSize();

							// Same here, offset the button with 5 pixels (in both sides) to see the outlined bordered of the drag drop
							// Also a custom function was required for InvisibleButton, because this should work as a dummy and should not add any events (only for rendering)
							UserInterface::InvisibleButtonWithNoBehaivour("##InvisibleButtonDragDrop", { viewportSize.x - 10.0f, viewportSize.y - 10.0f });

							if (ImGui::BeginDragDropTarget())
							{
								const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ANIMATION_NODE_EDITOR_DRAG_DROP);
								if (payload)
								{
									InputNodeHandle inputNodeHandle = *(InputNodeHandle*)payload->Data;
									Ref<AnimationNode> inputNode = m_CurrentAnimationBlueprint->AddInputNode(inputNodeHandle);
									ed::SetNodePosition(inputNode->NodeID.Get(), mousePos);
								}
								ImGui::EndDragDropTarget();
							}
						}
					}
					ImGui::EndChild();
					//ImGui::End();


					//ImGui::EndTable();
				}

			}

			// Check if the window is focused (also including child window)
			m_IsNodeEditorFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
			m_IsNodeEditorHovered = ImGui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows);

			ImGui::End();
			ImGui::PopStyleVar(2);
		}
	}

	void AnimationNoteEditor::Shutdown()
	{
		ed::DestroyEditor(m_Context);
	}

	void AnimationNoteEditor::SetActiveAnimationBlueprint(Ref<AnimationBlueprint> animationBlueprint)
	{
		m_CurrentAnimationBlueprint = Ref<AnimationBlueprint>::Create(animationBlueprint->m_MeshAsset);
		m_CurrentAnimationBlueprint->Copy(animationBlueprint.Raw());

		m_AnimationControllerBluePrint = animationBlueprint;
		m_SelectedInput = 0;

		for (auto& [nodeHandle, position] : m_CurrentAnimationBlueprint->m_NodePositionsMap)
			ed::SetNodePosition(nodeHandle.Get(), *(ImVec2*)&position);
			//m_CurrentAnimationBlueprint->m_NodePositionsMap[nodeHandle] = *(glm::vec2*)&ed::GetNodePosition(nodeHandle.Get());
	}

	Ref<Texture2D> AnimationNoteEditor::GetTextureByPinType(AnimationInput::Type pinType, bool isConnected)
	{
		switch (pinType)
		{
		case AnimationInput::Type::Float:
		case AnimationInput::Type::Int:
		case AnimationInput::Type::Bool:
			if (isConnected) return m_PinConnectedTex;
			return m_PinDisconnectedTex;
		case AnimationInput::Type::Animation:
			return m_AnimationPoseTex;
		}
		return nullptr;
	}

}