#pragma once

#include <string>
#include <unordered_map>

extern "C" {
	typedef struct _MonoClassField MonoClassField;
	typedef struct _MonoProperty MonoProperty;
}

namespace Frost
{
	enum class FieldType
	{
		None = 0,
		Float,
		Int,
		UnsignedInt,
		String,
		Vec2,
		Vec3,
		Vec4,
		ClassReference,
		Asset,
		Entity
	};

	inline const char* FieldTypeToString(FieldType type)
	{
		switch (type)
		{
			case FieldType::Float:       return "Float";
			case FieldType::Int:         return "Int";
			case FieldType::UnsignedInt: return "UnsignedInt";
			case FieldType::String:      return "String";
			case FieldType::Vec2:        return "Vec2";
			case FieldType::Vec3:        return "Vec3";
			case FieldType::Vec4:        return "Vec4";
			case FieldType::Asset:		 return "Asset";
			case FieldType::Entity:		 return "Entity";
		}
		FROST_ASSERT_MSG("Unknown field type!");
		return "Unknown";
	}

	static uint32_t GetFieldSize(FieldType type)
	{
		switch (type)
		{
			case FieldType::Float:       return 4;
			case FieldType::Int:         return 4;
			case FieldType::UnsignedInt: return 4;
			case FieldType::String:      return 8;
			case FieldType::Vec2:        return 4 * 2;
			case FieldType::Vec3:        return 4 * 3;
			case FieldType::Vec4:        return 4 * 4;
			case FieldType::Asset:       return 8;
			case FieldType::Entity:		 return 8;
		}
		FROST_ASSERT_MSG("Unknown field type!");
		return 0;
	}

	struct EntityInstance;
	class UUID;

	struct PublicField
	{
		std::string Name;
		std::string TypeName;
		FieldType Type;
		bool IsReadOnly;

		PublicField() = default;
		PublicField(const std::string& name, const std::string& typeName, FieldType type, bool isReadOnly = false);

		PublicField(const PublicField& other);
		PublicField(PublicField&& other);
		~PublicField();

		PublicField& operator=(const PublicField& other);
		PublicField& operator=(PublicField&& other) = default;

		void CopyStoredValueToRuntime(EntityInstance& entityInstance);
		void CopyStoredValueFromRuntime(EntityInstance& entityInstance);

		// Get Stored value
		template<typename T>
		T GetStoredValue() const
		{
			T value;
			GetStoredValue_Internal(&value);
			return value;
		}

		template <> // Template specialization
		const std::string& GetStoredValue() const
		{
			return *(std::string*)m_StoredValueBuffer;
		}

		// Set stored value
		template<typename T>
		void SetStoredValue(T value)
		{
			SetStoredValue_Internal(&value);
		}

		template<> // Template specialization
		void SetStoredValue(const std::string& value)
		{
			(*(std::string*)m_StoredValueBuffer).assign(value);
		}

		// Get runtime value
		template<typename T>
		T GetRuntimeValue(EntityInstance& entityInstace) const
		{
			T value;
			GetRuntimeValue_Internal(entityInstace, &value);
			return value;
		}

		template<> // Template specialization
		std::string GetRuntimeValue(EntityInstance& entityInstance) const
		{
			std::string value;
			GetRuntimeValue_Internal(entityInstance, value);
			return value;
		}

		// Set runtime value
		template<typename T>
		void SetRuntimeValue(EntityInstance& entityInstance, T value)
		{
			SetRuntimeValue_Internal(entityInstance, &value);
		}

		template<> // Template specialization
		void SetRuntimeValue(EntityInstance& entityInstance, const std::string& value)
		{
			SetRuntimeValue_Internal(entityInstance, value);
		}

		template<> // Template specialization
		void SetRuntimeValue(EntityInstance& entityInstance, const UUID& value)
		{
			SetRuntimeValue_Internal(entityInstance, value);
		}

#if 0
		void SetStoredValueRaw(void* src);
		void* GetStoredValueRaw() { return m_StoredValueBuffer; }

		void SetRuntimeValueRaw(EntityInstance& entityInstance, void* src);
		void GetRuntimeValueRaw(EntityInstance& entityInstance);
#endif

	private:
		MonoClassField* m_MonoClassField = nullptr;
		MonoProperty* m_MonoProperty = nullptr;
		uint8_t* m_StoredValueBuffer = nullptr;

		uint8_t* AllocateBuffer(FieldType type);

		// Internal Setters
		void SetRuntimeValue_Internal(EntityInstance& entityInstance, void* value);
		void SetRuntimeValue_Internal(EntityInstance& entityInstance, const std::string& value);
		void SetRuntimeValue_Internal(EntityInstance& entityInstance, const UUID& value);
		// Internal Getters
		void GetRuntimeValue_Internal(EntityInstance& entityInstance, void* outValue) const;
		void GetRuntimeValue_Internal(EntityInstance& entityInstance, std::string& outValue) const;
	

		void SetStoredValue_Internal(void* value);
		void GetStoredValue_Internal(void* outValue) const;


		friend class ScriptEngine;

		// This is supposed to copy the string from mono_property_get_value and keep it alive
		static HashMap<std::string, std::string> s_PublicFieldStringValue;
	};

	using ScriptModuleFieldMap = HashMap<std::string, HashMap<std::string, PublicField>>;

}