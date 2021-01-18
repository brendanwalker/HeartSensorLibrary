#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

// Adapted from: https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/
template <typename T>
class CircularBuffer 
{
public:
	explicit CircularBuffer(size_t initial_capacity) :
		m_buffer(new T[initial_capacity]),
		m_capacity(initial_capacity)
	{ 
		memset(m_buffer, 0, sizeof(T)*initial_capacity);
	}

	~CircularBuffer()
	{
		delete[] m_buffer;
	}

	void writeItem(T item)
	{
		m_buffer[m_writeIndex] = item;

		if (m_full)
		{
			m_readIndex = (m_readIndex + 1) % m_capacity;
		}

		m_writeIndex = (m_writeIndex + 1) % m_capacity;

		m_full = m_writeIndex == m_readIndex;
	}

	T readItem()
	{
		if (empty())
		{
			return T();
		}

		//Read data and advance the tail (we now have a free space)
		auto val = m_buffer[m_readIndex];
		m_full = false;
		m_readIndex = (m_readIndex + 1) % m_capacity;

		return val;
	}
	
	void reset()
	{
		m_writeIndex = m_readIndex;
		m_full = false;
	}

	bool isEmpty() const
	{
		//if head and tail are equal, we are empty
		return (!m_full && (m_writeIndex == m_readIndex));
	}

	bool isFull() const
	{
		//If tail is ahead the head by 1, we are full
		return m_full;
	}

	T* getBuffer() const
	{
		return m_buffer;
	}

	size_t getWriteIndex() const 
	{
		return m_writeIndex;
	}

	size_t getReadIndex() const
	{
		return m_readIndex;
	}

	size_t getCapacity() const
	{
		return m_capacity;
	}

	void setCapacity(size_t new_capacity)
	{
		if (new_capacity <= 0 || new_capacity == m_capacity)
			return;

		T *new_buffer = new T[new_capacity];
		memset(new_buffer, 0, sizeof(T)*new_capacity);

		if (!isEmpty())
		{
			// If the write_index has wrapped past the end of the buffer,
			// then we have to copy the buffer in two parts
			if (m_writeIndex < m_readIndex || m_full)
			{
				size_t upper_copy_count = m_capacity - m_readIndex;
				size_t lower_copy_count = m_readIndex;
				size_t new_space_remaining= new_capacity;

				// Copy the upper half to the start of the new buffer
				if (upper_copy_count < new_space_remaining)
				{
					// There is space for the all of the upper half
					std::memmove(new_buffer, &m_buffer[m_readIndex], upper_copy_count);
					new_space_remaining-= upper_copy_count;
				}
				else
				{
					// Not enough space, trim it down to fit
					std::memmove(new_buffer, &m_buffer[m_readIndex], new_capacity);
					new_space_remaining = 0;
				}

				// Copy the lower half next if there is space
				if (new_space_remaining > 0 && lower_copy_count > 0)
				{				
					if (lower_copy_count < new_space_remaining)
					{
						// There is space for all of the lower half
						std::memmove(&new_buffer[upper_copy_count], &m_buffer[0], lower_copy_count);
						new_space_remaining-= lower_copy_count;
					}
					else
					{
						// Not enough space, trim it down to fit
						std::memmove(&new_buffer[upper_copy_count], &m_buffer[0], new_space_remaining);
						new_space_remaining= 0;
					}
				}

				m_readIndex = 0;
				m_writeIndex = (new_capacity - new_space_remaining) % new_capacity;
				m_full= new_space_remaining > 0;
			}
			// If the write_index has not wrapped,
			// Then we can do a simple copy to the new buffer
			else
			{
				// Trim down the buffer if it doesn't fit in the new array
				size_t copy_count = std::min(m_readIndex - m_writeIndex + 1, new_capacity);

				std::memmove(new_buffer, &m_buffer[m_readIndex], copy_count);
				m_readIndex = 0;
				m_writeIndex = copy_count % new_capacity;
				m_full = m_writeIndex == m_readIndex;
			}
		}
		else
		{
			// For empty buffer, snap read and write indices back to start in case the new buffer shrank
			m_readIndex= 0;
			m_writeIndex= 0;
			m_full= false;
		}

		delete[] m_buffer;
		m_buffer = new_buffer;
		m_capacity = new_capacity;
	}

	size_t getSize() const
	{
		size_t size = m_capacity;

		if (!m_full)
		{
			if (m_writeIndex >= m_readIndex)
			{
				size = m_writeIndex - m_readIndex;
			}
			else
			{
				size = m_capacity + m_writeIndex - m_readIndex;
			}
		}

		return size;
	}

private:
	T* m_buffer;
	size_t m_writeIndex = 0;
	size_t m_readIndex = 0;
	bool m_full = false;
	size_t m_capacity;
};

#endif // CIRCULAR_BUFFER_H