#pragma once

#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"

#include "ScriptModuleField.h"

extern "C" {
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoImage MonoImage;
}

namespace Frost
{

	//struct EntityScriptClass;

	struct EntityScriptClass
	{
		std::string FullName;
		std::string ClassName;
		std::string NamespaceName;

		MonoClass* Class = nullptr;
		MonoMethod* Constructor = nullptr;
		MonoMethod* OnCreateMethod = nullptr;
		MonoMethod* OnDestroyMethod = nullptr;
		MonoMethod* OnUpdateMethod = nullptr;
		MonoMethod* OnPhysicsUpdateMethod = nullptr;

		// Physics
		MonoMethod* OnCollisionBeginMethod = nullptr;
		MonoMethod* OnCollisionEndMethod = nullptr;
		MonoMethod* OnTriggerBeginMethod = nullptr;
		MonoMethod* OnTriggerEndMethod = nullptr;

		void InitClassMethods(MonoImage* image);
	};

	struct EntityInstance
	{
	public:
		// Responsible for the name/namespace of class
		// and also for constructors/destructors/other internal methods...
		EntityScriptClass* ScriptClass = nullptr;

		// The point of the handle is to get back a MonoObject*
		uint32_t Handle = 0;
		Scene* SceneInstance = nullptr;

		MonoObject* GetInstance();
		bool IsRuntimeAvailable() const
		{
			return Handle != 0;
		}
	};

	struct EntityInstanceData
	{
		EntityInstance Instance;
		//ScriptModuleFieldMap ModuleFieldMap;
	};


	using EntityInstanceMap = HashMap<UUID, HashMap<UUID, EntityInstanceData>>;

	class ScriptEngine
	{
	public:
		static void Init(const std::string& assemblyPath);
		static void ShutDown();

		static bool LoadFrostRunTimeAssembly(const std::string& path);
		static bool LoadAppAssembly(const std::string& path);
		static bool ReloadAssembly(const std::string& path);

		static void OnHotReload(const std::string& path);

		static void SetSceneContext(Scene* scene);
		static Scene* GetCurrentSceneContext();

		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, Timestep ts);
		static void OnDestroyEntity(Entity entity);
		static void OnPhysicsUpdateEntity(Entity entity, float fixedTimeStep);

		static void OnCollisionBegin(Entity entity, Entity other);
		static void OnCollisionEnd(Entity entity, Entity other);
		static void OnTriggerBegin(Entity entity, Entity other);
		static void OnTriggerEnd(Entity entity, Entity other);

		static bool ModuleExists(const std::string& moduleName);

		static bool IsEntityModuleValid(Entity entity);

		static MonoObject* ConstructClass(const std::string& fullName, bool callConstructor, void** parameters);
		static MonoClass* GetCoreClass(const std::string& fullName);

		static void InitScriptEntity(Entity entity);
		static void ShutdownScriptEntity(Entity entity, const std::string& moduleName);
		static void ShutdownScene(Scene* scene);

		static void OnScriptComponentDestroyed(UUID sceneID, UUID entityID);

		static void InstantiateEntityClass(Entity entity);

		static void CopyEntityScriptData(Scene* dstScene, Scene* srcScene);

		static EntityInstanceMap& GetEntityInstanceMap();
		static EntityInstanceData& GetEntityInstanceData(UUID sceneID, UUID entityID);

	private:
		static void InitMono();
		static void ShutDownMono();
	};

}