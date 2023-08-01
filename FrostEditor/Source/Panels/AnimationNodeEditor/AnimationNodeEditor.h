#pragma once

#include "Panels/Panel.h"
#include "Frost/Renderer/Animation/AnimationBlueprint.h"
#include "Frost/Renderer/Texture.h"
#include "Frost/Renderer/Animation/AnimationNode.h"

namespace Frost
{

	class AnimationNoteEditor : public Panel
	{
	public:
		AnimationNoteEditor();
		virtual ~AnimationNoteEditor();

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void SetActiveAnimationBlueprint(Ref<AnimationBlueprint> animationBlueprint);

		Ref<Texture2D> GetTextureByPinType(AnimationInput::Type pinType, bool isConnected);

		bool IsNodeEditorFocused() const { return m_IsNodeEditorFocused; }
		bool IsNodeEditorHovered() const { return m_IsNodeEditorHovered; }

	private:
		bool m_Visibility = true;

		Ref<AnimationBlueprint> m_CurrentAnimationBlueprint = nullptr;
		Ref<AnimationBlueprint> m_AnimationControllerBluePrint = nullptr;
		InputNodeHandle m_SelectedInput = 0;

		bool m_IsNodeEditorFocused = false;
		bool m_IsNodeEditorHovered = false;

		Ref<Texture2D> m_PinConnectedTex;
		Ref<Texture2D> m_PinDisconnectedTex;
		Ref<Texture2D> m_AnimationPoseTex;
		Ref<Texture2D> m_AnimationPoseDisconnectedTex;
		Ref<Texture2D> m_SaveIconTex;
	};

}