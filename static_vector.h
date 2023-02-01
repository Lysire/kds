#pragma once

#include <cstddef>
#include <iterator>
#include <new>
#include <type_traits>

namespace ksv
{

    template<typename T, std::size_t N>
    class static_vector
    {
    public:
        // type aliases
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = T *;
        using const_pointer = const T *;
        using iterator = T *;
        using const_iterator = const T *;
        using riterator = std::reverse_iterator<iterator>;
        using const_riterator = std::reverse_iterator<const_iterator>;
        using difference_type = std::ptrdiff_t;
        using size_type = std::size_t;

        // ctors
        static_vector() = default;

        template<typename Iter>
        static_vector(Iter begin, Iter end)
        {
            validate_count(std::distance(begin, end));
            for (auto iter{begin}; iter != end; ++iter)
                pb_internal(*iter);
        }

        static_vector(std::initializer_list<T> list) : static_vector(std::begin(list), std::end(list)){};

        static_vector(size_type count, const T &value)
        {
            validate_count(count);
            for (size_type i{0}; i < count; ++i)
                pb_internal(value);
        }

        static_vector(const static_vector &other)
        {
            // for providing strong exception guarantee
            try
            {
                for (size_t i{0}; i < other.curr_size; ++i)
                    pb_internal(other[i]);
            }
            catch (...)
            {
                clear_elements();
                throw;// make sure exceptions continue propagating
            }
        }

        static_vector(static_vector &&other) noexcept : static_vector()
        {
            other.swap(*this);
        }

        // assignments
        static_vector &operator=(const static_vector &other)
        {
            static_vector tmp{other};
            tmp.swap(*this);
            return *this;
        }

        static_vector &operator=(static_vector &&other) noexcept
        {
            static_vector tmp{std::move(other)};
            tmp.swap(*this);
            return *this;
        }

        // dtor
        ~static_vector()
        {
            clear_elements();
        }

        // non-mutating functions
        [[nodiscard]] bool empty() const { return curr_size == 0; }

        [[nodiscard]] size_type size() const { return curr_size; }

        [[nodiscard]] size_type capacity() const { return N; }

        // validated element access
        const_reference at(size_type pos) const
        {
            validate_index(pos);
            return *cleaned_const_data_ptr(pos);
        }

        reference at(size_type pos)
        {
            validate_index(pos);
            return *cleaned_data_ptr(pos);
        }

        // non-validated element access
        const_reference operator[](size_type pos) const { return *cleaned_const_data_ptr(pos); }

        reference operator[](size_type pos) { return *cleaned_data_ptr(pos); }

        const_reference front() const { return *cleaned_const_data_ptr(); }

        reference front() { return *cleaned_data_ptr(); }

        const_reference back() const { return *cleaned_const_data_ptr(curr_size - 1); }

        reference back() { return *cleaned_data_ptr(curr_size - 1); }

        // iterators
        iterator begin() { return cleaned_data_ptr(); }

        riterator rbegin() { return riterator(end()); }

        const_iterator begin() const { return cleaned_const_data_ptr(); }

        const_iterator rbegin() const { return riterator(end()); }

        iterator end() { return cleaned_data_ptr(curr_size); }

        riterator rend() { return riterator(begin()); }

        const_iterator end() const { return cleaned_const_data_ptr(curr_size); }

        const_riterator rend() const { return riterator(begin()); }

        const_iterator cbegin() const { return begin(); }

        const_riterator crbegin() const { return rbegin(); }

        const_iterator cend() const { return end(); }

        const_riterator crend() const { return rend(); }

        // underlying buffer access
        pointer data() noexcept { return cleaned_data_ptr(); }

        const_pointer data() const noexcept { return cleaned_const_data_ptr(); }

        // mutating functions
        // addition
        void push_back(const_reference value)
        {
            validate_curr_size();
            pb_internal(value);
        }

        void push_back(value_type &&value)
        {
            validate_curr_size();
            mb_internal(std::move(value));
        }

        template<typename... Args>
        void emplace_back(Args &&...args)
        {
            validate_curr_size();
            eb_internal(std::forward<Args>(args)...);
        }

        // removal
        void pop_back()
        {
            --curr_size;
            std::destroy_at(cleaned_data_ptr(curr_size));
        }

        void clear()
        {
            clear_elements();
        }

        // swap
        friend void swap(static_vector &lhs, static_vector &rhs)
        {
            lhs.swap(rhs);
        }

        // comparison operators
        friend inline bool operator==(const static_vector &lhs, const static_vector &rhs)
        {
            return (lhs.size() == rhs.size()) && std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }

        friend inline bool operator!=(const static_vector &lhs, const static_vector &rhs)
        {
            return !(lhs == rhs);
        }

        friend inline bool operator<(const static_vector &lhs, const static_vector &rhs)
        {
            return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
        }

        friend inline bool operator>(const static_vector &lhs, const static_vector &rhs)
        {
            return rhs < lhs;
        }

        friend inline bool operator<=(const static_vector &lhs, const static_vector &rhs)
        {
            return !(lhs > rhs);
        }

        friend inline bool operator>=(const static_vector &lhs, const static_vector &rhs)
        {
            return !(lhs < rhs);
        }

    private:
        // instance fields
        alignas(T) std::byte buffer[sizeof(T) * N];// no objects of type T created yet
        size_type curr_size{0};

        // methods for obtaining (const) pointer to required object
        // A (since we use pointer to object B providing storage for A)
        pointer cleaned_data_ptr(size_t idx = 0) noexcept
        {
            pointer p = std::launder(reinterpret_cast<pointer>(buffer));
            return p + idx;
        }

        const_pointer cleaned_const_data_ptr(size_t idx = 0) const noexcept
        {
            const_pointer p = std::launder(reinterpret_cast<const_pointer>(buffer));
            return p + idx;
        }

        // methods for validation
        void validate_index(size_type index) const
        {
            if (index >= curr_size)
                throw std::out_of_range("Out of Range.");
        }

        void validate_curr_size() const
        {
            if (curr_size > N)
                throw std::length_error("Reached max capacity.");
        }

        void validate_count(size_type count) const
        {
            if (count > N)
                throw std::bad_alloc();
        }

        // for clearing
        void clear_elements()
        {
            pointer cleaned_ptr{cleaned_data_ptr()};
            for (size_t i{0}; i < curr_size; ++i)
                std::destroy_at(cleaned_ptr + (curr_size - 1 - i));// reverse order
            curr_size = 0;
        }

        // internally used modification functions
        void swap(static_vector &other)
        {
            std::swap_ranges(begin(), end(), other.begin());
            std::swap(this->curr_size, other.curr_size);
        }

        void pb_internal(const_reference value)
        {
            ::new (buffer + curr_size * sizeof(T)) T(value);
            ++curr_size;
        }

        void mb_internal(value_type &&value)
        {
            ::new (buffer + curr_size * sizeof(T)) T(std::move(value));
            ++curr_size;
        }

        template<typename... Args>
        void eb_internal(Args &&...args)
        {
            ::new (buffer + curr_size * sizeof(T)) T(std::forward<Args>(args)...);
            ++curr_size;
        }
    };

    // deduction guides
    template<typename T, typename... U>
    static_vector(T, U...) -> static_vector<std::enable_if_t<(std::is_same_v<T, U> && ...), T>, 1 + sizeof...(U)>;

}// namespace ksv
