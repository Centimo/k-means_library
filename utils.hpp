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
}
