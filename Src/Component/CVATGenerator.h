#pragma once

#include <Scriptable/CComponent.h>

namespace object { class CNode; }

namespace component
{
	class CVATGenerator : public scriptable::CComponent
	{
	public:
		CVATGenerator(const std::string& ComponentName, const std::string& RegistryName);
		virtual ~CVATGenerator();

		virtual bool OnLoaded(api::IGraphicsAPI* pGraphicsAPI, const std::shared_ptr<scene::CSceneController>& SceneController,
			const std::shared_ptr<object::C3DObject>& Object, const std::shared_ptr<object::CNode>& SelfNode) override;
	};
}