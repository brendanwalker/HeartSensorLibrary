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
    { }

    ~CircularBuffer()
    {
        delete[] m_buffer;
    }

    void pushHead(T item)
    {
        m_buffer[m_headIndex] = item;

        if (m_full)
        {
            m_tailIndex = (m_tailIndex + 1) % m_capacity;
        }

        m_headIndex = (m_headIndex + 1) % m_capacity;

        m_full = m_headIndex == m_tailIndex;
    }

    T popTail()
    {
        if (empty())
        {
            return T();
        }

        //Read data and advance the tail (we now have a free space)
        auto val = m_buffer[m_tailIndex];
        m_full = false;
        m_tailIndex = (m_tailIndex + 1) % m_capacity;

        return val;
    }

    void reset()
    {
        m_headIndex = m_tailIndex;
        m_full = false;
    }

    bool isEmpty() const
    {
        //if head and tail are equal, we are empty
        return (!m_full && (m_headIndex == m_tailIndex));
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

    size_t getHeadIndex() const
    {
        return m_headIndex;
    }

    size_t getTailIndex() const
    {
        return m_tailIndex;
    }

    size_t getCapacity() const
    {
        return m_capacity;
    }

    void setCapacity(size_t new_capacity)
    {
        if (new_capacity <= 0 || new_capacity == m_capacity)
            return;

        T* new_buffer = new T[new_capacity];

        if (m_tailIndex < m_headIndex)
        {
            size_t upper_copy_count = std::min(m_capacity - m_headIndex, new_capacity);
            size_t lower_copy_count = std::max(std::min(m_tailIndex + 1, new_capacity - upper_copy_count), 0);

            std::memmove(new_buffer, &m_buffer[m_headIndex], upper_copy_count);

            if (lower_copy_count > 0)
            {
                std::memmove(&new_buffer[upper_copy_count], &m_buffer[0], lower_copy_count);
            }

            m_headIndex = 0;
            m_tailIndex = (upper_copy_count + lower_copy_count) % new_capacity;
        }
        else
        {
            size_t copy_count = std::min(m_tailIndex - m_headIndex + 1, new_capacity);

            std::memmove(new_buffer, &m_buffer[m_headIndex], copy_count);
            m_headIndex = 0;
            m_tailIndex = copy_count % new_capacity;
        }

        delete[] m_buffer;
        m_buffer = new_buffer;
        m_capacity = new_capacity;
        m_full = m_headIndex == m_tailIndex;
    }

    size_t getSize() const
    {
        size_t size = m_capacity;

        if (!m_full)
        {
            if (m_headIndex >= m_tailIndex)
            {
                size = m_headIndex - m_tailIndex;
            }
            else
            {
                size = m_capacity + m_headIndex - m_tailIndex;
            }
        }

        return size;
    }

private:
    T* m_buffer;
    size_t m_headIndex = 0;
    size_t m_tailIndex = 0;
    bool m_full = false;
    const size_t m_capacity;
};

#endif // CIRCULAR_BUFFER_H