
#pragma once

/** A dynamic array that defers allocation to the first modifying access. Also has copy-on-write functionality.
Const references from the array before allocation will all be to the same default constructed value.
It is therefore important that default constructed values are indistinguishable from each other. */
template <typename T>
class cLazyVector
{
	static_assert((!std::is_reference<T>::value && !std::is_array<T>::value),
		"cLazyVector<T>: T must be a value type");
	static_assert(std::is_default_constructible<T>::value,
		"cLazyVector<T>: T must be default constructible");
public:
	using value_type = T;
	using pointer = T *;
	using const_pointer = const T *;
	using reference = T &;
	using const_reference = const T &;
	using size_type = int;
	using iterator = pointer;
	using const_iterator = const_pointer;

	cLazyVector(size_type a_Size) noexcept:
		m_Size{ a_Size }
	{
		ASSERT(a_Size > 0);
	}

	cLazyVector(const cLazyVector & a_Other):
		m_Array{ a_Other.m_Array },
		m_Size{ a_Other.m_Size }
	{
	}

	cLazyVector(cLazyVector && a_Other) noexcept:
		m_Array{ std::move(a_Other.m_Array) },
		m_Size{ a_Other.m_Size }
	{
	}

	cLazyVector & operator = (const cLazyVector & a_Other)
	{
		cLazyVector(a_Other).swap(*this);
		return *this;
	}

	cLazyVector & operator = (cLazyVector && a_Other) noexcept
	{
		m_Array = std::move(a_Other.m_Array);
		m_Size = a_Other.m_Size;
		return *this;
	}

	T & operator [] (size_type a_Idx)
	{
		ASSERT((0 <= a_Idx) && (a_Idx < m_Size));
		return data()[a_Idx];
	}

	const T & operator [] (size_type a_Idx) const
	{
		return GetAt(a_Idx);
	}

	// STL style interface

	const_iterator cbegin() const { return data(); }
	iterator        begin()       { return data(); }
	const_iterator  begin() const { return cbegin(); }

	const_iterator cend() const { return data() + m_Size; }
	iterator        end()       { return data() + m_Size; }
	const_iterator  end() const { return cend(); }

	size_type size() const noexcept { return m_Size; }

	const T * data() const
	{
		if (!IsStorageAllocated())
		{
			m_Array.reset(new std::vector<T>(m_Size));
		}
		return m_Array->data();
	}

	T * data()
	{
		if (m_Array.use_count() > 1)
		{
			// Have to copy-on-write
			auto old = std::move(m_Array);
			m_Array = std::shared_ptr<std::vector<T>>(m_Array.get());
			*m_Array = *old;
		}
		const cLazyVector * const_this = this;
		return const_cast<T *>(const_this->data());
	}

	void swap(cLazyVector & a_Other) noexcept
	{
		std::swap(m_Array, a_Other.m_Array);
		std::swap(m_Size, a_Other.m_Size);
	}

	friend void swap(cLazyVector & a_Lhs, cLazyVector & a_Rhs) noexcept
	{
		a_Lhs.swap(a_Rhs);
	}

	// Extra functions to help avoid allocation

	/** A const view of an element of the array. Never causes the array to allocate. */
	const T & GetAt(size_type a_Idx) const
	{
		ASSERT((0 <= a_Idx) && (a_Idx < m_Size));
		if (IsStorageAllocated())
		{
			return data()[a_Idx];
		}
		else
		{
			static const T DefaultValue;
			return DefaultValue;
		}
	}

	/** Returns true if the array has already been allocated. */
	inline bool IsStorageAllocated() const noexcept { return (bool) m_Array; }

private:
	// Mutable so const data() can allocate the array
	mutable std::shared_ptr<std::vector<T>> m_Array;
	size_type m_Size;
};

/** Same as cLazyVector, but with a length known at compile-time. */
template <typename T, int n>
class cLazyArray
{
	static_assert((!std::is_reference<T>::value && !std::is_array<T>::value),
			"cLazyArray<T>: T must be a value type");
	static_assert(std::is_default_constructible<T>::value,
			"cLazyArray<T>: T must be default constructible");
public:
	using value_type = T;
	using pointer = T *;
	using const_pointer = const T *;
	using reference = T &;
	using const_reference = const T &;
	using size_type = int;
	using iterator = pointer;
	using const_iterator = const_pointer;

	cLazyArray()
	{
	}

	cLazyArray(const cLazyArray & a_Other):
		m_Array{ a_Other.m_Array }
	{
	}

	cLazyArray(cLazyArray && a_Other) noexcept:
		m_Array{ std::move(a_Other.m_Array) }
	{
	}

	cLazyArray & operator = (const cLazyArray & a_Other)
	{
		cLazyArray(a_Other).swap(*this);
		return *this;
	}

	cLazyArray & operator = (cLazyArray && a_Other) noexcept
	{
		m_Array = std::move(a_Other.m_Array);
		return *this;
	}

	T & operator [] (size_type a_Idx)
	{
		ASSERT((0 <= a_Idx) && (a_Idx < n));
		return data()[a_Idx];
	}

	const T & operator [] (size_type a_Idx) const
	{
		return GetAt(a_Idx);
	}

	// STL style interface

	const_iterator cbegin() const { return data(); }
	iterator        begin()       { return data(); }
	const_iterator  begin() const { return cbegin(); }

	const_iterator cend() const { return data() + n; }
	iterator        end()       { return data() + n; }
	const_iterator  end() const { return cend(); }

	size_type size() const noexcept { return n; }

	const T * data() const
	{
		if (!IsStorageAllocated())
		{
			m_Array.reset(new std::vector<T>(n));
		}
		return m_Array->data();
	}

	T * data()
	{
		if (m_Array.use_count() > 1)
		{
			// Have to copy-on-write
			auto old = std::move(m_Array);
			m_Array = std::shared_ptr<std::array<T, n>>();
			*m_Array = *old;
		}
		const cLazyArray * const_this = this;
		return const_cast<T *>(const_this->data());
	}

	void swap(cLazyArray & a_Other) noexcept
	{
		std::swap(m_Array, a_Other.m_Array);
	}

	friend void swap(cLazyArray & a_Lhs, cLazyArray & a_Rhs) noexcept
	{
		a_Lhs.swap(a_Rhs);
	}

	// Extra functions to help avoid allocation

	/** A const view of an element of the array. Never causes the array to allocate. */
	const T & GetAt(size_type a_Idx) const
	{
		ASSERT((0 <= a_Idx) && (a_Idx < n));
		if (IsStorageAllocated())
		{
			return data()[a_Idx];
		}
		else
		{
			static const T DefaultValue;
			return DefaultValue;
		}
	}

	/** Returns true if the array has already been allocated. */
	inline bool IsStorageAllocated() const noexcept { return (bool) m_Array; }

private:
	// Mutable so const data() can allocate the array
	mutable std::shared_ptr<std::array<T, n>> m_Array;
};
