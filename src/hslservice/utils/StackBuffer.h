#ifndef STACK_BUFFER_H
#define STACK_BUFFER_H

#include <cstdint>
#include <memory>

template <size_t k_buffer_size>
class StackBuffer 
{
public:
    StackBuffer()
    {
        clear();
    }

    StackBuffer(uint8_t* arr, size_t size)
    {
        clear();

        if (arr != nullptr && size > 0)
        {
            writeBytes(arr, size);
        }
    }

    size_t bytesRemaining() 
	{
        return getSize() - m_readPosition;
    }

    void clear() 
	{
        m_readPosition = 0;
        m_writePosition = 0;
        memset(m_buffer, 0, getSize());
    }

    const uint8_t* getBuffer() const
    {
        return m_buffer;
    }

	uint32_t getSize() const
	{ 
		return k_buffer_size; 
	}

    // Read Functions
    uint8_t peekByte() const 
    {
        return read<uint8_t>(m_readPosition);
    }

    uint8_t readByte() 
    {
        return read<uint8_t>();
    }

    uint8_t getByteAt(size_t index) const 
    {
        return getAt<uint8_t>(index);
    }

    void readBytes(uint8_t* buf, size_t len) 
    {
        for (uint32_t i = 0; i < len; i++) 
        {
            buf[i] = read<uint8_t>();
        }
    }

    char readChar()
    {
        return read<char>();
    }

    char getCharAt(size_t index) const 
    {
        return getAt<char>(index);
    }

    double readDouble() 
    {
        return read<double>();
    }

    double getDoubleAt(size_t index) const 
    {
        return getAt<double>(index);
    }

    float readFloat() 
    {
        return read<float>();
    }

    float getFloatAt(size_t index) const 
    {
        return getAt<float>(index);
    }

    uint32_t readInt() 
    {
        return read<uint32_t>();
    }

    uint32_t getIntAt(size_t index) const 
    {
        return read<uint32_t>(index);
    }

    uint64_t readLong() 
    {
        return read<uint64_t>();
    }

    uint64_t getLongAt(size_t index) const 
    {
        return read<uint64_t>(index);
    }

    uint16_t readShort() 
    {
        return read<uint16_t>();
    }

    uint16_t getShortAt(size_t index) const 
    {
        return read<uint16_t>(index);
    }


    // Write Functions
    void write(StackBuffer* src) 
    {
        size_t len = src->size();

        for (size_t i = 0; i < len; i++)
        {
            write<uint8_t>(src->get(i));
        }
    }

    void writeByte(uint8_t b) 
    {
        write<uint8_t>(b);
    }

    void setByteAt(uint8_t b, size_t index) 
    {
        setAt<uint8_t>(b, index);
    }

    void writeBytes(uint8_t* b, size_t len) 
    {
        for (uint32_t i = 0; i < len; i++)
        {
            write<uint8_t>(b[i]);
        }
    }

    void writeBytesAt(uint8_t* b, size_t len, size_t index) 
    {
        m_writePosition = index;

        for (size_t i = 0; i < len; i++)
        {
            write<uint8_t>(b[i]);
        }
    }

    void writeChar(char value) 
    {
        write<char>(value);
    }

    void setCharAt(char value, size_t index) 
    {
        setAt<char>(value, index);
    }

    void writeDouble(double value) 
    {
        write<double>(value);
    }

    void setDoubleAt(double value, size_t index) 
    {
        setAt<double>(value, index);
    }

    void writeFloat(float value) 
    {
        write<float>(value);
    }

    void setFloatAt(float value, size_t index) 
    {
        setAt<float>(value, index);
    }

    void writeInt(uint32_t value) 
    {
        write<uint32_t>(value);
    }

    void setIntAt(uint32_t value, size_t index) 
    {
        setAt<uint32_t>(value, index);
    }

    void writeLong(uint64_t value) 
    {
        write<uint64_t>(value);
    }

    void setLongAt(uint64_t value, size_t index) 
    {
        setAt<uint64_t>(value, index);
    }

    void writeShort(uint16_t value) 
    {
        write<uint16_t>(value);
    }

    void setShortAt(uint16_t value, size_t index) 
    {
        setAt<uint16_t>(value, index);
    }

	// Buffer Position Accessors & Mutators
	void setReadPos(size_t r) 
    {
		m_readPosition = r;
	}

	size_t getReadPos() const 
    {
		return m_readPosition;
	}

	void setWritePos(size_t w) 
    {
		m_writePosition = w;
	}

	size_t getWritePos() const 
    {
		return m_writePosition;
	}

private:
	size_t m_writePosition;
	size_t m_readPosition;
	uint8_t m_buffer[k_buffer_size];

	template<typename T> T read() 
	{
		T data = getAt<T>(m_readPosition);
		m_readPosition += sizeof(T);
		return data;
	}

	template<typename T> T getAt(size_t index) const 
	{
        if (index + sizeof(T) <= getSize())
        {
            return *((T*)&m_buffer[index]);
        }

		return 0;
	}

	template<typename T> void write(T data) 
	{
		size_t s = sizeof(data);

        if (getSize() < (m_writePosition + s))
        {
            assert(false);
            return;
        }

		memcpy(&m_buffer[m_writePosition], (uint8_t*)&data, s);

		m_writePosition += s;
	}

	template<typename T> void setAt(T data, size_t index) 
	{
		if ((index + sizeof(data)) > getSize())
			return;

		memcpy(&m_buffer[index], (uint8_t*) &data, sizeof(data));
		m_writePosition = index + sizeof(data);
	}
};

#endif // STACK_BUFFER_H