#include "frostpch.h"
#include "ScriptModuleField.h"

#include "ScriptEngine.h"

#include "Frost/Core/UUID.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/mono-gc.h>

namespace Frost
{

	PublicField::PublicField(const std::string& name, const std::string& typeName, FieldType type, bool isReadOnly)
		: Name(name), TypeName(typeName), Type(type), IsReadOnly(isReadOnly)
	{
		if (Type != FieldType::String)
			m_StoredValueBuffer = AllocateBuffer(Type);
	}

	PublicField::PublicField(const PublicField& other)
		: Name(other.Name), TypeName(other.TypeName), Type(other.Type), IsReadOnly(other.IsReadOnly)
	{
		if (Type != FieldType::String)
		{
			m_StoredValueBuffer = AllocateBuffer(Type);
			memcpy(m_StoredValueBuffer, other.m_StoredValueBuffer, GetFieldSize(Type));
		}
		else
		{
			//uint32_t size = (*(std::string*)other.m_StoredValueBuffer).size();
			//
			//m_StoredValueBuffer = AllocateBuffer(Type);
			//
			//SetStoredValueRaw(other.m_StoredValueBuffer);

			m_StoredString = (*(std::string*)other.m_StoredValueBuffer);
			//const std::string& storedStr = m_StoredString;

			m_StoredValueBuffer = (uint8_t*)&m_StoredString;
			
			//(*(std::string*)m_StoredValueBuffer).assign(storedStr);


			//m_StoredValueBuffer = other.m_StoredValueBuffer;
			//(*(std::string*)m_StoredValueBuffer).assign(*(std::string*)other.m_StoredValueBuffer);

			//m_StoredValueBuffer = AllocateBuffer()
			//strcpy((char*)m_StoredValueBuffer, (const char*)other.m_StoredValueBuffer);
		}

		m_MonoClassField = other.m_MonoClassField;
		m_MonoProperty = other.m_MonoProperty;
	}

	PublicField::PublicField(PublicField&& other)
	{
		Name = std::move(other.Name);
		TypeName = std::move(other.TypeName);
		Type = other.Type;
		IsReadOnly = other.IsReadOnly;
		m_MonoClassField = other.m_MonoClassField;
		m_MonoProperty = other.m_MonoProperty;
		m_StoredValueBuffer = other.m_StoredValueBuffer;

		other.m_MonoClassField = nullptr;
		other.m_MonoProperty = nullptr;
		if (Type != FieldType::String)
			other.m_StoredValueBuffer = nullptr;
		else
		{
			m_StoredString = (*(std::string*)other.m_StoredValueBuffer);
			m_StoredValueBuffer = (uint8_t*)&m_StoredString;

			other.m_StoredString = "";
			other.m_StoredValueBuffer = nullptr;
		}
	}

	PublicField::~PublicField()
	{
		if (Type != FieldType::String)
		{
			delete[] m_StoredValueBuffer;
		}
		else
		{
			//FROST_CORE_WARN("Deleted string! Value: {0}", m_StoredString);
		}

	}

	void PublicField::CopyStoredValueToRuntime(EntityInstance& entityInstance)
	{
#if 1
		FROST_ASSERT_INTERNAL(entityInstance.GetInstance());

		if (IsReadOnly)
			return;

		if (Type == FieldType::ClassReference)
		{
			FROST_ASSERT(!TypeName.empty(), "The type name is wrong!");

			void* params[] = {
				&m_StoredValueBuffer
			};
			MonoObject* obj = ScriptEngine::ConstructClass(TypeName + ":.ctor(intptr)", true, params);
			mono_field_set_value(entityInstance.GetInstance(), m_MonoClassField, obj);
		}
		else if (Type == FieldType::Asset || Type == FieldType::Entity || Type == FieldType::Prefab)
		{
			FROST_ASSERT(!TypeName.empty(), "The type name is wrong!");

			void* params[] = { m_StoredValueBuffer };
			MonoObject* obj = ScriptEngine::ConstructClass(TypeName + ":.ctor(ulong)", true, params);
			if (m_MonoProperty)
			{
				void* data[] = { obj };
				mono_property_set_value(m_MonoProperty, entityInstance.GetInstance(), data, nullptr);
			}
			else
			{
				mono_field_set_value(entityInstance.GetInstance(), m_MonoClassField, obj);
			}
		}
		else if (Type == FieldType::String)
		{
			SetRuntimeValue_Internal(entityInstance, *(std::string*)m_StoredValueBuffer);
		}
		else
		{
			SetRuntimeValue_Internal(entityInstance, (void*)m_StoredValueBuffer);
		}

#endif


	}

	void PublicField::CopyStoredValueFromRuntime(EntityInstance& entityInstance)
	{
		FROST_ASSERT_INTERNAL(entityInstance.GetInstance());

		if (Type == FieldType::String)
		{
			if (m_MonoProperty)
			{
				MonoString* str = (MonoString*)mono_property_get_value(m_MonoProperty, entityInstance.GetInstance(), nullptr, nullptr);
				m_StoredString = mono_string_to_utf8(str);
				m_StoredValueBuffer = (uint8_t*)&m_StoredString;
			}
			else
			{
				MonoString* str;
				mono_field_get_value(entityInstance.GetInstance(), m_MonoClassField, &str);
				m_StoredString = mono_string_to_utf8(str);
				m_StoredValueBuffer = (uint8_t*)&m_StoredString;
			}
		}
		else
		{

			if (m_MonoProperty)
			{
				MonoObject* result = mono_property_get_value(m_MonoProperty, entityInstance.GetInstance(), nullptr, nullptr);
				memcpy(m_StoredValueBuffer, mono_object_unbox(result), GetFieldSize(Type));
			}
			else
			{
				mono_field_get_value(entityInstance.GetInstance(), m_MonoClassField, m_StoredValueBuffer);
			}
		}

	}

	void PublicField::SetStoredValueRaw(void* src)
	{
		if (IsReadOnly)
			return;

		uint32_t size = GetFieldSize(Type);
		memcpy(m_StoredValueBuffer, src, size);
	}

#if 0
	void PublicField::SetRuntimeValueRaw(EntityInstance& entityInstance, void* src)
	{

	}

	void PublicField::GetRuntimeValueRaw(EntityInstance& entityInstance)
	{

	}
#endif

	PublicField& PublicField::operator=(const PublicField& other)
	{
		if (&other != this)
		{
			Name = other.Name;
			TypeName = other.TypeName;
			Type = other.Type;
			IsReadOnly = other.IsReadOnly;
			m_MonoClassField = other.m_MonoClassField;
			m_MonoProperty = other.m_MonoProperty;
			if (Type != FieldType::String)
			{
				m_StoredValueBuffer = AllocateBuffer(Type);
				memcpy(m_StoredValueBuffer, other.m_StoredValueBuffer, GetFieldSize(Type));
			}
			else
			{
				m_StoredValueBuffer = other.m_StoredValueBuffer;
			}
		}

		return *this;
	}

	uint8_t* PublicField::AllocateBuffer(FieldType type)
	{
		uint32_t size = GetFieldSize(type);
		uint8_t* buffer = new uint8_t[size];
		memset(buffer, 0, size);
		return buffer;
	}

	void PublicField::GetStoredValue_Internal(void* outValue) const
	{
		uint32_t size = GetFieldSize(Type);
		memcpy(outValue, m_StoredValueBuffer, size);
	}

	void PublicField::SetStoredValue_Internal(void* value)
	{
		if (IsReadOnly)
			return;

		uint32_t size = GetFieldSize(Type);
		memcpy(m_StoredValueBuffer, value, size);
	}

	void PublicField::SetRuntimeValue_Internal(EntityInstance& entityInstance, void* value)
	{
		FROST_ASSERT_INTERNAL(entityInstance.GetInstance());

		if (IsReadOnly)
			return;

		if (m_MonoProperty)
		{
			void* data[] = { value };
			mono_property_set_value(m_MonoProperty, entityInstance.GetInstance(), data, nullptr);
		}
		else
		{
			mono_field_set_value(entityInstance.GetInstance(), m_MonoClassField, value);
		}

	}

	void PublicField::SetRuntimeValue_Internal(EntityInstance& entityInstance, const std::string& value)
	{
		FROST_ASSERT_INTERNAL(entityInstance.GetInstance());

		if (IsReadOnly)
			return;

		MonoString* monoString = mono_string_new(mono_domain_get(), value.c_str());

		if (m_MonoProperty)
		{
			void* data[] = { monoString };
			mono_property_set_value(m_MonoProperty, entityInstance.GetInstance(), data, nullptr);
		}
		else
		{
			mono_field_set_value(entityInstance.GetInstance(), m_MonoClassField, monoString);
		}
	}

	void PublicField::SetRuntimeValue_Internal(EntityInstance& entityInstance, const UUID& value)
	{
		FROST_ASSERT_INTERNAL(entityInstance.GetInstance());

		if (IsReadOnly)
			return;

		uint64_t entityUUID = value.Get();
		void* params[] = { &entityUUID };
		MonoObject* obj = ScriptEngine::ConstructClass(TypeName + ":.ctor(ulong)", true, params);
		if (m_MonoProperty)
		{
			void* data[] = { obj };
			mono_property_set_value(m_MonoProperty, entityInstance.GetInstance(), data, nullptr);
		}
		else
		{
			mono_field_set_value(entityInstance.GetInstance(), m_MonoClassField, obj);
		}
	}

	void PublicField::GetRuntimeValue_Internal(EntityInstance& entityInstance, void* outValue) const
	{
		FROST_ASSERT_INTERNAL(entityInstance.GetInstance());

		if (Type == FieldType::Entity || Type == FieldType::Prefab)
		{
			MonoObject* obj;
			if (m_MonoProperty)
				obj = mono_property_get_value(m_MonoProperty, entityInstance.GetInstance(), nullptr, nullptr);
			else
				mono_field_get_value(entityInstance.GetInstance(), m_MonoClassField, &obj);

			MonoProperty* idProperty = mono_class_get_property_from_name(entityInstance.ScriptClass->Class, "ID");
			MonoObject* idObject = mono_property_get_value(idProperty, obj, nullptr, nullptr);
			memcpy(outValue, mono_object_unbox(idObject), GetFieldSize(Type));
		}
		else
		{
			if (m_MonoProperty)
			{
				MonoObject* result = mono_property_get_value(m_MonoProperty, entityInstance.GetInstance(), nullptr, nullptr);
				memcpy(outValue, mono_object_unbox(result), GetFieldSize(Type));
			}
			else
			{
				mono_field_get_value(entityInstance.GetInstance(), m_MonoClassField, outValue);
			}
		}
	}

	void PublicField::GetRuntimeValue_Internal(EntityInstance& entityInstance, std::string& outValue) const
	{
		FROST_ASSERT_INTERNAL(entityInstance.GetInstance());

		MonoString* monoString;
		if (m_MonoProperty)
			monoString = (MonoString*)mono_property_get_value(m_MonoProperty, entityInstance.GetInstance(), nullptr, nullptr);
		else
			mono_field_get_value(entityInstance.GetInstance(), m_MonoClassField, &monoString);

		outValue = mono_string_to_utf8(monoString);
	}

#if 0
	void PublicField::GetRuntimeValue_Internal(EntityInstance& entityInstance, void* outValue) const
	{

	}

	void PublicField::GetRuntimeValue_Internal(EntityInstance& entityInstance, std::string& outValue) const
	{

	}
#endif


}