#include "CVATGenerator.h"
#include <Scene/CSceneController.h>
#include <Object/C3DObject.h>
#include <Message/Console.h>

namespace component
{
	CVATGenerator::CVATGenerator(const std::string& ComponentName, const std::string& RegistryName) :
		CComponent(ComponentName, RegistryName)
	{
	}

	CVATGenerator::~CVATGenerator()
	{
	}

	bool CVATGenerator::OnLoaded(api::IGraphicsAPI* pGraphicsAPI, const std::shared_ptr<scene::CSceneController>& SceneController,
		const std::shared_ptr<object::C3DObject>& Object, const std::shared_ptr<object::CNode>& SelfNode)
	{
		const auto& ClipList = Object->GetAnimationClipList();
		if (ClipList.empty()) return false;

		const auto& TargetClip = ClipList[0];
		TargetClip->SetIsLoop(false); // ループはしない
		TargetClip->SetMaxBlendTime(0.0f); // ブレンドしない

		// 最初のクリップを再生
		const auto& AnimationController = Object->GetAnimationController();
		if (!AnimationController) return false;
		AnimationController->ChangeMotion(0);

		const auto& Skeleton = AnimationController->GetSkeleton();

		// VATを焼く
		// データ整形
		float LocalTime = 0.0f;
		const float FPS = 30.0f;
		const float DeltaTime = 1.0f / FPS;

		std::vector<unsigned char> TextureData;

		int NumOfFrame = 0;

		for (;;)
		{
			if (!AnimationController->IsPlayingAnimation()) break;

			// アニメーションを更新
			if (!AnimationController->Update(DeltaTime)) break;

			// ワールド行列再計算
			Object->CalcWorldMatrix();

			// SkinMatrixを取得
			std::vector<glm::mat4> SkinMatrixList;
			AnimationController->CalCSkinMatrixList(SkinMatrixList, glm::mat4(1.0f));

			// SkinMatrixをテクスチャ用のデータとしてストレージする
			size_t ByteOffset = TextureData.size();
			size_t ByteSize = sizeof(float) * 16 * SkinMatrixList.size();

			TextureData.resize(TextureData.size() + ByteSize);
			std::memcpy(&TextureData[ByteOffset], &SkinMatrixList[0][0], ByteSize);

			// フレーム数
			NumOfFrame++;
		}

		// vec4が1つで1ピクセルとするのでmat4型(つまり1つのSkinMatrix)は4ピクセルで構成される
		// それがボーン数だけ存在するのでそれらを考慮したものがテクスチャの幅となる
		int TextureWidth = static_cast<int>(Skeleton->GetBoneList().size()) * (16 / 4);

		// 縦にボーンのフレームごとのデータが並ぶのでフレーム数がそのままテクスチャの高さになる
		int TextureHeight = NumOfFrame;

		// テクスチャを生成
		graphics::STextureSamplerParam SamplerParam{};
		SamplerParam.FilterMode = graphics::ETextureFilterMode::NEAREST;
		SamplerParam.EnabledAnisotropy = false;

		auto VertexAnimationTexture = pGraphicsAPI->CreateTexture(false, SamplerParam);
		if (!VertexAnimationTexture->Create(TextureData, TextureWidth, TextureHeight, 4, api::ERenderPassFormat::COLOR_FLOAT_RENDERPASS)) return false;

		// オブジェクトに渡す
		int TextureIndex = static_cast<int>(Object->GetTextureSet()->Get2DTextureList().size());
		Object->GetTextureSet()->Add2DTexture(VertexAnimationTexture);

		// マテリアルにアサインする
		for (const auto& Mesh : Object->GetMeshList())
		{
			for (const auto& Primitive : Mesh->GetPrimitiveList())
			{
				for (const auto& Renderer : Primitive->GetRendererList())
				{
					std::get<1>(Renderer)->ReplaceTextureIndex("vertexAnimationTexture", TextureIndex);
					std::get<1>(Renderer)->SetUniformValue("texW", &glm::vec1(static_cast<float>(TextureWidth))[0], sizeof(float));
					std::get<1>(Renderer)->SetUniformValue("texH", &glm::vec1(static_cast<float>(TextureHeight))[0], sizeof(float));
					std::get<1>(Renderer)->SetUniformValue("frameNum", &glm::vec1(static_cast<float>(NumOfFrame))[0], sizeof(float));
					std::get<1>(Renderer)->SetUniformValue("endtime", &glm::vec1(static_cast<float>(TargetClip->GetEndTime()))[0], sizeof(float));
					
					if (!std::get<1>(Renderer)->CreateRefTextureList(Object->GetTextureSet())) return false;
				}
			}
		}

		return true;
	}
}