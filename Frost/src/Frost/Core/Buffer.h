#pragma once

#include "Frost/Core/Log.h"

namespace Frost
{

	struct Buffer
	{
		void* Data;
		uint32_t Size;

		Buffer()
			: Data(nullptr), Size(0)
		{
		}

		Buffer(void* data, uint32_t size)
			: Data(data), Size(size)
		{
		}

		void Allocate(uint32_t size)
		{
			delete[] Data;
			Data = nullptr;

			if (size == 0)
				return;

			Data = new Byte[size];
			Size = size;
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		void Initialize()
		{
			if (Data)
				memset(Data, 0, Size);
		}

		template<typename T>
		T& Read(uint32_t offset = 0)
		{
			return *(T*)((Byte*)Data + offset);
		}

		void Write(void* data, uint32_t size, uint32_t offset = 0)
		{
			FROST_ASSERT(offset + size >= Size, "Buffer overflow!");
			memcpy((Byte*)Data + offset, data, size);
		}

		template<typename T>
		T* As()
		{
			return (T*)Data;
		}
		operator bool() const { return Data; }
		inline uint32_t GetSize() const { return Size; }
	};

}