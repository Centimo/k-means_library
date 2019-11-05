//
// Created by centimo on 03.11.2019.
//

#include <cmath>
#include <random>
#include <set>

#include "K_means_processor.h"


void K_means_processor::thread_worker(const Thread_data& thread_data)
{
  do
  {
    bool is_changed_local = true;

    for (Point& point : thread_data._points_range.make_linked_range(_points))
    {
      auto previous_cluster = point._cluster;
      float minimal_distance = std::numeric_limits<float>::max();
      for (const auto& cluster : _clusters)
      {
        auto current_distance =
            _buffer.squared_distance_between(point._range, cluster._buffer);

        if (current_distance < minimal_distance)
        {
          minimal_distance = current_distance;
          point._cluster = cluster._index;
        }
      }

      if (previous_cluster != point._cluster)
      {
        is_changed_local = false;
      }

      _clusters[point._cluster]._size.fetch_add(1, std::memory_order::memory_order_relaxed);
    }

    thread_data._is_changed.store(is_changed_local);

    synchronize_threads();

    if (is_converged())
    {
      break;
    }

    for (Point& point : thread_data._points_range.make_linked_range(_points))
    {
      _clusters[point._cluster]._buffer.atomic_write(
          [this, &point](float& value, size_t index)
          {
            value +=
                point._range.make_linked_range(_buffer)[index]
                / _clusters[point._cluster]._size.load(std::memory_order::memory_order_relaxed);
          });
    }

    synchronize_threads();

    if (thread_data._index < _clusters.size())
    {
      _clusters[thread_data._index]._size.store(0, std::memory_order::memory_order_relaxed);
    }

    synchronize_threads();
  }
  while (true);
}


void K_means_processor::synchronize_threads()
{
  size_t value = 0;

  if (_is_sync_up.load())
  {
    value = _synchronizer.fetch_add(1) + 1;

    while (value != _thread_number)
    {
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      value = _synchronizer.load();
    }

    _is_sync_up.store(false);
  }
  else
  {
    value = _synchronizer.fetch_sub(1) - 1;

    while (value != 0)
    {
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      value = _synchronizer.load();
    }

    _is_sync_up.store(true);
  }
}

bool K_means_processor::is_converged()
{
  for (const auto& thread_data : _threads)
  {
    if (!thread_data->_is_changed.load())
    {
      return false;
    }
  }

  return true;
}

K_means_processor::K_means_processor(Buffer<float>&& values_buffer,
                                     size_t points_number,
                                     size_t clusters_number,
                                     size_t threads_number = 1)
  : _thread_number(threads_number),
    _buffer(values_buffer),
    _synchronizer(0),
    _is_sync_up(false)
{
  size_t range_size = _buffer.size() / points_number;
  _points.reserve(points_number);
  for (size_t i = 0; i < points_number; ++i)
  {
    _points.emplace_back(Point{ Range { i * range_size, range_size },0 });
  }

  std::mt19937 generator;
  std::uniform_int_distribution<size_t> uniform_distribution(0, clusters_number - 1);
  std::set<size_t> random_points;

  do
  {
    random_points.insert(uniform_distribution(generator));
  }
  while (random_points.size() < clusters_number);

  _clusters.reserve(clusters_number);

  size_t parts_number = threads_number * 2;
  if (range_size < parts_number)
  {
    parts_number = range_size;
  }

  for (const auto& point_index : random_points)
  {
    _clusters.emplace_back(
        Cluster {
            Atomic_buffer<float>(_points[point_index]._range.make_linked_range(values_buffer), parts_number),
            0,
            0
        });
  }
}

