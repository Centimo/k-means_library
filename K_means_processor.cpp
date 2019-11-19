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
  std::vector<bool> _clusters_linking_table(_clusters.size(), false);
  std::vector<size_t> _linked_clusters;
  for (size_t i = thread_data._index; i < _clusters.size(); i += _threads.size())
  {
    _linked_clusters.push_back(i);
    _clusters_linking_table[i] = true;
  }

  std::vector<size_t> local_clusters_sizes(_clusters.size(), 0);

  size_t counter = 0;

  do
  {
    ++counter;
    bool is_changed_local = false;

    for (Point& point : thread_data._points_range)
    {
      auto previous_cluster = point._cluster;
      double minimal_distance = std::numeric_limits<double>::max();
      for (const auto& cluster : _clusters)
      {
        auto current_distance = squared_distance_between(cluster._center, _buffer[point._index], _dimensions_number);

        if (current_distance < minimal_distance)
        {
          minimal_distance = current_distance;
          point._cluster = cluster._index;
        }
      }

      if (previous_cluster != point._cluster)
      {
        is_changed_local = true;
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
      local_clusters_sizes[cluster_index] = _clusters[cluster_index]._size.load(std::memory_order::memory_order_relaxed);
      _clusters[cluster_index]._center.assign(_dimensions_number, 0.0);
    }

    for (Point& point : _points)
    {
      if (!_clusters_linking_table[point._cluster])
      {
        continue;
      }

      for (size_t i = 0; i < _dimensions_number; ++i)
      {
        _clusters[point._cluster][i] += _buffer[point._index][i] / local_clusters_sizes[point._cluster];
      }
    }

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

  while (_synchronization_phase_out.load(std::memory_order::memory_order_relaxed) % _threads_number != 0)
  {  }

  _synchronization_phase_out.compare_exchange_strong(value, 0);

  value = _synchronization_phase_in.fetch_add(1) + 1;
  while (value % _threads_number != 0)
  {
    value = _synchronization_phase_in.load();
    value = _synchronization_phase_in.load(std::memory_order::memory_order_relaxed);
  }

  _synchronization_phase_in.compare_exchange_strong(value, 0);

  _synchronization_phase_out.fetch_add(1);
}

bool K_means_processor::is_converged()
{
  for (const auto& thread_data : _threads)
  {
    if (thread_data->_is_changed.load(std::memory_order::memory_order_relaxed))
    {
      return false;
    }
  }

  return true;
}

K_means_processor::K_means_processor(std::vector< std::vector<double> >&& values_buffer,
                                     size_t dimensions_number,
                                     size_t points_number,
                                     size_t clusters_number,
                                     size_t threads_number = 1)
  : _dimensions_number(dimensions_number),
    _threads_number(threads_number),
    _buffer(std::forward<std::vector< std::vector<double> > >(values_buffer)),
    _synchronization_phase_in(0),
    _synchronization_phase_out(threads_number)
{
  _points.reserve(points_number);
  for (size_t i = 0; i < points_number; ++i)
  {
    _points.emplace_back(Point{ i, 0 });
  }

  std::mt19937 generator;
  std::uniform_int_distribution<size_t> uniform_distribution(0, points_number - 1);
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
                _buffer[_points[point_index]._index],
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
        Range<Point>(
          _points[i * (points_per_thread + 1)],
          points_per_thread + 1
        ),
        i
    );
  }

  for (size_t i = 0; i < _threads_number - threads_with_additional_point; ++i)
  {
    _threads[i + threads_with_additional_point] = std::make_unique<Thread_data>(
        std::ref(*this),
        Range<Point>(
            _points[threads_with_additional_point * (points_per_thread + 1) + i * points_per_thread],
            points_per_thread
        ),
        i
    );
  }
}

std::vector< std::vector<double> > K_means_processor::get_result()
{
  for (auto& thread : _threads)
  {
    if (thread->_thread.joinable())
    {
      thread->_thread.join();
    }
  }

  std::vector< std::vector<double> > result(_clusters.size());

  for (size_t i = 0; i < _clusters.size(); ++i)
  {
    result[i] = _clusters[i]._center;
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

K_means_processor::Thread_data::Thread_data(K_means_processor& processor, Range<Point>&& range, size_t index)
  : _points_range(range),
    _index(index),
    _is_changed(false),
    _thread(&K_means_processor::thread_worker, &processor, std::ref(*this))
{ }
