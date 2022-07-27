#pragma once

namespace Frost
{
	namespace RefUtils
	{
		void AddToLiveReferences(void* instance);
		void RemoveFromLiveReferences(void* instance);
		bool IsLive(void* instance);
	}

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
			RefUtils::AddToLiveReferences((void*)m_Instance);
		}

		Ref(std::nullptr_t n)
			: m_RefCount(new uint32_t(0)), m_Instance(nullptr) {}

		Ref(const Ref<T>& refPointer)
		{
			m_RefCount = refPointer.m_RefCount;
			m_Instance = refPointer.m_Instance;

			IncreaseRef();
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

			IncreaseRef();
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
				IncreaseRef();

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
				IncreaseRef();

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
		void IncreaseRef() const
		{
			if (m_Instance)
			{
				++(*m_RefCount);
				RefUtils::AddToLiveReferences((void*)m_Instance);
			}
		}

		void DecreaseRef()
		{
			if (m_RefCount)
			{
				if (*m_RefCount == 0)
				{
					delete m_RefCount;
					return;
				}

				--(*m_RefCount);
				if (*m_RefCount == 0)
				{
					if (m_Instance != nullptr)
					{
						RefUtils::RemoveFromLiveReferences((void*)m_Instance);
						delete m_Instance;
					}
					delete m_RefCount;
				}
			}
		}

	private:
		uint32_t* m_RefCount;
		T* m_Instance;

		friend class Ref;

		template<typename T>
		friend class WeakRef;
	};

	template<typename T>
	class WeakRef
	{
	public:
		WeakRef() = default;

		WeakRef(Ref<T> ref)
		{
			if (ref.Raw() == nullptr) return;

			m_Instance = ref.Raw();
			m_RefCount = ref.m_RefCount;
		}

		WeakRef(std::nullptr_t ptr)
		{
		}

		WeakRef& operator=(Ref<T> ref)
		{
			m_Instance = ref.Raw();
			m_RefCount = ref.m_RefCount;

			return *this;
		}

		WeakRef& operator=(std::nullptr_t ptr)
		{
			return *this;
		}

		template<typename T2>
		[[nodiscard]] Ref<T2> AsRef() const
		{
			Ref<T2> ref = Ref<T2>((T2*)m_Instance);

			// Delete the old ref count just allocated (from constructor)
			delete ref.m_RefCount;

			// Set the ref count that the weak ref previously had
			ref.m_RefCount = m_RefCount;
			++(*m_RefCount);

			return ref;
		}

		bool IsValid() const { return m_Instance != nullptr ? RefUtils::IsLive(m_Instance) : false; }
		operator bool() const { return IsValid(); }
	private:
		uint32_t* m_RefCount = nullptr;
		T* m_Instance = nullptr;
	};

}