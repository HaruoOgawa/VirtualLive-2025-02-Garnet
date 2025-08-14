#include "CLive1135App.h"

#include <Graphics/CDrawInfo.h>
#include <Graphics/CFrameRenderer.h>
#include <Graphics/PostProcess/CPostProcess.h>

#include <Camera/CCamera.h>
#include <Camera/CTraceCamera.h>
#ifdef USE_VIEWER_CAMERA
#include <Camera/CViewerCamera.h>
#endif // USE_VIEWER_CAMERA

#include <LoadWorker/CLoadWorker.h>
#include <Projection/CProjection.h>
#include <Message/Console.h>
#include <Interface/IGUIEngine.h>
#include <Timeline/CTimelineController.h>
#include <Scene/CSceneController.h>

#include "../../GUIApp/GUI/CGraphicsEditingWindow.h"
#include "../../GUIApp/Model/CFileModifier.h"

namespace app
{
	CLive1135App::CLive1135App() :
		m_SceneController(std::make_shared<scene::CSceneController>()),
		m_CameraSwitchToggle(true),
		m_MainCamera(nullptr),
#ifdef USE_VIEWER_CAMERA
		m_ViewCamera(std::make_shared<camera::CViewerCamera>(glm::vec3(0.0f, 2.0f, 45.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f))),
#else
		m_ViewCamera(std::make_shared<camera::CCamera>()),
#endif // USE_VIEWER_CAMERA
		m_TraceCamera(std::make_shared<camera::CTraceCamera>()),
		m_Projection(std::make_shared<projection::CProjection>()),
		m_DrawInfo(std::make_shared<graphics::CDrawInfo>()),
#ifdef USE_GUIENGINE
		m_GraphicsEditingWindow(std::make_shared<gui::CGraphicsEditingWindow>()),
#endif // USE_GUIENGINE
		m_FileModifier(std::make_shared<CFileModifier>()),
		m_TimelineController(std::make_shared<timeline::CTimelineController>()),
		m_PostProcess(std::make_shared<graphics::CPostProcess>("MainResultPass")),
		m_PRCamera(std::make_shared<camera::CCamera>()),
		m_PRProjection(std::make_shared<projection::CProjection>()),
		m_PRPlaneWorldMatrix(glm::mat4(1.0f)),
		m_PRPlanePos(glm::vec3(0.0f))
	{
		m_MainCamera = m_ViewCamera;

		auto LightCamera = std::make_shared<camera::CCamera>();
		LightCamera->SetCenter(glm::vec3(0.0f, 0.0f, 0.0f));
		LightCamera->SetPos(glm::vec3(0.0f, 1.0f, 1.0f) * 20.0f);
		m_DrawInfo->SetLightCamera(LightCamera);

		m_DrawInfo->GetLightProjection()->SetNear(2.0f);
		m_DrawInfo->GetLightProjection()->SetFar(100.0f);

		m_SceneController->SetDefaultPass("MainGeometryPass");

#ifdef USE_GUIENGINE
		m_GraphicsEditingWindow->SetDefaultPass("MainResultPass", "");
#endif
	}

	bool CLive1135App::Release(api::IGraphicsAPI* pGraphicsAPI)
	{
		return true;
	}

	bool CLive1135App::Initialize(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker)
	{
		pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\MioMikoSuba_Photo.json", m_SceneController));
		//pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\Live_1135.json", m_SceneController));
		//pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\ModelViewer.json", m_SceneController));

		// オフスクリーンレンダリング
		// GBufferを組み込んだレンダリングパイプラインではフレームバッファコピー周りがややこしく非効率なことになるのでMSAAは使わない
		// 代わりにFXAAのポストプロセスでアンチエイリアシングを行う
		{
			graphics::SRenderPassState State = graphics::SRenderPassState(6);
			State.InitColorList[3] = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
			State.Stencil = true;
			if (!pGraphicsAPI->CreateRenderPass("GBufferGenPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, State)) return false;
		}

		{
			graphics::SRenderPassState State = graphics::SRenderPassState(1);
			State.Stencil = true;

			// GBufferパスの深度をフォアグラウンドパスにコピーするので深度は初期化しない
			State.ClearDepth = false;
			State.ClearStencil = false;

			if (!pGraphicsAPI->CreateRenderPass("GBufferLightPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, State)) return false;
		}

		{
			graphics::SRenderPassState State = graphics::SRenderPassState(1);
			State.Stencil = true;

			// GBufferパスの深度をフォアグラウンドパスにコピーするので深度は初期化しない
			State.ClearColor = false;
			State.ClearDepth = false;
			State.ClearStencil = false;

			if (!pGraphicsAPI->CreateRenderPass("MainGeometryPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, State)) return false;
		}

		{
			graphics::SRenderPassState State = graphics::SRenderPassState(1);
			State.Stencil = true;

			if (!pGraphicsAPI->CreateRenderPass("MainResultPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, State)) return false;
		}

		// ShadomMap
		{
			graphics::SRenderPassState PassState{};
			PassState.ColorBuffer = true; // Shadowなのでいらないけど互換性でいるにしておく(Vulkan, WebGPUでFragmentShaderなしパターンにまだ対応していない)
			PassState.ColorTexture = true;
			PassState.DepthBuffer = true;
			PassState.DepthTexture = true;

			if (!pGraphicsAPI->CreateRenderPass("ShadowPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, PassState)) return false;
		}

		// 平面反射(PlanerReflection)用フレームバッファ
		{
			{
				graphics::SRenderPassState State = graphics::SRenderPassState(6);
				State.InitColorList[3] = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
				if (!pGraphicsAPI->CreateRenderPass("PlanerReflection_GBufferGenPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, State)) return false;
			}

			{
				graphics::SRenderPassState State = graphics::SRenderPassState(1);

				// GBufferパスの深度をフォアグラウンドパスにコピーするので深度は初期化しない
				State.ClearDepth = false;

				if (!pGraphicsAPI->CreateRenderPass("PlanerReflection_GBufferLightPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, State)) return false;
			}

			{
				graphics::SRenderPassState State = graphics::SRenderPassState(1);

				// GBufferパスの深度をフォアグラウンドパスにコピーするので深度は初期化しない
				State.ClearColor = false;
				State.ClearDepth = false;
				State.ClearStencil = false;

				if (!pGraphicsAPI->CreateRenderPass("PlanerReflection_GeometryPass", api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS, -1, -1, State)) return false;
			}
		}

		// ポストプロセス
		m_PostProcess->SetUseFXAA(true);
		m_PostProcess->SetUseBloom(true);
		if (!m_PostProcess->Initialize(pGraphicsAPI, pLoadWorker)) return false;

		m_MainFrameRenderer = std::make_shared<graphics::CFrameRenderer>(pGraphicsAPI, "", pGraphicsAPI->FindOffScreenRenderPass("MainResultPass")->GetFrameTextureList());
		if (!m_MainFrameRenderer->Create(pLoadWorker, "Resources\\Common\\MaterialFrame\\FrameTexture_MF.json")) return false;

		// ShadowPass Texture
		const auto& ShadowPass = pGraphicsAPI->FindOffScreenRenderPass("ShadowPass");
		if (ShadowPass)
		{
			m_SceneController->AddFrameTexture(ShadowPass->GetDepthTexture());
		}
		
		// PlanerReflection_GeometryPass Texture
		const auto& PlanerReflection_GeometryPass = pGraphicsAPI->FindOffScreenRenderPass("PlanerReflection_GeometryPass");
		if (PlanerReflection_GeometryPass)
		{
			m_SceneController->AddFrameTexture(PlanerReflection_GeometryPass->GetFrameTexture());
		}

		return true;
	}

	bool CLive1135App::ProcessInput(api::IGraphicsAPI* pGraphicsAPI)
	{
		return true;
	}

	bool CLive1135App::Resize(int Width, int Height)
	{
		m_Projection->SetScreenResolution(Width, Height);
		m_PRProjection->SetScreenResolution(Width, Height);

		m_DrawInfo->GetLightProjection()->SetScreenResolution(Width, Height);

		return true;
	}

	bool CLive1135App::Update(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker, const std::shared_ptr<input::CInputState>& InputState)
	{
		// 平面反射用カメラの位置を決定する
		// メインカメラと反射面がなすViewDirを面対象にした方向
		{
			glm::vec3 forwardWorldSpace = m_MainCamera->GetViewDir();
			glm::vec3 upWorldSpace = m_MainCamera->GetUpVector();
			glm::vec3 posWorldSpace = m_MainCamera->GetPos();
			glm::vec3 centerWorldSpace = m_MainCamera->GetCenter();

			// ワールド座標系から反射面座標系に変換
			glm::mat4 PlaneWorldMatrix = m_PRPlaneWorldMatrix;

			glm::vec3 forwardPlaneSpace = glm::inverse(PlaneWorldMatrix) * glm::vec4(forwardWorldSpace.x, forwardWorldSpace.y, forwardWorldSpace.z, 0.0f);
			glm::vec3 upPlaneSpace = glm::inverse(PlaneWorldMatrix) * glm::vec4(upWorldSpace.x, upWorldSpace.y, upWorldSpace.z, 0.0f);
			glm::vec3 posPlaneSpace = glm::inverse(PlaneWorldMatrix) * glm::vec4(posWorldSpace.x, posWorldSpace.y, posWorldSpace.z, 1.0f);
			glm::vec3 centerPlaneSpace = glm::inverse(PlaneWorldMatrix) * glm::vec4(centerWorldSpace.x, centerWorldSpace.y, centerWorldSpace.z, 1.0f);

			// 面対称な位置に変換
			forwardPlaneSpace.y *= -1.0f;
			upPlaneSpace.y *= -1.0f;
			posPlaneSpace.y *= -1.0f;
			centerPlaneSpace.y *= -1.0f;

			// 反射面座標系からワールド座標系に戻す
			forwardWorldSpace = PlaneWorldMatrix * glm::vec4(forwardPlaneSpace.x, forwardPlaneSpace.y, forwardPlaneSpace.z, 0.0f);
			upWorldSpace = PlaneWorldMatrix * glm::vec4(upPlaneSpace.x, upPlaneSpace.y, upPlaneSpace.z, 0.0f);
			posWorldSpace = PlaneWorldMatrix * glm::vec4(posPlaneSpace.x, posPlaneSpace.y, posPlaneSpace.z, 1.0f);
			centerWorldSpace = PlaneWorldMatrix * glm::vec4(centerPlaneSpace.x, centerPlaneSpace.y, centerPlaneSpace.z, 1.0f);

			// 反射カメラに反射ベクトルを反映する
			m_PRCamera->SetPos(posWorldSpace);
			m_PRCamera->SetUpVector(upWorldSpace);
			m_PRCamera->SetCenter(centerWorldSpace);
		}

		if (!m_FileModifier->Update(pLoadWorker)) return false;

		if (pLoadWorker->IsLoaded())
		{
			if (!m_TimelineController->Update(m_DrawInfo->GetDeltaSecondsTime(), InputState)) return false;
		}

		if (!m_SceneController->Update(pGraphicsAPI, pPhysicsEngine, pLoadWorker, m_MainCamera, m_Projection, m_DrawInfo, InputState, m_TimelineController)) return false;

		m_MainCamera->Update(m_DrawInfo->GetDeltaSecondsTime(), InputState);

		if (InputState->IsKeyUp(input::EKeyType::KEY_TYPE_SPACE))
		{
			m_CameraSwitchToggle = !m_CameraSwitchToggle;

			if (m_CameraSwitchToggle)
			{
				m_MainCamera = m_ViewCamera;
			}
			else
			{
				m_MainCamera = m_TraceCamera;
			}
		}

		if (!m_PostProcess->Update(pGraphicsAPI, pPhysicsEngine, pLoadWorker, m_MainCamera, m_Projection, m_DrawInfo, InputState)) return false;
		if (!m_MainFrameRenderer->Update(pGraphicsAPI, pPhysicsEngine, pLoadWorker, m_MainCamera, m_Projection, m_DrawInfo, InputState)) return false;

		return true;
	}

	bool CLive1135App::LateUpdate(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker)
	{
		if (!m_SceneController->LateUpdate(pGraphicsAPI, pPhysicsEngine, pLoadWorker, m_DrawInfo)) return false;

		return true;
	}

	bool CLive1135App::FixedUpdate(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker)
	{
		if (!m_SceneController->FixedUpdate(pGraphicsAPI, pPhysicsEngine, pLoadWorker, m_DrawInfo)) return false;

		return true;
	}

	bool CLive1135App::Draw(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker, const std::shared_ptr<input::CInputState>& InputState,
		const std::shared_ptr<gui::IGUIEngine>& GUIEngine)
	{
		// ShadowPass
		{
			if (!pGraphicsAPI->BeginRender("ShadowPass")) return false;
			// CameraとProjectionにはライト用の値を使用するように注意する
			if (!m_SceneController->Draw(pGraphicsAPI, m_DrawInfo->GetLightCamera(), m_DrawInfo->GetLightProjection(), m_DrawInfo)) return false;
			if (!pGraphicsAPI->EndRender()) return false;
		}

		// PlanerReflection
		{
			m_DrawInfo->SetSpatialCulling(true);
			m_DrawInfo->SetSpatialCullPos(glm::vec4(m_PRPlanePos.x, m_PRPlanePos.y, m_PRPlanePos.z, 1.0f));

			// PlanerReflection_GBufferGenPass
			{
				if (!pGraphicsAPI->BeginRender("PlanerReflection_GBufferGenPass")) return false;
				if (!m_SceneController->Draw(pGraphicsAPI, m_PRCamera, m_PRProjection, m_DrawInfo)) return false;
				if (!pGraphicsAPI->EndRender()) return false;
			}

			// PlanerReflection_GBufferLightPass
			{
				// フォアグラウンドパス(GBufferLightPass)にデファードパスの深度をコピーする
				if (!pGraphicsAPI->CopyDepthBuffer("PlanerReflection_GBufferGenPass", "PlanerReflection_GBufferLightPass")) return false;

				if (!pGraphicsAPI->BeginRender("PlanerReflection_GBufferLightPass")) return false;
				if (!m_SceneController->Draw(pGraphicsAPI, m_PRCamera, m_PRProjection, m_DrawInfo)) return false;
				if (!pGraphicsAPI->EndRender()) return false;
			}

			// PlanerReflection_GeometryPass
			{
				// フォアグラウンドパス(PlanerReflection_GeometryPass)にGBufferLightPassのカラー・深度をコピーする
				if (!pGraphicsAPI->CopyRenderPass("PlanerReflection_GBufferLightPass", "PlanerReflection_GeometryPass", true, true)) return false;

				if (!pGraphicsAPI->BeginRender("PlanerReflection_GeometryPass")) return false;
				if (!m_SceneController->Draw(pGraphicsAPI, m_PRCamera, m_PRProjection, m_DrawInfo)) return false;
				if (!pGraphicsAPI->EndRender()) return false;
			}

			m_DrawInfo->SetSpatialCulling(false);
		}

		// GBufferGenPass
		{
			if (!pGraphicsAPI->BeginRender("GBufferGenPass")) return false;
			if (!m_SceneController->Draw(pGraphicsAPI, m_MainCamera, m_Projection, m_DrawInfo)) return false;
			if (!pGraphicsAPI->EndRender()) return false;
		}

		// GBufferLightPass
		{
			// フォアグラウンドパス(GBufferLightPass)にデファードパスの深度をコピーする
			if (!pGraphicsAPI->CopyDepthBuffer("GBufferGenPass", "GBufferLightPass")) return false;

			if (!pGraphicsAPI->BeginRender("GBufferLightPass")) return false;
			if (!m_SceneController->Draw(pGraphicsAPI, m_MainCamera, m_Projection, m_DrawInfo)) return false;
			if (!pGraphicsAPI->EndRender()) return false;
		}

		// MainGeometryPass
		{
			// フォアグラウンドパス(MainGeometryPass)にGBufferLightPassのカラー・深度をコピーする
			if (!pGraphicsAPI->CopyRenderPass("GBufferLightPass", "MainGeometryPass", true, true)) return false;

			if (!pGraphicsAPI->BeginRender("MainGeometryPass")) return false;
			if (!m_SceneController->Draw(pGraphicsAPI, m_MainCamera, m_Projection, m_DrawInfo)) return false;
			if (!pGraphicsAPI->EndRender()) return false;
		}

		// MainResultPass 
		{
			// ポストプロセスに渡すためにここではコピーだけを行う
			// パスを始めてしまうとせっかくコピーした内容がリセットされてしまう
			// MainGeometryPassとMainResultPassを分離したのはMainGeometryPassではカラー・デプスを初期化しないようにしているため、
			// その影響でうまくポストプロセスが効かなくなるから
			if (!pGraphicsAPI->CopyRenderPass("MainGeometryPass", "MainResultPass", true, true)) return false;
		}

		// ポストプロセス
		if (!m_PostProcess->Draw(pGraphicsAPI, m_MainCamera, m_Projection, m_DrawInfo)) return false;

		// Main FrameBuffer
		{
			if (!pGraphicsAPI->BeginRender()) return false;

			if (!m_MainFrameRenderer->Draw(pGraphicsAPI, m_MainCamera, m_Projection, m_DrawInfo)) return false;

			// GUIEngine
			std::function<bool(void)> DrawGUIEngine = [this, pLoadWorker, GUIEngine, pGraphicsAPI, pPhysicsEngine, InputState]() {
#ifdef USE_GUIENGINE
				if (pLoadWorker->IsLoaded())
				{
					gui::SGUIParams GUIParams = gui::SGUIParams(shared_from_this(), GetObjectList(), m_SceneController, m_FileModifier, m_TimelineController, pLoadWorker, {}, pPhysicsEngine);
					GUIParams.CameraMode = (m_CameraSwitchToggle) ? "ViewCamera" : "TraceCamera";
					GUIParams.Camera = m_MainCamera;
					GUIParams.InputState = InputState;
					GUIParams.ValueRegistryList.emplace(m_PostProcess->GetBloomFilter()->GetRegistryName(), m_PostProcess->GetBloomFilter());

					if (!GUIEngine->BeginFrame(pGraphicsAPI)) return false;
					if (!m_GraphicsEditingWindow->Draw(pGraphicsAPI, GUIParams, GUIEngine))
					{
						Console::Log("[Error] InValid GUI\n");
						return false;
					}
					if (!GUIEngine->EndFrame(pGraphicsAPI)) return false;
				}
#endif // USE_GUIENGINE

				return true;
			};

			if (!pLoadWorker->Draw(pGraphicsAPI, m_MainCamera, m_Projection, m_DrawInfo)) return false;

			if (!pGraphicsAPI->EndRender(DrawGUIEngine)) return false;
		}

		return true;
	}

	std::shared_ptr<graphics::CDrawInfo> CLive1135App::GetDrawInfo() const
	{
		return m_DrawInfo;
	}

	// 起動準備完了
	bool CLive1135App::OnStartup(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker, const std::shared_ptr<gui::IGUIEngine>& GUIEngine)
	{
		const auto& TimelineFileName = m_SceneController->GetTimelineFileName();
		if (!TimelineFileName.empty()) pLoadWorker->AddLoadResource(std::make_shared<resource::CTimelineClipLoader>(TimelineFileName, m_TimelineController->GetClip()));

		return true;
	}

	// ロード完了イベント
	bool CLive1135App::OnLoaded(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker, const std::shared_ptr<gui::IGUIEngine>& GUIEngine)
	{
		if (!m_SceneController->Create(pGraphicsAPI, pPhysicsEngine)) return false;

		// 平面反射の座標を指定
		{
			// 今回はたまたま原点位置と平面の高さが一致しているのでわざわざノードから取得・計算するみたいなことはしない
			m_PRPlanePos = glm::vec3(0.0f, 0.0f, 0.0f);
			m_PRPlaneWorldMatrix = glm::translate(glm::mat4(1.0f), m_PRPlanePos);

			//m_PRProjection->EnabledObliqueMat(true, glm::vec4(m_PRPlanePos.x, m_PRPlanePos.y, m_PRPlanePos.z, 1.0f));
		}

		m_PostProcess->GetBloomFilter()->OnLoaded(m_SceneController);

		if (!m_TimelineController->Initialize(shared_from_this())) return false;

#ifdef USE_GUIENGINE
		{
			gui::SGUIParams GUIParams = gui::SGUIParams(shared_from_this(), GetObjectList(), m_SceneController, m_FileModifier, m_TimelineController, pLoadWorker, {}, pPhysicsEngine);
			GUIParams.ValueRegistryList.emplace(m_PostProcess->GetBloomFilter()->GetRegistryName(), m_PostProcess->GetBloomFilter());

			if (!m_GraphicsEditingWindow->OnLoaded(pGraphicsAPI, GUIParams, GUIEngine)) return false;
		}
#endif

		// カメラ
		{
			const auto& Object = m_SceneController->FindObjectByName("CameraObject");
			if (Object)
			{
				const auto& Node = Object->FindNodeByName("CameraNode");

				if (Node)
				{
					m_TraceCamera->SetTargetNode(Node);
				}
			}
		}

		const auto& Sound = m_SceneController->GetSound();
		const auto& SoundClip = std::get<0>(Sound);
		if (SoundClip)
		{
			SoundClip->PlayOneShot();
		}

		return true;
	}

	// フォーカスイベント
	void CLive1135App::OnFocus(bool Focused, api::IGraphicsAPI* pGraphicsAPI, resource::CLoadWorker* pLoadWorker)
	{
		if (Focused && pLoadWorker)
		{
			m_FileModifier->OnFileUpdated(pLoadWorker);
		}
	}

	// エラー通知イベント
	void CLive1135App::OnAssertError(const std::string& Message)
	{
#ifdef USE_GUIENGINE
		m_GraphicsEditingWindow->AddLog(gui::EGUILogType::Error, Message);
#endif
	}

	// Getter
	std::vector<std::shared_ptr<object::C3DObject>> CLive1135App::GetObjectList() const
	{
		std::vector<std::shared_ptr<object::C3DObject>> ObjectList;

		for (const auto& Object : m_SceneController->GetObjectList())
		{
			ObjectList.push_back(Object);
		}

		return ObjectList;
	}

	std::shared_ptr<scene::CSceneController> CLive1135App::GetSceneController() const
	{
		return m_SceneController;
	}
}