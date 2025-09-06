#include "CLive1135App.h"

#include <Graphics/CDrawInfo.h>
#include <Graphics/CFrameRenderer.h>
#include <Graphics/PostProcess/CPostProcess.h>

#include <Camera/CCamera.h>
#include <Camera/CLookUpTraceCamera.h>
#ifdef USE_VIEWER_CAMERA
#include <Camera/CViewerCamera.h>
#endif // USE_VIEWER_CAMERA

#include <LoadWorker/CLoadWorker.h>
#include <Projection/CProjection.h>
#include <Message/Console.h>
#include <Interface/IGUIEngine.h>
#include <Timeline/CTimelineController.h>
#include <Scene/CSceneController.h>
#include <Scriptable/CComponentResolver.h>

#include "../../GUIApp/GUI/CGraphicsEditingWindow.h"
#include "../../GUIApp/Model/CFileModifier.h"
#include "../../Component/CVATGenerator.h"
#include "../../Component/CCameraSwitcherComponent.h"

#ifdef USE_NETWORK
#include <Network/CUDPSocket.h>
#include <Network/DMX/CDMXDataHandler.h>
#include <Network/CNDIReceiver.h>
#endif

namespace app
{
	CLive1135App::CLive1135App() :
		m_SceneController(std::make_shared<scene::CSceneController>()),
		m_CameraSwitchToggle(true),
		m_MainCamera(nullptr),
#ifdef USE_VIEWER_CAMERA
		m_ViewCamera(std::make_shared<camera::CViewerCamera>(glm::vec3(0.0f, 2.0f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f))),
#else
		m_ViewCamera(std::make_shared<camera::CCamera>()),
#endif // USE_VIEWER_CAMERA
		m_CurrentLookUpCamera(nullptr),
		m_LookUpCameraA(std::make_shared<camera::CLookUpTraceCamera>()),
		m_LookUpCameraB(std::make_shared<camera::CLookUpTraceCamera>()),
		m_LookUpSwitchToggle(true),
		m_Projection(std::make_shared<projection::CProjection>()),
		m_DrawInfo(std::make_shared<graphics::CDrawInfo>()),
#ifdef USE_GUIENGINE
		m_GraphicsEditingWindow(std::make_shared<gui::CGraphicsEditingWindow>()),
#endif // USE_GUIENGINE
#ifdef USE_NETWORK
		m_UDPSocket(std::make_shared<network::CUDPSocket>("192.168.0.252", 6454)),
		m_DMXHandler(std::make_shared<network::CDMXDataHandler>()),
		m_NDIReceiver(std::make_shared<network::CNDIReceiver>()),
#endif // USE_NETWORK
		m_NDITexIndex(-1),
		m_FileModifier(std::make_shared<CFileModifier>()),
		m_TimelineController(std::make_shared<timeline::CTimelineController>()),
		m_PostProcess(std::make_shared<graphics::CPostProcess>("MainResultPass")),
		m_PRCamera(std::make_shared<camera::CCamera>()),
		m_PRProjection(std::make_shared<projection::CProjection>()),
		m_PRPlaneWorldMatrix(glm::mat4(1.0f)),
		m_PRPlanePos(glm::vec3(0.0f))
	{
		m_MainCamera = m_ViewCamera;
		m_CurrentLookUpCamera = m_LookUpCameraA;

#ifdef _DEBUG
		m_MainCamera = m_ViewCamera;
#else
		m_MainCamera = m_CurrentLookUpCamera;
#endif // _DEBUG

		auto LightCamera = std::make_shared<camera::CCamera>();
		LightCamera->SetCenter(glm::vec3(0.0f, 0.0f, 0.0f));
		LightCamera->SetPos(glm::vec3(0.0f, 1.0f, -1.0f) * 5.0f); // Live
		//LightCamera->SetPos(glm::vec3(0.0f, 1.0f, 1.0f) * 20.0f); // Photo
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
		m_UDPSocket->Close();

		return true;
	}

	bool CLive1135App::Initialize(api::IGraphicsAPI* pGraphicsAPI, physics::IPhysicsEngine* pPhysicsEngine, resource::CLoadWorker* pLoadWorker)
	{
		//pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\sample.json", m_SceneController));
		pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\Live_dotttabata.json", m_SceneController));
		//pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\Live_1135.json", m_SceneController));
		//pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\MioMikoSuba_Photo.json", m_SceneController));
		//pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\FubuMio.json", m_SceneController));
		//pLoadWorker->AddScene(std::make_shared<resource::CSceneLoader>("Resources\\User\\Scene\\FubuMio.json", m_SceneController));

#ifdef USE_NETWORK
		if (!m_UDPSocket->Initialize(shared_from_this(), true)) return false;

		// DMX準備
		{
			// ライト
			network::SDMXFixture Fixture{};
			Fixture.ChannelNameList = { "R", "G", "B", "A", "Dimmer", "Pan", "Tilt", "Angle", "Height"};

			Fixture.DeviceName = "Lift_MovingLight_Base";
			m_DMXHandler->RegistDeviceFixture(1, 0, 0, Fixture);

			Fixture.DeviceName = "Ceiling_MovingLight_Base";
			m_DMXHandler->RegistDeviceFixture(3, 0, 0, Fixture);

			Fixture.DeviceName = "Stage_MovingLight_Base";
			m_DMXHandler->RegistDeviceFixture(4, 0, 0, Fixture);
			
			Fixture.DeviceName = "UpperStage_MovingLight_Base";
			m_DMXHandler->RegistDeviceFixture(5, 0, 0, Fixture);
		}

		{
			// CameraSwitcher
			network::SDMXFixture Fixture{};
			Fixture.DeviceName = "CameraSwitcher";
			Fixture.ChannelNameList = {
				"ID",

				// CameraA
				"CameraA_PosX_Byte_0", "CameraA_PosX_Byte_1", "CameraA_PosX_Byte_2", "CameraA_PosX_Byte_3",
				"CameraA_PosY_Byte_0", "CameraA_PosY_Byte_1", "CameraA_PosY_Byte_2", "CameraA_PosY_Byte_3",
				"CameraA_PosZ_Byte_0", "CameraA_PosZ_Byte_1", "CameraA_PosZ_Byte_2", "CameraA_PosZ_Byte_3",
				"CameraA_ZAngle_Byte_0", "CameraA_ZAngle_Byte_1", "CameraA_ZAngle_Byte_2", "CameraA_ZAngle_Byte_3",
				"CameraA_CenterX_Byte_0", "CameraA_CenterX_Byte_1", "CameraA_CenterX_Byte_2", "CameraA_CenterX_Byte_3",
				"CameraA_CenterY_Byte_0", "CameraA_CenterY_Byte_1", "CameraA_CenterY_Byte_2", "CameraA_CenterY_Byte_3",
				"CameraA_CenterZ_Byte_0", "CameraA_CenterZ_Byte_1", "CameraA_CenterZ_Byte_2", "CameraA_CenterZ_Byte_3",

				// CameraB
				"CameraB_PosX_Byte_0", "CameraB_PosX_Byte_1", "CameraB_PosX_Byte_2", "CameraB_PosX_Byte_3",
				"CameraB_PosY_Byte_0", "CameraB_PosY_Byte_1", "CameraB_PosY_Byte_2", "CameraB_PosY_Byte_3",
				"CameraB_PosZ_Byte_0", "CameraB_PosZ_Byte_1", "CameraB_PosZ_Byte_2", "CameraB_PosZ_Byte_3",
				"CameraB_ZAngle_Byte_0", "CameraB_ZAngle_Byte_1", "CameraB_ZAngle_Byte_2", "CameraB_ZAngle_Byte_3",
				"CameraB_CenterX_Byte_0", "CameraB_CenterX_Byte_1", "CameraB_CenterX_Byte_2", "CameraB_CenterX_Byte_3",
				"CameraB_CenterY_Byte_0", "CameraB_CenterY_Byte_1", "CameraB_CenterY_Byte_2", "CameraB_CenterY_Byte_3",
				"CameraB_CenterZ_Byte_0", "CameraB_CenterZ_Byte_1", "CameraB_CenterZ_Byte_2", "CameraB_CenterZ_Byte_3",
			};

			m_DMXHandler->RegistDeviceFixture(2, 0, 0, Fixture);
		}

		// NDIレシーバー初期化
		if (!m_NDIReceiver->Initialize()) return false;
#endif

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
		if (!m_FileModifier->Update(pLoadWorker)) return false;

#ifdef USE_NETWORK
		if (pLoadWorker->IsLoaded())
		{
			if (!m_NDIReceiver->Update(this)) return false;
		}
#endif

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
				m_MainCamera = m_CurrentLookUpCamera;
			}
		}

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

	// コンポーネント作成
	std::shared_ptr<scriptable::CComponent> CLive1135App::CreateComponent(const std::string& ComponentType, const std::string& ValueRegistry)
	{
		if (ComponentType == "VATGenerator")
		{
			return std::make_shared<component::CVATGenerator>(ComponentType, ValueRegistry);
		}
		else if (ComponentType == "CameraSwitcher")
		{
			return std::make_shared<scriptable::CCameraSwitcherComponent>(ComponentType, ValueRegistry, shared_from_this(),
				std::vector<std::shared_ptr<camera::CLookUpTraceCamera>>({
					m_LookUpCameraA,
					m_LookUpCameraB,
				}));
		}

		return nullptr;
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

		// NDI用の空テクスチャを作成
		{
			auto EmptyTexture = pGraphicsAPI->CreateTexture(false);

			int pixelByteSize = 1920 * 1080 * 4;
			std::vector<unsigned char> emptyPixel;
			emptyPixel.resize(pixelByteSize, 0);

			EmptyTexture->Create(emptyPixel, 1920, 1080, 4, api::ERenderPassFormat::COLOR_BGRA);

			const auto& IndoorLiveStage = m_SceneController->FindObjectByName("IndoorLiveStage");
			if (IndoorLiveStage)
			{
				m_NDITexIndex = static_cast<int>(IndoorLiveStage->GetTextureSet()->Get2DTextureList().size());

				IndoorLiveStage->GetTextureSet()->Add2DTexture(EmptyTexture);

				std::vector<std::string> AssignTexNodeList = {
					"C01_Screen_Back",
					"C02_Screen_Front01",
					"C03_Screen_Front02",
					"C04_Screen_Celling_01",
				};

				for(const auto& NodeName : AssignTexNodeList)
				{
					const auto& Node = IndoorLiveStage->FindNodeByName(NodeName);
					if (Node)
					{
						int MeshIndex = Node->GetMeshIndex();
						if (MeshIndex == -1) continue;

						const auto& Mesh = IndoorLiveStage->GetMeshList()[MeshIndex];
						if (!Mesh) continue;

						for (const auto& Primitive : Mesh->GetPrimitiveList())
						{
							for (const auto& Renderer : Primitive->GetRendererList())
							{
								std::get<1>(Renderer)->SetUniformValue("useBaseColorTexture", &glm::ivec1(1)[0], sizeof(int));
								std::get<1>(Renderer)->ReplaceTextureIndex("baseColorTexture", m_NDITexIndex);
								std::get<1>(Renderer)->CreateRefTextureList(IndoorLiveStage->GetTextureSet());
							}
						}
					}
				}
			}
		}

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

		// DMXに照明灯体を渡す
		{
			std::map<std::string, int> BaseNameCountMap;
			BaseNameCountMap.emplace("Lift_MovingLight_Base.", 4);       // Net1
			BaseNameCountMap.emplace("Ceiling_MovingLight_Base.", 12);   // Net3
			BaseNameCountMap.emplace("StageLeft_MovingLight_Base.", 5);  // Net4
			BaseNameCountMap.emplace("StageRight_MovingLight_Base.", 5); // Net4
			BaseNameCountMap.emplace("UpperStageLeft_MovingLight_Base.", 5);   // Net5
			BaseNameCountMap.emplace("UpperStageRight_MovingLight_Base.", 5);  // Net5

			const auto& Object = m_SceneController->FindObjectByName("IndoorLiveStage");
			if (Object)
			{
				int NodeNum = static_cast<int>(Object->GetNodeList().size());

				for (const auto& pair : BaseNameCountMap)
				{
					const auto& BaseName = pair.first;
					int Count = pair.second;

					for (int i = 0; i < Count; i++)
					{
						// 数値を"003"のようなゼロ埋め3桁文字列に変換する
						char buf[4];
						snprintf(buf, sizeof(buf), "%03d", i);

						std::string NodeName = BaseName;
						NodeName += buf;

						const auto& SpotLight = Object->FindNodeByName(NodeName);

						for (const auto& Component : SpotLight->GetComponentList())
						{
							if (Component->GetComponentName() != "SpotLightDMXController") continue;

							std::string DMXFixtureName = std::string();

							if (BaseName == "Lift_MovingLight_Base.")
							{
								DMXFixtureName = "Lift_MovingLight_Base";
							}
							else if (BaseName == "Ceiling_MovingLight_Base.")
							{
								DMXFixtureName = "Ceiling_MovingLight_Base";
							}
							else if (BaseName == "StageLeft_MovingLight_Base." || BaseName == "StageRight_MovingLight_Base.")
							{
								DMXFixtureName = "Stage_MovingLight_Base";
								Component->GetValueRegistry()->SetValue("UpsideDown", graphics::EUniformValueType::VALUE_TYPE_INT, &glm::ivec1(1)[0], sizeof(int));
							}
							else if (BaseName == "UpperStageLeft_MovingLight_Base." || BaseName == "UpperStageRight_MovingLight_Base.")
							{
								DMXFixtureName = "UpperStage_MovingLight_Base";
								Component->GetValueRegistry()->SetValue("UpsideDown", graphics::EUniformValueType::VALUE_TYPE_INT, &glm::ivec1(1)[0], sizeof(int));
							}
							else
							{
								Console::Log("[Error] Unknown BaseName(%s)\n", BaseName.c_str());
								continue;
							}

							Component->GetValueRegistry()->SetValue("DMXFixtureName", graphics::EUniformValueType::VALUE_TYPE_STRING, DMXFixtureName.data(), DMXFixtureName.size() * sizeof(char));

							m_DMXHandler->AddDevice(DMXFixtureName, Component);
						}
					}
				}
			}
		}

		// DMXにカメラスイッチャーを渡す
		{
			const auto& Object = m_SceneController->FindObjectByName("CameraSwitcher");
			if (Object)
			{
				const auto& ComponentList = Object->GetComponentList();
				if (!ComponentList.empty())
				{
					const auto& Component = ComponentList[0];

					m_DMXHandler->AddDevice("CameraSwitcher", Component);
				}
			}
		}

		const auto& Sound = m_SceneController->GetSound();
		const auto& SoundClip = std::get<0>(Sound);
		if (SoundClip)
		{
			SoundClip->SetPlayPos(48.5f);
			SoundClip->PlayOneShot();
		}

		const auto& ookamimio = m_SceneController->FindObjectByName("ookamimio");
		if (ookamimio) ookamimio->GetAnimationController()->ChangeMotion("Dance");
		
		const auto& oozorasubaru = m_SceneController->FindObjectByName("oozorasubaru");
		if (oozorasubaru) oozorasubaru->GetAnimationController()->ChangeMotion("Dance");

		const auto& sakuramiko = m_SceneController->FindObjectByName("sakuramiko");
		if (sakuramiko) sakuramiko->GetAnimationController()->ChangeMotion("Dance");

		const auto& fubuki = m_SceneController->FindObjectByName("fubuki");
		if (fubuki) fubuki->GetAnimationController()->ChangeMotion("Dance");

		return true;
	}

	// オブジェクト個別のロード完了イベント
	bool CLive1135App::OnObjectLoaded(const std::shared_ptr<object::C3DObject>& Object, api::IGraphicsAPI* pGraphicsAPI, resource::CLoadWorker* pLoadWorker)
	{
		std::map<std::string, int> BaseNameCountMap;
		BaseNameCountMap.emplace("Lift_MovingLight_Base.", 4);
		BaseNameCountMap.emplace("Ceiling_MovingLight_Base.", 12);
		BaseNameCountMap.emplace("StageLeft_MovingLight_Base.", 5);
		BaseNameCountMap.emplace("StageRight_MovingLight_Base.", 5);
		BaseNameCountMap.emplace("UpperStageLeft_MovingLight_Base.", 5);
		BaseNameCountMap.emplace("UpperStageRight_MovingLight_Base.", 5);

		// スポットライトノードとコンポーネントをまとめて自動生成
		if (Object->GetObjectName() == "LightList") // 動作させないので_NotWorkingを付与しておく
		{
			int NodeNum = static_cast<int>(Object->GetNodeList().size());

			for (const auto& pair : BaseNameCountMap)
			{
				const auto& BaseName = pair.first;
				int Count = pair.second;

				for (int i = 0; i < Count; i++)
				{
					// 数値を"003"のようなゼロ埋め3桁文字列に変換する
					char buf[4];
					snprintf(buf, sizeof(buf), "%03d", i);

					std::string NodeName = BaseName;
					NodeName += buf;

					std::shared_ptr<object::CNode> Node = Object->FindNodeByName(NodeName);
					const bool Exist = (Node != nullptr);
					
					// ノード生成
					if(!Node) Node = std::make_shared<object::CNode>(-1, NodeNum);
					
					// ステージに置いてるライトなら上を向かせておく
					if (BaseName == "StageLeft_MovingLight_Base." || 
						BaseName == "StageRight_MovingLight_Base." ||
						BaseName == "UpperStageLeft_MovingLight_Base." ||
						BaseName == "UpperStageRight_MovingLight_Base.")
					{
						Node->SetRot(glm::angleAxis(glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
					}

					// 新規生成ではなくすでに存在しているノードだった場合は後の処理はスキップ
					if (Exist) continue; 

					//
					Node->SetName(NodeName);
					Object->AddNode(Node);

					// コンポーネント追加
					auto Component = scriptable::CComponentResolver::Resolve(this, "SpotLight", "");
					Component->Initialize(pGraphicsAPI, pLoadWorker);

					// 描画パスを指定
					std::string DefferdPassName = "GBufferGenPass";
					std::string LightingPassName = "GBufferLightPass";
					std::string ForegroundPassName = "MainGeometryPass";

					Component->GetValueRegistry()->SetValue("DefferdPassName", graphics::EUniformValueType::VALUE_TYPE_STRING, DefferdPassName.c_str(), sizeof(char) * DefferdPassName.size());
					Component->GetValueRegistry()->SetValue("LightingPassName", graphics::EUniformValueType::VALUE_TYPE_STRING, LightingPassName.c_str(), sizeof(char) * LightingPassName.size());
					Component->GetValueRegistry()->SetValue("ForegroundPassName", graphics::EUniformValueType::VALUE_TYPE_STRING, ForegroundPassName.c_str(), sizeof(char) * ForegroundPassName.size());

					Node->AddComponent(Component);

					NodeNum++;
				}
			}
		}
		else if (Object->GetObjectName() == "IndoorLiveStage")
		{
			std::map<std::string, std::shared_ptr<object::CNode>> EmitterList;

			for (const auto& pair : BaseNameCountMap)
			{
				const auto& BaseName = pair.first;
				int Count = pair.second;

				for (int i = 0; i < Count; i++)
				{
					// 数値を"003"のようなゼロ埋め3桁文字列に変換する
					char buf[4];
					snprintf(buf, sizeof(buf), "%03d", i);

					std::string NodeName = BaseName;
					NodeName += buf;

					const auto& Node = Object->FindNodeByName(NodeName);
					if (!Node) continue;

					// すでにコンポーネントが付与されている場合はスキップ
					if (!Node->GetComponentList().empty()) continue; 

					int ChildNodeInd_2nd = Node->GetChildrenNodeIndexList()[0];
					const auto& ChildNode_2nd = Object->FindNodeByIndex(ChildNodeInd_2nd);
					if (!ChildNode_2nd) continue;

					ChildNode_2nd->SetRot(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

					int ChildNodeInd_3rd = ChildNode_2nd->GetChildrenNodeIndexList()[0];
					const auto& ChildNode_3rd = Object->FindNodeByIndex(ChildNodeInd_3rd);
					if (!ChildNode_3rd) continue;

					EmitterList.emplace(NodeName, ChildNode_3rd);

					// コンポーネント追加
					auto Component = scriptable::CComponentResolver::Resolve(this, "SpotLightDMXController", "");
					Component->Initialize(pGraphicsAPI, pLoadWorker);

					// Y軸回転
					Component->GetValueRegistry()->SetValue("TiltNodeName", graphics::EUniformValueType::VALUE_TYPE_STRING, Node->GetName().c_str(), sizeof(char) * Node->GetName().size());
					
					// X軸回転
					Component->GetValueRegistry()->SetValue("PanNodeName", graphics::EUniformValueType::VALUE_TYPE_STRING, ChildNode_2nd->GetName().c_str(), sizeof(char) * ChildNode_2nd->GetName().size());
					
					//
					Component->GetValueRegistry()->SetValue("SpotLightFollowTarget", graphics::EUniformValueType::VALUE_TYPE_STRING, ChildNode_3rd->GetName().c_str(), sizeof(char) * ChildNode_3rd->GetName().size());
				
					//
					std::string SpotLightObjName = "LightList";
					Component->GetValueRegistry()->SetValue("SpotLightObjName", graphics::EUniformValueType::VALUE_TYPE_STRING, SpotLightObjName.c_str(), sizeof(char) * SpotLightObjName.size());
					// スポットライトノードはBaseNameと同じ名前にしている(1つ上のOnObjectLoadedの処理)
					Component->GetValueRegistry()->SetValue("SpotLightNodeName", graphics::EUniformValueType::VALUE_TYPE_STRING, NodeName.c_str(), sizeof(char) * NodeName.size());

					// コンポーネントをアタッチ
					Node->AddComponent(Component);
				}

				Object->CalcWorldMatrix();

				// Emitterのワールド座標を各スポットライトに指定する
				if(true)
				{
					const auto& LightList = m_SceneController->FindObjectByName("LightList");
					if (LightList)
					{
						for (const auto& pair : BaseNameCountMap)
						{
							const auto& BaseName = pair.first;
							int Count = pair.second;

							for (int i = 0; i < Count; i++)
							{
								// 数値を"003"のようなゼロ埋め3桁文字列に変換する
								char buf[4];
								snprintf(buf, sizeof(buf), "%03d", i);

								std::string NodeName = BaseName;
								NodeName += buf;

								const auto& SpotLight = LightList->FindNodeByName(NodeName);
								if (!SpotLight) continue;

								const auto& it = EmitterList.find(NodeName);
								if (it == EmitterList.end()) continue;
								const auto& Emitter = it->second;

								//
								const auto& Scale = Object->GetScale();
								const auto& Rotate = Object->GetRot();
								auto WorldPos = Emitter->GetWorldPos();

								WorldPos *= Scale;

								glm::vec4 RotPos = glm::vec4(WorldPos, 1.0f);
								RotPos = glm::mat4_cast(Rotate) * RotPos;
								WorldPos = glm::vec3(RotPos.x, RotPos.y, RotPos.z);

								SpotLight->SetPos(WorldPos);
							}
						}
					}
				}
			}
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

	// DMXデータ受信イベント
	void CLive1135App::OnReceiveArtNetDMX(unsigned short Net, unsigned short SubNet, unsigned short Universe, const std::vector<unsigned char>& DataBuffer)
	{
#ifdef USE_NETWORK

		if (!m_DMXHandler) return;

		m_DMXHandler->DispatchDMXData(Net, SubNet, Universe, DataBuffer);
#endif // USE_NETWORK
	}

	// NDIデータ受信イベント
	void CLive1135App::OnReceiveNDIImage(const std::vector<unsigned char>& pixelData, int Width, int Height, api::ERenderPassFormat RenderPassFormat)
	{
		if (!m_SceneController->IsLoaded()) return;

		const auto& IndoorLiveStage = m_SceneController->FindObjectByName("IndoorLiveStage");
		if (IndoorLiveStage)
		{
			const auto& TextureList = IndoorLiveStage->GetTextureSet()->Get2DTextureList();

			if (!TextureList.empty() && m_NDITexIndex >= 0)
			{
				TextureList[m_NDITexIndex]->ReplacePixelData(pixelData, Width, Height, RenderPassFormat);
			}
		}
	}

	// カスタムイベント発火
	void CLive1135App::OnRaisedEvent(const std::string& Type, const std::string& Params)
	{
		if (Type == "CameraSwitch")
		{
			m_LookUpSwitchToggle = !m_LookUpSwitchToggle;

			if (m_LookUpSwitchToggle)
			{
				m_CurrentLookUpCamera = m_LookUpCameraA;
			}
			else
			{
				m_CurrentLookUpCamera = m_LookUpCameraB;
			}
		}
	}
}