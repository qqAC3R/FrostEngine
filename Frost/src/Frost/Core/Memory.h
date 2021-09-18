#pragma once

#include <iostream>

namespace Frost
{

	template <typename T>
	class Ref
	{
	public:

		////////////////////////////////////////////////////
		// 	   CONSTRUCTORS
		////////////////////////////////////////////////////

		Ref()
			: m_RefCount(new uint32_t(0)), m_Instance(nullptr) {}

		Ref(T* pointer)
			: m_RefCount(new uint32_t(1)), m_Instance(pointer) {
			//std::cout << "Allocating memory" << std::endl;
		}

		Ref(std::nullptr_t n)
			: m_RefCount(new uint32_t(0)), m_Instance(nullptr) {}

		Ref(const Ref<T>& refPointer)
		{
			m_RefCount = refPointer.m_RefCount;
			m_Instance = refPointer.m_Instance;

			++(*m_RefCount);
		}

		Ref(Ref<T>&& refPointer)
		{
			m_RefCount = refPointer.m_RefCount;
			m_Instance = refPointer.m_Instance;

			refPointer.m_Instance = nullptr;
			refPointer.m_RefCount = nullptr;
		}

		template <typename Ts>
		Ref(const Ref<Ts>& refPointer)
		{
			m_RefCount = refPointer.m_RefCount;
			m_Instance = (T*)refPointer.m_Instance;

			++(*m_RefCount);
		}

		template <typename Ts>
		Ref(Ref<Ts>&& refPointer)
		{
			m_RefCount = refPointer.m_RefCount;
			m_Instance = (T*)refPointer.m_Instance;

			refPointer.m_Instance = nullptr;
			refPointer.m_RefCount = nullptr;
		}



		////////////////////////////////////////////////////
		// 	   OPERATOR `=`
		////////////////////////////////////////////////////
		Ref& operator=(const Ref<T>& refPointer)
		{
			DecreaseRef();

			m_RefCount = refPointer.m_RefCount;
			m_Instance = refPointer.m_Instance;

			if (m_Instance != nullptr)
				++(*m_RefCount);

			return *this;
		}

		Ref& operator=(Ref<T>&& refPointer)
		{
			DecreaseRef();

			m_RefCount = refPointer.m_RefCount;
			m_Instance = refPointer.m_Instance;

			refPointer.m_Instance = nullptr;
			refPointer.m_RefCount = nullptr;

			return *this;
		}


		template <typename Ts>
		Ref& operator=(const Ref<Ts>& refPointer)
		{
			DecreaseRef();

			m_RefCount = refPointer.m_RefCount;
			m_Instance = (T*)refPointer.m_Instance;

			if (m_Instance != nullptr)
				++(*m_RefCount);

			return *this;
		}

		template <typename Ts>
		Ref& operator=(Ref<T>&& refPointer)
		{
			DecreaseRef();

			m_RefCount = refPointer.m_RefCount;
			m_Instance = (T*)refPointer.m_Instance;

			refPointer.m_Instance = nullptr;
			refPointer.m_RefCount = nullptr;


			return *this;
		}




		////////////////////////////////////////////////////
		// 	   FUNCTIONS
		////////////////////////////////////////////////////

		template <typename... Args>
		static Ref<T> Create(Args&&... args)
		{
			return Ref<T>(new T(std::forward<Args>(args)...));
		}

		template<typename T2>
		[[nodiscard]] Ref<T2> As() const
		{
			return Ref<T2>(*this);
		}



		////////////////////////////////////////////////////
		// 	   OPERATORS
		////////////////////////////////////////////////////

		T* operator->() { return m_Instance; }
		const T* operator->() const { return m_Instance; }

		T& operator*() { return *m_Instance; }
		const T& operator*() const { return *m_Instance; }

		T* Raw() { return m_Instance; }
		[[nodiscard]] const T* Raw() const { return m_Instance; }

		void Reset()
		{
			DecreaseRef();
		}

		operator bool() { return m_Instance != nullptr; }
		operator bool() const { return m_Instance != nullptr; }

		~Ref()
		{
			DecreaseRef();
		}
	private:
		void DecreaseRef()
		{
			if (m_RefCount)
			{
				--(*m_RefCount);
				if (*m_RefCount == 0)
				{
					if (m_Instance != nullptr)
						delete m_Instance;
					delete m_RefCount;
				}
			}
		}

	private:
		uint32_t* m_RefCount;
		T* m_Instance;

		friend class Ref;
	};



}