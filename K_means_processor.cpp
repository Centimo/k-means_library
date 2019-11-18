//
// Created by centimo on 03.11.2019.
//

#include <cmath>
#include <random>
#include <set>

#include "K_means_processor.h"


using namespace K_means_lib::utils;

void K_means_processor::thread_worker(Thread_data& thread_data)
{
  std::set<size_t> _linked_clusters;
  for (size_t i = thread_data._index; i < _clusters.size(); i += _threads.size())
  {
    _linked_clusters.insert(i);
  }

  std::vector<std::vector<double> > local_clusters_centers;
  local_clusters_centers.resize(_clusters.size(), std::vector<double>(_dimensions_number, 0.0));

  std::vector<size_t> local_clusters_sizes(_clusters.size(), 0);

  boost::dynamic_bitset<> parts_processing_indicators(_clusters[0]._buffer.get_parts_number());
  size_t counter = 0;

  do
  {
    ++counter;
    bool is_changed_local = true;

    for (Point& point : thread_data._points_range.make_linked_range(_points))
    {
      auto previous_cluster = point._cluster;
      double minimal_distance = std::numeric_limits<double>::max();
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

    for (const auto cluster_index : _linked_clusters)
    {
      _clusters[cluster_index]._buffer.clear(0.0);
    }

    synchronize_threads();

    for (size_t i = 0; i < _clusters.size(); ++i)
    {
      local_clusters_sizes[i] = _clusters[i]._size.load(std::memory_order::memory_order_relaxed);
    }

    for (Point& point : thread_data._points_range.make_linked_range(_points))
    {
      for (size_t i = 0; i < _dimensions_number; ++i)
      {
        local_clusters_centers[point._cluster][i] +=
            point._range.make_linked_range(_buffer)[i] / local_clusters_sizes[point._cluster];
      }
    }

    for (size_t i = 0; i < _clusters.size(); ++i)
    {
      _clusters[i]._buffer.atomic_write(
          [this, &local_clusters_centers, i](double& value, size_t index)
          {
            value += local_clusters_centers[i][index];
          },
          parts_processing_indicators);

      local_clusters_centers[i].assign(_dimensions_number, 0.0);
    }

    synchronize_threads();

    for (const auto cluster_index : _linked_clusters)
    {
      _clusters[cluster_index]._size.store(0, std::memory_order::memory_order_relaxed);
    }

    synchronize_threads();
  }
  while (true);

  std::scoped_lock lock(_print_sync);
  std::cout << "Thread id: " << thread_data._index << ", counter: " << counter << std::endl;
}


void K_means_processor::synchronize_threads()
{
  size_t value = _threads_number;

  while (_synchronization_phase_out.load() % _threads_number != 0)
  {  }

  _synchronization_phase_out.compare_exchange_strong(value, 0);

  value = _synchronization_phase_in.fetch_add(1) + 1;
  while (value % _threads_number != 0)
  {
    value = _synchronization_phase_in.load();
  }

  _synchronization_phase_in.compare_exchange_strong(value, 0);

  _synchronization_phase_out.fetch_add(1);
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

K_means_processor::K_means_processor(Buffer<double>&& values_buffer,
                                     size_t points_number,
                                     size_t clusters_number,
                                     size_t threads_number = 1)
  : _dimensions_number(values_buffer.size() / points_number),
    _threads_number(threads_number),
    _buffer(std::forward<Buffer<double> >(values_buffer)),
    _synchronization_phase_in(0),
    _synchronization_phase_out(threads_number)
{
  _points.reserve(points_number);
  for (size_t i = 0; i < points_number; ++i)
  {
    _points.emplace_back(Point{ Range { i * _dimensions_number, _dimensions_number },0 });
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
  if (_dimensions_number < parts_number)
  {
    parts_number = _dimensions_number;
  }

  size_t cluster_index = 0;
  for (const auto& point_index : random_points)
  {
    _clusters.emplace_back(
                _points[point_index]._range.make_linked_range(_buffer),
                parts_number,
                cluster_index,
                0
              );

    ++cluster_index;
  }
}

void K_means_processor::start()
{
  _threads.resize(_threads_number);
  const size_t points_per_thread = _points.size() / _threads_number;
  const size_t threads_with_additional_point = _points.size()  % _threads_number;

  for (size_t i = 0; i < threads_with_additional_point; ++i)
  {
    _threads[i] = std::make_unique<Thread_data>(
        std::ref(*this),
        Range {
          i * (points_per_thread + 1),
          points_per_thread + 1
        },
        i
    );
  }

  for (size_t i = 0; i < _threads_number - threads_with_additional_point; ++i)
  {
    _threads[i + threads_with_additional_point] = std::make_unique<Thread_data>(
        std::ref(*this),
        Range {
            threads_with_additional_point * (points_per_thread + 1) + i * points_per_thread,
            points_per_thread
        },
        i
    );
  }
}

std::vector<K_means_processor::Cluster_result> K_means_processor::get_result()
{
  for (auto& thread : _threads)
  {
    if (thread->_thread.joinable())
    {
      thread->_thread.join();
    }
  }

  std::vector<Cluster_result> result(_clusters.size());

  for (size_t i = 0; i < _clusters.size(); ++i)
  {
    result[i]._center = _clusters[i]._buffer.get_buffer();
  }

  for (size_t i = 0; i < _points.size(); ++i)
  {
    result[_points[i]._cluster]._points.emplace_back(i);
  }

  return result;
}

K_means_processor::~K_means_processor()
{
  for (auto& thread : _threads)
  {
    if (thread->_thread.joinable())
    {
      thread->_thread.join();
    }
  }
}

K_means_processor::Thread_data::Thread_data(K_means_processor& processor, Range&& range, size_t index)
  : _points_range(range),
    _index(index),
    _is_changed(false),
    _thread(&K_means_processor::thread_worker, &processor, std::ref(*this))
{ }
