#include "frostpch.h"
#include "ScriptEngine.h"

#include "Frost/Script/ScriptEngineRegistry.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/mono-gc.h>

#include "Frost/Utils/FileSystem.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <winioctl.h>

namespace Frost
{
	// =========== STATIC FUNCTIONS ===============
	static MonoAssembly* LoadAssembly(const std::string& path);
	static MonoAssembly* LoadAssemblyFromFile(const char* filepath);

	static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params = nullptr);

	static MonoImage* GetAssemblyImage(MonoAssembly* assembly);
	static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass);
	static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);

	static uint32_t Instantiate(EntityScriptClass& scriptClass);

	static FieldType GetFrostFieldType(MonoType* monoType);
	static std::string GetStringProperty(const char* propertyName, MonoClass* classType, MonoObject* object);
	static void Destroy(uint32_t handle);

	static void PrintAssemblyTypes(MonoAssembly* assembly);
	// ============================================

	/*struct ScriptEngineData
	{
		MonoDomain* RootDomain;
		MonoDomain* AppDomain;

		MonoAssembly* CoreAssembly = nullptr;
	};*/

	//static ScriptEngineData* s_Data = nullptr;

	static MonoDomain* s_CurrentMonoDomain = nullptr;
	static MonoDomain* s_NewMonoDomain = nullptr;

	static std::string s_CoreAssemblyPath;
	static MonoAssembly* s_CoreAssembly = nullptr;
	static MonoAssembly* s_AppAssembly = nullptr;
	
	// Assembly images - those should be global for other files to access them
	MonoImage* s_CoreAssemblyImage = nullptr;
	MonoImage* s_AppAssemblyImage = nullptr;
	
	static MonoMethod* s_ExceptionMethod = nullptr;
	static MonoClass* s_EntityClass = nullptr;

	static HashMap<UUID, HashMap<UUID, EntityInstanceData>> s_EntityInstanceMap; // Scene UUID -> Entity UID -> Entity Instance Data
	static HashMap<std::string, EntityScriptClass> s_EntityClassMap;

	static Scene* s_SceneContext;

	MonoObject* EntityInstance::GetInstance()
	{
		FROST_ASSERT(Handle, "Entity has not been instantiated!");
		return mono_gchandle_get_target(Handle);
	}

	void EntityScriptClass::InitClassMethods(MonoImage* image)
	{

		Constructor = GetMethod(s_CoreAssemblyImage, "Frost.Entity:.ctor(ulong)");
		OnCreateMethod = GetMethod(image, FullName + ":OnCreate()");
		OnUpdateMethod = GetMethod(image, FullName + ":OnUpdate(single)");
		OnDestroyMethod = GetMethod(image, FullName + ":OnDestroy()");
		OnPhysicsUpdateMethod = GetMethod(image, FullName + ":OnPhysicsUpdate(single)");

		// Physics (Entity class)
		OnCollisionBeginMethod = GetMethod(s_CoreAssemblyImage, "Frost.Entity:OnCollisionBegin(ulong)");
		OnCollisionEndMethod = GetMethod(s_CoreAssemblyImage, "Frost.Entity:OnCollisionEnd(ulong)");
		OnTriggerBeginMethod = GetMethod(s_CoreAssemblyImage, "Frost.Entity:OnTriggerBegin(ulong)");
		OnTriggerEndMethod = GetMethod(s_CoreAssemblyImage, "Frost.Entity:OnTriggerEnd(ulong)");
	}

	static bool s_PostLoadCleanup = false;

	void ScriptEngine::Init(const std::string& assemblyPath)
	{
		InitMono();
		LoadFrostRunTimeAssembly(assemblyPath);
		PrintAssemblyTypes(s_CoreAssembly);
	}

	void ScriptEngine::InitMono()
	{
		mono_set_assemblies_path("mono/lib");

		MonoDomain* rootDomain = mono_jit_init("FrostJITRunTime");
		FROST_ASSERT_INTERNAL(rootDomain);
	}

	bool ScriptEngine::LoadFrostRunTimeAssembly(const std::string& path)
	{
		s_CoreAssemblyPath = path;
		if (s_CurrentMonoDomain)
		{
			s_NewMonoDomain = mono_domain_create_appdomain("FrostRuntime", nullptr);
			mono_domain_set(s_NewMonoDomain, false);
			s_PostLoadCleanup = true;
		}
		else
		{
			s_CurrentMonoDomain = mono_domain_create_appdomain("FrostRuntime", nullptr);
			mono_domain_set(s_CurrentMonoDomain, false);
			s_PostLoadCleanup = false;
		}

		s_CoreAssembly = LoadAssembly(s_CoreAssemblyPath);
		if (!s_CoreAssembly)
			return false;

		s_CoreAssemblyImage = GetAssemblyImage(s_CoreAssembly);

		s_ExceptionMethod = GetMethod(s_CoreAssemblyImage, "Frost.RuntimeException:OnException(object)");
		s_EntityClass = mono_class_from_name(s_CoreAssemblyImage, "Frost", "Entity");

		ScriptEngineRegistry::RegisterAll();

		return true;
	}

	/*bool ScriptEngine::LoadCoreAssembly_(const std::string& path)
	{
		s_CoreAssemblyPath = path;



		s_CoreAssembly = LoadAssembly(s_CoreAssemblyPath);
		if (!s_CoreAssembly)
			return false;

		s_ExceptionMethod = GetMethod(s_CoreAssemblyImage, "Frost.RuntimeException:OnException(object)");
		s_EntityClass = mono_class_from_name(s_CoreAssemblyImage, "Frost", "Entity");

		ScriptEngineRegistry::RegisterAll();

		return true;
	}*/

	bool ScriptEngine::LoadAppAssembly(const std::string& path)
	{
		if (s_AppAssembly)
		{
			s_AppAssembly = nullptr;
			s_AppAssemblyImage = nullptr;
			return ReloadAssembly(path);
		}

		auto appAssembly = LoadAssembly(path);
		if (!appAssembly)
			return false;

		auto appAssemblyImage = GetAssemblyImage(appAssembly);

		if (s_PostLoadCleanup)
		{
			mono_domain_unload(s_CurrentMonoDomain);
			s_CurrentMonoDomain = s_NewMonoDomain;
			s_NewMonoDomain = nullptr;
		}

		s_AppAssembly = appAssembly;
		s_AppAssemblyImage = appAssemblyImage;

		PrintAssemblyTypes(s_AppAssembly);

		return true;
	}

	bool ScriptEngine::ReloadAssembly(const std::string& path)
	{
		if (!LoadFrostRunTimeAssembly(s_CoreAssemblyPath))
			return false;

		if (!LoadAppAssembly(path))
			return false;

		if (s_EntityInstanceMap.size())
		{
			Scene* scene = ScriptEngine::GetCurrentSceneContext();
			FROST_ASSERT(scene, "No active scene!");

			// This stores all the deletion functions needed
			// It is explained a bit later why we need this
			Vector<std::function<void(HashMap<UUID, EntityInstanceData>&)>> deleteFunctions;

			auto& entityInstanceMap = s_EntityInstanceMap.find(scene->GetUUID());
			if (entityInstanceMap != s_EntityInstanceMap.end())
			{
				const auto& entityMap = scene->GetEntityMap();

				for (auto& [entityID, entityInstanceData] : entityInstanceMap->second)
				{
					FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

					// If the module (class) was previously alive in the assembly and after reloading it was deleted,
					// then we should delete the entity from the `s_EntityInstanceMap`
					const ScriptComponent& scriptComponent = entityMap.find(entityID)->second.GetComponent<ScriptComponent>();

					UUID entityUUID = UUID(entityID);
					if (!ModuleExists(scriptComponent.ModuleName))
					{
						// If we detected some entites with old modules (which might have to be deleted) 
						// we are earasing that Script Instance Entity class. However we should delay a bit the deletion,
						// because we are going through a loop, and we shouldn't interfere with the members yet.
						// (Only after looping through the whole hashmap, we can erase the members that are useless)
						deleteFunctions.push_back([entityUUID](HashMap<UUID, EntityInstanceData>& entityInstanceMap) {
							entityInstanceMap.erase(entityUUID);
						});
						continue;
					}

					InitScriptEntity(entityMap.at(entityID));
					InstantiateEntityClass(entityMap.at(entityID));
				}
			}

			// After looping, we can finally delete the useless members
			for (auto& func : deleteFunctions)
				func(entityInstanceMap->second);

		}

		return true;
	}

	bool s_FileTimeStapFirstTime = true;
	static std::filesystem::file_time_type s_FileTimestamp;

	void ScriptEngine::OnHotReload(const std::string& path)
	{
		auto currentTimeStamp = FileSystem::GetLastWriteTime(path);

		if (s_FileTimeStapFirstTime)
		{
			s_FileTimestamp = currentTimeStamp;
			s_FileTimeStapFirstTime = false;
		}

		if (s_FileTimestamp != currentTimeStamp)
		{
			FROST_CORE_TRACE("Engine has detected a change in script assembly! The assembly has been reloaded!");
			LoadAppAssembly(path);
			s_FileTimestamp = currentTimeStamp;
		}
	}

	void ScriptEngine::SetSceneContext(Scene* scene)
	{
		s_SceneContext = scene;
	}

	Scene* ScriptEngine::GetCurrentSceneContext()
	{
		return s_SceneContext;
	}

#if 0
	void ScriptEngine::Test(Entity entity)
	{
		auto& entityMap = s_EntityInstanceMap[s_SceneContext->GetUUID()];
		UUID id = entity.GetComponent<IDComponent>().ID;
		EntityInstanceData& entityInstaceData = ScriptEngine::GetEntityInstanceData(entity.GetSceneUUID(), id);


		ScriptComponent& scriptComponent = entity.GetComponent<ScriptComponent>();
		ScriptModuleFieldMap& moduleFieldMap = scriptComponent.ModuleFieldMap;
		if (moduleFieldMap.find(scriptComponent.ModuleName) != moduleFieldMap.end())
		{
			auto& publicFields = moduleFieldMap.at(scriptComponent.ModuleName);
			for (auto& [name, field] : publicFields)
			{
				float a = 5.0f;
				field.SetRuntimeValue(entityInstaceData.Instance, a);

				//switch (field.Type)
				//{
				//case FieldType::Int:
				//{
				//	int value = isRuntime ? field.GetRuntimeValue<int>(entityInstanceData.Instance) : //field.GetStoredValue<int>();
				//	if (UI::Property(field.Name.c_str(), value))
				//	{
				//		if (isRuntime)
				//			field.SetRuntimeValue(entityInstanceData.Instance, value);
				//		else
				//			field.SetStoredValue(value);
				//	}
				//	break;
				//}
			}
		}

		//ScriptModuleFieldMap& moduleFieldMap = sc.ModuleFieldMap;

		MonoMethod* method = GetMethod(s_AppAssemblyImage, "Frost.Player:Print()");
		CallMethod(entityInstaceData.Instance.GetInstance(), method, nullptr);

#if 0
		//entityInstace.ModuleFieldMap["Frost.Player"]
		ScriptComponent& scriptComponent = entity.GetComponent<ScriptComponent>();
		ScriptModuleFieldMap& moduleFieldMap = scriptComponent.ModuleFieldMap;
		auto& fieldMap = moduleFieldMap["Frost.Player"];
		fieldMap["m_Var"].CopyStoredValueToRuntime(entityInstace);
#endif

	}
#endif

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnCreateMethod)
			CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnCreateMethod);
	}

	void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnUpdateMethod)
		{
			void* args[] = { &ts };
			if(entityInstance.ScriptClass->OnUpdateMethod)
				CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnUpdateMethod, args);
		}
	}

	void ScriptEngine::OnDestroyEntity(Entity entity)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnDestroyMethod)
			CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnDestroyMethod);
	}

	void ScriptEngine::OnPhysicsUpdateEntity(Entity entity, float fixedTimeStep)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnUpdateMethod)
		{
			void* args[] = { &fixedTimeStep };
			if(entityInstance.ScriptClass->OnPhysicsUpdateMethod)
				CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnPhysicsUpdateMethod, args);
		}
	}

	void ScriptEngine::OnCollisionBegin(Entity entity, Entity other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnCollisionBeginMethod)
		{
			UUID id = other.GetUUID();
			void* args[] = { &id };
			CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnCollisionBeginMethod, args);
		}
	}

	void ScriptEngine::OnCollisionEnd(Entity entity, Entity other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnCollisionEndMethod)
		{
			UUID id = other.GetUUID();
			void* args[] = { &id };
			CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnCollisionEndMethod, args);
		}
	}

	void ScriptEngine::OnTriggerBegin(Entity entity, Entity other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnTriggerBeginMethod)
		{
			UUID id = other.GetUUID();
			void* args[] = { &id };
			CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnTriggerBeginMethod, args);
		}
	}

	void ScriptEngine::OnTriggerEnd(Entity entity, Entity other)
	{
		EntityInstance& entityInstance = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID()).Instance;
		if (entityInstance.ScriptClass->OnTriggerEndMethod)
		{
			UUID id = other.GetUUID();
			void* args[] = { &id };
			CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnTriggerEndMethod, args);
		}
	}

	bool ScriptEngine::ModuleExists(const std::string& moduleName)
	{
		if (!s_AppAssemblyImage) // No assembly loaded
			return false;

		std::string namespaceName, className;
		if (moduleName.find('.') != std::string::npos)
		{
			namespaceName = moduleName.substr(0, moduleName.find_last_of('.'));
			className = moduleName.substr(moduleName.find_last_of('.') + 1);
		}
		else
		{
			className = moduleName;
		}

		MonoClass* monoClass = mono_class_from_name(s_AppAssemblyImage, namespaceName.c_str(), className.c_str());
		if (!monoClass)
			return false;

		auto isEntitySubclass = mono_class_is_subclass_of(monoClass, s_EntityClass, 0);
		return isEntitySubclass;
	}

	bool ScriptEngine::IsEntityModuleValid(Entity entity)
	{
		return entity.HasComponent<ScriptComponent>() && ModuleExists(entity.GetComponent<ScriptComponent>().ModuleName);
	}

	MonoObject* ScriptEngine::ConstructClass(const std::string& fullName, bool callConstructor, void** parameters)
	{
		std::string namespaceName;
		std::string className;
		std::string parameterList;

		if (fullName.find('.') != std::string::npos)
		{
			namespaceName = fullName.substr(0, fullName.find_first_of('.'));
			className = fullName.substr(fullName.find_first_of('.') + 1, (fullName.find_first_of(':') - fullName.find_first_of('.')) - 1);
		}

		if (fullName.find(":") != std::string::npos)
		{
			parameterList = fullName.substr(fullName.find_first_of(':'));
		}

		MonoClass* klass = mono_class_from_name(s_CoreAssemblyImage, namespaceName.c_str(), className.c_str());
		FROST_ASSERT(klass, "Could not find class!");
		MonoObject* obj = mono_object_new(mono_domain_get(), klass);

		if (callConstructor)
		{
			MonoMethodDesc* desc = mono_method_desc_new(parameterList.c_str(), NULL);
			MonoMethod* constructor = mono_method_desc_search_in_class(desc, klass);
			MonoObject* exception = nullptr;
			mono_runtime_invoke(constructor, obj, parameters, &exception);
		}

		return obj;
	}

	MonoClass* ScriptEngine::GetCoreClass(const std::string& fullName)
	{
		std::string namespaceName;
		std::string className;

		if (fullName.find('.') != std::string::npos)
		{
			namespaceName = fullName.substr(0, fullName.find_last_of('.'));
			className = fullName.substr(fullName.find_last_of('.') + 1);
		}

		MonoClass* klass = mono_class_from_name(s_CoreAssemblyImage, namespaceName.c_str(), className.c_str());

		return klass;

	}

	void ScriptEngine::InitScriptEntity(Entity entity)
	{
		UUID id = entity.GetComponent<IDComponent>().ID;
		ScriptComponent& scriptComponent = entity.GetComponent<ScriptComponent>();

		FROST_CORE_TRACE("Initializing Script Entity ID: {0} (Scene: {1})", id, entity.m_Scene->GetUUID());
		auto& moduleName = scriptComponent.ModuleName;
		if (moduleName.empty())
			return;

		if (!ModuleExists(moduleName))
		{
			FROST_CORE_ERROR("Entity references non-existent script module '{0}'", moduleName);
			return; 
		}

		// Initialize the entity scripting class (with all the construct and methods)
		EntityScriptClass& scriptClass = s_EntityClassMap[moduleName];
		scriptClass.FullName = moduleName;
		if (moduleName.find('.') != std::string::npos)
		{
			scriptClass.NamespaceName = moduleName.substr(0, moduleName.find_last_of('.'));
			scriptClass.ClassName = moduleName.substr(moduleName.find_last_of('.') + 1);
		}
		else
		{
			scriptClass.ClassName = moduleName;
		}

		scriptClass.Class = GetClass(s_AppAssemblyImage, scriptClass);
		scriptClass.InitClassMethods(s_AppAssemblyImage);

		EntityInstanceData& entityInstanceData = s_EntityInstanceMap[entity.m_Scene->GetUUID()][id];
		EntityInstance& entityInstance = entityInstanceData.Instance;
		entityInstance.ScriptClass = &scriptClass;


		// Save old fields
		// The point of this is that if you reload the script entity, then the values
		// of fields and properties will be preserved (so long as the reloaded entity still has those fields, and they
		// are the same type)
		ScriptModuleFieldMap& moduleFieldMap = scriptComponent.ModuleFieldMap;
		auto& fieldMap = moduleFieldMap[moduleName];

		HashMap<std::string, PublicField> oldFields;
		oldFields.reserve(fieldMap.size());
		for (auto& [fieldName, field] : fieldMap)
			oldFields.emplace(fieldName, std::move(field));

		// Default construct an instance of the script class
		// We then use this to set initial values for any public fields that are
		// not already in the fieldMap
		entityInstance.Handle = Instantiate(*entityInstance.ScriptClass);
		
		void* param[] = { &id };
		CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->Constructor, param);

		// Retrieve public fields (TODO: cache these fields if the module is used more than once)
		fieldMap.clear();
		{
			MonoClassField* iter;
			void* ptr = 0;
			while ((iter = mono_class_get_fields(scriptClass.Class, &ptr)) != NULL)
			{
				const char* name = mono_field_get_name(iter);
				uint32_t flags = mono_field_get_flags(iter);
				if ((flags & MONO_FIELD_ATTR_PUBLIC) == 0)
					continue;

				MonoType* fieldType = mono_field_get_type(iter);
				FieldType frostFieldType = GetFrostFieldType(fieldType);

				char* typeName = mono_type_get_name(fieldType);

				auto old = oldFields.find(name);
				if ((old != oldFields.end()) && (old->second.TypeName == typeName))
				{
					fieldMap.emplace(name, std::move(oldFields.at(name)));
					PublicField& field = fieldMap.at(name);
					field.m_MonoClassField = iter;
					continue;
				}

				if (frostFieldType == FieldType::ClassReference)
					continue;

				// TODO: Attributes
				MonoCustomAttrInfo* attr = mono_custom_attrs_from_field(scriptClass.Class, iter);

				PublicField field = { name, typeName, frostFieldType };
				field.m_MonoClassField = iter;
				field.CopyStoredValueFromRuntime(entityInstance);
				fieldMap.emplace(name, std::move(field));
			}
		}

		{
			MonoProperty* iter;
			void* ptr = 0;
			while ((iter = mono_class_get_properties(scriptClass.Class, &ptr)) != NULL)
			{
				const char* propertyName = mono_property_get_name(iter);

				if (oldFields.find(propertyName) != oldFields.end())
				{
					fieldMap.emplace(propertyName, std::move(oldFields.at(propertyName)));
					PublicField& field = fieldMap.at(propertyName);
					field.m_MonoProperty = iter;
					continue;
				}

				MonoMethod* propertySetter = mono_property_get_set_method(iter);
				MonoMethod* propertyGetter = mono_property_get_get_method(iter);

				uint32_t setterFlags = 0;
				uint32_t getterFlags = 0;

				bool isReadOnly = false;
				MonoType* monoType = nullptr;

				if (propertySetter)
				{
					void* i = nullptr;
					MonoMethodSignature* sig = mono_method_signature(propertySetter);
					setterFlags = mono_method_get_flags(propertySetter, nullptr);
					isReadOnly = (setterFlags & MONO_METHOD_ATTR_PRIVATE) != 0;
					monoType = mono_signature_get_params(sig, &i);
				}

				if (propertyGetter)
				{
					MonoMethodSignature* sig = mono_method_signature(propertyGetter);
					getterFlags = mono_method_get_flags(propertyGetter, nullptr);

					if (monoType != nullptr)
						monoType = mono_signature_get_return_type(sig);

					if ((getterFlags & MONO_METHOD_ATTR_PRIVATE) != 0)
						continue;
				}

				if ((setterFlags & MONO_METHOD_ATTR_STATIC) != 0)
					continue;

				FieldType type = GetFrostFieldType(monoType);
				if (type == FieldType::ClassReference)
					continue;

				char* typeName = mono_type_get_name(monoType);

				PublicField field = { propertyName, typeName, type, isReadOnly };
				field.m_MonoProperty = iter;
				field.CopyStoredValueFromRuntime(entityInstance);
				fieldMap.emplace(propertyName, std::move(field));
			}
		}

		// After we set the initial values for any public fields that are
		// not already in the fieldMap, we can delete it.
		Destroy(entityInstance.Handle);
		entityInstance.Handle = 0;
	}

	void ScriptEngine::ShutdownScriptEntity(Entity entity, const std::string& moduleName)
	{
#if 0
		{
			EntityInstanceData& entityInstanceData = GetEntityInstanceData(entity.GetSceneUUID(), entity.GetUUID());
			ScriptModuleFieldMap& moduleFieldMap = entityInstanceData.ModuleFieldMap;
			if (moduleFieldMap.find(moduleName) != moduleFieldMap.end())
				moduleFieldMap.erase(moduleName);
		}
#endif

		{
			//FROST_CORE_WARN("Shut down entity with script module {0}", moduleName);

			ScriptComponent& scriptComponent = entity.GetComponent<ScriptComponent>();
			ScriptModuleFieldMap& moduleFieldMap = scriptComponent.ModuleFieldMap;
			if (moduleFieldMap.find(moduleName) != moduleFieldMap.end())
				moduleFieldMap.erase(moduleName);

			EntityInstanceData& entityInstanceData = GetEntityInstanceData(entity.m_Scene->GetUUID(), entity.GetComponent<IDComponent>().ID);
			Destroy(entityInstanceData.Instance.Handle);
			s_EntityInstanceMap[entity.m_Scene->GetUUID()].erase(entity.GetComponent<IDComponent>().ID);
		}
	}

	void ScriptEngine::ShutdownScene(Scene* scene)
	{
		auto& sceneInstances = s_EntityInstanceMap[scene->GetUUID()];

		if (sceneInstances.size() > 0)
		{
			Vector<Entity> sceneInstanceEntities;
			for (auto& [entityUuid, entityInstanceData] : sceneInstances)
			{
				Entity entity = scene->FindEntityByUUID(entityUuid);
				sceneInstanceEntities.push_back(entity);
			}

			for (auto& entity : sceneInstanceEntities)
			{
				ScriptComponent& scriptComponent = entity.GetComponent<ScriptComponent>();
				
				if (ModuleExists(scriptComponent.ModuleName))
				{
					ShutdownScriptEntity(entity, scriptComponent.ModuleName);
				}
			}
		}

		s_EntityInstanceMap.erase(scene->GetUUID());
	}

	void ScriptEngine::OnScriptComponentDestroyed(UUID sceneID, UUID entityID)
	{
		if (s_EntityInstanceMap.find(sceneID) != s_EntityInstanceMap.end())
		{
			auto& entityMap = s_EntityInstanceMap.at(sceneID);
			FROST_ASSERT_INTERNAL(bool(entityMap.find(entityID) != entityMap.end()));
			if (entityMap.find(entityID) != entityMap.end())
				entityMap.erase(entityID);
		}
	}

	void ScriptEngine::InstantiateEntityClass(Entity entity)
	{
		Scene* scene = entity.m_Scene;
		UUID id = entity.GetComponent<IDComponent>().ID;
		FROST_CORE_TRACE("InstantiateEntityClass {0} ({1})", id, entity.m_Handle);
		ScriptComponent& scriptComponent = entity.GetComponent<ScriptComponent>();

		auto& moduleName = scriptComponent.ModuleName;

		// Firstly, we get the entity instance data from the hashmap.
		// If the entity instance data is not found in hashmap, then we create it and set the public fields
		EntityInstanceData& entityInstanceData = GetEntityInstanceData(scene->GetUUID(), id);
		EntityInstance& entityInstance = entityInstanceData.Instance;
		FROST_ASSERT_INTERNAL(entityInstance.ScriptClass);

		// We create the entity instance handle
		entityInstance.Handle = Instantiate(*entityInstance.ScriptClass);

		// Then we call the entity instance constructor
		void* param[] = { &id };
		CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->Constructor, param);

		// Set all public fields to their appropriate values
		ScriptModuleFieldMap& moduleFieldMap = scriptComponent.ModuleFieldMap;
		if (moduleFieldMap.find(moduleName) != moduleFieldMap.end())
		{
			auto& publicFields = moduleFieldMap.at(moduleName);
			for (auto& [name, field] : publicFields)
				field.CopyStoredValueToRuntime(entityInstance);
		}
	}

	void ScriptEngine::CopyEntityScriptData(Scene* dstScene, Scene* srcScene)
	{
		//FROST_ASSERT_INTERNAL(bool(s_EntityInstanceMap.find(dstScene->GetUUID()) != s_EntityInstanceMap.end()));
		FROST_ASSERT_INTERNAL(bool(s_EntityInstanceMap.find(srcScene->GetUUID()) != s_EntityInstanceMap.end()));

		auto& srcEntityInstanceMap = s_EntityInstanceMap.at(srcScene->GetUUID());


		Vector<UUID> deletedEntityIDs;
		for (auto& [entityID, entityInstanceData] : srcEntityInstanceMap)
		{
			Entity srcEntity = srcScene->FindEntityByUUID(entityID);

			if (srcEntity.HasComponent<ScriptComponent>())
			{
				auto& srcModuleFieldMap = srcEntity.GetComponent<ScriptComponent>().ModuleFieldMap;

				for (auto& [moduleName, srcFieldMap] : srcModuleFieldMap)
				{

					Entity dstEntity = srcScene->FindEntityByUUID(entityID);
					auto& dstModuleFieldMap = srcEntity.GetComponent<ScriptComponent>().ModuleFieldMap;

					for (auto& [fieldName, field] : srcFieldMap)
					{
						FROST_ASSERT_INTERNAL(bool(dstModuleFieldMap.find(moduleName) != dstModuleFieldMap.end()));
						auto& dstFieldMap = dstModuleFieldMap.at(moduleName);
						FROST_ASSERT_INTERNAL(bool(dstFieldMap.find(fieldName) != dstFieldMap.end()));

						//dstFieldMap.at(fieldName).SetStoredValueRaw(field.m_StoredValueBuffer);
					}
				}
			}
			else
			{
				// If the entity doesn't have the ScriptComponent anymore, we should delete it from the EntityInstanceMap
				deletedEntityIDs.push_back(entityID);
				//srcEntityInstanceMap.erase(entityID);
			}
		}

		for (auto& deletedEntityID : deletedEntityIDs)
		{
			srcEntityInstanceMap.erase(deletedEntityID);
		}
	}

	EntityInstanceData& ScriptEngine::GetEntityInstanceData(UUID sceneID, UUID entityID)
	{
		// TODO: Fix this
		auto& entityIDMap = s_EntityInstanceMap[sceneID];

		FROST_ASSERT(bool(s_EntityInstanceMap.find(sceneID) != s_EntityInstanceMap.end()), "Invalid scene ID!");

		// Entity hasn't been initialized yet
		//FROST_ASSERT(bool(entityIDMap.find(entityID) != entityIDMap.end()), "Entity not found!");
		
		if (entityIDMap.find(entityID) == entityIDMap.end())
			ScriptEngine::InitScriptEntity(s_SceneContext->GetEntityMap().at(entityID));

		return entityIDMap.at(entityID);
	}

	EntityInstanceMap& ScriptEngine::GetEntityInstanceMap()
	{
		return s_EntityInstanceMap;
	}

	void ScriptEngine::ShutDown()
	{
		ShutDownMono();
	}

	void ScriptEngine::ShutDownMono()
	{
		s_NewMonoDomain = mono_domain_create_appdomain("FrostRuntime_ShutDown", nullptr);
		mono_domain_set(s_NewMonoDomain, false);

		mono_domain_unload(s_CurrentMonoDomain);

		s_CurrentMonoDomain = nullptr;
		s_NewMonoDomain = nullptr;
	}


	/////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////

	static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params)
	{
		MonoObject* pException = nullptr;
		MonoObject* result = mono_runtime_invoke(method, object, params, &pException);

		if (pException)
		{
			MonoClass* exceptionClass = mono_object_get_class(pException);
			MonoType* exceptionType = mono_class_get_type(exceptionClass);
			const char* typeName = mono_type_get_name(exceptionType);
			std::string message = GetStringProperty("Message", exceptionClass, pException);
			std::string stackTrace = GetStringProperty("StackTrace", exceptionClass, pException);

			FROST_CORE_ERROR("{0}: {1}. Stack Trace: {2}", typeName, message, stackTrace);

			void* args[] = { pException };
			MonoObject* result = mono_runtime_invoke(s_ExceptionMethod, nullptr, args, nullptr);
		}

		return result;
	}

	static uint32_t Instantiate(EntityScriptClass& scriptClass)
	{
		MonoObject* instance = mono_object_new(s_CurrentMonoDomain, scriptClass.Class);
		if (!instance)
			FROST_CORE_ERROR("[ScriptEngine] mono_object_new failed");

		mono_runtime_object_init(instance);
		uint32_t handle = mono_gchandle_new(instance, false);
		return handle;
	}

	static void Destroy(uint32_t handle)
	{
		mono_gchandle_free(handle);
	}

	static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass)
	{
		MonoClass* monoClass = mono_class_from_name(image, scriptClass.NamespaceName.c_str(), scriptClass.ClassName.c_str());
		if (!monoClass)
			FROST_CORE_ERROR("[ScriptEngine] mono_class_from_name failed");

		return monoClass;
	}

	static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc)
	{
		FROST_ASSERT_INTERNAL(image);
		MonoMethodDesc* desc = mono_method_desc_new(methodDesc.c_str(), NULL);
		if (!desc)
			FROST_CORE_ERROR("[ScriptEngine] `mono_method_desc_new` filed: {0}", methodDesc);

		MonoMethod* method = mono_method_desc_search_in_image(desc, image);
		if (!desc)
			FROST_CORE_ERROR("[ScriptEngine] `mono_method_desc_search_in_image` filed: {0}", methodDesc);

		return method;
	}

	static MonoImage* GetAssemblyImage(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		if (!image)
			FROST_ASSERT_MSG("mono_assembly_get_image failed");

		return image;
	}

	static MonoAssembly* LoadAssembly(const std::string& path)
	{
		MonoAssembly* assembly = LoadAssemblyFromFile(path.c_str());

		if (!assembly)
		{
			FROST_ASSERT_MSG("Could not load assembly: {0}", path);
		}
		else
		{
			FROST_CORE_INFO("Successfully loaded assembly: {0}", path);
		}
		
		return assembly;
	}

	static MonoAssembly* LoadAssemblyFromFile(const char* filepath)
	{
		if (filepath == NULL)
		{
			return NULL;
		}

		HANDLE file = CreateFileA(filepath, FILE_READ_ACCESS, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			return NULL;
		}

		DWORD file_size = GetFileSize(file, NULL);
		if (file_size == INVALID_FILE_SIZE)
		{
			CloseHandle(file);
			return NULL;
		}

		void* file_data = malloc(file_size);
		if (file_data == NULL)
		{
			CloseHandle(file);
			return NULL;
		}

		DWORD read = 0;
		ReadFile(file, file_data, file_size, &read, NULL);
		if (file_size != read)
		{
			free(file_data);
			CloseHandle(file);
			return NULL;
		}

		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char*>(file_data), file_size, 1, &status, 0);

		if (status != MONO_IMAGE_OK)
		{
			return NULL;
		}

		auto assemb = mono_assembly_load_from_full(image, filepath, &status, 0);
		free(file_data);
		CloseHandle(file);
		mono_image_close(image);
		return assemb;
	}

	static FieldType GetFrostFieldType(MonoType* monoType)
	{
		int type = mono_type_get_type(monoType);
		switch (type)
		{
			case MONO_TYPE_R4: return FieldType::Float;
			case MONO_TYPE_I4: return FieldType::Int;
			case MONO_TYPE_U4: return FieldType::UnsignedInt;
			case MONO_TYPE_STRING: return FieldType::String;
			case MONO_TYPE_CLASS:
			{
				char* name = mono_type_get_name(monoType);
				//if (strcmp(name, "Frost.Prefab") == 0) return FieldType::Asset;
				if (strcmp(name, "Frost.Entity") == 0) return FieldType::Entity;
				if (strcmp(name, "Frost.Prefab") == 0) return FieldType::Prefab;
				return FieldType::ClassReference;
			}
			case MONO_TYPE_VALUETYPE:
			{
				char* name = mono_type_get_name(monoType);
				if (strcmp(name, "Frost.Vector2") == 0) return FieldType::Vec2;
				if (strcmp(name, "Frost.Vector3") == 0) return FieldType::Vec3;
				if (strcmp(name, "Frost.Vector4") == 0) return FieldType::Vec4;
			}
		}
		return FieldType::None;
	}

	static void PrintAssemblyTypes(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

			FROST_CORE_TRACE("{0}.{1}", nameSpace, name);
		}
	}

	static std::string GetStringProperty(const char* propertyName, MonoClass* classType, MonoObject* object)
	{
		MonoProperty* property = mono_class_get_property_from_name(classType, propertyName);
		MonoMethod* getterMethod = mono_property_get_get_method(property);
		MonoString* string = (MonoString*)mono_runtime_invoke(getterMethod, object, NULL, NULL);
		return string != nullptr ? std::string(mono_string_to_utf8(string)) : "";
	}
}