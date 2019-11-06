//
// Created by centimo on 04.11.2019.
//

#pragma once

#include <atomic>
#include <vector>
#include <functional>
#include <cmath>


template <template <typename> class Container_type, typename Value_type>
class Linked_range;

template <typename Value_type>
class Buffer : public std::vector<Value_type>
{ };

struct Range
{
  size_t _first_element_index;
  size_t _length;

  template <template <typename> class Container_type, typename Value_type>
  Linked_range<Container_type, Value_type> make_linked_range(Container_type<Value_type>& buffer) const
  {
    return Linked_range<Container_type, Value_type>(*this, buffer);
  }
};

template <template <typename> class Container_type, typename Value_type>
class Linked_range : public Range
{
  Container_type<Value_type>& _buffer;

public:
  Linked_range(const Range& range, Container_type<Value_type>& buffer)
    : Range(range),
      _buffer(buffer)
  {  }

  Value_type& operator[] (size_t index) const
  {
    return _buffer[_first_element_index + _length];
  }

  [[nodiscard]]
  auto begin() const
  {
    return _buffer.begin() + _first_element_index;
  }

  [[nodiscard]]
  auto end() const
  {
    return _buffer.begin() + _first_element_index + _length;
  }
};

template <typename Value_type>
class Atomic_buffer
{
  struct Part
  {
    Linked_range<Buffer, Value_type> _range;
    mutable std::atomic_flag _is_busy;

    Part(const Part& copy)
        : _range(copy._range),
          _is_busy(ATOMIC_FLAG_INIT)
    { }

    explicit
    Part(Linked_range<Buffer, Value_type>&& range)
        : _range(range),
          _is_busy(ATOMIC_FLAG_INIT)
    { }
  };

  static std::vector<Part> make_parts(Buffer<Value_type>& buffer, size_t elements_number, size_t parts_number)
  {
    std::vector<Part> result;
    result.reserve(parts_number);
    const size_t elements_per_part = elements_number / parts_number;
    const size_t parts_with_additional_element = elements_number % parts_number;

    for (size_t i = 0; i < parts_with_additional_element; ++i)
    {
      result.emplace_back(
          Part(
            Linked_range<Buffer, Value_type>(
              Range{ i * (elements_per_part + 1), elements_per_part + 1}, buffer
            )));
    }

    for (size_t i = 0; i < parts_number - parts_with_additional_element; ++i)
    {
      result.emplace_back(
          Part(
            Linked_range<Buffer, Value_type>(
              Range{ parts_with_additional_element * (elements_per_part + 1) + i * elements_per_part, elements_per_part },
                    buffer
            )));
    }

    return result;
  }

public:
  explicit Atomic_buffer(const Linked_range<Buffer, Value_type>& range, size_t parts_number)
    : _elements(range),
      _parts(make_parts(_elements, _elements.size(), parts_number))
  {  }

  void atomic_write(const std::function<void (Value_type&, size_t)>& function)
  {
    std::vector<bool> parts_processing_indicators(_parts.size(), false);

    bool is_done = false;
    while (!is_done)
    {
      is_done = true;

      for (size_t part_index = 0; part_index < parts_processing_indicators.size(); ++part_index)
      {
        Part& part = _parts[part_index];
        if (parts_processing_indicators[part_index])
        {
          continue;
        }

        if (!part._is_busy.test_and_set())
        {
          for (size_t i = 0; i < part._range._length; ++i)
          {
            function(part._range[i], part._range._first_element_index + i);
          }

          parts_processing_indicators[part_index] = true;
          part._is_busy.clear();
        }
        else
        {
          is_done = false;
        }
      }
    }
  }

  auto& operator[] (size_t index)
  {
    return _elements[index];
  }

  const auto& operator[] (size_t index) const
  {
    return _elements[index];
  }

  std::vector<Value_type> get_buffer()
  {
    return std::move(_elements);
  }

  void clear(const Value_type& value)
  {
    _elements.assign(_elements.size(), value);
  }

private:
  std::vector<Part> _parts;
  Buffer<Value_type> _elements;
};


template <>
class Buffer<float> : public std::vector<float>
{
public:
  Buffer() : std::vector<float>() {};
  explicit Buffer(const Linked_range<Buffer, float>& range) : std::vector<float>(range.begin(), range.end()) {};

  float squared_distance_between(
      const Range& first_vector,
      const Atomic_buffer<float>& second_vector)
  {
    float result = 0.0;
    for (size_t i = 0; i < first_vector._length; ++i)
    {
      result += std::pow(first_vector.make_linked_range(*this)[i] - second_vector[i], 2);
    }

    return result;
  }
};

