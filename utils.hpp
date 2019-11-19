//
// Created by centimo on 04.11.2019.
//

#pragma once

#include <atomic>
#include <vector>
#include <functional>
#include <cmath>
#include <iostream>

#include <boost/dynamic_bitset.hpp>


namespace K_means_lib::utils
{
  template <typename Value_type>
  class Range
  {
    Value_type& _first_element;
    const size_t _length;

  public:
    Range(Value_type& first_element, size_t length)
        : _first_element(first_element),
          _length(length)
    { }

    inline
    const Value_type& operator[](size_t index) const
    {
      return *(&_first_element + index);
    }

    inline
    Value_type& operator[](size_t index)
    {
      return *(&_first_element + index);
    }

    [[nodiscard]]
    auto begin() const
    {
      return &_first_element;
    }

    [[nodiscard]]
    auto end() const
    {
      return (&_first_element + _length);
    }

    void print() const
    {
      std::cout  << (*this)[0];
      for (size_t i = 1; i < _length; ++i)
      {
        std::cout <<  " " << (*this)[i];
      }

      std::cout  << std::endl;
    };

    size_t size() const
    {
      return _length;
    }
  };

  template <typename Value_type>
  class Atomic_buffer : public std::vector<double>
  {
    struct Part
    {
      Range<Value_type> _range;
      mutable std::atomic_flag _is_busy;

      Part(const Part& copy)
          : _range(copy._range),
            _is_busy(ATOMIC_FLAG_INIT)
      { }

      explicit
      Part(Range<Value_type>&& range)
          : _range(std::forward<Range<Value_type> >(range)),
            _is_busy(ATOMIC_FLAG_INIT)
      { }
    };

    static std::vector<Part> make_parts(std::vector<double>& container, size_t parts_number)
    {
      std::vector<Part> result;
      result.reserve(parts_number);
      const size_t elements_per_part = container.size() / parts_number;
      const size_t parts_with_additional_element = container.size() % parts_number;

      for (size_t i = 0; i < parts_with_additional_element; ++i)
      {
        result.emplace_back(
            Part(
                Range<Value_type>(
                    container[i * (elements_per_part + 1)],
                    elements_per_part + 1
                )));
      }

      for (size_t i = 0; i < parts_number - parts_with_additional_element; ++i)
      {
        result.emplace_back(
            Part(
                Range<Value_type>(
                    container[parts_with_additional_element * (elements_per_part + 1) + i * elements_per_part],
                    elements_per_part
                )));
      }

      return result;
    }

  public:
    explicit Atomic_buffer(std::vector<double>& source, size_t parts_number)
        : std::vector<double>(source)
    {
      _parts = make_parts(*this, parts_number);
    }

    void atomic_write(
        const std::function<void(Value_type&, size_t)>& function,
        boost::dynamic_bitset<>& parts_processing_indicators)
    {
      parts_processing_indicators.reset();

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
            for (size_t i = 0; i < part._range.size(); ++i)
            {
              function(part._range[i], part_index * part._range.size() + i);
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

    std::vector<Value_type> get_buffer()
    {
      return std::move(*this);
    }

    void clear(const Value_type& value)
    {
      (*this).assign(this->size(), value);
    }

    size_t get_parts_number() const
    {
      return _parts.size();
    }

    double squared_distance_between(const std::vector<double>& point, size_t size) const
    {
      double result = 0.0;
      for (size_t i = 0; i < size; ++i)
      {
        result +=
            (point[i] - (*this)[i])
            * (point[i] - (*this)[i]);
      }

      return result;
    }

  private:
    std::vector<Part> _parts;
  };
}
